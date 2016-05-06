/*
 * SessionRnbParser.cpp
 *
 * Copyright (C) 2009-16 by RStudio, Inc.
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */

#include "SessionRmdNotebook.hpp"
#include "SessionRnbParser.hpp"
#include "NotebookChunkDefs.hpp"
#include "NotebookCache.hpp"

#include <core/Base64.hpp>
#include <core/FileSerializer.hpp>
#include <core/SafeConvert.hpp>
#include <core/http/Util.hpp>

#include <session/SessionOptions.hpp>

#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>

using namespace rstudio::core;

namespace rstudio {
namespace session {
namespace modules {
namespace rmarkdown {
namespace notebook {

namespace {

// define parameters for extracting scripts and stylesheets from the .Rnb HTML
struct extractDef
{
   const char* tag;
   const char* attr;
   const char* mimeType;
   const char* extension;
   const char* fmt;
};

struct extractDef extractDefs[2] = 
{ { "script", "src",  "application/x-javascript", "js",  
    "<script src=\"%1%\"></script>\n" },
  { "link",   "href", "text/css",                 "css",
    "<link href=\"%1%\" rel=\"stylesheet\" type=\"text/css\" />\n" } };

Error extractTagAttrs(const std::string& tag,
                      const std::string& attr,
                      const std::string& contents, 
                      std::vector<std::string>* pValues)
{
   std::string::const_iterator pos = contents.begin(); 

   // Not robust to all formulations (e.g. doesn't allow for attributes between
   // the tag and attr, or single-quoted/unquoted attributes), but we only need
   // to parse canonical Pandoc output
   boost::regex re("<\\s*" + tag + "\\s*" + attr + 
                   "\\s*=\\s*\"([^\"]+)\"[^>]*>", boost::regex::icase);

   // Iterate over all matches 
   boost::smatch match;
   while (boost::regex_search(pos, contents.end(), match, re, 
                              boost::match_default))
   {
      // record script src contents
      pValues->push_back(match.str(1));

      // continue search from end of match
      pos = match[0].second;
   }

   return Success();
}

core::Error saveChunkResources(const std::string& contents, 
                             const FilePath& cacheFolder,
                             std::string* pHeader)
{
   const char* dataBase64 = "base64";
   const char* dataUtf8   = "charset=utf-8";

   // ensure we have a folder to save scripts to
   cacheFolder.complete(kChunkLibDir).ensureDirectory();

   int scriptId = 0;
   for (unsigned i = 0; i < sizeof(extractDefs) / sizeof(extractDefs[0]); i++)
   {
      const struct extractDef& extract = extractDefs[i];

      // form the MIME type to extract
      std::string dataMarker("data:");
      dataMarker.append(extract.mimeType);
      dataMarker.append(";");

      // enumerate values from file
      std::vector<std::string> values;
      Error error = extractTagAttrs(extract.tag, extract.attr, contents, 
                                    &values);
      if (error)
         return error;

      BOOST_FOREACH(const std::string& value, values)
      {
         // move to next ID
         scriptId++;

         // ignore non-self-contained scripts
         if (value.substr(0, dataMarker.length()) != dataMarker)
         {
            LOG_WARNING_MESSAGE("Skipping non-self-contained value: " +
                  value.substr(0, std::min(value.length(),
                                           static_cast<size_t>(256))));
            continue;
         }

         // associate with library file
         std::string id = safe_convert::numberToString(scriptId);
         FilePath libFile = cacheFolder.complete(kChunkLibDir).complete(
               "rnb-resource-" + id + "." + extract.extension);

         // ascertain encoding type
         std::string decoded;
         if (value.substr(dataMarker.length(), strlen(dataBase64)) == 
             dataBase64)
         {
            error = base64::decode(value.substr(
                     dataMarker.length() + strlen(dataBase64) + 1, 
                     std::string::npos), &decoded);
            if (error)
            {
               LOG_ERROR(error);
               continue;
            }
         }
         else if (value.substr(dataMarker.length(), strlen(dataUtf8)) ==
                  dataUtf8)
         {
            // URL encoded data -- decode and write as string
            decoded = http::util::urlDecode(value.substr(
                     dataMarker.length() + strlen(dataUtf8) + 1, 
                     std::string::npos));
         }
         else
         {
            LOG_WARNING_MESSAGE("Unrecognized encoded value: " +
                  value.substr(0, std::min(value.length(),
                                           static_cast<size_t>(256))));
            continue;
         }

         error = core::writeStringToFile(libFile, decoded);
         if (error)
         {
            LOG_ERROR(error);
            continue;
         }

         // append to header
         boost::format fmt(extract.fmt);
         pHeader->append(
               boost::str(fmt % (kChunkLibDir "/" + libFile.filename())));
      }
   }

   return Success();
}

core::Error saveChunkHtml(const std::string& chunkId,
                          const std::string& header,
                          const std::string& body, 
                          const FilePath& cacheFolder)
{
   // open the chunk HTML file for writing
   FilePath target = cacheFolder.complete(chunkId + ".html");
   boost::shared_ptr<std::ostream> pStream;
   Error error = target.open_w(&pStream, true);
   if (error)
      return error;

   // extract chunk header includes
   std::string headerContents;
   FilePath headerHtml = options().rResourcesPath().complete("notebook").
      complete("in_header.html");
   error = readStringFromFile(headerHtml, &headerContents);
   if (error)
      return error;

   // append caller specified header
   headerContents.append(header);

   *pStream << 
      "<html>\n"
      "<head>\n" <<
      // TODO: insert dependent scripts
      headerContents << 
      "</head>\n"
      "<body>\n" <<
      body <<
      "</body>\n"
      "</html>\n";

   return Success();
}

core::Error extractChunks(const std::string& contents,
                          const std::string& headerContents,
                          const FilePath& docPath,
                          const FilePath& cacheFolder)
{
   Error error;
   int ordinal = 0;   
   std::string::const_iterator start, pos = contents.begin(); 
   boost::regex re("<!--\\s+rnb-chunk-(\\w+)-(\\d+)\\s+(\\d+)\\s+-->");
   boost::smatch match;
   json::Array chunkDefs;
   while (boost::regex_search(pos, contents.end(), match, re, 
                              boost::match_default))
   {
      int id = safe_convert::stringTo<int>(match.str(2), 0);
      if (match.str(1) == "start") 
      {
         start = match[0].second;
         ordinal = id;
      }
      else if (match.str(1) == "end")
      {
         if (id != ordinal) 
         {
            LOG_WARNING_MESSAGE("Unexpected chunk marker: " + match.str(0) + 
                  " (expected terminator " + safe_convert::numberToString(id) + 
                  ")");
            continue;
         }

         // create the chunk definition
         std::string chunkId("rnbchunk" + match.str(2));
         json::Object chunkDef;
         chunkDef["chunk_id"]  = chunkId;
         chunkDef["row"]       = safe_convert::stringTo<int>(match.str(3), 1) - 1;
         chunkDef["visible"]   = true;
         chunkDef["row_count"] = 1;
         chunkDefs.push_back(chunkDef);

         // save the chunk contents
         error = saveChunkHtml(chunkId, 
               headerContents,
               std::string(start, match[0].first),
               cacheFolder);
         if (error)
            return error;
      }

      // move to the next match
      pos = match[0].second;
   }

   return setChunkDefs(docPath.absolutePath(), "", std::time(NULL), chunkDefs);
}


} // anonymous namespace

core::Error parseRnb(const core::FilePath& rnbFile, 
                     const core::FilePath& cacheFolder)
{
   std::string contents;
   Error error = readStringFromFile(rnbFile, &contents);
   if (error)
      return error;
   error = cacheFolder.ensureDirectory();
   if (error)
      return error;
   std::string header;
   error = saveChunkResources(contents, cacheFolder, &header);
   if (error)
      return error;
   error = extractChunks(contents, header, rnbFile, cacheFolder);
   if (error) 
      return error;

   return Success();
}


} // namespace notebook
} // namespace rmarkdown
} // namespace modules
} // namespace session
} // namespace rstudio


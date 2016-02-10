/*
 * GetActiveDocumentContextDispatchEvent.java
 *
 * Copyright (C) 2009-13 by RStudio, Inc.
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
package org.rstudio.studio.client.events;

import com.google.gwt.event.shared.EventHandler;

import org.rstudio.core.client.js.JavaScriptSerializable;
import org.rstudio.studio.client.application.events.CrossWindowEvent;

@JavaScriptSerializable
public class GetActiveDocumentContextDispatchEvent extends CrossWindowEvent<GetActiveDocumentContextDispatchEvent.Handler>
{
   public interface Handler extends EventHandler
   {
      void onGetActiveDocumentContextDispatch(GetActiveDocumentContextDispatchEvent event);
   }
   
   public GetActiveDocumentContextDispatchEvent()
   {
      this(null);
   }
   
   public GetActiveDocumentContextDispatchEvent(GetActiveDocumentContextEvent event)
   {
      event_ = event;
   }
   
   public GetActiveDocumentContextEvent getEvent()
   {
      return event_;
   }
   
   @Override
   public boolean forward()
   {
      return false;
   }
   
   private final GetActiveDocumentContextEvent event_;
   
   // Boilerplate ----
   
   @Override
   public Type<Handler> getAssociatedType()
   {
      return TYPE;
   }

   @Override
   protected void dispatch(Handler handler)
   {
      handler.onGetActiveDocumentContextDispatch(this);
   }
   
   public static final Type<Handler> TYPE = new Type<Handler>();

}

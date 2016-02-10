/*
 * DataImportFileChooser.java
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

package org.rstudio.studio.client.workbench.views.environment.dataimport;

import org.rstudio.core.client.files.FileSystemItem;
import org.rstudio.core.client.widget.Operation;
import org.rstudio.core.client.widget.ProgressIndicator;
import org.rstudio.core.client.widget.ProgressOperationWithInput;
import org.rstudio.core.client.widget.ThemedButton;
import org.rstudio.studio.client.RStudioGinjector;

import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.Style.Unit;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.dom.client.ClickHandler;
import com.google.gwt.event.logical.shared.ValueChangeEvent;
import com.google.gwt.event.logical.shared.ValueChangeHandler;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.Timer;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.TextBox;
import com.google.gwt.user.client.ui.Widget;

public class DataImportFileChooser extends Composite
{
   private static String browseModeCaption_ = "Browse...";
   private static String updateModeCaption_ = "Update";
   private boolean updateMode_ = false;
   private String lastTextBoxValue_;
   private int checkTextBoxInterval_ = 250;
   private final Operation updateOperation_;
   
   private static DataImportFileChooserUiBinder uiBinder = GWT
         .create(DataImportFileChooserUiBinder.class);
   
   interface DataImportFileChooserUiBinder
         extends UiBinder<Widget, DataImportFileChooser>
   {
   }
   
   public DataImportFileChooser(Operation updateOperation,
                                boolean growTextbox)
   {  
      initWidget(uiBinder.createAndBindUi(this));
      
      updateOperation_ = updateOperation;
      
      if (growTextbox)
      {
         locationTextBox_.getElement().getStyle().setHeight(22, Unit.PX);
         locationTextBox_.getElement().getStyle().setMarginTop(0, Unit.PX);
      }
      
      locationTextBox_.addValueChangeHandler(new ValueChangeHandler<String>()
      {
         @Override
         public void onValueChange(ValueChangeEvent<String> arg0)
         {
         }
      });
      
      actionButton_.addClickHandler(new ClickHandler()
      {
         public void onClick(ClickEvent event)
         {
            if (updateMode_)
            {
               updateOperation_.execute();
            }
            else
            {
               RStudioGinjector.INSTANCE.getFileDialogs().openFile(
                     "Choose File",
                     RStudioGinjector.INSTANCE.getRemoteFileSystemContext(),
                     FileSystemItem.createFile(getText()),
                     new ProgressOperationWithInput<FileSystemItem>()
                     {
                        public void execute(FileSystemItem input,
                                            ProgressIndicator indicator)
                        {
                           if (input == null)
                              return;
   
                           locationTextBox_.setText(input.getPath());
                           preventModeChange();
                           
                           indicator.onCompleted();
                           
                           updateOperation_.execute();
                        }
                     });
            }
         }
      });
      
      checkForTextBoxChange();
   }
   
   public String getText()
   {
      return locationTextBox_.getText();
   }
   
   @Override
   public void onDetach()
   {
      checkTextBoxInterval_ = 0;
   }
   
   public void setFocus()
   {
      locationTextBox_.setFocus(true);
   }
   
   @UiField
   TextBox locationTextBox_;
   
   @UiField
   ThemedButton actionButton_;
   
   private void checkForTextBoxChange()
   {
      if (checkTextBoxInterval_ == 0)
         return;
      
      // Check continuously for changes in the textbox to reliably detect changes even when OS pastes text
      new Timer()
      {
         @Override
         public void run()
         {
            if (lastTextBoxValue_ != null && locationTextBox_.getText() != lastTextBoxValue_)
            {
               switchToUpdateMode(!locationTextBox_.getText().isEmpty());
            }
            
            lastTextBoxValue_ = locationTextBox_.getText();
            checkForTextBoxChange();
         }
      }.schedule(checkTextBoxInterval_);
   }
   
   private void preventModeChange()
   {
      lastTextBoxValue_ = locationTextBox_.getText();
   }
   
   private void switchToUpdateMode(Boolean updateMode)
   {
      if (updateMode_ != updateMode)
      {
         updateMode_ = updateMode;
         if (updateMode)
         {
            actionButton_.setText(updateModeCaption_);
         }
         else
         {
            actionButton_.setText(browseModeCaption_);
         }
      }
   }
}

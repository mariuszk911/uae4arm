#include <algorithm>
#ifdef USE_SDL2
#include <guisan.hpp>
#include <SDL_ttf.h>
#include <guisan/sdl.hpp>
#include <guisan/sdl/sdltruetypefont.hpp>
#else
#include <guichan.hpp>
#include <SDL/SDL_ttf.h>
#include <guichan/sdl.hpp>
#include "sdltruetypefont.hpp"
#endif
#include <iostream>
#include <sstream>
#include "SelectorEntry.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "fsdb.h"
#include "gui.h"
#include "gui_handling.h"

#ifdef ANDROIDSDL
#include "androidsdl_event.h"
#endif

#define DIALOG_WIDTH 520
#define DIALOG_HEIGHT 400

static bool dialogResult = false;
static bool dialogFinished = false;
static bool createNew = false;
static char workingDir[MAX_PATH];
static const char **filefilter;
static bool dialogCreated = false;
static int selectedOnStart = -1;

static gcn::Window *wndSelectFile;
static gcn::Button* cmdOK;
static gcn::Button* cmdCancel;
static gcn::ListBox* lstFiles;
static gcn::ScrollArea* scrAreaFiles;
static gcn::TextField *txtCurrent;
static gcn::Label *lblFilename;
static gcn::TextField *txtFilename;


class SelectFileListModel : public gcn::ListModel
{
  std::vector<std::string> dirs;
  std::vector<std::string> files;

  public:
    SelectFileListModel(const char * path)
    {
      changeDir(path);
    }
      
    int getNumberOfElements()
    {
      return dirs.size() + files.size();
    }
      
    std::string getElementAt(int i)
    {
      if(i >= dirs.size() + files.size() || i < 0)
        return "---";
      if(i < dirs.size())
        return dirs[i];
      return files[i - dirs.size()];
    }
      
    void changeDir(const char *path)
    {
      ReadDirectory(path, &dirs, &files);
      if(dirs.size() == 0)
        dirs.push_back("..");
      FilterFiles(&files, filefilter);
    }

    bool isDir(int i)
    {
      return (i < dirs.size());
    }
};
static SelectFileListModel *fileList;


class FileButtonActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if (actionEvent.getSource() == cmdOK)
      {
        int selected_item;
        selected_item = lstFiles->getSelected();
        if(createNew)
        {
          char tmp[MAX_PATH];
          if(txtFilename->getText().length() <= 0)
            return;
          strncpy(tmp, workingDir, MAX_PATH - 1);
          strncat(tmp, "/", MAX_PATH - 1);
          strncat(tmp, txtFilename->getText().c_str(), MAX_PATH - 1);
          if(strstr(tmp, filefilter[0]) == NULL)
            strncat(tmp, filefilter[0], MAX_PATH - 1);
          if(my_existsfile(tmp) == 1)
            return; // File already exists
          strncpy(workingDir, tmp, MAX_PATH - 1);
          dialogResult = true;
        }
        else
        {
          if(fileList->isDir(selected_item))
            return; // Directory selected -> Ok not possible
          strncat(workingDir, "/", MAX_PATH - 1);
          strncat(workingDir, fileList->getElementAt(selected_item).c_str(), MAX_PATH - 1);
          dialogResult = true;
        }
      }
      dialogFinished = true;
    }
};
static FileButtonActionListener* fileButtonActionListener;


static void checkfoldername (char *current)
{
	char *ptr;
	char actualpath [MAX_PATH];
	DIR *dir;
	
	if (dir = opendir(current))
	{ 
	  fileList->changeDir(current);
	  ptr = realpath(current, actualpath);
	  strncpy(workingDir, ptr, MAX_PATH - 1);
	  closedir(dir);
	}
  else
    strncpy(workingDir, start_path_data, MAX_PATH - 1);
  txtCurrent->setText(workingDir);
}

static void checkfilename(char *current)
{
  char actfile[MAX_PATH];
  extractFileName(current, actfile);
  for(int i=0; i<fileList->getNumberOfElements(); ++i)
  {
    if(!fileList->isDir(i) && !strcasecmp(fileList->getElementAt(i).c_str(), actfile))
    {
      lstFiles->setSelected(i);
      selectedOnStart = i;
      break;
    }
  }
}


class SelectFileActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      int selected_item;
      char foldername[MAX_PATH] = "";

      selected_item = lstFiles->getSelected();
      strncpy(foldername, workingDir, MAX_PATH - 1);
      strncat(foldername, "/", MAX_PATH - 1);
      strncat(foldername, fileList->getElementAt(selected_item).c_str(), MAX_PATH - 1);
      if(fileList->isDir(selected_item))
        checkfoldername(foldername);
      else if(!createNew)
      {
        strncpy(workingDir, foldername, sizeof(workingDir) - 1);
        dialogResult = true;
        dialogFinished = true;
      }
    }
};
static SelectFileActionListener* selectFileActionListener;

#ifdef ANDROID
class EditFilePathActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
       char tmp[MAX_PATH];
       strncpy(tmp, txtCurrent->getText().c_str(), MAX_PATH - 1);
       checkfoldername(tmp);
    }
};
static EditFilePathActionListener* editFilePathActionListener;
#endif

static void InitSelectFile(const char *title)
{
	wndSelectFile = new gcn::Window("Load");
	wndSelectFile->setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
  wndSelectFile->setPosition((GUI_WIDTH - DIALOG_WIDTH) / 2, (GUI_HEIGHT - DIALOG_HEIGHT) / 2);
  wndSelectFile->setBaseColor(gui_baseCol + 0x202020);
  wndSelectFile->setCaption(title);
  wndSelectFile->setTitleBarHeight(TITLEBAR_HEIGHT);
  
  fileButtonActionListener = new FileButtonActionListener();
  
	cmdOK = new gcn::Button("Ok");
	cmdOK->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	cmdOK->setPosition(DIALOG_WIDTH - DISTANCE_BORDER - 2 * BUTTON_WIDTH - DISTANCE_NEXT_X, DIALOG_HEIGHT - 2 * DISTANCE_BORDER - BUTTON_HEIGHT - 10);
  cmdOK->setBaseColor(gui_baseCol + 0x202020);
  cmdOK->addActionListener(fileButtonActionListener);
  
	cmdCancel = new gcn::Button("Cancel");
	cmdCancel->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	cmdCancel->setPosition(DIALOG_WIDTH - DISTANCE_BORDER - BUTTON_WIDTH, DIALOG_HEIGHT - 2 * DISTANCE_BORDER - BUTTON_HEIGHT - 10);
  cmdCancel->setBaseColor(gui_baseCol + 0x202020);
  cmdCancel->addActionListener(fileButtonActionListener);

  txtCurrent = new gcn::TextField();
  txtCurrent->setSize(DIALOG_WIDTH - 2 * DISTANCE_BORDER - 4, TEXTFIELD_HEIGHT);
  txtCurrent->setPosition(DISTANCE_BORDER, 10);
#ifdef ANDROID
  txtCurrent->setEnabled(true);
  editFilePathActionListener =  new EditFilePathActionListener();
  txtCurrent->addActionListener(editFilePathActionListener);
#else
  txtCurrent->setEnabled(false);
#endif

  selectFileActionListener = new SelectFileActionListener();
  fileList = new SelectFileListModel(".");

  lstFiles = new gcn::ListBox(fileList);
  lstFiles->setSize(800, 252);
  lstFiles->setBaseColor(gui_baseCol);
  lstFiles->setWrappingEnabled(true);
  lstFiles->addActionListener(selectFileActionListener);
  
  scrAreaFiles = new gcn::ScrollArea(lstFiles);
#ifdef USE_SDL2
	scrAreaFiles->setBorderSize(1);
#else
  scrAreaFiles->setFrameSize(1);
#endif
  scrAreaFiles->setPosition(DISTANCE_BORDER, 10 + TEXTFIELD_HEIGHT + 10);
  scrAreaFiles->setSize(DIALOG_WIDTH - 2 * DISTANCE_BORDER - 4, 272);
  scrAreaFiles->setScrollbarWidth(20);
  scrAreaFiles->setBaseColor(gui_baseCol + 0x202020);

  if(createNew)
  {
    scrAreaFiles->setSize(DIALOG_WIDTH - 2 * DISTANCE_BORDER - 4, 272 - TEXTFIELD_HEIGHT - DISTANCE_NEXT_Y);
    lblFilename = new gcn::Label("Filename:");
    lblFilename->setSize(80, LABEL_HEIGHT);
    lblFilename->setAlignment(gcn::Graphics::LEFT);
    lblFilename->setPosition(DISTANCE_BORDER, scrAreaFiles->getY() + scrAreaFiles->getHeight() + DISTANCE_NEXT_Y);
    txtFilename = new gcn::TextField();
    txtFilename->setSize(120, TEXTFIELD_HEIGHT);
    txtFilename->setId("Filename");
    txtFilename->setPosition(lblFilename->getX() + lblFilename->getWidth() + DISTANCE_NEXT_X, lblFilename->getY());
    
    wndSelectFile->add(lblFilename);
    wndSelectFile->add(txtFilename);
  }
  
  wndSelectFile->add(cmdOK);
  wndSelectFile->add(cmdCancel);
  wndSelectFile->add(txtCurrent);
  wndSelectFile->add(scrAreaFiles);
  
  gui_top->add(wndSelectFile);
  
  lstFiles->requestFocus();
  lstFiles->setSelected(0);
  wndSelectFile->requestModalFocus();
}


static void ExitSelectFile(void)
{
  wndSelectFile->releaseModalFocus();
  gui_top->remove(wndSelectFile);

  delete cmdOK;
  delete cmdCancel;
  delete fileButtonActionListener;
  
  delete txtCurrent;
  delete lstFiles;
  delete scrAreaFiles;
  delete selectFileActionListener;
#ifdef ANDROID
  delete editFilePathActionListener;
#endif
  delete fileList;
  if(createNew)
  {
    delete lblFilename;
    delete txtFilename;
  }
  
  delete wndSelectFile;
}


static void SelectFileLoop(void)
{
#ifndef USE_SDL2
  FocusBugWorkaround(wndSelectFile);  
#endif
  
  while(!dialogFinished)
  {
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
      if (event.type == SDL_KEYDOWN)
      {
        switch(event.key.keysym.sym)
        {
          case VK_ESCAPE:
            dialogFinished = true;
            break;
            
          case VK_LEFT:
            {
              gcn::FocusHandler* focusHdl = gui_top->_getFocusHandler();
              gcn::Widget* activeWidget = focusHdl->getFocused();
              if(activeWidget == lstFiles)
                cmdCancel->requestFocus();
              else if(activeWidget == cmdCancel)
                cmdOK->requestFocus();
              else if(activeWidget == cmdOK)
                if(createNew)
                  txtFilename->requestFocus();
                else
                  lstFiles->requestFocus();
              else if(activeWidget == txtFilename)
                lstFiles->requestFocus();
              continue;
            }
            break;
            
          case VK_RIGHT:
            {
              gcn::FocusHandler* focusHdl = gui_top->_getFocusHandler();
              gcn::Widget* activeWidget = focusHdl->getFocused();
              if(activeWidget == lstFiles)
                if(createNew)
                  txtFilename->requestFocus();
                else
                  cmdOK->requestFocus();
              else if(activeWidget == txtFilename)
                cmdOK->requestFocus();
              else if(activeWidget == cmdCancel)
                lstFiles->requestFocus();
              else if(activeWidget == cmdOK)
                cmdCancel->requestFocus();
              continue;
            }
            break;

				  case VK_X:
				  case VK_A:
            event.key.keysym.sym = SDLK_RETURN;
            gui_input->pushInput(event); // Fire key down
            event.type = SDL_KEYUP;  // and the key up
            break;
				  default: 
					  break;
        }
      }

      //-------------------------------------------------
      // Send event to guichan/guisan-controls
      //-------------------------------------------------
#ifdef ANDROIDSDL
        androidsdl_event(event, gui_input);
#else
        gui_input->pushInput(event);
#endif
    }

    // Now we let the Gui object perform its logic.
    uae_gui->logic();
    // Now we let the Gui object draw itself.
    uae_gui->draw();
    // Finally we update the screen.
    wait_for_vsync();
#ifdef USE_SDL2
		UpdateGuiScreen();
#else
    SDL_Flip(gui_screen);
#endif
    
    if(!dialogCreated)
    {
      dialogCreated = true;
      if(selectedOnStart >= 0)
        scrAreaFiles->setVerticalScrollAmount(selectedOnStart * 19);
    }
  }  
}

static int Already_init = 0;

bool SelectFile(const char *title, char *value, const char *filter[], bool create)
{
  dialogResult = false;
  dialogFinished = false;
  createNew = create;
  filefilter = filter;
  dialogCreated = false;
  selectedOnStart = -1;
  
	if (Already_init == 0)
	{
    InitSelectFile(title);
		Already_init = 1;
	}
	else
	{
		strncpy(value, workingDir, MAX_PATH - 1);
		gui_top->add(wndSelectFile);
		wndSelectFile->setCaption(title);
		wndSelectFile->requestModalFocus();
		wndSelectFile->setVisible(true);
		gui_top->moveToTop(wndSelectFile);
	}

  extractPath(value, workingDir);
  checkfoldername(workingDir);
  checkfilename(value);
  
  SelectFileLoop();
	wndSelectFile->releaseModalFocus();
	wndSelectFile->setVisible(false);
  
  if(dialogResult)
    strncpy(value, workingDir, MAX_PATH - 1);
	else
		strncpy(workingDir, value, MAX_PATH - 1);
  return dialogResult;
}

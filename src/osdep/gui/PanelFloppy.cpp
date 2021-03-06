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
#include "SelectorEntry.hpp"
#include "UaeRadioButton.hpp"
#include "UaeDropDown.hpp"
#include "UaeCheckBox.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "include/memory-uae.h"
#include "disk.h"
#include "uae.h"
#include "gui.h"
#include "gui_handling.h"


static gcn::UaeCheckBox* chkDFx[4];
static gcn::UaeDropDown* cboDFxType[4];
static gcn::UaeCheckBox* chkDFxWriteProtect[4];
static gcn::Button* cmdDFxInfo[4];
static gcn::Button* cmdDFxEject[4];
static gcn::Button* cmdDFxSelect[4];
static gcn::UaeDropDown* cboDFxFile[4];
static gcn::Label* lblDriveSpeed;
static gcn::Label* lblDriveSpeedInfo;
static gcn::Slider* sldDriveSpeed;
static gcn::UaeCheckBox* chkLoadConfig;
static gcn::Button *cmdSaveForDisk;
static gcn::Button *cmdCreateDDDisk;
static gcn::Button *cmdCreateHDDisk;

static const char *diskfile_filter[] = { ".adf", ".adz", ".fdi", ".zip", ".dms", ".gz", ".xz", "\0" };
static const char *drivespeedlist[] = { "100% (compatible)", "200%", "400%", "800%" };
static const int drivespeedvalues[] = { 100, 200, 400, 800 };

static void AdjustDropDownControls(void);
static bool bLoadConfigForDisk = false;
static bool bIgnoreListChange = false;


class DriveTypeListModel : public gcn::ListModel
{
  private:
    std::vector<std::string> types;
      
  public:
    DriveTypeListModel()
    {
      types.push_back("Disabled");
      types.push_back("3.5'' DD");
      types.push_back("3.5'' HD");
      types.push_back("5.25'' SD");
      types.push_back("3.5'' ESCOM");
    }

    int getNumberOfElements()
    {
      return types.size();
    }

    std::string getElementAt(int i)
    {
      if(i < 0 || i >= types.size())
        return "---";
      return types[i];
    }
};
static DriveTypeListModel driveTypeList;


class DiskfileListModel : public gcn::ListModel
{
  public:
    DiskfileListModel()
    {
    }
    
    int getNumberOfElements()
    {
      return lstMRUDiskList.size();
    }

    std::string getElementAt(int i)
    {
      if(i < 0 || i >= lstMRUDiskList.size())
        return "---";
      return lstMRUDiskList[i];
    }
};
static DiskfileListModel diskfileList;


class DriveTypeActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    //---------------------------------------
	    // New drive type selected
	    //---------------------------------------
	    for(int i=0; i<4; ++i)
	    {
	      if (actionEvent.getSource() == cboDFxType[i])
          changed_prefs.floppyslots[i].dfxtype = cboDFxType[i]->getSelected() - 1;
      }
      RefreshPanelFloppy();
      RefreshPanelQuickstart();
    }
};
static DriveTypeActionListener* driveTypeActionListener;


class DFxCheckActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    if(actionEvent.getSource() == chkLoadConfig)
	      bLoadConfigForDisk = chkLoadConfig->isSelected();
	    else
      {
  	    for(int i=0; i<4; ++i)
  	    {
  	      if (actionEvent.getSource() == chkDFx[i])
          {
      	    //---------------------------------------
            // Drive enabled/disabled
      	    //---------------------------------------
            if(chkDFx[i]->isSelected())
              changed_prefs.floppyslots[i].dfxtype = DRV_35_DD;
            else
              changed_prefs.floppyslots[i].dfxtype = DRV_NONE;
          }
          else if(actionEvent.getSource() == chkDFxWriteProtect[i])
          {
      	    //---------------------------------------
            // Write-protect changed
      	    //---------------------------------------
            disk_setwriteprotect (&changed_prefs, i, changed_prefs.floppyslots[i].df, chkDFxWriteProtect[i]->isSelected());
            if(disk_getwriteprotect(&changed_prefs, changed_prefs.floppyslots[i].df) != chkDFxWriteProtect[i]->isSelected()) {
              // Failed to change write protection -> maybe filesystem doesn't support this
              chkDFxWriteProtect[i]->setSelected(!chkDFxWriteProtect[i]->isSelected());
              ShowMessage("Set/Clear write protect", "Failed to change write permission.", "Maybe underlying filesystem doesn't support this.", "Ok", "");
            }
            DISK_reinsert(i);
          }
        }
      }
      RefreshPanelFloppy();
      RefreshPanelQuickstart();
    }
};
static DFxCheckActionListener* dfxCheckActionListener;


class DFxButtonActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    for(int i=0; i<4; ++i)
	    {
	      if (actionEvent.getSource() == cmdDFxInfo[i])
        {
    	    //---------------------------------------
          // Show info about current disk-image
    	    //---------------------------------------
          if(changed_prefs.floppyslots[i].dfxtype != DRV_NONE && strlen(changed_prefs.floppyslots[i].df) > 0)
            ; // ToDo: Show info dialog
        }
        else if (actionEvent.getSource() == cmdDFxEject[i])
        {
    	    //---------------------------------------
          // Eject disk from drive
    	    //---------------------------------------
          disk_eject(i);
          strncpy(changed_prefs.floppyslots[i].df, "", MAX_DPATH - 1);
          AdjustDropDownControls();
        }
        else if (actionEvent.getSource() == cmdDFxSelect[i])
        {
    	    //---------------------------------------
          // Select disk for drive
    	    //---------------------------------------
    	    char tmp[MAX_PATH];

    	    if(strlen(changed_prefs.floppyslots[i].df) > 0)
    	      strncpy(tmp, changed_prefs.floppyslots[i].df, MAX_PATH - 1);
    	    else
    	      strncpy(tmp, currentDir, MAX_PATH - 1);
    	    if(SelectFile("Select disk image file", tmp, diskfile_filter))
  	      {
      	    if(strncmp(changed_prefs.floppyslots[i].df, tmp, MAX_PATH))
      	    {
        	    strncpy(changed_prefs.floppyslots[i].df, tmp, sizeof(changed_prefs.floppyslots[i].df) - 1);
        	    disk_insert(i, tmp);
        	    AddFileToDiskList(tmp, 1);
        	    extractPath(tmp, currentDir);

        	    if(i == 0 && chkLoadConfig->isSelected())
      	      {
      	        // Search for config of disk
      	        extractFileName(changed_prefs.floppyslots[i].df, tmp);
      	        removeFileExtension(tmp);
      	        LoadConfigByName(tmp);
      	      }
              AdjustDropDownControls();
      	    }
  	      }
  	      cmdDFxSelect[i]->requestFocus();
        }
      }
      RefreshPanelFloppy();
      RefreshPanelQuickstart();
    }
};
static DFxButtonActionListener* dfxButtonActionListener;


class DiskFileActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    for(int i=0; i<4; ++i)
	    {
	      if (actionEvent.getSource() == cboDFxFile[i])
        {
    	    //---------------------------------------
          // Diskimage from list selected
    	    //---------------------------------------
    	    if(!bIgnoreListChange)
  	      {
      	    int idx = cboDFxFile[i]->getSelected();

      	    if(idx < 0)
    	      {
              disk_eject(i);
              strncpy(changed_prefs.floppyslots[i].df, "", MAX_DPATH - 1);
              AdjustDropDownControls();
    	      }
    	      else
      	    {
        	    if(diskfileList.getElementAt(idx).compare(changed_prefs.floppyslots[i].df))
    	        {
          	    strncpy(changed_prefs.floppyslots[i].df, diskfileList.getElementAt(idx).c_str(), sizeof(changed_prefs.floppyslots[i].df) - 1);
          	    disk_insert(i, changed_prefs.floppyslots[i].df);
          	    lstMRUDiskList.erase(lstMRUDiskList.begin() + idx);
          	    lstMRUDiskList.insert(lstMRUDiskList.begin(), changed_prefs.floppyslots[i].df);
                bIgnoreListChange = true;
                cboDFxFile[i]->setSelected(0);
                bIgnoreListChange = false;

          	    if(i == 0 && chkLoadConfig->isSelected())
        	      {
        	        // Search for config of disk
        	        char tmp[MAX_PATH];
        	        
        	        extractFileName(changed_prefs.floppyslots[i].df, tmp);
        	        removeFileExtension(tmp);
        	        LoadConfigByName(tmp);
                }
              }
      	    }
      	  }
        }
      }
      RefreshPanelFloppy();
      RefreshPanelQuickstart();
    }
};
static DiskFileActionListener* diskFileActionListener;


class DriveSpeedSliderActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
  		changed_prefs.floppy_speed = drivespeedvalues[(int)sldDriveSpeed->getValue()];
  		RefreshPanelFloppy();
    }
};
static DriveSpeedSliderActionListener* driveSpeedSliderActionListener;


class SaveForDiskActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      //---------------------------------------
      // Save configuration for current disk
      //---------------------------------------
      if(strlen(changed_prefs.floppyslots[0].df) > 0)
      {
        char filename[MAX_DPATH];
        char diskname[MAX_PATH];
        
        extractFileName(changed_prefs.floppyslots[0].df, diskname);
        removeFileExtension(diskname);
        
        fetch_configurationpath(filename, MAX_DPATH);
        strncat(filename, diskname, MAX_DPATH - 1);
        strncat(filename, ".uae", MAX_DPATH - 1);
        
			  snprintf(changed_prefs.description, sizeof (changed_prefs.description) - 1, "Configuration for disk '%s'", diskname);
        if(cfgfile_save(&changed_prefs, filename, 0))
          RefreshPanelConfig();
      }
    }
};
static SaveForDiskActionListener* saveForDiskActionListener;


class CreateDiskActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if(actionEvent.getSource() == cmdCreateDDDisk)
      {
        // Create 3.5'' DD Disk
        char tmp[MAX_PATH];
        char diskname[MAX_PATH];
        strncpy(tmp, currentDir, MAX_PATH - 1);
        if(SelectFile("Create 3.5'' DD disk file", tmp, diskfile_filter, true))
        {
          extractFileName(tmp, diskname);
          removeFileExtension(diskname);
          diskname[31] = '\0';
          disk_creatediskfile(&changed_prefs, tmp, 0, DRV_35_DD, -1, diskname, false, false, NULL);
    	    AddFileToDiskList(tmp, 1);
    	    extractPath(tmp, currentDir);
        }
        cmdCreateDDDisk->requestFocus();
      }
      else if(actionEvent.getSource() == cmdCreateHDDisk)
      {
        // Create 3.5'' HD Disk
        char tmp[MAX_PATH];
        char diskname[MAX_PATH];
        strncpy(tmp, currentDir, MAX_PATH - 1);
        if(SelectFile("Create 3.5'' HD disk file", tmp, diskfile_filter, true))
        {
          extractFileName(tmp, diskname);
          removeFileExtension(diskname);
          diskname[31] = '\0';
          disk_creatediskfile(&changed_prefs, tmp, 0, DRV_35_HD, -1, diskname, false, false, NULL);
    	    AddFileToDiskList(tmp, 1);
    	    extractPath(tmp, currentDir);
        }
        cmdCreateHDDisk->requestFocus();
      }
    }
};
static CreateDiskActionListener* createDiskActionListener;


void InitPanelFloppy(const struct _ConfigCategory& category)
{
	int posX;
	int posY = DISTANCE_BORDER;
	int i;
	
	dfxCheckActionListener = new DFxCheckActionListener();
	driveTypeActionListener = new DriveTypeActionListener();
	dfxButtonActionListener = new DFxButtonActionListener();
	diskFileActionListener = new DiskFileActionListener();
	driveSpeedSliderActionListener = new DriveSpeedSliderActionListener();
	saveForDiskActionListener = new SaveForDiskActionListener();
	createDiskActionListener = new CreateDiskActionListener();
	
	for(i=0; i<4; ++i)
	{
	  char tmp[21];
	  snprintf(tmp, 20, "DF%d:", i); 
	  chkDFx[i] = new gcn::UaeCheckBox(tmp);
	  chkDFx[i]->addActionListener(dfxCheckActionListener);
	  
	  cboDFxType[i] = new gcn::UaeDropDown(&driveTypeList);
    cboDFxType[i]->setSize(106, DROPDOWN_HEIGHT);
    cboDFxType[i]->setBaseColor(gui_baseCol);
	  snprintf(tmp, 20, "cboType%d", i);
	  cboDFxType[i]->setId(tmp);
    cboDFxType[i]->addActionListener(driveTypeActionListener);
	  
	  chkDFxWriteProtect[i] = new gcn::UaeCheckBox("Write-protected");
	  chkDFxWriteProtect[i]->addActionListener(dfxCheckActionListener);
	  snprintf(tmp, 20, "chkWP%d", i);
	  chkDFxWriteProtect[i]->setId(tmp);
	  
    cmdDFxInfo[i] = new gcn::Button("?");
    cmdDFxInfo[i]->setSize(SMALL_BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
    cmdDFxInfo[i]->setBaseColor(gui_baseCol);
    cmdDFxInfo[i]->addActionListener(dfxButtonActionListener);

    cmdDFxEject[i] = new gcn::Button("Eject");
    cmdDFxEject[i]->setSize(SMALL_BUTTON_WIDTH * 2, SMALL_BUTTON_HEIGHT);
    cmdDFxEject[i]->setBaseColor(gui_baseCol);
	  snprintf(tmp, 20, "cmdEject%d", i);
	  cmdDFxEject[i]->setId(tmp);
    cmdDFxEject[i]->addActionListener(dfxButtonActionListener);

    cmdDFxSelect[i] = new gcn::Button("...");
    cmdDFxSelect[i]->setSize(SMALL_BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
    cmdDFxSelect[i]->setBaseColor(gui_baseCol);
	  snprintf(tmp, 20, "cmdSel%d", i);
	  cmdDFxSelect[i]->setId(tmp);
    cmdDFxSelect[i]->addActionListener(dfxButtonActionListener);

	  cboDFxFile[i] = new gcn::UaeDropDown(&diskfileList);
    cboDFxFile[i]->setSize(category.panel->getWidth() - 2 * DISTANCE_BORDER, DROPDOWN_HEIGHT);
    cboDFxFile[i]->setBaseColor(gui_baseCol);
	  snprintf(tmp, 20, "cboDisk%d", i);
	  cboDFxFile[i]->setId(tmp);
    cboDFxFile[i]->addActionListener(diskFileActionListener);

    if(i == 0)
    {
      chkLoadConfig = new gcn::UaeCheckBox("Load config with same name as disk");
      chkLoadConfig->setId("LoadDiskCfg");
      chkLoadConfig->addActionListener(dfxCheckActionListener);
    }
	}

	lblDriveSpeed = new gcn::Label("Floppy Drive Emulation Speed:");
  sldDriveSpeed = new gcn::Slider(0, 3);
  sldDriveSpeed->setSize(110, SLIDER_HEIGHT);
  sldDriveSpeed->setBaseColor(gui_baseCol);
	sldDriveSpeed->setMarkerLength(20);
	sldDriveSpeed->setStepLength(1);
	sldDriveSpeed->setId("DriveSpeed");
  sldDriveSpeed->addActionListener(driveSpeedSliderActionListener);
  lblDriveSpeedInfo = new gcn::Label(drivespeedlist[0]);

  cmdSaveForDisk = new gcn::Button("Save config for disk");
  cmdSaveForDisk->setSize(160, BUTTON_HEIGHT);
  cmdSaveForDisk->setBaseColor(gui_baseCol);
  cmdSaveForDisk->setId("SaveForDisk");
  cmdSaveForDisk->addActionListener(saveForDiskActionListener);

  cmdCreateDDDisk = new gcn::Button("Create 3.5'' DD disk");
  cmdCreateDDDisk->setSize(160, BUTTON_HEIGHT);
  cmdCreateDDDisk->setBaseColor(gui_baseCol);
  cmdCreateDDDisk->setId("CreateDD");
  cmdCreateDDDisk->addActionListener(createDiskActionListener);

  cmdCreateHDDisk = new gcn::Button("Create 3.5'' HD disk");
  cmdCreateHDDisk->setSize(160, BUTTON_HEIGHT);
  cmdCreateHDDisk->setBaseColor(gui_baseCol);
  cmdCreateHDDisk->setId("CreateHD");
  cmdCreateHDDisk->addActionListener(createDiskActionListener);
	
	for(i=0; i<4; ++i)
	{
	  posX = DISTANCE_BORDER;
	  category.panel->add(chkDFx[i], posX, posY);
#ifdef ANDROID
	  posX += 85;
#else
	  posX += 100;
#endif
	  category.panel->add(cboDFxType[i], posX, posY);
	  posX += cboDFxType[i]->getWidth() + 2 * DISTANCE_NEXT_X;
	  category.panel->add(chkDFxWriteProtect[i], posX, posY);
	  posX += chkDFxWriteProtect[i]->getWidth() + 4 * DISTANCE_NEXT_X;
//	  category.panel->add(cmdDFxInfo[i], posX, posY);
	  posX += cmdDFxInfo[i]->getWidth() + DISTANCE_NEXT_X;
	  category.panel->add(cmdDFxEject[i], posX, posY);
	  posX += cmdDFxEject[i]->getWidth() + DISTANCE_NEXT_X;
	  category.panel->add(cmdDFxSelect[i], posX, posY);
	  posY += chkDFx[i]->getHeight() + 8;

	  category.panel->add(cboDFxFile[i], DISTANCE_BORDER, posY);
	  if(i == 0)
    {
  	  posY += cboDFxFile[i]->getHeight() + 8;
      category.panel->add(chkLoadConfig, DISTANCE_BORDER, posY);
    }
	  posY += cboDFxFile[i]->getHeight() + DISTANCE_NEXT_Y + 4;
  }
  
  posX = DISTANCE_BORDER;
  category.panel->add(lblDriveSpeed, posX, posY);
  posX += lblDriveSpeed->getWidth() + 8;
  category.panel->add(sldDriveSpeed, posX, posY);
  posX += sldDriveSpeed->getWidth() + DISTANCE_NEXT_X;
  category.panel->add(lblDriveSpeedInfo, posX, posY);
  posY += sldDriveSpeed->getHeight() + DISTANCE_NEXT_Y;

  posY = category.panel->getHeight() - DISTANCE_BORDER - BUTTON_HEIGHT;
  category.panel->add(cmdSaveForDisk, DISTANCE_BORDER, posY);
  category.panel->add(cmdCreateDDDisk, cmdSaveForDisk->getX() + cmdSaveForDisk->getWidth() + DISTANCE_NEXT_X, posY);
  category.panel->add(cmdCreateHDDisk, cmdCreateDDDisk->getX() + cmdCreateDDDisk->getWidth() + DISTANCE_NEXT_X, posY);
  
  RefreshPanelFloppy();
}


void ExitPanelFloppy(void)
{
	for(int i=0; i<4; ++i)
	{
	  delete chkDFx[i];
	  delete cboDFxType[i];
	  delete chkDFxWriteProtect[i];
	  delete cmdDFxInfo[i];
	  delete cmdDFxEject[i];
	  delete cmdDFxSelect[i];
	  delete cboDFxFile[i];
	}
  delete chkLoadConfig;
  delete lblDriveSpeed;
  delete sldDriveSpeed;
  delete lblDriveSpeedInfo;
  delete cmdSaveForDisk;
  delete cmdCreateDDDisk;
  delete cmdCreateHDDisk;
  
  delete dfxCheckActionListener;
  delete driveTypeActionListener;
  delete dfxButtonActionListener;
  delete diskFileActionListener;
  delete driveSpeedSliderActionListener;
  delete saveForDiskActionListener;
  delete createDiskActionListener;
}


static void AdjustDropDownControls(void)
{
  int i, j;
  
  bIgnoreListChange = true;
  
  for(i=0; i<4; ++i)
  {
    cboDFxFile[i]->clearSelected();

    if((changed_prefs.floppyslots[i].dfxtype != DRV_NONE) && strlen(changed_prefs.floppyslots[i].df) > 0)
    {
      for(j=0; j<lstMRUDiskList.size(); ++j)
      {
        if(!lstMRUDiskList[j].compare(changed_prefs.floppyslots[i].df))
        {
          cboDFxFile[i]->setSelected(j);
          break;
        }
      }
    }
  }
       
  bIgnoreListChange = false;
}


void RefreshPanelFloppy(void)
{
  int i;
  bool prevAvailable = true;
  
  AdjustDropDownControls();

  changed_prefs.nr_floppies = 0;
  for(i=0; i<4; ++i)
  {
    bool driveEnabled = changed_prefs.floppyslots[i].dfxtype != DRV_NONE;
    chkDFx[i]->setSelected(driveEnabled);
    cboDFxType[i]->setSelected(changed_prefs.floppyslots[i].dfxtype + 1);
    chkDFxWriteProtect[i]->setSelected(disk_getwriteprotect(&changed_prefs, changed_prefs.floppyslots[i].df));
    chkDFx[i]->setEnabled(prevAvailable);
    cboDFxType[i]->setEnabled(prevAvailable);
    
	  chkDFxWriteProtect[i]->setEnabled(driveEnabled && !changed_prefs.floppy_read_only);
    cmdDFxInfo[i]->setEnabled(driveEnabled);
    cmdDFxEject[i]->setEnabled(driveEnabled);
    cmdDFxSelect[i]->setEnabled(driveEnabled);
    cboDFxFile[i]->setEnabled(driveEnabled);
    
    prevAvailable = driveEnabled;
    if(driveEnabled)
      changed_prefs.nr_floppies = i + 1;
  }

  chkLoadConfig->setSelected(bLoadConfigForDisk);
  
  for(i=0; i<4; ++i)
  {
    if(changed_prefs.floppy_speed == drivespeedvalues[i])
    {
      sldDriveSpeed->setValue(i);
      lblDriveSpeedInfo->setCaption(drivespeedlist[i]);
      break;
    }
  }
}


bool HelpPanelFloppy(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("You can enable/disable each drive by clicking the checkbox next to DFx or select the drive type in the dropdown");
  helptext.push_back("control. \"3.5'' DD\" is the right choise for nearly all ADF and ADZ files.");
  helptext.push_back("The option \"Write-protected\" indicates if the emulator can write to the ADF. Changing the write protection of the");
  helptext.push_back("disk file may fail because of missing rights on the host filesystem.");
  helptext.push_back("The button \"...\" opens a dialog to select the required disk file. With the dropdown control, you can select one of");
  helptext.push_back("the disks you recently used.");
  helptext.push_back(" ");
  helptext.push_back("You can reduce the loading time for lot of games by increasing the floppy drive emulation speed. A few games");
  helptext.push_back("will not load with higher drive speed and you have to select 100%.");
  helptext.push_back(" ");
  helptext.push_back("\"Save config for disk\" will create a new configuration file with the name of the disk in DF0. This configuration will");
  helptext.push_back("be loaded each time you select the disk and have the option \"Load config with same name as disk\" enabled.");
  helptext.push_back(" ");
  helptext.push_back("With the buttons \"Create 3.5'' DD disk\" and \"Create 3.5'' HD disk\" you can create a new and empty disk.");
  return true;
}

/*
  Hatari - dlgDisk.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgDisk_rcsid[] = "Hatari $Id: dlgDisk.c,v 1.3 2008-05-25 19:58:56 thothy Exp $";

#include <assert.h>
#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "floppy.h"


#define DISKDLG_EJECTA      4
#define DISKDLG_BROWSEA     5
#define DISKDLG_DISKA       6
#define DISKDLG_EJECTB      8
#define DISKDLG_BROWSEB     9
#define DISKDLG_DISKB       10
#define DISKDLG_IMGDIR      12
#define DISKDLG_BROWSEIMG   13
#define DISKDLG_AUTOB       14
#define DISKDLG_CREATEIMG   15
#define DISKDLG_PROTOFF     17
#define DISKDLG_PROTON      18
#define DISKDLG_PROTAUTO    19
#define DISKDLG_EJECTHDIMG  23
#define DISKDLG_BROWSEHDIMG 24
#define DISKDLG_DISKHDIMG   25
#define DISKDLG_UNMOUNTGDOS 27
#define DISKDLG_BROWSEGDOS  28
#define DISKDLG_DISKGDOS    29
#define DISKDLG_BOOTHD      30
#define DISKDLG_EXIT        31


/* The disks dialog: */
static SGOBJ diskdlg[] =
{
	{ SGBOX, 0, 0, 0,0, 64,25, NULL },

	{ SGBOX, 0, 0, 1,1, 62,12, NULL },
	{ SGTEXT, 0, 0, 25,1, 12,1, "Floppy disks" },
	{ SGTEXT, 0, 0, 2,2, 8,1, "Drive A:" },
	{ SGBUTTON, 0, 0, 46,2, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,2, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 3,3, 58,1, NULL },
	{ SGTEXT, 0, 0, 2,4, 8,1, "Drive B:" },
	{ SGBUTTON, 0, 0, 46,4, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,4, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 3,5, 58,1, NULL },
	{ SGTEXT, 0, 0, 2,7, 32,1, "Default floppy images directory:" },
	{ SGTEXT, 0, 0, 3,8, 58,1, NULL },
	{ SGBUTTON, 0, 0, 54,7, 8,1, "Browse" },
	{ SGCHECKBOX, 0, 0, 2,10, 16,1, "Auto insert B" },
	{ SGBUTTON, 0, 0, 42,10, 20,1, "Create blank image" },
	{ SGTEXT, 0, 0, 2,12, 17,1, "Write protection:" },
	{ SGRADIOBUT, 0, 0, 21,12, 5,1, "Off" },
	{ SGRADIOBUT, 0, 0, 28,12, 5,1, "On" },
	{ SGRADIOBUT, 0, 0, 34,12, 6,1, "Auto" },

	{ SGBOX, 0, 0, 1,14, 62,8, NULL },
	{ SGTEXT, 0, 0, 27,14, 10,1, "Hard disks" },
	{ SGTEXT, 0, 0, 2,15, 9,1, "HD image:" },
	{ SGBUTTON, 0, 0, 46,15, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,15, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 3,16, 58,1, NULL },
	{ SGTEXT, 0, 0, 2,17, 13,1, "GEMDOS drive:" },
	{ SGBUTTON, 0, 0, 46,17, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,17, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 3,18, 58,1, NULL },
	{ SGCHECKBOX, 0, 0, 2,20, 14,1, "Boot from HD" },

	{ SGBUTTON, SG_DEFAULT, 0, 22,23, 20,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};


/**
 * Let user browse given disk, insert disk if one selected.
 * return FALSE if no disk selected, otherwise return TRUE.
 */
static bool DlgDisk_BrowseDisk(char *dlgname, int drive, int diskid)
{
	char *selname, *zip_path;
	const char *tmpname;

	assert(drive >= 0 && drive < 2);
	if (EmulationDrives[drive].bDiskInserted)
		tmpname = EmulationDrives[drive].szFileName;
	else
		tmpname = DialogParams.DiskImage.szDiskImageDirectory;

	selname = SDLGui_FileSelect(tmpname, &zip_path, FALSE);
	if (selname)
	{
		if (!File_DoesFileNameEndWithSlash(selname) && File_Exists(selname))
		{
			char *realname;
			/* FIXME: This shouldn't be done here but in Dialog_CopyDialogParamsToConfiguration */
			realname = Floppy_ZipInsertDiskIntoDrive(drive, selname, zip_path);
			/* TODO: error dialog when this fails */
			if (realname)
			{
				File_ShrinkName(dlgname, realname, diskdlg[diskid].w);
				free(realname);
			}
			if (zip_path)
				free(zip_path);
		}
		else
		{
			/* FIXME: This shouldn't be done here but in Dialog_CopyDialogParamsToConfiguration */
			Floppy_EjectDiskFromDrive(0, FALSE);
			dlgname[0] = 0;
		}
		free(selname);
		return TRUE;
	}
	return FALSE;
}


/**
 * Let user browse given directory, set directory if one selected.
 * return FALSE if none selected, otherwise return TRUE.
 */
static bool DlgDisk_BrowseDir(char *dlgname, char *confname, int maxlen)
{
	char *str, *selname;

	selname = SDLGui_FileSelect(confname, NULL, FALSE);
	if (selname)
	{
		strcpy(confname, selname);
		free(selname);

		str = strrchr(confname, PATHSEP);
		if (str != NULL)
			str[1] = 0;
		File_CleanFileName(confname);
		File_ShrinkName(dlgname, confname, maxlen);
		return TRUE;
	}
	return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  Show and process the disk image dialog.
*/
void Dialog_DiskDlg(void)
{
	int but, i;
	char dlgnamea[64], dlgnameb[64], dlgdiskdir[64];
	char dlgnamegdos[64], dlgnamehdimg[64];

	SDLGui_CenterDlg(diskdlg);

	/* Set up dialog to actual values: */

	/* Disk name A: */
	if (EmulationDrives[0].bDiskInserted)
		File_ShrinkName(dlgnamea, EmulationDrives[0].szFileName,
		                diskdlg[DISKDLG_DISKA].w);
	else
		dlgnamea[0] = 0;
	diskdlg[DISKDLG_DISKA].txt = dlgnamea;

	/* Disk name B: */
	if (EmulationDrives[1].bDiskInserted)
		File_ShrinkName(dlgnameb, EmulationDrives[1].szFileName,
		                diskdlg[DISKDLG_DISKB].w);
	else
		dlgnameb[0] = 0;
	diskdlg[DISKDLG_DISKB].txt = dlgnameb;

	/* Default image directory: */
	File_ShrinkName(dlgdiskdir, DialogParams.DiskImage.szDiskImageDirectory,
	                diskdlg[DISKDLG_IMGDIR].w);
	diskdlg[DISKDLG_IMGDIR].txt = dlgdiskdir;

	/* Auto insert disk B: */
	if (DialogParams.DiskImage.bAutoInsertDiskB)
		diskdlg[DISKDLG_AUTOB].state |= SG_SELECTED;
	else
		diskdlg[DISKDLG_AUTOB].state &= ~SG_SELECTED;

	/* Write protection */
	for (i = DISKDLG_PROTOFF; i <= DISKDLG_PROTAUTO; i++)
	{
		diskdlg[i].state &= ~SG_SELECTED;
	}
	diskdlg[DISKDLG_PROTOFF+DialogParams.DiskImage.nWriteProtection].state |= SG_SELECTED;

	/* Boot from harddisk? */
	if (DialogParams.HardDisk.bBootFromHardDisk)
		diskdlg[DISKDLG_BOOTHD].state |= SG_SELECTED;
	else
		diskdlg[DISKDLG_BOOTHD].state &= ~SG_SELECTED;

	/* GEMDOS hard disk directory: */
	if (DialogParams.HardDisk.bUseHardDiskDirectories)
		File_ShrinkName(dlgnamegdos, DialogParams.HardDisk.szHardDiskDirectories[0],
		                diskdlg[DISKDLG_DISKGDOS].w);
	else
		dlgnamegdos[0] = 0;
	diskdlg[DISKDLG_DISKGDOS].txt = dlgnamegdos;

	/* Hard disk image: */
	if (DialogParams.HardDisk.bUseHardDiskImage)
		File_ShrinkName(dlgnamehdimg, DialogParams.HardDisk.szHardDiskImage,
		                diskdlg[DISKDLG_DISKHDIMG].w);
	else
		dlgnamehdimg[0] = 0;
	diskdlg[DISKDLG_DISKHDIMG].txt = dlgnamehdimg;

	/* Draw and process the dialog */
	do
	{
		but = SDLGui_DoDialog(diskdlg, NULL);
		switch (but)
		{
		 case DISKDLG_EJECTA:                         /* Eject disk in drive A: */
			Floppy_EjectDiskFromDrive(0, FALSE);
			dlgnamea[0] = 0;
			break;
		 case DISKDLG_BROWSEA:                        /* Choose a new disk A: */
			DlgDisk_BrowseDisk(dlgnamea, 0, DISKDLG_DISKA);
			break;
		 case DISKDLG_EJECTB:                         /* Eject disk in drive B: */
			Floppy_EjectDiskFromDrive(1, FALSE);
			dlgnameb[0] = 0;
			break;
		case DISKDLG_BROWSEB:                         /* Choose a new disk B: */
			DlgDisk_BrowseDisk(dlgnameb, 1, DISKDLG_DISKB);
			break;
		 case DISKDLG_BROWSEIMG:
			DlgDisk_BrowseDir(dlgdiskdir,
			                 DialogParams.DiskImage.szDiskImageDirectory,
			                 diskdlg[DISKDLG_IMGDIR].w);
			break;
		 case DISKDLG_CREATEIMG:
			DlgNewDisk_Main();
			break;
		 case DISKDLG_UNMOUNTGDOS:
			DialogParams.HardDisk.bUseHardDiskDirectories = FALSE;
			dlgnamegdos[0] = 0;
			break;
		 case DISKDLG_BROWSEGDOS:
			if (DlgDisk_BrowseDir(dlgnamegdos,
			                     DialogParams.HardDisk.szHardDiskDirectories[0],
			                     diskdlg[DISKDLG_DISKGDOS].w))
				DialogParams.HardDisk.bUseHardDiskDirectories = TRUE;
			break;
		 case DISKDLG_EJECTHDIMG:
			DialogParams.HardDisk.bUseHardDiskImage = FALSE;
			dlgnamehdimg[0] = 0;
			break;
		 case DISKDLG_BROWSEHDIMG:
			if (SDLGui_FileConfSelect(dlgnamehdimg,
			                          DialogParams.HardDisk.szHardDiskImage,
			                          diskdlg[DISKDLG_DISKHDIMG].w, FALSE))
				DialogParams.HardDisk.bUseHardDiskImage = TRUE;
			break;
		}
	}
	while (but != DISKDLG_EXIT && but != SDLGUI_QUIT
	        && but != SDLGUI_ERROR && !bQuitProgram);

	/* Read values from dialog: */

	for (i = DISKDLG_PROTOFF; i <= DISKDLG_PROTAUTO; i++)
	{
		if (diskdlg[i].state & SG_SELECTED)
		{
			DialogParams.DiskImage.nWriteProtection = i-DISKDLG_PROTOFF;
			break;
		}
	}

	DialogParams.DiskImage.bAutoInsertDiskB = (diskdlg[DISKDLG_AUTOB].state & SG_SELECTED);
	DialogParams.HardDisk.bBootFromHardDisk = (diskdlg[DISKDLG_BOOTHD].state & SG_SELECTED);
}

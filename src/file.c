/*
  Hatari - file.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Common file access functions.
*/
char File_rcsid[] = "Hatari $Id: file.c,v 1.11 2003-12-25 18:45:22 thothy Exp $";

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "main.h"
#include "file.h"
#include "floppy.h"
#include "createBlankImage.h"
#include "memAlloc.h"
#include "misc.h"



#ifdef __BEOS__
/* The scandir() and alphasort() functions aren't available on BeOS, */
/* so let's declare them here... */
#include <dirent.h>

#undef DIRSIZ

#define DIRSIZ(dp)                                          \
        ((sizeof(struct dirent) - sizeof(dp)->d_name) +     \
		(((dp)->d_reclen + 1 + 3) &~ 3))


/*-----------------------------------------------------------------------*/
/*
  Alphabetic order comparison routine for those who want it.
*/
int alphasort(const void *d1, const void *d2)
{
  return(strcmp((*(struct dirent **)d1)->d_name, (*(struct dirent **)d2)->d_name));
}


/*-----------------------------------------------------------------------*/
/*
  Scan a directory for all its entries
*/
int scandir(const char *dirname,struct dirent ***namelist, int(*select) __P((struct dirent *)), int (*dcomp) __P((const void *, const void *)))
{
  register struct dirent *d, *p, **names;
  register size_t nitems;
  struct stat stb;
  long arraysz;
  DIR *dirp;

  if ((dirp = opendir(dirname)) == NULL)
    return(-1);

  if (fstat(dirp->fd, &stb) < 0)
    return(-1);

  /*
   * estimate the array size by taking the size of the directory file
   * and dividing it by a multiple of the minimum size entry.
   */
  arraysz = (stb.st_size / 24);

  names = (struct dirent **)malloc(arraysz * sizeof(struct dirent *));
  if (names == NULL)
    return(-1);

  nitems = 0;

  while ((d = readdir(dirp)) != NULL) {

     if (select != NULL && !(*select)(d))
       continue;       /* just selected names */

     /*
      * Make a minimum size copy of the data
      */

     p = (struct dirent *)malloc(DIRSIZ(d));
     if (p == NULL)
       return(-1);

     p->d_ino = d->d_ino;
     p->d_reclen = d->d_reclen;
     /*p->d_namlen = d->d_namlen;*/
     bcopy(d->d_name, p->d_name, p->d_reclen + 1);

     /*
      * Check to make sure the array has space left and
      * realloc the maximum size.
      */

     if (++nitems >= arraysz) {

       if (fstat(dirp->fd, &stb) < 0)
         return(-1);     /* just might have grown */

       arraysz = stb.st_size / 12;

       names = (struct dirent **)realloc((char *)names, arraysz * sizeof(struct dirent *));
       if (names == NULL)
         return(-1);
     }

     names[nitems-1] = p;
   }

   closedir(dirp);

   if (nitems && dcomp != NULL)
     qsort(names, nitems, sizeof(struct dirent *), dcomp);

   *namelist = names;

   return(nitems);
}


#endif /* __BEOS__ */



/*-----------------------------------------------------------------------*/
/*
  Remove any '/'s from end of filenames, but keeps / intact
*/
void File_CleanFileName(char *pszFileName)
{
  int len;

  len = strlen(pszFileName);

  /* Security length check: */
  if (len > FILENAME_MAX)
  {
    pszFileName[FILENAME_MAX-1] = 0;
    len = FILENAME_MAX;
  }

  /* Remove end slash from filename! But / remains! Doh! */
  if( len>2 && pszFileName[len-1]=='/' )
    pszFileName[len-1] = 0;
}


/*-----------------------------------------------------------------------*/
/*
  Add '/' to end of filename
*/
void File_AddSlashToEndFileName(char *pszFileName)
{
  /* Check dir/filenames */
  if (strlen(pszFileName)!=0) {
    if (pszFileName[strlen(pszFileName)-1]!='/')
      strcat(pszFileName,"/");  /* Must use end slash */
  }
}


/*-----------------------------------------------------------------------*/
/*
  Does filename extension match? If so, return TRUE
*/
BOOL File_DoesFileExtensionMatch(char *pszFileName, char *pszExtension)
{
  if ( strlen(pszFileName) < strlen(pszExtension) )
    return(FALSE);
  /* Is matching extension? */
  if ( !strcasecmp(&pszFileName[strlen(pszFileName)-strlen(pszExtension)], pszExtension) )
    return(TRUE);

  /* No */
  return(FALSE);
}


/*-----------------------------------------------------------------------*/
/*
  Check if filename is from root
  
  Return TRUE if filename is '/', else give FALSE
*/
BOOL File_IsRootFileName(char *pszFileName)
{
  if (pszFileName[0]=='\0')     /* If NULL string return! */
    return(FALSE);

  if (pszFileName[0]=='/')
    return(TRUE);

  return(FALSE);
}


/*-----------------------------------------------------------------------*/
/*
  Return string, to remove 'C:' part of filename
*/
char *File_RemoveFileNameDrive(char *pszFileName)
{
  if ( (pszFileName[0]!='\0') && (pszFileName[1]==':') )
    return(&pszFileName[2]);
  else
    return(pszFileName);
}


/*-----------------------------------------------------------------------*/
/*
  Check if filename end with a '/'
  
  Return TRUE if filename ends with '/'
*/
BOOL File_DoesFileNameEndWithSlash(char *pszFileName)
{
  if (pszFileName[0]=='\0')    /* If NULL string return! */
    return(FALSE);

  /* Does string end in a '/'? */
  if (pszFileName[strlen(pszFileName)-1]=='/')
    return(TRUE);

  return(FALSE);
}


/*-----------------------------------------------------------------------*/
/*
  Remove any double '/'s  from end of filenames. So just the one
*/
void File_RemoveFileNameTrailingSlashes(char *pszFileName)
{
  int Length;

  /* Do have slash at end of filename? */
  Length = strlen(pszFileName);
  if (Length>=3) {
    if (pszFileName[Length-1]=='/') {     /* Yes, have one previous? */
      if (pszFileName[Length-2]=='/')
        pszFileName[Length-1] = '\0';     /* then remove it! */
    }
  }
}


/*-----------------------------------------------------------------------*/
/*
  Does filename end with a .ST.GZ extension? If so, return TRUE
*/
BOOL File_FileNameIsSTGZ(char *pszFileName)
{
  return(File_DoesFileExtensionMatch(pszFileName,".st.gz"));
}


/*-----------------------------------------------------------------------*/
/*
  Does filename end with a .MSA.GZ extension? If so, return TRUE
*/
BOOL File_FileNameIsMSAGZ(char *pszFileName)
{
  return(File_DoesFileExtensionMatch(pszFileName,".msa.gz"));
}


/*-----------------------------------------------------------------------*/
/*
  Does filename end with a .ZIP extension? If so, return TRUE
*/
BOOL File_FileNameIsZIP(char *pszFileName)
{
  return(File_DoesFileExtensionMatch(pszFileName,".zip"));
}


/*-----------------------------------------------------------------------*/
/*
  Does filename end with a .MSA extension? If so, return TRUE
*/
BOOL File_FileNameIsMSA(char *pszFileName)
{
  return(File_DoesFileExtensionMatch(pszFileName,".msa"));
}


/*-----------------------------------------------------------------------*/
/*
  Does filename end with a .ST extension? If so, return TRUE
*/
BOOL File_FileNameIsST(char *pszFileName)
{
  return(File_DoesFileExtensionMatch(pszFileName,".st"));
}


/*-----------------------------------------------------------------------*/
/*
  Read file from PC into memory, allocate memory for it if need to (pass Address as NULL)
  Also may pass 'unsigned long' if want to find size of file read (may pass as NULL)
*/
void *File_Read(char *pszFileName, void *pAddress, long *pFileSize, char *ppszExts[])
{
  FILE *DiscFile;
  void *pFile=NULL;
  long FileSize=0;

  /* Does the file exist? If not, see if can scan for other extensions and try these */
  if (!File_Exists(pszFileName) && ppszExts) {
    /* Try other extensions, if suceeds correct filename is now in 'pszFileName' */
    File_FindPossibleExtFileName(pszFileName,ppszExts);
  }

  /* Open our file */
  DiscFile = fopen(pszFileName, "rb");
  if (DiscFile!=NULL) {
    /* Find size of TOS image - 192k or 256k */
    fseek(DiscFile, 0, SEEK_END);
    FileSize = ftell(DiscFile);
    fseek(DiscFile, 0, SEEK_SET);
    /* Find pointer to where to load, allocate memory if pass NULL */
    if (pAddress)
      pFile = pAddress;
    else
      pFile = Memory_Alloc(FileSize);
    /* Read in... */
    if (pFile)
      fread((char *)pFile, 1, FileSize, DiscFile);

    fclose(DiscFile);
  }
  /* Store size of file we read in (or 0 if failed) */
  if (pFileSize)
    *pFileSize = FileSize;

  return(pFile);        /* Return to where read in/allocated */
}


/*-----------------------------------------------------------------------*/
/*
  Save file to PC, return FALSE if errors
*/
BOOL File_Save(char *pszFileName, void *pAddress,long Size,BOOL bQueryOverwrite)
{
  FILE *DiscFile;
  BOOL bRet=FALSE;

  /* Check if need to ask user if to overwrite */
  if (bQueryOverwrite) {
    /* If file exists, ask if OK to overwrite */
    if (!File_QueryOverwrite(pszFileName))
      return(FALSE);
  }

  /* Create our file */
  DiscFile = fopen(pszFileName, "wb");
  if (DiscFile!=NULL) {
    /* Write data, set success flag */
    if ( fwrite(pAddress, 1, Size, DiscFile)==Size )
      bRet = TRUE;

    fclose(DiscFile);
  }

  return(bRet);
}


/*-----------------------------------------------------------------------*/
/*
  Return size of file, -1 if error
*/
int File_Length(char *pszFileName)
{
  FILE *DiscFile;
  int FileSize;
  DiscFile = fopen(pszFileName, "rb");
  if (DiscFile!=NULL) {
    fseek(DiscFile, 0, SEEK_END);
    FileSize = ftell(DiscFile);
    fseek(DiscFile, 0, SEEK_SET);
    fclose(DiscFile);
    return(FileSize);
  }

  return(-1);
}


/*-----------------------------------------------------------------------*/
/*
  Return TRUE if file exists
*/
BOOL File_Exists(char *pszFileName)
{
  FILE *DiscFile;

  /* Attempt to open file */
  DiscFile = fopen(pszFileName, "rb");
  if (DiscFile!=NULL) {
    fclose(DiscFile);
    return(TRUE);
  }
  return(FALSE);
}


/*-----------------------------------------------------------------------*/
/*
  Delete file, return TRUE if OK
*/
BOOL File_Delete(char *pszFileName)
{
  /* Delete the file (must be closed first) */
  return( remove(pszFileName) );
}


/*-----------------------------------------------------------------------*/
/*
  Find if file exists, and if so ask user if OK to overwrite
*/
BOOL File_QueryOverwrite(char *pszFileName)
{

  char szString[FILENAME_MAX];

  /* Try and find if file exists */
  if (File_Exists(pszFileName))
  {
    /* File does exist, are we OK to overwrite? */
    sprintf(szString,"File '%s' exists, overwrite?",pszFileName);
/* FIXME: */
#if 0
    if (MessageBox(hWnd,szString,PROG_NAME,MB_YESNO | MB_DEFBUTTON2 | MB_ICONSTOP)==IDNO)
      return(FALSE);
#else
    fprintf(stderr, "Overwriting file '%s'!\n", pszFileName);
#endif
  }

  return(TRUE);
}


/*-----------------------------------------------------------------------*/
/*
  Try filename with various extensions and check if file exists - if so return correct name
*/
BOOL File_FindPossibleExtFileName(char *pszFileName, char *ppszExts[])
{
  char szSrcDir[256], szSrcName[128], szSrcExt[32];
  char szTempFileName[FILENAME_MAX];
  int i=0;

  /* Split filename into parts */
  File_splitpath(pszFileName, szSrcDir, szSrcName, szSrcExt);

  /* Scan possible extensions */
  while(ppszExts[i]) {
    /* Re-build with new file extension */
    File_makepath(szTempFileName, szSrcDir, szSrcName, ppszExts[i]);
    /* Does this file exist? */
    if (File_Exists(szTempFileName)) {
      /* Copy name for return */
      strcpy(pszFileName,szTempFileName);
      return(TRUE);
    }

    /* Next one */
    i++;
  }

  /* No, none of the files exist */
  return(FALSE);
}


/*-----------------------------------------------------------------------*/
/*
  Split a complete filename into path, filename and extension.
  If pExt is NULL, don't split the extension from the file name!
*/
void File_splitpath(char *pSrcFileName, char *pDir, char *pName, char *pExt)
{
  char *ptr1, *ptr2;

  /* Build pathname: */
  ptr1 = strrchr(pSrcFileName, '/');
  if( ptr1 )
  {
    strcpy(pDir, pSrcFileName);
    strcpy(pName, ptr1+1);
    pDir[ptr1-pSrcFileName+1] = 0;
  }
  else
  {
    strcpy(pDir, "./");
    strcpy(pName, pSrcFileName);
  }

  /* Build the raw filename: */
  if( pExt!=NULL )
  {
    ptr2 = strrchr(pName+1, '.');
    if( ptr2 )
    {
      pName[ptr2-pName] = 0;
      /* Copy the file extension: */
      strcpy(pExt, ptr2+1);
    }
    else
      pExt[0] = 0;
   }
}


/*-----------------------------------------------------------------------*/
/*
  Build a complete filename from path, filename and extension.
  pExt can also be NULL.
*/
void File_makepath(char *pDestFileName, char *pDir, char *pName, char *pExt)
{
  strcpy(pDestFileName, pDir);
  if( strlen(pDestFileName)==0 )
    strcpy(pDestFileName, "./");
  if( pDestFileName[strlen(pDestFileName)-1]!='/' )
    strcat(pDestFileName, "/");

  strcat(pDestFileName, pName);

  if( pExt!=NULL )
  {
    if( strlen(pExt)>0 && pExt[0]!='.' )
      strcat(pDestFileName, ".");
    strcat(pDestFileName, pExt);
  }
}


/*-----------------------------------------------------------------------*/
/*
  Shrink a file name to a certain length and insert some dots if we cut
  something away (usefull for showing file names in a dialog).
*/
void File_ShrinkName(char *pDestFileName, char *pSrcFileName, int maxlen)
{
  int srclen = strlen(pSrcFileName);
  if( srclen<maxlen )
    strcpy(pDestFileName, pSrcFileName);  /* It fits! */
  else
  {
    strncpy(pDestFileName, pSrcFileName, maxlen/2);
    if(maxlen&1)  /* even or uneven? */
      pDestFileName[maxlen/2-1] = 0;
    else
      pDestFileName[maxlen/2-2] = 0;
    strcat(pDestFileName, "...");
    strcat(pDestFileName, &pSrcFileName[strlen(pSrcFileName)-maxlen/2+1]);
  }
}


/*-----------------------------------------------------------------------*/
/*
  Create a clean absolute file name from a (possibly) relative file name.
  I.e. filter out all occurancies of "./" and "../".
  pFileName needs to point to a buffer of at least FILENAME_MAX bytes.
*/
void File_MakeAbsoluteName(char *pFileName)
{
  char *pTempName = Memory_Alloc(FILENAME_MAX);
  int inpos, outpos;

  inpos = 0;

  /* Is it already an absolute name? */
  if(pFileName[0] == '/')
  {
    outpos = 0;
  }
  else
  {
    getcwd(pTempName, FILENAME_MAX);
    File_AddSlashToEndFileName(pTempName);
    outpos = strlen(pTempName);
  }

  /* Now filter out the relative paths "./" and "../" */
  while (pFileName[inpos] != 0 && outpos < FILENAME_MAX)
  {
    if (pFileName[inpos] == '.' && pFileName[inpos+1] == '/')
    {
      /* Ignore "./" */
      inpos += 2;
    }
    else if (pFileName[inpos] == '.' && pFileName[inpos+1] == '.' && pFileName[inpos+2] == '/')
    {
      /* Handle "../" */
      char *pSlashPos;
      inpos += 3;
      pTempName[outpos - 1] = 0;
      pSlashPos = strrchr(pTempName, '/');
      if (pSlashPos)
      {
        *(pSlashPos + 1) = 0;
        outpos = strlen(pTempName);
      }
      else
      {
        pTempName[0] = '/';
        outpos = 1;
      }
    }
    else
    {
      /* Copy until next slash or end of input string */
      while (pFileName[inpos] != 0 && outpos < FILENAME_MAX)
      {
        pTempName[outpos++] = pFileName[inpos++];
        if (pFileName[inpos - 1] == '/')  break;
      }
    }
  }

  pTempName[outpos] = 0;

  strcpy(pFileName, pTempName);          /* Copy back */
  Memory_Free(pTempName);
}

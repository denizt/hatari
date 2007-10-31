/*
  Hatari - file.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_FILE_H
#define HATARI_FILE_H

extern void File_CleanFileName(char *pszFileName);
extern void File_AddSlashToEndFileName(char *pszFileName);
extern BOOL File_DoesFileExtensionMatch(const char *pszFileName, const char *pszExtension);
extern const char *File_RemoveFileNameDrive(const char *pszFileName);
extern BOOL File_DoesFileNameEndWithSlash(char *pszFileName);
extern Uint8 *File_Read(const char *pszFileName, long *pFileSize, const char * const ppszExts[]);
extern BOOL File_Save(const char *pszFileName, const void *pAddress, size_t Size, BOOL bQueryOverwrite);
extern int File_Length(const char *pszFileName);
extern BOOL File_Exists(const char *pszFileName);
extern BOOL File_QueryOverwrite(const char *pszFileName);
extern BOOL File_FindPossibleExtFileName(char *pszFileName,const char * const ppszExts[]);
extern void File_splitpath(const char *pSrcFileName, char *pDir, char *pName, char *Ext);
extern void File_makepath(char *pDestFileName, const char *pDir, const char *pName, const char *pExt);
extern void File_ShrinkName(char *pDestFileName, const char *pSrcFileName, int maxlen);
extern FILE *File_Open(const char *path, const char *mode);
extern FILE *File_Close(FILE *fp);
extern void File_MakeAbsoluteSpecialName(char *pszFileName);
extern void File_MakeAbsoluteName(char *pszFileName);
extern void File_MakeValidPathName(char *pPathName);

#endif /* HATARI_FILE_H */

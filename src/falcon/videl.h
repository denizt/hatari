/*
  Hatari - videl.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_VIDEL_H
#define HATARI_VIDEL_H

extern void VIDEL_renderScreen(void);
extern void VIDEL_renderScreenNoZoom(void);
extern void VIDEL_renderScreenZoom(void);
extern void VIDEL_reset(void);
extern void VIDEL_updateColors(void);
extern void VIDEL_setRendering(BOOL render);
extern int VIDEL_getScreenBpp(void);
extern int VIDEL_getScreenWidth(void);
extern int VIDEL_getScreenHeight(void);
extern long VIDEL_getVideoramAddress(void);
extern void VIDEL_ColorRegsWrite(void);
extern void VIDEL_ShiftModeWriteWord(void);

#endif /* _VIDEL_H */

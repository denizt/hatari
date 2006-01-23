/*
  Hatari - video.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_VIDEO_H
#define HATARI_VIDEO_H

/*
  All the following processor timings are based on a bog standard 8MHz 68000 as
  found in all standard STs:

  Clock cycles per line (50Hz)      : 512
  NOPs per scan line (50Hz)         : 128
  Scan lines per VBL (50Hz)         : 313 (64 at top,200 screen,49 bottom)

  Clock cycles per line (60Hz)      : 508
  NOPs per scan line (60Hz)         : 127
  Scan lines per VBL (60Hz)         : 263

  Clock cycles per VBL (50Hz)       : 160256
  NOPs per VBL (50Hz)               : 40064

  Pixels per clock cycle (low res)  : 1
  Pixels per clock cycle (med res)  : 2
  Pixels per clock cycle (high res) : 4
  Pixels per NOP (low res)          : 4
  Pixels per NOP (med res)          : 8
  Pixels per NOP (high res)         : 16
*/

/* Scan lines per frame */
#define SCANLINES_PER_FRAME_50HZ 313    /* Number of scan lines per frame in 50 Hz */
#define SCANLINES_PER_FRAME_60HZ 263    /* Number of scan lines per frame in 60 Hz */
#define MAX_SCANLINES_PER_FRAME 313     /* Max. number of scan lines per frame */


extern BOOL bUseHighRes;
extern int nVBLs,nHBL;
extern int nStartHBL, nEndHBL;
extern int OverscanMode;
extern Uint16 HBLPalettes[];
extern Uint16 *pHBLPalettes;
extern Uint32 HBLPaletteMasks[];
extern Uint32 *pHBLPaletteMasks;
extern Uint32 VideoBase;
extern int nScreenRefreshRate;

extern int nScanlinesPerFrame;
extern int nCyclesPerLine;

/* Legacy defines */
#define SCREEN_START_CYCLE  96          /* Cycle first normal pixel appears on */
#define CYCLES_VBL_IN       (SCREEN_START_HBL*nCyclesPerLine)     /* ((28+64)*CYCLES_PER_LINE) */
#define CYCLES_PER_FRAME    (nScanlinesPerFrame*nCyclesPerLine)  /* Cycles per VBL @ 50fps = 160256 */
#define CYCLES_PER_SEC      (CYCLES_PER_FRAME*50) /* Cycles per second */
#define CYCLES_HBL          (nCyclesPerLine+96)   /* Cycles for first HBL - very inaccurate on ST */

extern void Video_Reset(void);
extern void Video_MemorySnapShot_Capture(BOOL bSave);
extern void Video_InterruptHandler_VBL(void);
extern void Video_InterruptHandler_EndLine(void);
extern void Video_InterruptHandler_HBL(void);
extern void Video_SetScreenRasters(void);

extern void Video_ScreenCounterHigh_ReadByte(void);
extern void Video_ScreenCounterMed_ReadByte(void);
extern void Video_ScreenCounterLow_ReadByte(void);
extern void Video_Sync_ReadByte(void);
extern void Video_BaseLow_ReadByte(void);
extern void Video_LineWidth_ReadByte(void);
extern void Video_ShifterMode_ReadByte(void);

extern void Video_ScreenBaseSTE_WriteByte(void);
extern void Video_ScreenCounter_WriteByte(void);
extern void Video_Sync_WriteByte(void);
extern void Video_Color0_WriteWord(void);
extern void Video_Color1_WriteWord(void);
extern void Video_Color2_WriteWord(void);
extern void Video_Color3_WriteWord(void);
extern void Video_Color4_WriteWord(void);
extern void Video_Color5_WriteWord(void);
extern void Video_Color6_WriteWord(void);
extern void Video_Color7_WriteWord(void);
extern void Video_Color8_WriteWord(void);
extern void Video_Color9_WriteWord(void);
extern void Video_Color10_WriteWord(void);
extern void Video_Color11_WriteWord(void);
extern void Video_Color12_WriteWord(void);
extern void Video_Color13_WriteWord(void);
extern void Video_Color14_WriteWord(void);
extern void Video_Color15_WriteWord(void);
extern void Video_ShifterMode_WriteByte(void);

#endif  /* HATARI_VIDEO_H */

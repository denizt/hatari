/*
  Hatari - history.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_HISTORY_H
#define HATARI_HISTORY_H

/* what processors are tracked */
typedef enum {
	HISTORY_TRACK_NONE = 0,
	HISTORY_TRACK_CPU = 1,
	HISTORY_TRACK_DSP = 2,
	HISTORY_TRACK_ALL = 3,
} history_type_t;

extern history_type_t HistoryTracking;

static inline bool History_TrackCpu(void)
{
	return HistoryTracking & HISTORY_TRACK_CPU;
}
static inline bool History_TrackDsp(void)
{
	return HistoryTracking & HISTORY_TRACK_DSP;
}

/* for debugcpu/dsp.c */
extern void History_AddCpu(void);
extern void History_AddDsp(void);

/* for debugInfo.c */
extern void History_Show(Uint32 count);

/* for debugui */
extern void History_Mark(debug_reason_t reason);
extern int History_Parse(int nArgc, char *psArgv[]);

#endif

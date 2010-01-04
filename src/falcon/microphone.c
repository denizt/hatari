/*
  Hatari - microphone.c
  microphone (jack connector) emulation (Falcon mode only)

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This program uses the PortAudio Portable Audio Library.
  For more information see: http://www.portaudio.com
  Copyright (c) 1999-2000 Ross Bencina and Phil Burk
*/

#include "main.h"

#if HAVE_PORTAUDIO

#include <portaudio.h>
#include "microphone.h"
#include "crossbar.h"


#define FRAMES_PER_BUFFER (64)

/* Static functions */
static void Microphone_Error (void);
static int Microphone_Terminate(void);

static int Microphone_Callback (const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData);

/* Static datas */
static PaStreamParameters micro_inputParameters;
static PaStream *micro_stream;
static PaError  micro_err;

static int   micro_sampleRate;
static Sint16 micro_buffer_L[FRAMES_PER_BUFFER];	/* left buffer */
static Sint16 micro_buffer_R[FRAMES_PER_BUFFER];	/* right buffer */


/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int Microphone_Callback (const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
{
	unsigned int i;
	const Sint16 *in = inputBuffer;
	
	Sint16 *out_L = (Sint16*)micro_buffer_L;
	Sint16 *out_R = (Sint16*)micro_buffer_R;

	if (inputBuffer == NULL) {
		memset(micro_buffer_L, 0, sizeof(micro_buffer_L));
		memset(micro_buffer_R, 0, sizeof(micro_buffer_R));
	}
	else {
		for (i=0; i<framesPerBuffer; i++) {
			*out_L ++ = *in++;	/* left data */
			*out_R ++ = *in++;	/* right data */
		}
	}

	/* send buffer to crossbar */
	Crossbar_GetMicrophoneDatas(micro_buffer_L, micro_buffer_R, framesPerBuffer);
	
	/* get Next Microphone datas */
	return paContinue;
}


/*******************************************************************/

/**
 * Microphone (jack) inits : init portaudio microphone emulation
 *   - sampleRate : system sound frequency
 */
int Microphone_Start(int sampleRate)
{
	micro_sampleRate = sampleRate;

	/* Initialize portaudio */
	micro_err = Pa_Initialize();
	if (micro_err != paNoError) {
		Microphone_Error();
		return 0;
	}

	/* Initialize microphone parameters */
	micro_inputParameters.device = Pa_GetDefaultInputDevice();	/* default input device */
	if (micro_inputParameters.device == paNoDevice) {
		fprintf (stderr, "Microphone: No input device found.\n");
		Microphone_Terminate();
		return 0;
	}
	
	micro_inputParameters.channelCount = 2;				/* stereo input */
	micro_inputParameters.sampleFormat = paInt16;			/* 16 bits sound */
	micro_inputParameters.suggestedLatency = Pa_GetDeviceInfo (micro_inputParameters.device)->defaultLowInputLatency;
	micro_inputParameters.hostApiSpecificStreamInfo = NULL;

	/* Open Microphone stream */
	micro_err = Pa_OpenStream (
		&micro_stream,
		&micro_inputParameters,
		NULL,
		micro_sampleRate,
		FRAMES_PER_BUFFER,
		0, /* paClipOff, we won't output out of range samples so don't bother clipping them */
		Microphone_Callback,
		NULL);
	if (micro_err != paNoError) {
		Microphone_Error();
		return 0;
	}

	/* Start microphone recording */
	micro_err = Pa_StartStream( micro_stream );
	if (micro_err != paNoError) {
		Microphone_Error();
		return 0;
	}
	
	return 1;
}

/**
 * Microphone (jack) Error : display current portaudio error
 */
static void Microphone_Error(void)
{
	fprintf (stderr, "An error occured while using the portaudio stream\n");
	fprintf (stderr, "Error number: %d\n", micro_err);
	fprintf (stderr, "Error message: %s\n", Pa_GetErrorText (micro_err));

	Microphone_Terminate();
}

/**
 * Microphone (jack) Terminate : terminate the microphone emulation
 */
static int Microphone_Terminate(void)
{
	micro_err = Pa_Terminate();
	if (micro_err != paNoError) {
		fprintf (stderr, "PortAudio error: %s\n", Pa_GetErrorText(micro_err));
		return -1;
	}
	return 0;
}

/**
 * Microphone (jack) stop : stops the current recording
 */
int Microphone_Stop(void)
{
	/* Close Microphone stream */
	micro_err = Pa_CloseStream(micro_stream);
	if (micro_err != paNoError) {
		Microphone_Error();
		return -1;
	}

	return Microphone_Terminate();
}

#endif /* HAVE PORTAUDIO */

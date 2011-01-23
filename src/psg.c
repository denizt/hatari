/*
  Hatari - psg.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Programmable Sound Generator (YM-2149) - PSG

  Also used for the printer (centronics) port emulation (PSG Port B, Register 15)
*/


/* 2007/04/14	[NP]	First approximation to get cycle accurate accesses to ff8800/02	*/
/*			by cumulating wait state of 1 cycle and rounding the final	*/
/*			result to 4.							*/
/* 2007/04/29	[NP]	Functions PSG_Void_WriteByte and PSG_Void_ReadByte to handle	*/
/*			accesses to $ff8801/03. These adresses have no effect, but they	*/
/*			must give a 1 cycle wait state (e.g. move.l d0,ff8800).		*/
/* 2007/09/29	[NP]	Replace printf by calls to HATARI_TRACE.			*/
/* 2007/10/23	[NP]	In PSG_Void_WriteByte, add a wait state only if no wait state	*/
/*			were added so far (hack, but gives good result).		*/
/* 2007/11/18	[NP]	In PSG_DataRegister_WriteByte, set unused bit to 0, in case	*/
/*			the data reg is read later (fix Mindbomb Demo / BBC).		*/
/* 2008/04/20	[NP]	In PSG_DataRegister_WriteByte, set unused bit to 0 for register	*/
/*			6 too (noise period).						*/
/* 2008/07/27	[NP]	Better separation between accesses to the YM hardware registers	*/
/*			and the sound rendering routines. Use Sound_WriteReg() to pass	*/
/*			all writes to the sound rendering functions. This allows to	*/
/*			have sound.c independant of psg.c (to ease replacement of	*/
/*			sound.c	by another rendering method).				*/
/* 2008/08/11	[NP]	Set drive leds.							*/
/* 2008/10/16	[NP]	When writing to $ff8800, register select should not be masked	*/
/*			with 0xf, it's a real 8 bits register where all bits are	*/
/*			significant. This means only value <16 should be considered as	*/
/*			valid register selection. When reg select is >= 16, all writes	*/
/*			and reads in $ff8802 should be ignored.				*/
/*			(fix European Demo Intro, which sets addr reg to 0x10 when	*/
/*			sample playback is disabled).					*/
/* 2008/12/21	[NP]	After testing different cases on a real STF, rewrite registers	*/
/*			handling. As only pins BC1 and BDIR are used in an Atari to	*/
/*			address the YM2149, this means only 1 bit is necessary to access*/
/*			select/data registers. Other variations of the $ff88xx addresses*/
/*			will point to either $ff8800 or $ff8802. Only bit 1 of $ff88xx	*/
/*			is useful to know which register is accessed in the YM2149.	*/
/*			So, it's possible to access the YM2149 with $ff8801 and $ff8803	*/
/*			but under conditions : the write to a shadow address (bit 0=1)	*/
/*			can't be made by an instruction that writes to the same address	*/
/*			with bit 0=0 at the same time (.W or .L access).		*/
/*			In that case, only the address with bit 0=0 is taken into	*/
/*			account. This means a write to $ff8801/03 will succeed only if	*/
/*			the access size is .B (byte) or the opcode is a movep (because	*/
/*			in that case we won't access the same register with 2 different	*/
/*			addresses) (fix the game X-Out, which uses movep.w to write to	*/
/*			$ff8801/03).							*/
/*			Refactorize some code for cleaner handling of these accesses.	*/
/*			Only reads to $ff8800 will return a data, reads to $ff8801/02/03*/
/*			always return 0xff (tested on STF).				*/
/*			When PSGRegisterSelect > 15, reads to $ff8800 also return 0xff.	*/
/* 2009/01/24	[NP]	Remove redundant test, as movep implies SIZE_BYTE access.	*/


/* Emulating wait states when accessing $ff8800/01/02/03 with different 'move' variants	*/
/* is a complex task. So far, adding 1 cycle wait state to each access and rounding the	*/
/* final number to 4 gives some good results, but this is certainly not the way it's	*/
/* working for real in the ST.								*/
/* The following examples show some verified wait states for different accesses :	*/
/*	lea     $ffff8800,a1								*/
/*	lea     $ffff8802,a2								*/
/*	lea     $ffff8801,a3								*/
/*											*/
/*	movep.w d1,(a1)         ; 20 16+4       (ventura loader)			*/
/*	movep.l d1,(a1)         ; 28 24+4       (ventura loader, ulm loader)		*/
/*											*/
/*	movep.l d6,0(a5)        ; 28 24+4       (SNY I, TCB)				*/
/*	movep.w d5,0(a5)        ; 20 16+4       (SNY I, TCB)				*/
/*											*/
/*	move.b d1,(a1)          ; 12 8+4						*/
/*	move.b d1,(a2)          ; 12 8+4						*/
/*	move.b d1,(a3)          ; 12 8+4        (crickey ulm hidden)			*/
/*											*/
/*	move.w d1,(a1)          ; 12 8+4						*/
/*	move.w d1,(a2)          ; 12 8+4						*/
/*	move.l d1,(a1)          ; 16 12+4       (ulm loader)				*/
/*											*/
/*	movem.l d1,(a1)         ; 20 16+4						*/
/*	movem.l d1-d2,(a1)      ; 28 24+4						*/
/*	movem.l d1-d3,(a1)      ; 40 32+4+4						*/
/*	movem.l d1-d4,(a1)      ; 48 40+4+4						*/
/*	movem.l d1-d5,(a1)      ; 60 48+4+4+4						*/
/*	movem.l d1-d6,(a1)      ; 68 56+4+4+4						*/
/*	movem.l d1-d7,(a1)      ; 80 64+4+4+4+4						*/
/*	movem.l d0-d7,(a1)      ; 88 72+4+4+4+4						*/
/*											*/
/*	movep.w	d0,(a3)				(X-Out)					*/
/*											*/
/* This gives the following "model" :							*/
/*	- each access to $ff8800 or $ff8802 add 1 cycle wait state			*/
/*	- accesses to $ff8801 or $ff8803 are considered "valid" only if we don't access	*/
/*	  the corresponding "non shadow" addresses $ff8800/02 at the same time.		*/
/*	  This means only .B size (move.b for example) or movep opcode will work.	*/
/*	  If the access is valid, add 1 cycle wait state, else ignore the write and	*/
/*	  don't add any cycle.								*/



const char PSG_fileid[] = "Hatari psg.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "ioMem.h"
#include "joy.h"
#include "log.h"
#include "m68000.h"
#include "memorySnapShot.h"
#include "sound.h"
#include "printer.h"            /* because Printer I/O goes through PSG Register 15 */
#include "psg.h"
#if ENABLE_DSP_EMU
#include "falcon/dsp.h"
#endif
#include "screen.h"
#include "video.h"
#include "statusbar.h"
#include "mfp.h"


Uint8 PSGRegisterSelect;        /* 0xff8800 (read/write) */
Uint8 PSGRegisters[16];         /* Register in PSG, see PSG_REG_xxxx */

static unsigned int LastStrobe=0; /* Falling edge of Strobe used for printer */


/*-----------------------------------------------------------------------*/
/**
 * Reset variables used in PSG
 */
void PSG_Reset(void)
{
	PSGRegisterSelect = 0;
	memset(PSGRegisters, 0, sizeof(PSGRegisters));
	LastStrobe=0;
}


/*-----------------------------------------------------------------------*/
/**
 * Save/Restore snapshot of local variables ('MemorySnapShot_Store' handles type)
 */
void PSG_MemorySnapShot_Capture(bool bSave)
{
	/* Save/Restore details */
	MemorySnapShot_Store(&PSGRegisterSelect, sizeof(PSGRegisterSelect));
	MemorySnapShot_Store(PSGRegisters, sizeof(PSGRegisters));
	MemorySnapShot_Store(&LastStrobe, sizeof(LastStrobe));
}


/*-----------------------------------------------------------------------*/
/**
 * Write byte to the YM address register (usually 0xff8800). This is used
 * as a selector for when we read/write the YM data register (0xff8802).
 */
void PSG_Set_SelectRegister(Uint8 val)
{
	/* Store register used to read/write in $ff8802. This register */
	/* is 8 bits on the YM2149, this means it should not be masked */
	/* with 0xf. Instead, we keep the 8 bits, but we must ignore */
	/* read/write to ff8802 when PSGRegisterSelect >= 16 */
	PSGRegisterSelect = val;

	if (LOG_TRACE_LEVEL(TRACE_PSG_WRITE))
	{
		int FrameCycles, HblCounterVideo, LineCycles;
		Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
		LOG_TRACE_PRINT("ym write reg=0x%x video_cyc=%d %d@%d pc=%x instr_cycle %d\n",
		                PSGRegisterSelect, FrameCycles, LineCycles, HblCounterVideo,
		                M68000_GetPC(), CurrentInstrCycles);
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Read byte from 0xff8800, return PSG data
 */
Uint8 PSG_Get_DataRegister(void)
{
	/* Is a valid PSG register currently selected ? */
	if ( PSGRegisterSelect >= 16 )
		return 0xff;				/* not valid, return 0xff */

	if (PSGRegisterSelect == 14)
	{
		/* Second parallel port joystick uses centronics strobe bit as fire button: */
		if (ConfigureParams.Joysticks.Joy[JOYID_PARPORT2].nJoystickMode != JOYSTICK_DISABLED)
		{
			if (Joy_GetStickData(JOYID_PARPORT2) & 0x80)
				PSGRegisters[14] &= ~32;
			else
				PSGRegisters[14] |= 32;
		}
	}
	else if (PSGRegisterSelect == 15)
	{
		/* PSG register 15 is parallel port data register - used by parallel port joysticks: */
		if (ConfigureParams.Joysticks.Joy[JOYID_PARPORT1].nJoystickMode != JOYSTICK_DISABLED)
		{
			PSGRegisters[15] &= 0x0f;
			PSGRegisters[15] |= ~Joy_GetStickData(JOYID_PARPORT1) << 4;
		}
		if (ConfigureParams.Joysticks.Joy[JOYID_PARPORT2].nJoystickMode != JOYSTICK_DISABLED)
		{
			PSGRegisters[15] &= 0xf0;
			PSGRegisters[15] |= ~Joy_GetStickData(JOYID_PARPORT2) & 0x0f;
		}
	}

	/* Read data last selected by register */
	return PSGRegisters[PSGRegisterSelect];
}


/*-----------------------------------------------------------------------*/
/**
 * Write byte to YM's register (0xff8802), store according to PSG select register (0xff8800)
 */
void PSG_Set_DataRegister(Uint8 val)
{
	if (LOG_TRACE_LEVEL(TRACE_PSG_WRITE))
	{
		int FrameCycles, HblCounterVideo, LineCycles;
		Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
		LOG_TRACE_PRINT("ym write data reg=0x%x val=0x%x video_cyc=%d %d@%d pc=%x instr_cycle %d\n",
		                PSGRegisterSelect, val, FrameCycles, LineCycles,
		                HblCounterVideo, M68000_GetPC(), CurrentInstrCycles);
	}

	/* Is a valid PSG register currently selected ? */
	if ( PSGRegisterSelect >= 16 )
		return;					/* not valid, ignore write and do nothing */

	/* Create samples up until this point with current values */
	Sound_Update(false);

	/* Copy value to PSGRegisters[] */
	PSGRegisters[PSGRegisterSelect] = val;

	/* Clear unused bits for some regs */
	if ( ( PSGRegisterSelect == PSG_REG_CHANNEL_A_COARSE ) || ( PSGRegisterSelect == PSG_REG_CHANNEL_B_COARSE )
		|| ( PSGRegisterSelect == PSG_REG_CHANNEL_C_COARSE ) || ( PSGRegisterSelect == PSG_REG_ENV_SHAPE ) )
	  PSGRegisters[PSGRegisterSelect] &= 0x0f;	/* only keep bits 0 - 3 */

	else if ( ( PSGRegisterSelect == PSG_REG_CHANNEL_A_AMP ) || ( PSGRegisterSelect == PSG_REG_CHANNEL_B_AMP )
		|| ( PSGRegisterSelect == PSG_REG_CHANNEL_C_AMP ) || ( PSGRegisterSelect == PSG_REG_NOISE_GENERATOR ) )
	  PSGRegisters[PSGRegisterSelect] &= 0x1f;	/* only keep bits 0 - 4 */



	if ( PSGRegisterSelect < NUM_PSG_SOUND_REGISTERS )
	{
		/* Copy sound related registers 0..13 to the sound module's internal buffer */
		Sound_WriteReg ( PSGRegisterSelect , PSGRegisters[PSGRegisterSelect] );
	}

	else if ( PSGRegisterSelect == PSG_REG_IO_PORTA )
	{
	/*
	 * FIXME: This is only a prelimary dirty hack!
	 * Port B (Printer port) - writing here needs to be dispatched to the printer
	 * STROBE (Port A bit5) does a short LOW and back to HIGH when the char is valid
	 * To print you need to write the character byte to IOB and you need to toggle STROBE
	 * (like EmuTOS does).
	 */
		/* Printer dispatching only when printing is activated */
		if (ConfigureParams.Printer.bEnablePrinting)
		{
			/* Bit 5 - Centronics strobe? If STROBE is low and the LastStrobe was high,
					then print/transfer to the emulated Centronics port.
			 */
			if (LastStrobe && ( (PSGRegisters[PSG_REG_IO_PORTA]&(1<<5)) == 0 ))
			{
				/* Seems like we want to print something... */
				Printer_TransferByteTo(PSGRegisters[PSG_REG_IO_PORTB]);
				/* Initiate a possible GPIP0 Printer BUSY interrupt */
				MFP_InputOnChannel(MFP_GPIP_0_BIT,MFP_IERB,&MFP_IPRB);
				/* Initiate a possible GPIP1 Falcon ACK interrupt */
				if (ConfigureParams.System.nMachineType == MACHINE_FALCON)
					MFP_InputOnChannel(MFP_GPIP_1_BIT,MFP_IERB,&MFP_IPRB);
			}
		}
		LastStrobe = PSGRegisters[PSG_REG_IO_PORTA]&(1<<5);

		/* Bit 0-2 : side and drive select */
		if ( (PSGRegisters[PSG_REG_IO_PORTA]&(1<<1)) == 0 )
		{
			/* floppy drive A is ON */
			Statusbar_SetFloppyLed(DRIVE_LED_A, true);
		}
		else
		{
			Statusbar_SetFloppyLed(DRIVE_LED_A, false);
		}
		if ( (PSGRegisters[PSG_REG_IO_PORTA]&(1<<2)) == 0 )
		{
			/* floppy drive B is ON */
			Statusbar_SetFloppyLed(DRIVE_LED_B, true);
		}
		else
		{
			Statusbar_SetFloppyLed(DRIVE_LED_B, false);
		}

		/* Bit 3 - Centronics as input */
		if(PSGRegisters[PSG_REG_IO_PORTA]&(1<<3))
		{
			/* FIXME: might be needed if we want to emulate sound sampling hardware */
		}
		
		/* handle Falcon specific bits in PORTA of the PSG */
		if (ConfigureParams.System.nMachineType == MACHINE_FALCON)
		{
			/* Bit 4 - DSP reset? */
			if(PSGRegisters[PSG_REG_IO_PORTA]&(1<<4))
			{
				Log_Printf(LOG_DEBUG, "Calling DSP_Reset?\n");
#if ENABLE_DSP_EMU
				if (ConfigureParams.System.nDSPType == DSP_TYPE_EMU) {
					DSP_Reset();
				}
#endif
			}
			/* Bit 6 - Internal Speaker control */
			if(PSGRegisters[PSG_REG_IO_PORTA]&(1<<6))
			{
				/*Log_Printf(LOG_DEBUG, "Falcon: Internal Speaker state\n");*/
				/* FIXME: add code to handle? (if we want to emulate the speaker at all? */
			}
			/* Bit 7 - Reset IDE? */
			if(PSGRegisters[PSG_REG_IO_PORTA]&(1<<7))
			{
				Log_Printf(LOG_DEBUG, "Falcon: Reset IDE subsystem\n");
				/* FIXME: add code to handle IDE reset */
			}
		}
	
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Read byte from 0xff8800. Return current content of data register
 */
void PSG_ff8800_ReadByte(void)
{
	M68000_WaitState(1);				/* [NP] FIXME not 100% accurate, but gives good results */

	IoMem[IoAccessCurrentAddress] = PSG_Get_DataRegister();

	if (LOG_TRACE_LEVEL(TRACE_PSG_READ))
	{
		int FrameCycles, HblCounterVideo, LineCycles;
		Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
		LOG_TRACE_PRINT("ym read data %x=0x%x video_cyc=%d %d@%d pc=%x instr_cycle %d\n",
		                IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress],
		                FrameCycles, LineCycles, HblCounterVideo, M68000_GetPC(), CurrentInstrCycles);
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Read byte from 0xff8801/02/03. Return 0xff.
 */
void PSG_ff880x_ReadByte(void)
{
	M68000_WaitState(1);				/* [NP] FIXME not 100% accurate, but gives good results */

	IoMem[IoAccessCurrentAddress] = 0xff;

	if (LOG_TRACE_LEVEL(TRACE_PSG_READ))
	{
		int FrameCycles, HblCounterVideo, LineCycles;
		Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
		LOG_TRACE_PRINT("ym read void %x=0x%x video_cyc=%d %d@%d pc=%x instr_cycle %d\n",
		                IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress],
		                FrameCycles, LineCycles, HblCounterVideo, M68000_GetPC(), CurrentInstrCycles);
	}
}



/*-----------------------------------------------------------------------*/
/**
 * Write byte to 0xff8800. Set content of YM's address register.
 */
void PSG_ff8800_WriteByte(void)
{
//	M68000_WaitState(4);
	M68000_WaitState(1);				/* [NP] FIXME not 100% accurate, but gives good results */

	if (LOG_TRACE_LEVEL(TRACE_PSG_WRITE))
	{
		int FrameCycles, HblCounterVideo, LineCycles;
		Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
		LOG_TRACE_PRINT("ym write %x=0x%x video_cyc=%d %d@%d pc=%x instr_cycle %d\n",
		                IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress],
		                FrameCycles, LineCycles, HblCounterVideo, M68000_GetPC(), CurrentInstrCycles);
	}

	PSG_Set_SelectRegister ( IoMem[IoAccessCurrentAddress] );
}


/*-----------------------------------------------------------------------*/
/**
 * Write byte to 0xff8801. Set content of YM's address register under conditions.
 * Address 0xff8801 is a shadow version of 0xff8800, so both addresses can't be written
 * at the same time by the same instruction. This means only a .B access or
 * a movep will have a valid effect, other accesses are ignored.
 */
void PSG_ff8801_WriteByte(void)
{
	if ( nIoMemAccessSize == SIZE_BYTE )		/* byte access or movep */
	{	
		M68000_WaitState(1);			/* [NP] FIXME not 100% accurate, but gives good results */
	
		if (LOG_TRACE_LEVEL(TRACE_PSG_WRITE))
		{
			int FrameCycles, HblCounterVideo, LineCycles;
			Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
			LOG_TRACE_PRINT("ym write %x=0x%x video_cyc=%d %d@%d pc=%x instr_cycle %d\n",
					IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress],
					FrameCycles, LineCycles, HblCounterVideo, M68000_GetPC(), CurrentInstrCycles);
		}
	
		PSG_Set_SelectRegister ( IoMem[IoAccessCurrentAddress] );
	}

	else
	{						/* do nothing, just a trace if needed */
		if (LOG_TRACE_LEVEL(TRACE_PSG_WRITE))
		{
			int FrameCycles, HblCounterVideo, LineCycles;
			Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
			LOG_TRACE_PRINT("ym write ignored %x=0x%x video_cyc=%d %d@%d pc=%x instr_cycle %d\n",
					IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress],
					FrameCycles, LineCycles, HblCounterVideo, M68000_GetPC(), CurrentInstrCycles);
		}
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Write byte to 0xff8802. Set content of YM's data register.
 */
void PSG_ff8802_WriteByte(void)
{
//	M68000_WaitState(4);
	M68000_WaitState(1);				/* [NP] FIXME not 100% accurate, but gives good results */

	if (LOG_TRACE_LEVEL(TRACE_PSG_WRITE))
	{
		int FrameCycles, HblCounterVideo, LineCycles;
		Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
		LOG_TRACE_PRINT("ym write %x=0x%x video_cyc=%d %d@%d pc=%x instr_cycle %d\n",
				IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress],
				FrameCycles, LineCycles, HblCounterVideo, M68000_GetPC(), CurrentInstrCycles);
	}

	PSG_Set_DataRegister ( IoMem[IoAccessCurrentAddress] );
}


/*-----------------------------------------------------------------------*/
/**
 * Write byte to 0xff8803. Set content of YM's data register under conditions.
 * Address 0xff8803 is a shadow version of 0xff8802, so both addresses can't be written
 * at the same time by the same instruction. This means only a .B access or
 * a movep will have a valid effect, other accesses are ignored.
 */
void PSG_ff8803_WriteByte(void)
{
	if ( nIoMemAccessSize == SIZE_BYTE )		/* byte access or movep */
	{	
		M68000_WaitState(1);			/* [NP] FIXME not 100% accurate, but gives good results */
	
		if (LOG_TRACE_LEVEL(TRACE_PSG_WRITE))
		{
			int FrameCycles, HblCounterVideo, LineCycles;
			Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
			LOG_TRACE_PRINT("ym write %x=0x%x video_cyc=%d %d@%d pc=%x instr_cycle %d\n",
					IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress],
					FrameCycles, LineCycles, HblCounterVideo, M68000_GetPC(), CurrentInstrCycles);
		}
		
		PSG_Set_DataRegister ( IoMem[IoAccessCurrentAddress] );
	}

	else
	{						/* do nothing, just a trace if needed */
		if (LOG_TRACE_LEVEL(TRACE_PSG_WRITE))
		{
			int FrameCycles, HblCounterVideo, LineCycles;
			Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
			LOG_TRACE_PRINT("ym write ignored %x=0x%x video_cyc=%d %d@%d pc=%x instr_cycle %d\n",
					IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress],
					FrameCycles, LineCycles, HblCounterVideo, M68000_GetPC(), CurrentInstrCycles);
		}
	}
}

 /*
  * UAE - The Un*x Amiga Emulator - CPU core
  * 
  * MC68000 emulation
  *
  * Copyright 1995 Bernd Schmidt
  *
  * Adaptation to Hatari by Thomas Huth
  *
  * This file is distributed under the GNU Public License, version 2 or at
  * your option any later version. Read the file gpl.txt for details.
  */

#ifndef UAE_NEWCPU_H
#define UAE_NEWCPU_H

#include "readcpu.h"
#include "m68k.h"


/* custom chip support */
#define SPCFLAG_STOP 2
/*#define SPCFLAG_COPPER 4*/
#define SPCFLAG_INT 8
#define SPCFLAG_BRK 16
#define SPCFLAG_EXTRA_CYCLES 32
#define SPCFLAG_TRACE 64
#define SPCFLAG_DOTRACE 128
#define SPCFLAG_DOINT 256
/*#define SPCFLAG_BLTNASTY 512*/
#define SPCFLAG_EXEC 1024
#define SPCFLAG_MODE_CHANGE 8192


#ifndef SET_CFLG

#define SET_CFLG(x) (CFLG = (x))
#define SET_NFLG(x) (NFLG = (x))
#define SET_VFLG(x) (VFLG = (x))
#define SET_ZFLG(x) (ZFLG = (x))
#define SET_XFLG(x) (XFLG = (x))

#define GET_CFLG CFLG
#define GET_NFLG NFLG
#define GET_VFLG VFLG
#define GET_ZFLG ZFLG
#define GET_XFLG XFLG

#define CLEAR_CZNV do { \
 SET_CFLG (0); \
 SET_ZFLG (0); \
 SET_NFLG (0); \
 SET_VFLG (0); \
} while (0)

#define COPY_CARRY (SET_XFLG (GET_CFLG))
#endif

extern int areg_byteinc[];
extern int imm8_table[];

extern int movem_index1[256];
extern int movem_index2[256];
extern int movem_next[256];

extern int fpp_movem_index1[256];
extern int fpp_movem_index2[256];
extern int fpp_movem_next[256];

extern int broken_in;

typedef unsigned long cpuop_func (uae_u32) REGPARAM;

struct cputbl {
    cpuop_func *handler;
    int specific;
    uae_u16 opcode;
};

extern unsigned long op_illg (uae_u32) REGPARAM;

typedef char flagtype;

/* You can set this to long double to be more accurate. However, the
   resulting alignment issues will cost a lot of performance in some
   apps */
#define USE_LONG_DOUBLE 0

#if USE_LONG_DOUBLE
typedef long double fptype;
#else
typedef double fptype;
#endif

extern struct regstruct
{
    uae_u32 regs[16];
    uaecptr  usp,isp,msp;
    uae_u16 sr;
    flagtype t1;
    flagtype t0;
    flagtype s;
    flagtype m;
    flagtype x;
    flagtype stopped;
    int intmask;

    uae_u32 pc;
    uae_u8 *pc_p;
    uae_u8 *pc_oldp;

    uae_u32 vbr,sfc,dfc;

    fptype fp[8];
    fptype fp_result;

    uae_u32 fpcr,fpsr,fpiar;
    uae_u32 fpsr_highbyte;

    uae_u32 spcflags;

    uae_u32 prefetch_pc;
    uae_u32 prefetch;
} regs, lastint_regs;

STATIC_INLINE void set_special (uae_u32 x)
{
    regs.spcflags |= x;
}

STATIC_INLINE void unset_special (uae_u32 x)
{
    regs.spcflags &= ~x;
}

#define m68k_dreg(r,num) ((r).regs[(num)])
#define m68k_areg(r,num) (((r).regs + 8)[(num)])


STATIC_INLINE void m68k_setpc (uaecptr newpc)
{
    regs.pc_p = regs.pc_oldp = get_real_address (newpc);
    regs.pc = newpc;
}

STATIC_INLINE uaecptr m68k_getpc (void)
{
    return regs.pc + ((char *)regs.pc_p - (char *)regs.pc_oldp);
}

STATIC_INLINE uaecptr m68k_getpc_p (uae_u8 *p)
{
    return regs.pc + ((char *)p - (char *)regs.pc_oldp);
}

#define get_ibyte(o) do_get_mem_byte((uae_u8 *)(regs.pc_p + (o) + 1))
#define get_iword(o) do_get_mem_word((uae_u16 *)(regs.pc_p + (o)))
#define get_ilong(o) do_get_mem_long((uae_u32 *)(regs.pc_p + (o)))

STATIC_INLINE void refill_prefetch (uae_u32 currpc, uae_u32 offs)
{
    uae_u32 t = (currpc + offs) & ~3;
    uae_s32 pc_p_offs = t - currpc;
    uae_u8 *ptr = regs.pc_p + pc_p_offs;
    uae_u32 r;
#ifdef UNALIGNED_PROFITABLE
    r = *(uae_u32 *)ptr;
    regs.prefetch = r;
#else
    r = do_get_mem_long ((uae_u32 *)ptr);
    do_put_mem_long (&regs.prefetch, r);
#endif
    /* printf ("PC %lx T %lx PCPOFFS %d R %lx\n", currpc, t, pc_p_offs, r); */
    regs.prefetch_pc = t;
}

STATIC_INLINE uae_u32 get_ibyte_prefetch (uae_s32 o)
{
    uae_u32 currpc = m68k_getpc ();
    uae_u32 addr = currpc + o + 1;
    uae_u32 offs = addr - regs.prefetch_pc;
    uae_u32 v;
    if (offs > 3) {
	refill_prefetch (currpc, o + 1);
	offs = addr - regs.prefetch_pc;
    }
    v = do_get_mem_byte (((uae_u8 *)&regs.prefetch) + offs);
    if (offs >= 2)
	refill_prefetch (currpc, 4);
    /* printf ("get_ibyte PC %lx ADDR %lx OFFS %lx V %lx\n", currpc, addr, offs, v); */
    return v;
}
STATIC_INLINE uae_u32 get_iword_prefetch (uae_s32 o)
{
    uae_u32 currpc = m68k_getpc ();
    uae_u32 addr = currpc + o;
    uae_u32 offs = addr - regs.prefetch_pc;
    uae_u32 v;
    if (offs > 3) {
	refill_prefetch (currpc, o);
	offs = addr - regs.prefetch_pc;
    }
    v = do_get_mem_word ((uae_u16 *)(((uae_u8 *)&regs.prefetch) + offs));
    if (offs >= 2)
	refill_prefetch (currpc, 4);
    /* printf ("get_iword PC %lx ADDR %lx OFFS %lx V %lx\n", currpc, addr, offs, v); */
    return v;
}
STATIC_INLINE uae_u32 get_ilong_prefetch (uae_s32 o)
{
    uae_u32 v = get_iword_prefetch (o);
    v <<= 16;
    v |= get_iword_prefetch (o + 2);
    return v;
}

#define m68k_incpc(o) (regs.pc_p += (o))

STATIC_INLINE void fill_prefetch_0 (void)
{
}

#define fill_prefetch_2 fill_prefetch_0

/* These are only used by the 68020/68881 code, and therefore don't
 * need to handle prefetch.  */
STATIC_INLINE uae_u32 next_ibyte (void)
{
    uae_u32 r = get_ibyte (0);
    m68k_incpc (2);
    return r;
}

STATIC_INLINE uae_u32 next_iword (void)
{
    uae_u32 r = get_iword (0);
    m68k_incpc (2);
    return r;
}

STATIC_INLINE uae_u32 next_ilong (void)
{
    uae_u32 r = get_ilong (0);
    m68k_incpc (4);
    return r;
}

#define m68k_setpc_bcc  m68k_setpc
#define m68k_setpc_rte  m68k_setpc

STATIC_INLINE void m68k_setstopped (int stop)
{
    regs.stopped = stop;
    /* A traced STOP instruction drops through immediately without
       actually stopping.  */
    if (stop && (regs.spcflags & SPCFLAG_DOTRACE) == 0)
	regs.spcflags |= SPCFLAG_STOP;
}

/* m68k_do_rts, m68k_do_bsr and m68k_do_jsr were originally defined in
 * compiler.h, but since that header file has been removed from Hatari,
 * they are now defined here: */
STATIC_INLINE void m68k_do_rts(void)
{
    m68k_setpc(get_long(m68k_areg(regs, 7)));
    m68k_areg(regs, 7) += 4;
}

STATIC_INLINE void m68k_do_bsr(uaecptr oldpc, uae_s32 offset)
{
    m68k_areg(regs, 7) -= 4;
    put_long(m68k_areg(regs, 7), oldpc);
    m68k_incpc(offset);
}

STATIC_INLINE void m68k_do_jsr(uaecptr oldpc, uaecptr dest)
{
    m68k_areg(regs, 7) -= 4;
    put_long(m68k_areg(regs, 7), oldpc);
    m68k_setpc(dest);
}


extern uae_u32 get_disp_ea_020 (uae_u32 base, uae_u32 dp);
extern uae_u32 get_disp_ea_000 (uae_u32 base, uae_u32 dp);

extern uae_s32 ShowEA (FILE *, int reg, amodes mode, wordsizes size, char *buf);

extern void MakeSR (void);
extern void MakeFromSR (void);
extern void Exception (int, uaecptr);
extern void dump_counts (void);
extern int m68k_move2c (int, uae_u32 *);
extern int m68k_movec2 (int, uae_u32 *);
extern void m68k_divl (uae_u32, uae_u32, uae_u16, uaecptr);
extern void m68k_mull (uae_u32, uae_u32, uae_u16);
extern void build_cpufunctbl(void);
extern void init_m68k (void);
extern void m68k_go (int);
extern void m68k_dumpstate (FILE *, uaecptr *);
extern void m68k_disasm (FILE *, uaecptr, uaecptr *, int);
extern void m68k_reset (void);

extern void mmu_op (uae_u32, uae_u16);

extern void fpp_opp (uae_u32, uae_u16);
extern void fdbcc_opp (uae_u32, uae_u16);
extern void fscc_opp (uae_u32, uae_u16);
extern void ftrapcc_opp (uae_u32,uaecptr);
extern void fbcc_opp (uae_u32, uaecptr, uae_u32);
extern void fsave_opp (uae_u32);
extern void frestore_opp (uae_u32);

extern int lastInstructionCycles;

/* Opcode of faulting instruction */
extern uae_u16 last_op_for_exception_3;
/* PC at fault time */
extern uaecptr last_addr_for_exception_3;
/* Address that generated the exception */
extern uaecptr last_fault_for_exception_3;

#define CPU_OP_NAME(a) op ## a

/* 68040 */
extern struct cputbl op_smalltbl_0_ff[];
/* 68020 + 68881 */
extern struct cputbl op_smalltbl_1_ff[];
/* 68020 */
extern struct cputbl op_smalltbl_2_ff[];
/* 68010 */
extern struct cputbl op_smalltbl_3_ff[];
/* 68000 */
extern struct cputbl op_smalltbl_4_ff[];
/* 68000 slow but compatible.  */
extern struct cputbl op_smalltbl_5_ff[];

extern cpuop_func *cpufunctbl[65536];

#endif	/* UAE_NEWCPU_H */

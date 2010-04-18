/*
 * Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
 *
 * (c) Copyright 1996 - 2001 Gary Henderson (gary.henderson@ntlworld.com) and
 *                           Jerremy Koot (jkoot@snes9x.com)
 *
 * Super FX C emulator code 
 * (c) Copyright 1997 - 1999 Ivar (ivar@snes9x.com) and
 *                           Gary Henderson.
 * Super FX assembler emulator code (c) Copyright 1998 zsKnight and _Demo_.
 *
 * DSP1 emulator code (c) Copyright 1998 Ivar, _Demo_ and Gary Henderson.
 * C4 asm and some C emulation code (c) Copyright 2000 zsKnight and _Demo_.
 * C4 C code (c) Copyright 2001 Gary Henderson (gary.henderson@ntlworld.com).
 *
 * DOS port code contains the works of other authors. See headers in
 * individual files.
 *
 * Snes9x homepage: http://www.snes9x.com
 *
 * Permission to use, copy, modify and distribute Snes9x in both binary and
 * source form, for non-commercial purposes, is hereby granted without fee,
 * providing that this license information and copyright notice appear with
 * all copies and any derived work.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event shall the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Snes9x is freeware for PERSONAL USE only. Commercial users should
 * seek permission of the copyright holders first. Commercial use includes
 * charging money for Snes9x or software derived from Snes9x.
 *
 * The copyright holders request that bug fixes and improvements to the code
 * should be forwarded to them so everyone can benefit from the modifications
 * in future versions.
 *
 * Super NES and Super Nintendo Entertainment System are trademarks of
 * Nintendo Co., Limited and its subsidiary companies.
 */
#include <fcntl.h>
#include <sys/mman.h>
#include "snes9x.h"
#include "memmap.h"
#include "cpuops.h"
#include "ppu.h"
#include "cpuexec.h"
#include "debug.h"
#include "snapshot.h"
#include "gfx.h"
#include "missing.h"
#include "apu.h"
#include "dma.h"
#include "fxemu.h"
#include "sa1.h"
#include "unix/jz4740.h"

/* Define this to the CPU frequency */
#define CPU_FREQ 336000000    /* CPU clock: 336 MHz */
#define CFG_EXTAL 12000000    /* EXT clock: 12 Mhz */

// SDRAM Timings, unit: ns
#define SDRAM_TRAS		45	/* RAS# Active Time */
#define SDRAM_RCD		20	/* RAS# to CAS# Delay */
#define SDRAM_TPC		20	/* RAS# Precharge Time */
#define SDRAM_TRWL		7	/* Write Latency Time */
//#define SDRAM_TREF	        15625	/* Refresh period: 4096 refresh cycles/64ms */ 
#define SDRAM_TREF      7812  /* Refresh period: 8192 refresh cycles/64ms */

static unsigned long jz_dev=0;
static volatile unsigned long *jz_cdcregl, *jz_cpmregl, *jz_emcregl, *jz_gpioregl, *jz_lcdregl;
static unsigned short *jz_emcregs;
extern int CLK_FREQ;

void a320_init(void)
{
 	if(!jz_dev)  jz_dev = open("/dev/mem", O_RDWR);  
	jz_gpioregl=(unsigned long  *)mmap(0, 0x300, PROT_READ|PROT_WRITE, MAP_SHARED, jz_dev, 0x10010000);
	jz_cpmregl=(unsigned long  *)mmap(0, 0x10, PROT_READ|PROT_WRITE, MAP_SHARED, jz_dev, 0x10000000);
	jz_emcregl=(unsigned long  *)mmap(0, 0x90, PROT_READ|PROT_WRITE, MAP_SHARED, jz_dev, 0x13010000);
	jz_lcdregl=(unsigned long  *)mmap(0, 0x90, PROT_READ|PROT_WRITE, MAP_SHARED, jz_dev, 0x13050000);
	jz_emcregs=(unsigned short *)jz_emcregl;
}

inline int sdram_convert(unsigned int pllin,unsigned int *sdram_freq)
{
	register unsigned int ns, dmcr,tmp;
 
	ns = 1000000000 / pllin;
	tmp = SDRAM_TRAS/ns;
	if (tmp < 4) tmp = 4;
	if (tmp > 11) tmp = 11;
	dmcr |= ((tmp-4) << EMC_DMCR_TRAS_BIT);

	tmp = SDRAM_RCD/ns;
	if (tmp > 3) tmp = 3;
	dmcr |= (tmp << EMC_DMCR_RCD_BIT);

	tmp = SDRAM_TPC/ns;
	if (tmp > 7) tmp = 7;
	dmcr |= (tmp << EMC_DMCR_TPC_BIT);

	tmp = SDRAM_TRWL/ns;
	if (tmp > 3) tmp = 3;
	dmcr |= (tmp << EMC_DMCR_TRWL_BIT);

	tmp = (SDRAM_TRAS + SDRAM_TPC)/ns;
	if (tmp > 14) tmp = 14;
	dmcr |= (((tmp + 1) >> 1) << EMC_DMCR_TRC_BIT);

	/* Set refresh registers */
	tmp = SDRAM_TREF/ns;
	tmp = tmp/64 + 1;
	if (tmp > 0xff) tmp = 0xff;
        *sdram_freq = tmp; 

	return 0;

}
 
void pll_init(unsigned int clock)
{
	register unsigned int cfcr, plcr1;
	unsigned int sdramclock = 0;
	int n2FR[33] = {
		0, 0, 1, 2, 3, 0, 4, 0, 5, 0, 0, 0, 6, 0, 0, 0,
		7, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0,
		9
	};
	//int div[5] = {1, 4, 4, 4, 4}; /* divisors of I:S:P:L:M */
  	int div[5] = {1, 3, 3, 3, 3}; /* divisors of I:S:P:L:M */
	int nf, pllout2;

	cfcr = CPM_CPCCR_CLKOEN |
		(n2FR[div[0]] << CPM_CPCCR_CDIV_BIT) | 
		(n2FR[div[1]] << CPM_CPCCR_HDIV_BIT) | 
		(n2FR[div[2]] << CPM_CPCCR_PDIV_BIT) |
		(n2FR[div[3]] << CPM_CPCCR_MDIV_BIT) |
		(n2FR[div[4]] << CPM_CPCCR_LDIV_BIT);

	pllout2 = (cfcr & CPM_CPCCR_PCS) ? clock : (clock / 2);

	/* Init UHC clock */
//	REG_CPM_UHCCDR = pllout2 / 48000000 - 1;
    	jz_cpmregl[0x6C>>2] = pllout2 / 48000000 - 1;
	nf = clock * 2 / CFG_EXTAL;
	plcr1 = ((nf - 2) << CPM_CPPCR_PLLM_BIT) | /* FD */
		(0 << CPM_CPPCR_PLLN_BIT) |	/* RD=0, NR=2 */
		(0 << CPM_CPPCR_PLLOD_BIT) |    /* OD=0, NO=1 */
		(0x20 << CPM_CPPCR_PLLST_BIT) | /* PLL stable time */
		CPM_CPPCR_PLLEN;                /* enable PLL */          

	/* init PLL */
//	REG_CPM_CPCCR = cfcr;
//	REG_CPM_CPPCR = plcr1;
      	jz_cpmregl[0] = cfcr;
    	jz_cpmregl[0x10>>2] = plcr1;
	
  	sdram_convert(clock,&sdramclock);
 	if(sdramclock > 0)
  	{
//	REG_EMC_RTCOR = sdramclock;
//	REG_EMC_RTCNT = sdramclock;	  
      	jz_emcregs[0x8C>>1] = sdramclock;
    	jz_emcregs[0x88>>1] = sdramclock;	

  	}else
  	{
  	printf("sdram init fail!\n");
  	while(1);
  	} 
	
}

/* faking gp2x cpuctrl.c */
void cpuctrl_init(void)
{
}

void cpuctrl_deinit(void)
{
}

void set_FCLK(unsigned MHZ)
{
	pll_init(MHZ*1000000);
}

void Disable_940(void)
{
}

void gp2x_video_wait_vsync(void)
{
}

void set_RAM_Timings(int tRC, int tRAS, int tWR, int tMRD, int tRFC, int tRP, int tRCD)
{
}

void set_gamma(int g100, int A_SNs_curve)
{
}

void set_LCD_custom_rate(int rate)
{
}

void unset_LCD_custom_rate(void)
{
}

void S9xMainLoop (void)
{
	struct SCPUState *cpu = &CPU;
	struct SICPU *icpu = &ICPU;
	struct SIAPU *iapu = &IAPU;
	struct SAPU *apu = &APU;
	struct SRegisters *reg = &Registers;
	struct SAPURegisters *areg = &APURegisters;

//	set_FCLK(CLK_FREQ);

    for (;;)
    {
		APU_EXECUTE ();

		if (cpu->Flags)
		{
			if (cpu->Flags & NMI_FLAG)
			{
				if (--cpu->NMICycleCount == 0)
				{
					cpu->Flags &= ~NMI_FLAG;
					if (cpu->WaitingForInterrupt)
					{
						cpu->WaitingForInterrupt = FALSE;
						++cpu->PC;
					}
					S9xOpcode_NMI ();
				}
			}

#ifdef DEBUGGER
			if ((cpu->Flags & BREAK_FLAG) &&
			!(cpu->Flags & SINGLE_STEP_FLAG))
			{
				for (int Break = 0; Break != 6; Break++)
				{
					if (S9xBreakpoint[Break].Enabled &&
					S9xBreakpoint[Break].Bank == Registers.PB &&
					S9xBreakpoint[Break].Address == cpu->PC - cpu->PCBase)
					{
						if (S9xBreakpoint[Break].Enabled == 2)
							S9xBreakpoint[Break].Enabled = TRUE;
						else
							cpu->Flags |= DEBUG_MODE_FLAG;
					}
				}
			}
#endif
			CHECK_SOUND ();

			if (cpu->Flags & IRQ_PENDING_FLAG)
			{
				if (cpu->IRQCycleCount == 0)
				{
					if (cpu->WaitingForInterrupt)
					{
						cpu->WaitingForInterrupt = FALSE;
						cpu->PC++;
					}
					if (cpu->IRQActive)
//					if (cpu->IRQActive && !Settings.DisableIRQ)
					{
						if (!CHECKFLAG (IRQ))
							S9xOpcode_IRQ ();
					}
					else
						cpu->Flags &= ~IRQ_PENDING_FLAG;
				}
				else
					cpu->IRQCycleCount--;
			}
#ifdef DEBUGGER
			if (cpu->Flags & DEBUG_MODE_FLAG)
				break;
#endif
			if (cpu->Flags & SCAN_KEYS_FLAG)
				break;
#ifdef DEBUGGER
			if (cpu->Flags & TRACE_FLAG)
				S9xTrace ();

			if (cpu->Flags & SINGLE_STEP_FLAG)
			{
				cpu->Flags &= ~SINGLE_STEP_FLAG;
				cpu->Flags |= DEBUG_MODE_FLAG;
			}
#endif
		} //if (CPU.Flags)
#ifdef CPU_SHUTDOWN
		cpu->PCAtOpcodeStart = cpu->PC;
#endif
#ifdef VAR_CYCLES
		cpu->Cycles += cpu->MemSpeed;
#else
		cpu->Cycles += icpu->Speed [*cpu->PC];
#endif
		(*icpu->S9xOpcodes [*cpu->PC++].S9xOpcode) (reg, icpu, cpu);
//#ifndef _ZAURUS
		if (SA1.Executing)
			S9xSA1MainLoop ();
//#endif
		DO_HBLANK_CHECK();
	} // for(;;)
    Registers.PC = cpu->PC - cpu->PCBase;
    S9xPackStatus ();
    areg->PC = iapu->PC - iapu->RAM;
    S9xAPUPackStatus_OP ();
    if (cpu->Flags & SCAN_KEYS_FLAG)
    {
#ifdef DEBUGGER
		if (!(cpu->Flags & FRAME_ADVANCE_FLAG))
#endif
			S9xSyncSpeed ();
		cpu->Flags &= ~SCAN_KEYS_FLAG;
    }
//#ifndef _ZAURUS
    if (cpu->BRKTriggered && Settings.SuperFX && !cpu->TriedInterleavedMode2)
    {
		cpu->TriedInterleavedMode2 = TRUE;
		cpu->BRKTriggered = FALSE;
		S9xDeinterleaveMode2 ();
    }
//#endif
}


void S9xSetIRQ (uint32 source, struct SCPUState *cpu)
{
    cpu->IRQActive |= source;
    cpu->Flags |= IRQ_PENDING_FLAG;
    cpu->IRQCycleCount = 3;
    if (cpu->WaitingForInterrupt)
    {
		// Force IRQ to trigger immediately after WAI - 
		// Final Fantasy Mystic Quest crashes without this.
		cpu->IRQCycleCount = 0;
		cpu->WaitingForInterrupt = FALSE;
		cpu->PC++;
    }
}

void S9xClearIRQ (uint32 source)
{
    CLEAR_IRQ_SOURCE (source);
}

void S9xDoHBlankProcessing (struct SCPUState *cpu, struct SAPU *apu, struct SIAPU *iapu)
{
	struct SPPU *ppu = &PPU;
	struct InternalPPU *ippu = &IPPU;

#ifdef CPU_SHUTDOWN
    cpu->WaitCounter++;
#endif
    switch (cpu->WhichEvent)
    {
    case HBLANK_START_EVENT:
		if (ippu->HDMA && cpu->V_Counter <= ppu->ScreenHeight)
			ippu->HDMA = S9xDoHDMA (ippu, ppu, cpu);
	break;

    case HBLANK_END_EVENT:
//#ifndef _ZAURUS
	S9xSuperFXExec ();
//#endif
#ifndef STORM
	if (Settings.SoundSync)
	    S9xGenerateSound ();
#endif

	cpu->Cycles -= Settings.H_Max;
	if (iapu->APUExecuting)
	    apu->Cycles -= Settings.H_Max;
	else
	    apu->Cycles = 0;

	cpu->NextEvent = -1;
	ICPU.Scanline++;

	if (++cpu->V_Counter > (Settings.PAL ? SNES_MAX_PAL_VCOUNTER : SNES_MAX_NTSC_VCOUNTER))
	{
	    ppu->OAMAddr = ppu->SavedOAMAddr;
	    ppu->OAMFlip = 0;
	    cpu->V_Counter = 0;
	    cpu->NMIActive = FALSE;
	    ICPU.Frame++;
	    ppu->HVBeamCounterLatched = 0;
	    cpu->Flags |= SCAN_KEYS_FLAG;
	    S9xStartHDMA ();
	}

	if (ppu->VTimerEnabled && !ppu->HTimerEnabled &&
	    cpu->V_Counter == ppu->IRQVBeamPos)
	{
	    S9xSetIRQ (PPU_V_BEAM_IRQ_SOURCE, cpu);
	}

	if (cpu->V_Counter == ppu->ScreenHeight + FIRST_VISIBLE_LINE)
	{
	    // Start of V-blank
	    S9xEndScreenRefresh (ppu);
	    ppu->FirstSprite = 0;
	    ippu->HDMA = 0;
	    // Bits 7 and 6 of $4212 are computed when read in S9xGetPPU.
	    missing.dma_this_frame = 0;
	    ippu->MaxBrightness = ppu->Brightness;
	    ppu->ForcedBlanking = (Memory.FillRAM [0x2100] >> 7) & 1;

	    Memory.FillRAM[0x4210] = 0x80;
	    if (Memory.FillRAM[0x4200] & 0x80)
	    {
			cpu->NMIActive = TRUE;
			cpu->Flags |= NMI_FLAG;
			cpu->NMICycleCount = cpu->NMITriggerPoint;
	    }
    }

	if (cpu->V_Counter == ppu->ScreenHeight + 3)
	    S9xUpdateJoypads (&IPPU);

	if (cpu->V_Counter == FIRST_VISIBLE_LINE)
	{
	    Memory.FillRAM[0x4210] = 0;
	    cpu->Flags &= ~NMI_FLAG;
	    S9xStartScreenRefresh ();
	}
	if (cpu->V_Counter >= FIRST_VISIBLE_LINE &&
	    cpu->V_Counter < ppu->ScreenHeight + FIRST_VISIBLE_LINE)
	{
	    RenderLine (cpu->V_Counter - FIRST_VISIBLE_LINE, ppu);
	}
	// Use TimerErrorCounter to skip update of SPC700 timers once
	// every 128 updates. Needed because this section of code is called
	// once every emulated 63.5 microseconds, which coresponds to
	// 15.750KHz, but the SPC700 timers need to be updated at multiples
	// of 8KHz, hence the error correction.
//	IAPU.TimerErrorCounter++;
//	if (IAPU.TimerErrorCounter >= )
//	    IAPU.TimerErrorCounter = 0;
//	else
	{
	    if (apu->TimerEnabled [2])
	    {
			apu->Timer [2] += 4;
			while (apu->Timer [2] >= apu->TimerTarget [2])
			{
				iapu->RAM [0xff] = (iapu->RAM [0xff] + 1) & 0xf;
				apu->Timer [2] -= apu->TimerTarget [2];
#ifdef SPC700_SHUTDOWN		
				iapu->WaitCounter++;
				iapu->APUExecuting = TRUE;
#endif		
			}
	    }
	    if (cpu->V_Counter & 1)
	    {
			if (apu->TimerEnabled [0])
			{
				apu->Timer [0]++;
				if (apu->Timer [0] >= apu->TimerTarget [0])
				{
					iapu->RAM [0xfd] = (iapu->RAM [0xfd] + 1) & 0xf;
					apu->Timer [0] = 0;
#ifdef SPC700_SHUTDOWN		
					iapu->WaitCounter++;
					iapu->APUExecuting = TRUE;
#endif		    
			    }
			}
			if (apu->TimerEnabled [1])
			{
				apu->Timer [1]++;
				if (apu->Timer [1] >= apu->TimerTarget [1])
				{
					iapu->RAM [0xfe] = (iapu->RAM [0xfe] + 1) & 0xf;
					apu->Timer [1] = 0;
#ifdef SPC700_SHUTDOWN		
					iapu->WaitCounter++;
					iapu->APUExecuting = TRUE;
#endif		    
			    }
			}
	    }
	}
	break; //HBLANK_END_EVENT

    case HTIMER_BEFORE_EVENT:
    case HTIMER_AFTER_EVENT:
	if (ppu->HTimerEnabled &&
	    (!ppu->VTimerEnabled || cpu->V_Counter == ppu->IRQVBeamPos))
	{
	    S9xSetIRQ (PPU_H_BEAM_IRQ_SOURCE, cpu);
	}
	break;
    }
//    S9xReschedule ();
    uint8 which;
    long max;
    
    if (cpu->WhichEvent == HBLANK_START_EVENT ||
	cpu->WhichEvent == HTIMER_AFTER_EVENT)
    {
	which = HBLANK_END_EVENT;
	max = Settings.H_Max;
    }
    else
    {
	which = HBLANK_START_EVENT;
	max = Settings.HBlankStart;
    }

    if (ppu->HTimerEnabled &&
        (long) ppu->HTimerPosition < max &&
	(long) ppu->HTimerPosition > cpu->NextEvent &&
	(!ppu->VTimerEnabled ||
	 (ppu->VTimerEnabled && cpu->V_Counter == ppu->IRQVBeamPos)))
    {
	which = (long) ppu->HTimerPosition < Settings.HBlankStart ?
			HTIMER_BEFORE_EVENT : HTIMER_AFTER_EVENT;
	max = ppu->HTimerPosition;
    }
    cpu->NextEvent = max;
    cpu->WhichEvent = which;
}

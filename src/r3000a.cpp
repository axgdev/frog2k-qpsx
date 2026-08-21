/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *   schultz.ryan@gmail.com, http://rschultz.ath.cx/code.php               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA 02111-1307 USA.            *
 ***************************************************************************/

/*
* R3000A CPU functions.
*/

//senquack - May 22 2016 NOTE:
// These have all been updated to use new PSXINT_* interrupts enum and intCycle
// struct from PCSX Reloaded/Rearmed (much cleaner, no magic numbers)

#include "r3000a.h"
#include "cdrom.h"
#include "mdec.h"
#include "gte.h"
#include "psxevents.h"
#include "profiler.h"  /* v094: Detailed CPU profiling */

extern "C" void xlog(const char *fmt, ...);

/* These defaults keep the generic QPSX tree buildable outside the SF2000
 * frontend.  The frontend overrides them in its production flag set and the
 * resulting values are printed once so a physical-device log identifies the
 * exact A/B candidate that produced it. */
#ifndef QPSX_MIPS_DISPATCH_CACHE_ENTRIES
#define QPSX_MIPS_DISPATCH_CACHE_ENTRIES 64
#endif
#ifndef QPSX_GTE_NATIVE_DIVIDE
#define QPSX_GTE_NATIVE_DIVIDE 0
#endif
#ifndef QPSX_GTE_HOT_O3
#define QPSX_GTE_HOT_O3 0
#endif
#ifndef QPSX_MIPS_PSMEM_REG
#define QPSX_MIPS_PSMEM_REG 0
#endif
#ifndef QPSX_MIPS_PERSISTENT_RETURN_RA
#define QPSX_MIPS_PERSISTENT_RETURN_RA 0
#endif
#ifndef QPSX_MIPS_FOLD_DIRECT_JUMPS
#define QPSX_MIPS_FOLD_DIRECT_JUMPS 0
#endif
#ifndef QPSX_MIPS_FOLD_DIRECT_JUMPS_MAX
#define QPSX_MIPS_FOLD_DIRECT_JUMPS_MAX 0
#endif
#ifndef QPSX_MIPS_FOLD_DIRECT_JUMPS_BYTES
#define QPSX_MIPS_FOLD_DIRECT_JUMPS_BYTES 0
#endif
#ifndef QPSX_PROFILER_ENABLED
#define QPSX_PROFILER_ENABLED 0
#endif
#ifndef QPSX_RUNTIME_TELEMETRY
#define QPSX_RUNTIME_TELEMETRY 0
#endif
#ifndef QPSX_GPU_FIXED_FAST_PATH
#define QPSX_GPU_FIXED_FAST_PATH 0
#endif
#ifndef QPSX_GPU_FIXED_LIGHTING
#define QPSX_GPU_FIXED_LIGHTING QPSX_GPU_FIXED_FAST_PATH
#endif
#ifndef QPSX_GPU_RUNTIME_METRICS
#define QPSX_GPU_RUNTIME_METRICS 0
#endif
#ifndef QPSX_GPU_RECIP_TABLE_BITS
#define QPSX_GPU_RECIP_TABLE_BITS 0
#endif
#ifndef QPSX_GPU_4BPP_FULLMASK
#define QPSX_GPU_4BPP_FULLMASK 0
#endif
#ifndef QPSX_GPU_4BPP_FULLMASK_MIN_PIXELS
#define QPSX_GPU_4BPP_FULLMASK_MIN_PIXELS 16
#endif
#ifndef QPSX_GPU_4BPP_FULLMASK_PACKED_WRITES
#define QPSX_GPU_4BPP_FULLMASK_PACKED_WRITES 0
#endif
#if defined(SHMEM_MIRRORING) || defined(TMPFS_MIRRORING)
#define QPSX_MIPS_VIRTUAL_MIRRORING 1
#else
#define QPSX_MIPS_VIRTUAL_MIRRORING 0
#endif
#ifndef QPSX_MIPS_FAST_MEM_CONVERT
#define QPSX_MIPS_FAST_MEM_CONVERT 0
#endif
#ifndef QPSX_MIPS_PROPAGATE_FUZZY_ADDR
#define QPSX_MIPS_PROPAGATE_FUZZY_ADDR 0
#endif
#ifndef QPSX_GPU_LINEAR_4BPP
#define QPSX_GPU_LINEAR_4BPP 0
#endif
#ifndef QPSX_GPU_PACKED_TILE_WRITES
#define QPSX_GPU_PACKED_TILE_WRITES 0
#endif
#ifndef QPSX_GPU_PACKED_SPRITE_4BPP
#define QPSX_GPU_PACKED_SPRITE_4BPP 0
#endif
#ifndef QPSX_GPU_PACKED_POLY_WRITES
#define QPSX_GPU_PACKED_POLY_WRITES 0
#endif
#ifndef QPSX_GPU_4BPP_GOURAUD_FLATV
#define QPSX_GPU_4BPP_GOURAUD_FLATV 0
#endif
#ifndef QPSX_GPU_4BPP_GOURAUD_FLATV_MIN_PIXELS
#define QPSX_GPU_4BPP_GOURAUD_FLATV_MIN_PIXELS 16
#endif
#ifndef QPSX_GPU_4BPP_GOURAUD_CACHE
#define QPSX_GPU_4BPP_GOURAUD_CACHE 0
#endif
#ifndef QPSX_GPU_4BPP_FLATV
#define QPSX_GPU_4BPP_FLATV 0
#endif
#ifndef QPSX_GPU_4BPP_FLATV_MIN_PIXELS
#define QPSX_GPU_4BPP_FLATV_MIN_PIXELS 16
#endif
#ifndef QPSX_GPU_4BPP_FLATV_ROW
#define QPSX_GPU_4BPP_FLATV_ROW 0
#endif
#ifndef QPSX_GPU_4BPP_FLATV_ROW_MIN_PIXELS
#define QPSX_GPU_4BPP_FLATV_ROW_MIN_PIXELS 16
#endif
#ifndef QPSX_GPU_4BPP_PALETTE_LUT
#define QPSX_GPU_4BPP_PALETTE_LUT 0
#endif
#ifndef QPSX_GPU_HOT_DRIVER_ORDER
#define QPSX_GPU_HOT_DRIVER_ORDER 0
#endif
#ifndef QPSX_MIPS_DISPATCH_CACHE_GP
#define QPSX_MIPS_DISPATCH_CACHE_GP 0
#endif
#ifndef QPSX_MIPS_DISPATCH_CACHE_GP_TRUST_ABI
#define QPSX_MIPS_DISPATCH_CACHE_GP_TRUST_ABI 0
#endif
#ifndef QPSX_MIPS_DISPATCH_BRANCH_LIKELY
#define QPSX_MIPS_DISPATCH_BRANCH_LIKELY 0
#endif
#ifndef QPSX_MIPS_DISPATCH_FRAME_BRANCH_LIKELY
#define QPSX_MIPS_DISPATCH_FRAME_BRANCH_LIKELY 0
#endif
#ifndef QPSX_BUILD_TAG
#define QPSX_BUILD_TAG "untagged"
#endif
#ifndef QPSX_BUILD_FINGERPRINT
#define QPSX_BUILD_FINGERPRINT "untracked"
#endif
#ifndef QPSX_GE_RAW_VRAM
#define QPSX_GE_RAW_VRAM 0
#endif
#ifndef QPSX_MIPS_ASM_MEM_READS
#define QPSX_MIPS_ASM_MEM_READS 0
#endif
#ifndef QPSX_LINUX_RAM_HELPER_FASTPATH
#define QPSX_LINUX_RAM_HELPER_FASTPATH 0
#endif
#ifndef QPSX_HLE_LAZY_EVENT_CHECK
#define QPSX_HLE_LAZY_EVENT_CHECK 0
#endif
#ifndef QPSX_GPU_GOURAUD_LINE_FLATFAST
#define QPSX_GPU_GOURAUD_LINE_FLATFAST 0
#endif

#ifdef PSXREC
extern "C" void recLogTelemetry(void);
extern u32 *recMem;
#endif

PcsxConfig Config;
R3000Acpu *psxCpu=NULL;
psxRegisters psxRegs;

int psxInit() {
	printf("Running PCSX Version %s (%s).\n", PACKAGE_VERSION, __DATE__);
	/* Keep each fingerprint record below qpsx_adapter's 320-byte kmsg
	 * buffer. Losing the complete line makes physical A/B logs ambiguous,
	 * especially when a long build tag is used. */
	xlog("QPSX: build_id=%s config=%s dispatch_cache=%d dispatch_gp=%d dispatch_gp_abi=%d "
	     "dispatch_bl=%d dispatch_frame_bl=%d psxM_reg=%d gte_native_div=%d "
	     "gte_hot_o3=%d return_ra=%d",
	     QPSX_BUILD_TAG, QPSX_BUILD_FINGERPRINT,
	     QPSX_MIPS_DISPATCH_CACHE_ENTRIES, QPSX_MIPS_DISPATCH_CACHE_GP,
	     QPSX_MIPS_DISPATCH_CACHE_GP_TRUST_ABI,
	     QPSX_MIPS_DISPATCH_BRANCH_LIKELY,
	     QPSX_MIPS_DISPATCH_FRAME_BRANCH_LIKELY,
	     QPSX_MIPS_PSMEM_REG,
	     QPSX_GTE_NATIVE_DIVIDE, QPSX_GTE_HOT_O3,
	     QPSX_MIPS_PERSISTENT_RETURN_RA);
	xlog("QPSX: fold=%d/%d/%d profiler=%d telemetry=%d raw_vram=%d gpu_fixed=%d "
	     "gpu_light_fast=%d gpu_linear4=%d gpu_tile32=%d gpu_sprite4=%d "
	     "gpu_poly32=%d gpu_gflatv=%d gpu_gcache=%d gpu_fullmask=%d gpu_pack4=%d gpu_hot_order=%d gpu_metrics=%d gpu_recip=%d "
	     "mirror=%d fast_mem=%d",
	     QPSX_MIPS_FOLD_DIRECT_JUMPS,
	     QPSX_MIPS_FOLD_DIRECT_JUMPS_MAX, QPSX_MIPS_FOLD_DIRECT_JUMPS_BYTES,
	     QPSX_PROFILER_ENABLED, QPSX_RUNTIME_TELEMETRY, QPSX_GE_RAW_VRAM,
	     QPSX_GPU_FIXED_FAST_PATH, QPSX_GPU_FIXED_LIGHTING,
	     QPSX_GPU_LINEAR_4BPP,
	     QPSX_GPU_PACKED_TILE_WRITES, QPSX_GPU_PACKED_SPRITE_4BPP,
	     QPSX_GPU_PACKED_POLY_WRITES,
	     QPSX_GPU_4BPP_GOURAUD_FLATV, QPSX_GPU_4BPP_GOURAUD_CACHE,
	     QPSX_GPU_4BPP_FULLMASK,
	     QPSX_GPU_4BPP_FULLMASK_PACKED_WRITES,
	     QPSX_GPU_HOT_DRIVER_ORDER,
	     QPSX_GPU_RUNTIME_METRICS, QPSX_GPU_RECIP_TABLE_BITS,
	     QPSX_MIPS_VIRTUAL_MIRRORING,
	     QPSX_MIPS_FAST_MEM_CONVERT);
	xlog("QPSX: mips_opts fuzzy_addiu=%d",
	     QPSX_MIPS_PROPAGATE_FUZZY_ADDR);
	xlog("QPSX: mem_opts ram_helper=%d asm_reads=%d hle_lazy=%d",
	     QPSX_LINUX_RAM_HELPER_FASTPATH, QPSX_MIPS_ASM_MEM_READS,
	     QPSX_HLE_LAZY_EVENT_CHECK);
	xlog("QPSX: gpu_line_opts gouraud_flatfast=%d",
	     QPSX_GPU_GOURAUD_LINE_FLATFAST);
	xlog("QPSX: gpu_flatv=%d min_pixels=%d gpu_flatv_row=%d row_min=%d palette_lut=%d gcache=%d fullmask=%d fullmask_min=%d pack4=%d",
	     QPSX_GPU_4BPP_FLATV, QPSX_GPU_4BPP_FLATV_MIN_PIXELS,
	     QPSX_GPU_4BPP_FLATV_ROW, QPSX_GPU_4BPP_FLATV_ROW_MIN_PIXELS,
	     QPSX_GPU_4BPP_PALETTE_LUT, QPSX_GPU_4BPP_GOURAUD_CACHE,
	     QPSX_GPU_4BPP_FULLMASK,
	     QPSX_GPU_4BPP_FULLMASK_MIN_PIXELS,
	     QPSX_GPU_4BPP_FULLMASK_PACKED_WRITES);

#ifdef PSXREC
	#ifndef interpreter_none
	if (Config.Cpu == CPU_INTERPRETER) {
		psxCpu = &psxInt;
	} else
	#endif
	psxCpu = &psxRec;
#else
	psxCpu = &psxInt;
#endif

	// Initialize CPU *before* calling psxMemInit(), so it can make any
	//  memory mappings it needs for psxM,psxH etc.
	if (psxCpu->Init() < 0)
		return -1;
	#ifdef PSXREC
	/* The generated-code window is process-relative on Linux NOMMU.  Record
	 * its actual address so the QEMU cache model never mistakes a kernel text
	 * range for recRAM when ASLR/loader placement changes. */
	xlog("QPSX: rec address recMem=%08x", (unsigned)(uptr)recMem);
	#endif
	return psxMemInit();
}

void psxReset() {
	xlog("QPSX: >>> psxReset() START <<<\n");
	xlog("QPSX: Config.HLE=%d\n", Config.HLE);

	psxCpu->Reset();
	xlog("QPSX: psxCpu->Reset() done\n");

	psxMemReset();
	xlog("QPSX: psxMemReset() done\n");

	memset(&psxRegs, 0, sizeof(psxRegs));

	psxRegs.writeok = 1;

	psxRegs.pc = 0xbfc00000; // Start in bootstrap
	xlog("QPSX: PC set to 0x%08X (BIOS entry)\n", psxRegs.pc);

	psxRegs.psxM = psxM;	// PSX Memory
	psxRegs.psxP = psxP;	// PSX Memory
	psxRegs.psxR = psxR;	// PSX Memory
	psxRegs.psxH = psxH;	// PSX Memory

	psxRegs.CP0.r[12] = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	psxRegs.CP0.r[15] = 0x00000002; // PRevID = Revision ID, same as R3000A

	psxEvqueueInit();  // Event scheduler queue
	xlog("QPSX: psxEvqueueInit() done\n");

	psxHwReset();
	xlog("QPSX: psxHwReset() done\n");

	psxBiosInit();
	xlog("QPSX: psxBiosInit() done\n");

	// QPSX_052: Don't call psxExecuteBios() here for real BIOS
	// It will be called from libretro-core AFTER CD-ROM is opened
	if (Config.HLE) {
		xlog("QPSX: Using HLE BIOS - no BIOS execution needed\n");
	} else {
		xlog("QPSX: Using REAL BIOS - psxExecuteBios() will be called after CD open\n");
	}

	xlog("QPSX: <<< psxReset() DONE >>>\n");
}

void psxShutdown() {
	// Shutdown CPU *before* calling psxMemShutdown(), to allow it to unmap
	//  psxM,psxH etc, if it has done so.
#ifdef PSXREC
	/* This is intentionally once per emulator lifetime: counters are updated
	 * only when a block is compiled, so this log has no steady-state cost. */
	#if QPSX_RUNTIME_TELEMETRY
	recLogTelemetry();
	#endif
#endif
	psxCpu->Shutdown();

	psxMemShutdown();
	psxBiosShutdown();

}

void psxException(u32 code, u32 bd) {
	PROFILE_START(PROF_CPU_EXCEPTION);

	// Set the Cause, preserving R/W 'interrupt pending field' bits 8,9
	// (From Notaz's PCSX Rearmed)
	psxRegs.CP0.n.Cause = (psxRegs.CP0.n.Cause & 0x300) | code;

	// Set the EPC & PC
	if (bd) {
		psxRegs.CP0.n.Cause|= 0x80000000;
		psxRegs.CP0.n.EPC = (psxRegs.pc - 4);
	} else
		psxRegs.CP0.n.EPC = (psxRegs.pc);

	if (psxRegs.CP0.n.Status & 0x400000)
		psxRegs.pc = 0xbfc00180;
	else
		psxRegs.pc = 0x80000080;

	// Set the Status
	psxRegs.CP0.n.Status = (psxRegs.CP0.n.Status &~0x3f) |
						  ((psxRegs.CP0.n.Status & 0xf) << 2);

	if (!Config.HLE && (((PSXMu32(psxRegs.CP0.n.EPC) >> 24) & 0xfe) == 0x4a)) {
		// "hokuto no ken" / "Crash Bandicot 2" ... fix
		PSXMu32ref(psxRegs.CP0.n.EPC)&= SWAPu32(~0x02000000);
	}

	if (Config.HLE) {
		psxBiosException();
	}

	PROFILE_END(PROF_CPU_EXCEPTION);
}

void psxBranchTest()
{
	/* v141 DEBUG: Log psxBranchTest entry to verify it's called */
	static int branch_entry_count = 0;
	if (branch_entry_count < 10) {
		xlog("BRANCH: psxBranchTest entry #%d cycle=%u", branch_entry_count, psxRegs.cycle);
		branch_entry_count++;
	}

	//senquack - Do not rearrange the math here! Events' sCycle val can end up
	// negative (very large unsigned int) when a PSXINT_RESET_CYCLE_VAL event
	// resets psxRegs.cycle to 0 and subtracts the previous psxRegs.cycle value
	// from each event's sCycle value. If you were instead to test like this:
	// 'while ((psxRegs.cycle >= (psxRegs.intCycle[X].sCycle + psxRegs.intCycle[X].cycle)',
	// it could fail for events that were past-due at the moment of adjustment.
	while ((psxRegs.cycle - psxRegs.intCycle[PSXINT_NEXT_EVENT].sCycle) >=
			psxRegs.intCycle[PSXINT_NEXT_EVENT].cycle) {
		// After dispatching the most-imminent event, this will update
		//  the intCycle[PSXINT_NEXT_EVENT] element.
		psxEvqueueDispatchAndRemoveFront(&psxRegs);
	}

	psxRegs.io_cycle_counter = psxRegs.intCycle[PSXINT_NEXT_EVENT].sCycle +
	                           psxRegs.intCycle[PSXINT_NEXT_EVENT].cycle;

	// Are one or more HW IRQ bits set in both their status and mask registers?
	if (psxHu32(0x1070) & psxHu32(0x1074)) {
		// Are both HW IRQ mask bit and IRQ master-enable bit set in CP0 status reg?
		if ((psxRegs.CP0.n.Status & 0x401) == 0x401) {
			psxException(0x400, 0);
		}

		// If CP0 SR value didn't allow a HW IRQ exception here, it is likely
		//  because a game is currently inside an exception handler.
		//  It is therefore important that the RFE 'return-from-exception'
		//  instruction resets psxRegs.io_cycle_counter to 0. This ensures that
		//  psxBranchTest() is called again as soon as possible so that any
		//  pending HW IRQs are handled.
	}
}


/* QPSX_053: Check if BIOS execution ended - PC entered kseg0 RAM
 * Based on PCSX-ReARMed - only check for 0x80xxxxxx range
 * Don't exit early on kuseg addresses (kernel shell uses those during boot)
 */
static inline int psxExecuteBiosEnded(void) {
	// Only kseg0: 0x80000000-0x807FFFFF (like PCSX-ReARMed)
	return (psxRegs.pc & 0xFF800000) == 0x80000000;
}


/* QPSX_054: Fixed BIOS execution - must call psxBranchTest() for interrupts!
 * The BIOS waits for timer/CD-ROM interrupts. Without calling psxBranchTest(),
 * interrupts never fire and BIOS loops forever at ~0xBFC00434.
 */
void psxExecuteBios() {
	xlog("QPSX: psxExecuteBios() START, PC=0x%08X\n", psxRegs.pc);

	unsigned int iterations = 0;
	const unsigned int max_iterations = 5000000;

	while (!psxExecuteBiosEnded() && iterations < max_iterations) {
		// QPSX_054: Force psxBranchTest to be called by setting io_cycle_counter
		psxRegs.io_cycle_counter = psxRegs.cycle;

		psxCpu->ExecuteBlock(0);

		// QPSX_054: Explicitly call psxBranchTest after each block
		// This processes timer/CD-ROM events and delivers interrupts
		psxBranchTest();

		iterations++;

		// Log progress every 100k iterations
		if (iterations % 100000 == 0) {
			xlog("QPSX: BIOS iter=%u PC=0x%08X cycle=%u\n",
			     iterations, psxRegs.pc, psxRegs.cycle);
		}
	}

	if (iterations >= max_iterations) {
		xlog("QPSX: BIOS timeout after %u iters, PC=0x%08X\n", iterations, psxRegs.pc);
	} else {
		xlog("QPSX: psxExecuteBios() DONE after %u iters, PC=0x%08X\n", iterations, psxRegs.pc);
	}
}

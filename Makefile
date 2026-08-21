# PCSX4ALL libretro Makefile for SF2000
# Based on UAE4ALL libretro Makefile - follow EXACT same patterns!
# QPSX_111 - MIPS32 ASM CDDA mixer + runtime toggle
# QPSX_110 - CDDA Runtime Options, Profiler CDDA, DirectBlockLUT fix
# QPSX_100 - Direct Block LUT: 1-level lookup replaces 2-level psxRecLUT
# QPSX_099 - Pure ASM memory functions (psxmem_asm.S)
# QPSX_043 - REAL BIOS: Use scph5501.bin instead of HLE + debug logging

NAME    = pcsx4all
O       = o
RM      = rm -f

# GPU plugin selection
GPU     = gpu_unai
SPU     = spu_pcsxrearmed

# Use MIPS recompiler
RECOMPILER = mips

# SF2000 platform - FLAGS FROM MULTICORE FRAMEWORK (beetle-psx + main Makefile)
ifeq ($(platform), sf2000)
    TARGET := _libretro_sf2000.a
    MIPS=/opt/mips32-mti-elf/2019.09-03-2/bin/mips-mti-elf-
    CC = $(MIPS)gcc
    CXX = $(MIPS)g++
    AR = $(MIPS)ar
    # FLAGS EXACTLY FROM MULTICORE (main Makefile + beetle-psx):
    CFLAGS += -EL -march=mips32 -mtune=mips32 -msoft-float -ffast-math
    CFLAGS += -G0 -mno-abicalls -fno-pic -ffreestanding
    CFLAGS += -ffunction-sections -fdata-sections
    CFLAGS += -fno-use-cxa-atexit
    CFLAGS += -DSF2000 -DNO_THREADS
    STATIC_LINKING = 1
else
    TARGET = $(NAME)_libretro.so
    CC = gcc
    CXX = g++
endif

all: $(TARGET)

HOST_CXX ?= c++
GPU_POLY2043_TEST = tests/gpu_poly2043_diff
GPU_DMA_CHAIN_TEST = tests/gpu_dma_chain_fast_diff
GTE_INTPL_TEST = tests/gte_intpl_diff
GTE_RTPT_LAYOUT_TEST = tests/gte_rtpt_layout_audit
PSXMEM_CLASSIFIER_TEST = tests/psxmem_asm_classifier

# The classifier audit assembles the production helper with a real MIPS
# compiler and disassembles the resulting object.  Prefer an explicitly
# supplied CROSS_COMPILE, then a toolchain on PATH; callers using a toolchain
# outside those locations can set QPSX_ASM_CROSS_CC directly (and, if needed,
# QPSX_ASM_CROSS_OBJDUMP).
QPSX_ASM_CROSS_CC ?= $(if $(CROSS_COMPILE),$(CROSS_COMPILE)gcc,$(shell command -v mipsel-unknown-linux-uclibc-gcc 2>/dev/null || command -v mipsel-mti-elf-gcc 2>/dev/null || command -v mips-mti-elf-gcc 2>/dev/null))
QPSX_ASM_CROSS_OBJDUMP ?= $(if $(CROSS_COMPILE),$(CROSS_COMPILE)objdump,$(patsubst %gcc,%objdump,$(QPSX_ASM_CROSS_CC)))

# Build the real GTE translation unit for the host differential test.  The
# source has many platform entry points, so function sections plus linker GC
# retain only INTPL and avoid pulling the emulator's platform services into
# this small test binary.  Defining __mips__ selects the same intentional
# non-paranoid overflow policy used by the MIPS32 target; it does not make the
# host compiler emit MIPS instructions.
GTE_INTPL_TEST_CXXFLAGS = -std=c++11 -O2 -Wall -Wextra -Werror \
	-ffunction-sections -fdata-sections -D__mips__ \
	-DINLINE='static inline' -DQPSX_GTE_INTPL_OPTIMIZE=1 \
	-DQPSX_PROFILER_ENABLED=0 -Isrc -Isrc/port/libretro

.PHONY: check qpsx-asm-read-audit qpsx-asm-classifier-audit \
	qpsx-asm-classifier-cross-audit gte-rtpt-asm-audit
check: $(GPU_POLY2043_TEST) $(GPU_DMA_CHAIN_TEST) $(GTE_INTPL_TEST) \
	$(GTE_RTPT_LAYOUT_TEST) $(PSXMEM_CLASSIFIER_TEST) qpsx-asm-read-audit \
	gte-rtpt-asm-audit
	./$(GPU_POLY2043_TEST)
	./$(GPU_DMA_CHAIN_TEST)
	./$(GTE_INTPL_TEST)
	./$(GTE_RTPT_LAYOUT_TEST)
	./$(PSXMEM_CLASSIFIER_TEST)

qpsx-asm-classifier-audit: $(PSXMEM_CLASSIFIER_TEST)
	./$(PSXMEM_CLASSIFIER_TEST)

# Assemble both Linux helper configurations.  The classifier itself is
# identical, while the first three branches in the fast configuration are
# deliberately retained in the audit so changes cannot accidentally fold the
# broad RAM mirror test over a hardware alias.
qpsx-asm-classifier-cross-audit:
	@set -eu; \
	test -n '$(QPSX_ASM_CROSS_CC)' || { \
		echo 'qpsx-asm-classifier-cross-audit: set QPSX_ASM_CROSS_CC (or CROSS_COMPILE)' >&2; exit 2; \
	}; \
	test -n '$(QPSX_ASM_CROSS_OBJDUMP)' || { \
		echo 'qpsx-asm-classifier-cross-audit: set QPSX_ASM_CROSS_OBJDUMP' >&2; exit 2; \
	}; \
	tmp=$$(mktemp -d); \
	trap 'rm -rf "$$tmp"' EXIT HUP INT TERM; \
	for fastpath in 0 1; do \
		$(QPSX_ASM_CROSS_CC) -EL -march=mips32 -mno-abicalls -fno-pic \
			-ffreestanding -DQPSX_LINUX_RAM_HELPER_FASTPATH=$$fastpath \
			-DQPSX_LINUX_ALLOCATED_RAM=1 -DQPSX_MIPS_PSMEM_REG=1 \
			-c src/psxmem_asm.S -o "$$tmp/psxmem-$$fastpath.o"; \
		$(QPSX_ASM_CROSS_OBJDUMP) -dr -M no-aliases \
			"$$tmp/psxmem-$$fastpath.o" > "$$tmp/psxmem-$$fastpath.dis"; \
		for fn in psxMemWrite8_asm psxMemWrite16_asm psxMemWrite32_asm \
			psxMemRead8_asm psxMemRead16_asm psxMemRead32_asm; do \
			block=$$(awk -v fn="$$fn" \
				'$$0 ~ "<" fn ">:" {inside=1} inside {print} inside && /^$$/ {exit}' \
				"$$tmp/psxmem-$$fastpath.dis"); \
			test -n "$$block"; \
			printf '%s\n' "$$block" | grep -Eq '[[:space:]]31097fff[[:space:]]+andi[[:space:]]+t1,t0' || { \
				echo "$$fn: missing exact 0x7fff mask (fastpath=$$fastpath)" >&2; exit 1; \
			}; \
			printf '%s\n' "$$block" | grep -Eq '[[:space:]]240a1f80[[:space:]]+' || { \
				echo "$$fn: missing 0x1f80 compare constant (fastpath=$$fastpath)" >&2; exit 1; \
			}; \
			printf '%s\n' "$$block" | grep -Eq '[[:space:]]340abf80[[:space:]]+' || { \
				echo "$$fn: missing 0xbf80 compare constant (fastpath=$$fastpath)" >&2; exit 1; \
			}; \
			test "$$(printf '%s\n' "$$block" | grep -Ec '[[:space:]]beq[[:space:]]+t1,t2,')" -ge 1; \
			test "$$(printf '%s\n' "$$block" | grep -Ec '[[:space:]]beq[[:space:]]+t0,t2,')" -eq 1; \
			if printf '%s\n' "$$block" | grep -Eq '[[:space:]]31091f80[[:space:]]'; then \
				echo "$$fn: classifier still uses the old broad mask (fastpath=$$fastpath)" >&2; exit 1; \
			fi; \
		done; \
	done; \
	echo 'qpsx-asm-classifier-cross-audit: MIPS assembly/disassembly verified for both RAM-helper modes'

gte-rtpt-asm-audit:
	@set -eu; tmp=$$(mktemp -d); \
	trap 'rm -rf "$$tmp"' EXIT; \
	$(HOST_CXX) -E -P -Isrc src/gte_rtpt_asm.S -o "$$tmp/rtpt.s"; \
	grep -F 'gte_RTPT_asm:' "$$tmp/rtpt.s" >/dev/null; \
	grep -F '264' "$$tmp/rtpt.s" >/dev/null; \
	grep -F '392' "$$tmp/rtpt.s" >/dev/null; \
	if grep -E '^#define OFF_[A-Za-z0-9_]+[[:space:]]+[0-9]' src/gte_rtpt_asm.S >/dev/null; then \
		echo 'gte-rtpt-asm-audit: hard-coded local RTPT offsets remain' >&2; exit 1; \
	fi; \
	echo 'gte-rtpt-asm-audit: assembly expands shared RTPT layout constants'

qpsx-asm-read-audit:
	$(MAKE) -f Makefile.libretro QPSX_AUDIT_CXX="$(HOST_CXX)" asm-read-audit

$(GPU_POLY2043_TEST): tests/gpu_poly2043_diff.cpp
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra -Werror $< -o $@

$(GPU_DMA_CHAIN_TEST): tests/gpu_dma_chain_fast_diff.cpp \
		src/gpu/gpulib/gpu_dma_chain_fast.h
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra -Werror $< -o $@

$(GTE_INTPL_TEST): tests/gte_intpl_diff.cpp src/gte.cpp src/gte.h \
	src/r3000a.h src/psxcommon.h src/psxmem.h src/port/libretro/port.h \
	src/profiler.h
	tmp=$$(mktemp -d); \
	trap 'rm -rf "$$tmp"' EXIT; \
	$(HOST_CXX) $(GTE_INTPL_TEST_CXXFLAGS) -c src/gte.cpp -o "$$tmp/gte.o"; \
	$(HOST_CXX) $(GTE_INTPL_TEST_CXXFLAGS) -c $< -o "$$tmp/test.o"; \
	$(HOST_CXX) -Wl,--gc-sections "$$tmp/test.o" "$$tmp/gte.o" -o $@

$(GTE_RTPT_LAYOUT_TEST): tests/gte_rtpt_layout_audit.cpp src/r3000a.h \
	src/psxcommon.h src/psxmem.h src/psxcounters.h src/psxbios.h \
	src/port/libretro/port.h src/gte_rtpt_layout.h
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra -Werror \
		-Isrc -Isrc/port/libretro $< -o $@

$(PSXMEM_CLASSIFIER_TEST): tests/psxmem_asm_classifier.cpp
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra -Werror $< -o $@

# Port selection - libretro (not SDL!)
PORT = libretro

# Common flags (based on UAE4ALL pattern)
ifeq ($(platform), sf2000)
MORE_CFLAGS = -Os -Isrc/ -Isrc/spu/$(SPU) -Isrc/gpu/$(GPU) \
	-Isrc/plugin_lib \
	-Isrc/port/$(PORT) \
	-Ilibretro/core -Ilibretro/include \
	-fomit-frame-pointer -fno-threadsafe-statics \
	-Wno-unused -Wno-format -Wno-sign-compare \
	-fno-exceptions -fno-rtti \
	-DINLINE="static __inline__" \
	-D$(shell echo $(GPU) | tr a-z A-Z) \
	-D$(shell echo $(SPU) | tr a-z A-Z)
else
MORE_CFLAGS = -g -O2 -Isrc/ -Isrc/spu/$(SPU) -Isrc/gpu/$(GPU) \
	-Isrc/plugin_lib \
	-Isrc/port/$(PORT) \
	-Ilibretro/core -Ilibretro/include \
	-fomit-frame-pointer -fno-threadsafe-statics \
	-Wno-unused -Wno-format -Wno-sign-compare \
	-fno-exceptions -fno-rtti \
	-DINLINE="static __inline__" \
	-D$(shell echo $(GPU) | tr a-z A-Z) \
	-D$(shell echo $(SPU) | tr a-z A-Z)
endif

# Libretro defines
MORE_CFLAGS += -D__LIBRETRO__
MORE_CFLAGS += -DHAVE_LIBRETRO

# QPSX_035: Enable MIPS recompiler for SF2000
# Cache flush now uses __builtin___clear_cache() -> _flush_cache() from multicore framework
MORE_CFLAGS += -DPSXREC -D$(RECOMPILER)

# Use gpulib
MORE_CFLAGS += -DUSE_GPULIB
MORE_CFLAGS += -Isrc/gpu/gpulib

# HLE BIOS
MORE_CFLAGS += -DHLE_BIOS

# NO ZLIB on SF2000 (bare metal has no zlib)
ifeq ($(platform), sf2000)
MORE_CFLAGS += -DNO_ZLIB
endif

# XA audio hack
MORE_CFLAGS += -DXA_HACK

CFLAGS  += $(MORE_CFLAGS)
CXXFLAGS = $(CFLAGS)

# Object files - Core PSX emulation
OBJS = \
	src/r3000a.o \
	src/misc.o \
	src/plugins.o \
	src/psxmem.o \
	src/psxhw.o \
	src/psxcounters.o \
	src/psxdma.o \
	src/psxbios.o \
	src/psxhle.o \
	src/psxevents.o \
	src/psxcommon.o \
	src/psxinterpreter.o \
	src/mdec.o \
	src/decode_xa.o \
	src/cdriso.o \
	src/cdrom.o \
	src/ppf.o \
	src/sio.o \
	src/pad.o \
	src/gte.o \
	src/profiler.o

# MIPS Recompiler - QPSX_035: Enabled for SF2000 with custom cache flush
OBJS += \
	src/recompiler/mips/recompiler.o \
	src/recompiler/mips/mips_codegen.o \
	src/recompiler/mips/mips_disasm.o \
	src/recompiler/mips/mem_mapping.o

# GPU - using gpulib + unai
OBJS += \
	src/gpu/$(GPU)/gpulib_if.o \
	src/gpu/gpulib/gpu.o \
	src/gpu/gpulib/vout_port.o

# QPSX v091: MIPS32 Assembly optimizations (SF2000 only)
# QPSX v099: Added psxmem_asm.o for memory write optimization
ifeq ($(platform), sf2000)
OBJS += \
	src/gpu/$(GPU)/gpu_inner_mips32.o \
	src/gte_asm.o \
	src/psxmem_asm.o
endif

# SPU - pcsxrearmed with libretro audio backend
OBJS += \
	src/spu/$(SPU)/spu.o \
	src/spu/$(SPU)/dma.o \
	src/spu/$(SPU)/freeze.o \
	src/spu/$(SPU)/out.o \
	src/spu/$(SPU)/nullsnd.o \
	src/spu/$(SPU)/registers.o \
	src/spu/$(SPU)/libretro.o

# QPSX v111: MIPS32 ASM CDDA mixer (SF2000 only)
ifeq ($(platform), sf2000)
OBJS += \
	src/spu/$(SPU)/cdda_mix_asm.o
endif

# Plugin lib
OBJS += \
	src/plugin_lib/plugin_lib.o \
	src/plugin_lib/perfmon.o \
	src/plugin_lib/pl_sshot.o

# Libretro port
OBJS += \
	src/port/$(PORT)/port.o

# Libretro core
OBJS += \
	libretro/core/libretro-core.o

$(TARGET): $(OBJS)
ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $(OBJS)
else
	$(CXX) -shared -o $(TARGET) $(OBJS) $(LDFLAGS)
endif

clean:
	$(RM) $(TARGET) $(OBJS)

# Compilation rules
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

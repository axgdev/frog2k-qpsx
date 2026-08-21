/***************************************************************************
 *   QPSX v101 - MIPS32 Assembly Memory Functions
 *
 *   Hand-optimized assembly for SF2000 (MIPS32 918MHz, Little-Endian)
 *
 *   v101: FIXED - restores code invalidation logic from v099
 *         The v100 version that removed recClear caused vector corruption!
 *
 *   Functions:
 *     - psxMemWrite32_asm(u32 mem, u32 value)
 *     - psxMemWrite16_asm(u32 mem, u16 value)
 *     - psxMemWrite8_asm(u32 mem, u8 value)
 *     - psxMemRead32_asm(u32 mem) -> u32
 *     - psxMemRead16_asm(u32 mem) -> u16
 *     - psxMemRead8_asm(u32 mem) -> u8
 *
 *   External symbols needed:
 *     - psxMemWLUT: pointer to the Write LUT table (64K entries)
 *     - psxMemRLUT: pointer to the Read LUT table (64K entries)
 *     - psxH: Hardware registers base
 *     - psxHwWrite32/16/8: HW write functions (extern "C" in psxhw.h)
 *     - psxHwRead32/16/8: HW read functions (extern "C" in psxhw.h)
 *     - recClear: Code invalidation function (extern "C" in recompiler.cpp)
 *   v367: g_opt_skip_code_inv REMOVED - code invalidation always happens
 *
 ***************************************************************************/

#if defined(SF2000) || defined(__mips__)

    .set noreorder
    .set noat

    .text
    .align 4

/*--------------------------------------------------------------------------
 * External symbols
 *--------------------------------------------------------------------------*/
    .extern psxMemWLUT
    .extern psxMemRLUT
    .extern psxM
    .extern psxH
    .extern psxHwWrite32
    .extern psxHwWrite16
    .extern psxHwWrite8

/* When the recompiler reserves $s7 for psxM, indirect read helpers are
 * entered with that callee-saved register live.  Reusing it removes the
 * global-pointer load from each RAM read.  Keep the global load for the
 * generic ABI, where callers are not required to establish $s7. */
#if defined(QPSX_MIPS_PSMEM_REG) && QPSX_MIPS_PSMEM_REG
#define QPSX_ASM_HAS_PSMEM_REG 1
#else
#define QPSX_ASM_HAS_PSMEM_REG 0
#endif

/* The PS1 hardware window has exactly three high-half aliases.  Since the
 * helpers receive a 16-bit high half in $t0, masking bit 15 folds only
 * 0x1f80 and 0x9f80 together; 0xbf80 still needs its own comparison.  Keep
 * this sequence shared by every exported helper so a dormant write entry
 * point cannot retain the historical mask-only false positives.  The first
 * branch delay slot also computes m for both the match and fall-through
 * paths. */
    .macro QPSX_ASM_HW_CLASSIFY target
    andi    $t1, $t0, 0x7FFF
    li      $t2, 0x1F80
    beq     $t1, $t2, \target
    andi    $t3, $a0, 0xFFFF
    li      $t2, 0xBF80
    beq     $t0, $t2, \target
    nop
    .endm

/* psxMem{R,W}LUT are C u8 ** variables: the symbol names the global pointer,
 * not the first LUT entry.  Load that pointer before applying t*4, then load
 * the selected page pointer.  Keep the result in $t4 and the byte offset in
 * $t3 so every helper has the same register contract. */
    .macro QPSX_ASM_LUT_LOOKUP table
    la      $t4, \table
    lw      $t4, 0($t4)             /* $t4 = LUT base */
    sll     $t1, $t0, 2             /* $t1 = t * 4 (pointer size) */
    addu    $t4, $t4, $t1           /* $t4 = &LUT[t] */
    lw      $t4, 0($t4)             /* $t4 = LUT[t] page pointer */
    .endm

    .extern psxHwRead32
    .extern psxHwRead16
    .extern psxHwRead8
    /* v367: g_opt_skip_code_inv REMOVED - hardcoded to always do recClear */
    .extern recClear

/*==========================================================================
 * psxMemWrite32_asm - Write 32-bit value to PSX memory
 *
 * Arguments:
 *   $a0 = mem (PSX address)
 *   $a1 = value (32-bit value to write)
 *==========================================================================*/
    .global psxMemWrite32_asm
    .ent psxMemWrite32_asm
    .type psxMemWrite32_asm, @function

psxMemWrite32_asm:
    /* Extract high 16 bits: t = mem >> 16 */
    srl     $t0, $a0, 16            /* $t0 = t = mem >> 16 */

    /* Check for exact aliases 0x1f80, 0x9f80, and 0xbf80. */
    QPSX_ASM_HW_CLASSIFY .Lhw_write32

    /*----------------------------------------------------------------------
     * RAM Fast Path (most common case)
     * Load LUT pointer: p = psxMemWLUT[t]
     *----------------------------------------------------------------------*/
    QPSX_ASM_LUT_LOOKUP psxMemWLUT

    beqz    $t4, .Lnull_ptr32       /* if p == NULL, handle error */
    nop

    /* Direct RAM store - THE HOT PATH */
    addu    $t5, $t4, $t3           /* $t5 = p + m */
    sw      $a1, 0($t5)             /* *(u32*)(p + m) = value */

    /*----------------------------------------------------------------------
     * v367: Code invalidation - ALWAYS (g_opt_skip_code_inv hardcoded to 0)
     *----------------------------------------------------------------------*/
    /* Call recClear(mem, 1) for code invalidation */
    addiu   $sp, $sp, -8
    sw      $ra, 4($sp)

    /* recClear(u32 addr, u32 size) - $a0 already has mem */
    li      $a1, 1                  /* size = 1 */
    la      $t0, recClear
    jalr    $t0
    nop

    lw      $ra, 4($sp)
    addiu   $sp, $sp, 8

.Ldone32:
    jr      $ra
    nop

    /*----------------------------------------------------------------------
     * HW Region handler
     *----------------------------------------------------------------------*/
.Lhw_write32:
    /* Check if scratchpad: m < 0x400 */
    sltiu   $t1, $t3, 0x400
    beqz    $t1, .Lhw_io_write32
    nop

    /* Scratchpad write: psxH[m] = value */
    la      $t4, psxH
    lw      $t4, 0($t4)             /* $t4 = psxH base */
    addu    $t4, $t4, $t3           /* $t4 = psxH + m */
    sw      $a1, 0($t4)             /* store to scratchpad */
    jr      $ra
    nop

.Lhw_io_write32:
    /* Hardware I/O: call psxHwWrite32(mem, value) */
    la      $t0, psxHwWrite32
    jr      $t0                     /* tail call optimization */
    nop

.Lnull_ptr32:
    /* NULL pointer - check for cache control port 0xfffe0130 */
    li      $t1, 0xfffe0130
    bne     $a0, $t1, .Ldone32
    nop

    /* Cache control port - just return for now */
    jr      $ra
    nop

    .end psxMemWrite32_asm
    .size psxMemWrite32_asm, .-psxMemWrite32_asm


/*==========================================================================
 * psxMemWrite16_asm - Write 16-bit value to PSX memory
 *==========================================================================*/
    .global psxMemWrite16_asm
    .ent psxMemWrite16_asm
    .type psxMemWrite16_asm, @function

psxMemWrite16_asm:
    srl     $t0, $a0, 16            /* $t0 = t = mem >> 16 */

    /* Check for exact aliases 0x1f80, 0x9f80, and 0xbf80. */
    QPSX_ASM_HW_CLASSIFY .Lhw_write16

    /* RAM path */
    QPSX_ASM_LUT_LOOKUP psxMemWLUT

    beqz    $t4, .Ldone16
    nop

    addu    $t5, $t4, $t3           /* $t5 = p + m */
    sh      $a1, 0($t5)             /* *(u16*)(p + m) = value */

    /* v367: Code invalidation - ALWAYS (hardcoded) */
    /* Align address for Clear: mem & ~3 */
    addiu   $sp, $sp, -8
    sw      $ra, 4($sp)

    li      $t1, -4                 /* 0xFFFFFFFC */
    and     $a0, $a0, $t1           /* $a0 = mem & ~3 */
    li      $a1, 1
    la      $t0, recClear
    jalr    $t0
    nop

    lw      $ra, 4($sp)
    addiu   $sp, $sp, 8

.Ldone16:
    jr      $ra
    nop

.Lhw_write16:
    sltiu   $t1, $t3, 0x400
    beqz    $t1, .Lhw_io_write16
    nop

    /* Scratchpad */
    la      $t4, psxH
    lw      $t4, 0($t4)
    addu    $t4, $t4, $t3
    sh      $a1, 0($t4)
    jr      $ra
    nop

.Lhw_io_write16:
    la      $t0, psxHwWrite16
    jr      $t0
    nop

    .end psxMemWrite16_asm
    .size psxMemWrite16_asm, .-psxMemWrite16_asm


/*==========================================================================
 * psxMemWrite8_asm - Write 8-bit value to PSX memory
 *==========================================================================*/
    .global psxMemWrite8_asm
    .ent psxMemWrite8_asm
    .type psxMemWrite8_asm, @function

psxMemWrite8_asm:
    srl     $t0, $a0, 16            /* $t0 = t = mem >> 16 */

    /* Check for exact aliases 0x1f80, 0x9f80, and 0xbf80. */
    QPSX_ASM_HW_CLASSIFY .Lhw_write8

    /* RAM path */
    QPSX_ASM_LUT_LOOKUP psxMemWLUT

    beqz    $t4, .Ldone8
    nop

    addu    $t5, $t4, $t3           /* $t5 = p + m */
    sb      $a1, 0($t5)             /* *(u8*)(p + m) = value */

    /* v367: Code invalidation - ALWAYS (hardcoded) */
    /* Align address for Clear: mem & ~3 */
    addiu   $sp, $sp, -8
    sw      $ra, 4($sp)

    li      $t1, -4
    and     $a0, $a0, $t1
    li      $a1, 1
    la      $t0, recClear
    jalr    $t0
    nop

    lw      $ra, 4($sp)
    addiu   $sp, $sp, 8

.Ldone8:
    jr      $ra
    nop

.Lhw_write8:
    sltiu   $t1, $t3, 0x400
    beqz    $t1, .Lhw_io_write8
    nop

    /* Scratchpad */
    la      $t4, psxH
    lw      $t4, 0($t4)
    addu    $t4, $t4, $t3
    sb      $a1, 0($t4)
    jr      $ra
    nop

.Lhw_io_write8:
    la      $t0, psxHwWrite8
    jr      $t0
    nop

    .end psxMemWrite8_asm
    .size psxMemWrite8_asm, .-psxMemWrite8_asm


/*==========================================================================
 * psxMemRead32_asm - Read 32-bit value from PSX memory
 *==========================================================================*/
    .global psxMemRead32_asm
    .ent psxMemRead32_asm
    .type psxMemRead32_asm, @function

psxMemRead32_asm:
    srl     $t0, $a0, 16            /* $t0 = t = mem >> 16 */

#if defined(QPSX_LINUX_RAM_HELPER_FASTPATH) && QPSX_LINUX_RAM_HELPER_FASTPATH && defined(QPSX_LINUX_ALLOCATED_RAM)
    /* Linux keeps the PS1 RAM in one stable 2 MiB object.  The 8 MiB
     * KUSEG/KSEG0/KSEG1 mirror can therefore skip the read LUT entirely. */
    andi    $t3, $a0, 0xFFFF        /* m = mem & 0xffff */
    srl     $t1, $t0, 7             /* high16 >> 7: 0, 0x100, or 0x140 */
    beq     $t1, $zero, .Lram_read32
    nop
    li      $t2, 0x100
    beq     $t1, $t2, .Lram_read32
    nop
    li      $t2, 0x140
    beq     $t1, $t2, .Lram_read32
    nop
#endif

    /* Check for exact aliases 0x1f80, 0x9f80, and 0xbf80. */
    QPSX_ASM_HW_CLASSIFY .Lhw_read32

    /* RAM/ROM path - use Read LUT */
    QPSX_ASM_LUT_LOOKUP psxMemRLUT

    beqz    $t4, .Lread_null32
    nop

    /* Direct memory read - THE HOT PATH */
    addu    $t5, $t4, $t3           /* $t5 = p + m */
    lw      $v0, 0($t5)             /* $v0 = *(u32*)(p + m) */
    jr      $ra
    nop

#if defined(QPSX_LINUX_RAM_HELPER_FASTPATH) && QPSX_LINUX_RAM_HELPER_FASTPATH && defined(QPSX_LINUX_ALLOCATED_RAM)
.Lram_read32:
#if QPSX_ASM_HAS_PSMEM_REG
    sll     $t5, $a0, 11             /* keep the low 21 address bits */
    srl     $t5, $t5, 11
    addu    $t4, $s7, $t5             /* t4 = live psxM + offset */
#else
    la      $t4, psxM
    lw      $t4, 0($t4)              /* t4 = psxM base */
    sll     $t5, $a0, 11
    srl     $t5, $t5, 11
    addu    $t4, $t4, $t5
#endif
    lw      $v0, 0($t4)
    jr      $ra
    nop
#endif

.Lhw_read32:
    /* Check if scratchpad: m < 0x400 */
    sltiu   $t1, $t3, 0x400
    beqz    $t1, .Lhw_io_read32
    nop

    /* Scratchpad read */
    la      $t4, psxH
    lw      $t4, 0($t4)
    addu    $t4, $t4, $t3
    lw      $v0, 0($t4)
    jr      $ra
    nop

.Lhw_io_read32:
    la      $t0, psxHwRead32
    jr      $t0                     /* tail call */
    nop

.Lread_null32:
    li      $v0, 0
    jr      $ra
    nop

    .end psxMemRead32_asm
    .size psxMemRead32_asm, .-psxMemRead32_asm


/*==========================================================================
 * psxMemRead16_asm - Read 16-bit value from PSX memory
 *==========================================================================*/
    .global psxMemRead16_asm
    .ent psxMemRead16_asm
    .type psxMemRead16_asm, @function

psxMemRead16_asm:
    srl     $t0, $a0, 16
#if defined(QPSX_LINUX_RAM_HELPER_FASTPATH) && QPSX_LINUX_RAM_HELPER_FASTPATH && defined(QPSX_LINUX_ALLOCATED_RAM)
    andi    $t3, $a0, 0xFFFF
    srl     $t1, $t0, 7
    beq     $t1, $zero, .Lram_read16
    nop
    li      $t2, 0x100
    beq     $t1, $t2, .Lram_read16
    nop
    li      $t2, 0x140
    beq     $t1, $t2, .Lram_read16
    nop
#endif
    /* Check for exact aliases 0x1f80, 0x9f80, and 0xbf80. */
    QPSX_ASM_HW_CLASSIFY .Lhw_read16

    QPSX_ASM_LUT_LOOKUP psxMemRLUT

    beqz    $t4, .Lread_null16
    nop

    addu    $t5, $t4, $t3
    lhu     $v0, 0($t5)
    jr      $ra
    nop

#if defined(QPSX_LINUX_RAM_HELPER_FASTPATH) && QPSX_LINUX_RAM_HELPER_FASTPATH && defined(QPSX_LINUX_ALLOCATED_RAM)
.Lram_read16:
#if QPSX_ASM_HAS_PSMEM_REG
    sll     $t5, $a0, 11
    srl     $t5, $t5, 11
    addu    $t4, $s7, $t5
#else
    la      $t4, psxM
    lw      $t4, 0($t4)
    sll     $t5, $a0, 11
    srl     $t5, $t5, 11
    addu    $t4, $t4, $t5
#endif
    lhu     $v0, 0($t4)
    jr      $ra
    nop
#endif

.Lhw_read16:
    sltiu   $t1, $t3, 0x400
    beqz    $t1, .Lhw_io_read16
    nop

    la      $t4, psxH
    lw      $t4, 0($t4)
    addu    $t4, $t4, $t3
    lhu     $v0, 0($t4)
    jr      $ra
    nop

.Lhw_io_read16:
    la      $t0, psxHwRead16
    jr      $t0
    nop

.Lread_null16:
    li      $v0, 0
    jr      $ra
    nop

    .end psxMemRead16_asm
    .size psxMemRead16_asm, .-psxMemRead16_asm


/*==========================================================================
 * psxMemRead8_asm - Read 8-bit value from PSX memory
 *==========================================================================*/
    .global psxMemRead8_asm
    .ent psxMemRead8_asm
    .type psxMemRead8_asm, @function

psxMemRead8_asm:
    srl     $t0, $a0, 16
#if defined(QPSX_LINUX_RAM_HELPER_FASTPATH) && QPSX_LINUX_RAM_HELPER_FASTPATH && defined(QPSX_LINUX_ALLOCATED_RAM)
    andi    $t3, $a0, 0xFFFF
    srl     $t1, $t0, 7
    beq     $t1, $zero, .Lram_read8
    nop
    li      $t2, 0x100
    beq     $t1, $t2, .Lram_read8
    nop
    li      $t2, 0x140
    beq     $t1, $t2, .Lram_read8
    nop
#endif
    /* Check for exact aliases 0x1f80, 0x9f80, and 0xbf80. */
    QPSX_ASM_HW_CLASSIFY .Lhw_read8

    QPSX_ASM_LUT_LOOKUP psxMemRLUT

    beqz    $t4, .Lread_null8
    nop

    addu    $t5, $t4, $t3
    lbu     $v0, 0($t5)
    jr      $ra
    nop

#if defined(QPSX_LINUX_RAM_HELPER_FASTPATH) && QPSX_LINUX_RAM_HELPER_FASTPATH && defined(QPSX_LINUX_ALLOCATED_RAM)
.Lram_read8:
#if QPSX_ASM_HAS_PSMEM_REG
    sll     $t5, $a0, 11
    srl     $t5, $t5, 11
    addu    $t4, $s7, $t5
#else
    la      $t4, psxM
    lw      $t4, 0($t4)
    sll     $t5, $a0, 11
    srl     $t5, $t5, 11
    addu    $t4, $t4, $t5
#endif
    lbu     $v0, 0($t4)
    jr      $ra
    nop
#endif

.Lhw_read8:
    sltiu   $t1, $t3, 0x400
    beqz    $t1, .Lhw_io_read8
    nop

    la      $t4, psxH
    lw      $t4, 0($t4)
    addu    $t4, $t4, $t3
    lbu     $v0, 0($t4)
    jr      $ra
    nop

.Lhw_io_read8:
    la      $t0, psxHwRead8
    jr      $t0
    nop

.Lread_null8:
    li      $v0, 0
    jr      $ra
    nop

    .end psxMemRead8_asm
    .size psxMemRead8_asm, .-psxMemRead8_asm


#endif /* SF2000 || __mips__ */

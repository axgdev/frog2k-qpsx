/***************************************************************************
 * QPSX RTPT common-case kernel
 *
 * This is intentionally a separate object from the legacy diagnostic GTE
 * assembly file. The legacy file contains experimental stubs with C-only
 * symbols; keeping this leaf kernel isolated lets the linker pull it in
 * without dragging those unresolved experiments into Linux builds.
 *
 * The caller selects this routine only for the explicit SF2000 A/B build.
 * It implements the normal PS1 fixed-point envelope (no saturation/overflow
 * checks), uses the MIPS32 HI/LO multiply accumulator, and makes no C calls.
 * The regular C RTPT remains the compatibility path.
 ***************************************************************************/

.set noreorder
.set noat

#define OFF_CP2D        264
#define OFF_CP2C        392

#define OFF_SZ0         (OFF_CP2D + 16*4)
#define OFF_SZ1         (OFF_CP2D + 17*4)
#define OFF_SZ2         (OFF_CP2D + 18*4)
#define OFF_SZ3         (OFF_CP2D + 19*4)
#define OFF_IR0         (OFF_CP2D + 8*4)
#define OFF_IR1         (OFF_CP2D + 9*4)
#define OFF_IR2         (OFF_CP2D + 10*4)
#define OFF_IR3         (OFF_CP2D + 11*4)
#define OFF_SXY0        (OFF_CP2D + 12*4)
#define OFF_SXY1        (OFF_CP2D + 13*4)
#define OFF_SXY2        (OFF_CP2D + 14*4)
#define OFF_MAC0        (OFF_CP2D + 24*4)
#define OFF_MAC1        (OFF_CP2D + 25*4)
#define OFF_MAC2        (OFF_CP2D + 26*4)
#define OFF_MAC3        (OFF_CP2D + 27*4)

#define OFF_R11         (OFF_CP2C + 0*4)
#define OFF_R12         (OFF_CP2C + 0*4 + 2)
#define OFF_R13         (OFF_CP2C + 1*4)
#define OFF_R21         (OFF_CP2C + 1*4 + 2)
#define OFF_R22         (OFF_CP2C + 2*4)
#define OFF_R23         (OFF_CP2C + 2*4 + 2)
#define OFF_R31         (OFF_CP2C + 3*4)
#define OFF_R32         (OFF_CP2C + 3*4 + 2)
#define OFF_R33         (OFF_CP2C + 4*4)
#define OFF_TRX         (OFF_CP2C + 5*4)
#define OFF_TRY         (OFF_CP2C + 6*4)
#define OFF_TRZ         (OFF_CP2C + 7*4)
#define OFF_OFX         (OFF_CP2C + 24*4)
#define OFF_OFY         (OFF_CP2C + 25*4)
#define OFF_H           (OFF_CP2C + 26*4)
#define OFF_DQA         (OFF_CP2C + 27*4)
#define OFF_DQB         (OFF_CP2C + 28*4)
#define OFF_FLAG        (OFF_CP2C + 31*4)

.text
.align 2
.globl gte_RTPT_asm
.ent gte_RTPT_asm
gte_RTPT_asm:
    /* Keep the nine matrix terms in callee-saved registers for all vertices.
     * This leaf does not touch gp or ra, so the frame is only eight words. */
    addiu   $sp, $sp, -32
    sw      $s0, 0($sp)
    sw      $s1, 4($sp)
    sw      $s2, 8($sp)
    sw      $s3, 12($sp)
    sw      $s4, 16($sp)
    sw      $s5, 20($sp)
    sw      $s6, 24($sp)
    sw      $s7, 28($sp)

    la      $t0, psxRegs

    lh      $s0, OFF_R11($t0)
    lh      $s1, OFF_R12($t0)
    lh      $s2, OFF_R13($t0)
    lh      $s3, OFF_R21($t0)
    lh      $s4, OFF_R22($t0)
    lh      $s5, OFF_R23($t0)
    lh      $s6, OFF_R31($t0)
    lh      $s7, OFF_R32($t0)

    lw      $t1, OFF_TRX($t0)
    lw      $t2, OFF_TRY($t0)
    lw      $t3, OFF_TRZ($t0)
    lhu     $t4, OFF_H($t0)
    lw      $t5, OFF_OFX($t0)
    lw      $t6, OFF_OFY($t0)
    lh      $t7, OFF_DQA($t0)
    lw      $t8, OFF_DQB($t0)

    /* RTPT's depth FIFO shifts SZ3 into SZ0 before producing SZ1..SZ3. */
    lhu     $t9, OFF_SZ3($t0)
    sh      $t9, OFF_SZ0($t0)
    sw      $zero, OFF_FLAG($t0)
    /* R33 is invariant across all three vertices.  Keep it in T9; A3 is
     * free again after the depth divide and is used as the carry scratch for
     * the 64-bit screen-offset products. */
    lh      $t9, OFF_R33($t0)

.macro RTPT_VERTEX VXYOFF,VZOFF,SXYOFF,SZOFF
    lh      $a0, \VXYOFF($t0)          /* vx */
    lh      $a1, \VXYOFF+2($t0)        /* vy */
    lh      $a2, \VZOFF($t0)           /* vz */
    /* MAC1 = TRX + (R11*vx + R12*vy + R13*vz) / 4096. */
    sll     $v0, $t1, 12
    sra     $v1, $t1, 20
    mtlo    $v0
    mthi    $v1
    madd    $s0, $a0
    madd    $s1, $a1
    madd    $s2, $a2
    mflo    $v0
    mfhi    $v1
    srl     $v0, $v0, 12
    sll     $v1, $v1, 20
    addu    $v0, $v0, $v1
    sw      $v0, OFF_MAC1($t0)
    sh      $v0, OFF_IR1($t0)

    /* MAC2 = TRY + (R21*vx + R22*vy + R23*vz) / 4096. */
    sll     $v0, $t2, 12
    sra     $v1, $t2, 20
    mtlo    $v0
    mthi    $v1
    madd    $s3, $a0
    madd    $s4, $a1
    madd    $s5, $a2
    mflo    $v0
    mfhi    $v1
    srl     $v0, $v0, 12
    sll     $v1, $v1, 20
    addu    $v0, $v0, $v1
    sw      $v0, OFF_MAC2($t0)
    sh      $v0, OFF_IR2($t0)

    /* MAC3/depth = TRZ + (R31*vx + R32*vy + R33*vz) / 4096. */
    sll     $v0, $t3, 12
    sra     $v1, $t3, 20
    mtlo    $v0
    mthi    $v1
    madd    $s6, $a0
    madd    $s7, $a1
    madd    $t9, $a2
    mflo    $v0
    mfhi    $v1
    srl     $v0, $v0, 12
    sll     $v1, $v1, 20
    addu    $v0, $v0, $v1
    sw      $v0, OFF_MAC3($t0)
    sh      $v0, OFF_IR3($t0)
    sh      $v0, \SZOFF($t0)

    /* Native MIPS divide implements H << 16 / SZ.  GAS's two-operand
     * `divu rd,rs,rt` pseudo emits a software BREAK on a zero denominator
     * (and an unnecessary MFLO into rd).  A clipped zero-depth vertex is
     * legal in the PS1 scene stream, so use the real three-operand encoding
     * and take the same capped quotient used by limE instead of trapping. */
    /* SZ is the clamped low half of MAC3 in the common positive-depth
     * envelope.  Reusing V0 avoids a dependent D-cache load per vertex. */
    andi    $a3, $v0, 0xffff
    sll     $v0, $t4, 16
    bnez    $a3, 1f
    .word   0x0047001b                   /* delay slot: divu $zero,$v0,$a3 */
    lui     $a2, 2                       /* limE(0xffffffff) = 0x1ffff */
    addiu   $a2, $a2, -1
    b       2f
    nop
1:
    mflo    $a2                         /* quotient, retained for Y/IR0 */
2:
    lh      $a0, OFF_IR1($t0)
    lh      $a1, OFF_IR2($t0)

    /* SX = (OFX + IR1*quotient) >> 16. */
    mult    $a0, $a2
    mflo    $v0
    mfhi    $v1
    addu    $v0, $v0, $t5
    sltu    $a3, $v0, $t5
    addu    $v1, $v1, $a3
    srl     $v0, $v0, 16
    sll     $v1, $v1, 16
    addu    $v0, $v0, $v1
    sh      $v0, \SXYOFF($t0)

    /* SY = (OFY + IR2*quotient) >> 16. */
    mult    $a1, $a2
    mflo    $v0
    mfhi    $v1
    addu    $v0, $v0, $t6
    sltu    $a3, $v0, $t6
    addu    $v1, $v1, $a3
    srl     $v0, $v0, 16
    sll     $v1, $v1, 16
    addu    $v0, $v0, $v1
    sh      $v0, \SXYOFF+2($t0)
.endm

    RTPT_VERTEX OFF_CP2D+0, OFF_CP2D+4, OFF_SXY0, OFF_SZ1
    RTPT_VERTEX OFF_CP2D+8, OFF_CP2D+12, OFF_SXY1, OFF_SZ2
    RTPT_VERTEX OFF_CP2D+16, OFF_CP2D+20, OFF_SXY2, OFF_SZ3

    /* MAC0/IR0 use the final vertex quotient, just as the C implementation. */
    mult    $t7, $a2
    mflo    $v0
    mfhi    $v1
    addu    $v0, $v0, $t8
    sltu    $a3, $v0, $t8
    addu    $v1, $v1, $a3
    sw      $v0, OFF_MAC0($t0)
    srl     $v0, $v0, 12
    sll     $v1, $v1, 20
    addu    $v0, $v0, $v1
    sh      $v0, OFF_IR0($t0)

    lw      $s0, 0($sp)
    lw      $s1, 4($sp)
    lw      $s2, 8($sp)
    lw      $s3, 12($sp)
    lw      $s4, 16($sp)
    lw      $s5, 20($sp)
    lw      $s6, 24($sp)
    lw      $s7, 28($sp)
    addiu   $sp, $sp, 32
    jr      $ra
    nop
.end gte_RTPT_asm

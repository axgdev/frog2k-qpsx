#include <cstddef>

#include "r3000a.h"
#include "gte_rtpt_layout.h"

/* gte_rtpt_asm.S deliberately uses constants instead of C expressions. Keep
 * this compile-time contract next to the host checks so a psxRegisters edit
 * cannot silently corrupt the opt-in leaf kernel. */
static_assert(sizeof(psxGPRRegs) == 34u * sizeof(u32),
              "RTPT ABI: GPR register block changed");
static_assert(sizeof(psxCP0Regs) == 32u * sizeof(u32),
              "RTPT ABI: CP0 register block changed");
static_assert(sizeof(psxCP2Data) == 32u * sizeof(u32),
              "RTPT ABI: CP2 data register block changed");
static_assert(sizeof(psxCP2Ctrl) == 32u * sizeof(u32),
              "RTPT ABI: CP2 control register block changed");
static_assert(offsetof(psxRegisters, CP2D) == QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: OFF_CP2D no longer matches psxRegisters");
static_assert(offsetof(psxRegisters, CP2C) == QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: OFF_CP2C no longer matches psxRegisters");

/* The vertex macro addresses all three input vectors as raw offsets. */
static_assert(offsetof(psxCP2Data, n.v0) == 0u,
              "RTPT ABI: V0 offset changed");
static_assert(offsetof(psxCP2Data, n.v1) == 8u,
              "RTPT ABI: V1 offset changed");
static_assert(offsetof(psxCP2Data, n.v2) == 16u,
              "RTPT ABI: V2 offset changed");
static_assert(offsetof(SVector3D, x) == 0u &&
                  offsetof(SVector3D, y) == 2u &&
                  offsetof(SVector3D, z) == 4u,
              "RTPT ABI: vertex component offsets changed");

/* Data-register offsets used by the assembly kernel. */
static_assert(offsetof(psxCP2Data, n.ir0) ==
                  QPSX_RTPT_OFF_IR0 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: IR0 offset changed");
static_assert(offsetof(psxCP2Data, n.ir1) ==
                  QPSX_RTPT_OFF_IR1 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: IR1 offset changed");
static_assert(offsetof(psxCP2Data, n.ir2) ==
                  QPSX_RTPT_OFF_IR2 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: IR2 offset changed");
static_assert(offsetof(psxCP2Data, n.ir3) ==
                  QPSX_RTPT_OFF_IR3 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: IR3 offset changed");
static_assert(offsetof(psxCP2Data, n.sxy0) ==
                  QPSX_RTPT_OFF_SXY0 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: SXY0 offset changed");
static_assert(offsetof(psxCP2Data, n.sxy1) ==
                  QPSX_RTPT_OFF_SXY1 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: SXY1 offset changed");
static_assert(offsetof(psxCP2Data, n.sxy2) ==
                  QPSX_RTPT_OFF_SXY2 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: SXY2 offset changed");
static_assert(offsetof(psxCP2Data, n.sz0) ==
                  QPSX_RTPT_OFF_SZ0 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: SZ0 offset changed");
static_assert(offsetof(psxCP2Data, n.sz1) ==
                  QPSX_RTPT_OFF_SZ1 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: SZ1 offset changed");
static_assert(offsetof(psxCP2Data, n.sz2) ==
                  QPSX_RTPT_OFF_SZ2 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: SZ2 offset changed");
static_assert(offsetof(psxCP2Data, n.sz3) ==
                  QPSX_RTPT_OFF_SZ3 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: SZ3 offset changed");
static_assert(offsetof(psxCP2Data, n.mac0) ==
                  QPSX_RTPT_OFF_MAC0 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: MAC0 offset changed");
static_assert(offsetof(psxCP2Data, n.mac1) ==
                  QPSX_RTPT_OFF_MAC1 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: MAC1 offset changed");
static_assert(offsetof(psxCP2Data, n.mac2) ==
                  QPSX_RTPT_OFF_MAC2 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: MAC2 offset changed");
static_assert(offsetof(psxCP2Data, n.mac3) ==
                  QPSX_RTPT_OFF_MAC3 - QPSX_RTPT_OFF_CP2D,
              "RTPT ABI: MAC3 offset changed");

/* The kernel loads every packed halfword of the rotation matrix. */
static_assert(offsetof(psxCP2Ctrl, n.rMatrix.m11) ==
                  QPSX_RTPT_OFF_R11 - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: R11 offset changed");
static_assert(offsetof(psxCP2Ctrl, n.rMatrix.m12) ==
                  QPSX_RTPT_OFF_R12 - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: R12 offset changed");
static_assert(offsetof(psxCP2Ctrl, n.rMatrix.m13) ==
                  QPSX_RTPT_OFF_R13 - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: R13 offset changed");
static_assert(offsetof(psxCP2Ctrl, n.rMatrix.m21) ==
                  QPSX_RTPT_OFF_R21 - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: R21 offset changed");
static_assert(offsetof(psxCP2Ctrl, n.rMatrix.m22) ==
                  QPSX_RTPT_OFF_R22 - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: R22 offset changed");
static_assert(offsetof(psxCP2Ctrl, n.rMatrix.m23) ==
                  QPSX_RTPT_OFF_R23 - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: R23 offset changed");
static_assert(offsetof(psxCP2Ctrl, n.rMatrix.m31) ==
                  QPSX_RTPT_OFF_R31 - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: R31 offset changed");
static_assert(offsetof(psxCP2Ctrl, n.rMatrix.m32) ==
                  QPSX_RTPT_OFF_R32 - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: R32 offset changed");
static_assert(offsetof(psxCP2Ctrl, n.rMatrix.m33) ==
                  QPSX_RTPT_OFF_R33 - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: R33 offset changed");

/* Control-register offsets used by the assembly kernel. */
static_assert(offsetof(psxCP2Ctrl, n.trX) ==
                  QPSX_RTPT_OFF_TRX - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: TRX offset changed");
static_assert(offsetof(psxCP2Ctrl, n.trY) ==
                  QPSX_RTPT_OFF_TRY - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: TRY offset changed");
static_assert(offsetof(psxCP2Ctrl, n.trZ) ==
                  QPSX_RTPT_OFF_TRZ - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: TRZ offset changed");
static_assert(offsetof(psxCP2Ctrl, n.ofx) ==
                  QPSX_RTPT_OFF_OFX - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: OFX offset changed");
static_assert(offsetof(psxCP2Ctrl, n.ofy) ==
                  QPSX_RTPT_OFF_OFY - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: OFY offset changed");
static_assert(offsetof(psxCP2Ctrl, n.h) ==
                  QPSX_RTPT_OFF_H - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: H offset changed");
static_assert(offsetof(psxCP2Ctrl, n.dqa) ==
                  QPSX_RTPT_OFF_DQA - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: DQA offset changed");
static_assert(offsetof(psxCP2Ctrl, n.dqb) ==
                  QPSX_RTPT_OFF_DQB - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: DQB offset changed");
static_assert(offsetof(psxCP2Ctrl, n.flag) ==
                  QPSX_RTPT_OFF_FLAG - QPSX_RTPT_OFF_CP2C,
              "RTPT ABI: FLAG offset changed");

int main()
{
	return 0;
}

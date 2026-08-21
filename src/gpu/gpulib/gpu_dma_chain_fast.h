/*
 * Tiny, pure predicate shared by the optional-chain test and its production
 * finalization rule. Chain state itself stays as scalar locals in
 * GPU_dmaChain so the MIPS hot loop has no abstraction or state-object tax.
 */
#ifndef QPSX_GPU_DMA_CHAIN_FAST_H
#define QPSX_GPU_DMA_CHAIN_FAST_H

#include <stdint.h>

static inline int
qpsx_gpu_dma_chain_finish_needed(uint32_t old_e3, uint32_t new_e3)
{
  return old_e3 != new_e3;
}

/* This is the integer expression used by decide_frameskip_allow().  Keep it
 * pure so the optional finalizer can live in its own object without moving
 * gpu.o, and so its boundary behaviour is testable on the host. */
static inline uint32_t
qpsx_gpu_frameskip_allow_value(uint32_t interlace, uint32_t cmd_e3,
                               int screen_x, int screen_y,
                               int screen_w, int screen_h)
{
  uint32_t x = cmd_e3 & 0x3ff;
  uint32_t y = (cmd_e3 >> 10) & 0x3ff;
  return interlace ||
    (uint32_t)(x - screen_x) >= (uint32_t)screen_w ||
    (uint32_t)(y - screen_y) >= (uint32_t)screen_h;
}

static inline int
qpsx_gpu_dma_chain_can_fast(int cmd_len, uint32_t frameskip_active,
                            int dma_height)
{
  return cmd_len == 0 && !frameskip_active && !dma_height;
}

#endif /* QPSX_GPU_DMA_CHAIN_FAST_H */

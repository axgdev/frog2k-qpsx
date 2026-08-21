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

#endif /* QPSX_GPU_DMA_CHAIN_FAST_H */

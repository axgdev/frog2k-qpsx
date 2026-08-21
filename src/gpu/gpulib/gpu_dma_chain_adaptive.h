/* Small interface for the optional long linked-list batching path.  The
 * predictor predicate is pure so the host test can exercise the exact gate;
 * all mutable GPU state remains in GPU_dmaChain. */
#ifndef QPSX_GPU_DMA_CHAIN_ADAPTIVE_H
#define QPSX_GPU_DMA_CHAIN_ADAPTIVE_H

#include <stdint.h>

#ifndef QPSX_GPU_DMA_CHAIN_ADAPTIVE_MIN_PREV_WORK
#define QPSX_GPU_DMA_CHAIN_ADAPTIVE_MIN_PREV_WORK 0
#endif

/* Use the already-maintained last-list record as a zero-new-store predictor.
 * A recent chain in this frame, or the immediately preceding frame, is
 * enough evidence to enter the cold walker; stale records cannot enable it. */
static inline int qpsx_gpu_dma_chain_adaptive_prev_ready(
    unsigned last_frame, unsigned frame, unsigned work, unsigned threshold,
    uint32_t frameskip_active, int dma_height)
{
  int adjacent = last_frame == frame ||
    (frame != 0 && last_frame == frame - 1);
  return adjacent && threshold != 0 && work >= threshold &&
    !frameskip_active && !dma_height;
}

#if defined(__GNUC__)
#define QPSX_GPU_ADAPTIVE_NOINLINE __attribute__((noinline))
#define QPSX_GPU_ADAPTIVE_SECTION __attribute__((section(".text.qpsx_dma_adaptive")))
#else
#define QPSX_GPU_ADAPTIVE_NOINLINE
#define QPSX_GPU_ADAPTIVE_SECTION
#endif

#ifdef __cplusplus
extern "C" {
#endif
int qpsx_gpu_adaptive_cmd_buffer(uint32_t *data, int count,
                                 int *vram_dirty);
void qpsx_gpu_adaptive_finish(int vram_dirty, uint32_t old_e3);
long qpsx_gpu_adaptive_dma_chain(uint32_t *rambase, uint32_t start_addr);
int qpsx_gpu_adaptive_legacy_cmd_buffer(uint32_t *data, int count);
#ifdef __cplusplus
}
#endif

#endif /* QPSX_GPU_DMA_CHAIN_ADAPTIVE_H */

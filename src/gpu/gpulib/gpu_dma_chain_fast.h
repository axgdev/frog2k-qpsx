/*
 * Small, pure orchestration state for the optional linked-list GPU fast path.
 *
 * The command parser remains the production implementation.  This header only
 * decides when its per-buffer bookkeeping may be committed at the end of a
 * chain, and is deliberately independent of the global GPU object so the
 * state-machine test can exercise the exact production decisions cheaply.
 */
#ifndef QPSX_GPU_DMA_CHAIN_FAST_H
#define QPSX_GPU_DMA_CHAIN_FAST_H

#include <stdint.h>

struct qpsx_gpu_dma_chain_fast_state {
  int active;
  int saw_node;
  int pending_dirty;
  uint32_t old_e3;
};

static inline int
qpsx_gpu_dma_chain_fast_begin(struct qpsx_gpu_dma_chain_fast_state *state,
                              int cmd_len, int frameskip_active,
                              int dma_active, uint32_t ex3)
{
  state->active = (cmd_len == 0 && !frameskip_active && !dma_active);
  state->saw_node = 0;
  state->pending_dirty = 0;
  state->old_e3 = ex3;
  return state->active;
}

/*
 * Record one already-processed node.  A return value of one means the caller
 * must commit pending bookkeeping before entering the next node.  E3 is
 * intentionally accumulated from chain entry: while frameskip is inactive,
 * its per-node allow value cannot affect parsing and the final value is the
 * only observable one.  A nonzero `left` or an active transfer makes the
 * following node use the legacy finalized path; the current node is still
 * processed exactly once.
 */
static inline int
qpsx_gpu_dma_chain_fast_node(struct qpsx_gpu_dma_chain_fast_state *state,
                             int segment_dirty, int left, int dma_active,
                             int frameskip_active)
{
  int unsafe;

  if (!state->active)
    return 0;

  state->saw_node = 1;
  state->pending_dirty |= segment_dirty;

  unsafe = (left != 0) || dma_active || frameskip_active;
  if (unsafe)
    state->active = 0;

  return unsafe;
}

static inline void
qpsx_gpu_dma_chain_fast_commit(
    struct qpsx_gpu_dma_chain_fast_state *state)
{
  state->pending_dirty = 0;
}

#endif /* QPSX_GPU_DMA_CHAIN_FAST_H */

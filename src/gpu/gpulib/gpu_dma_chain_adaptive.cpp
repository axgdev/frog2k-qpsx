/* Cold, optional parser entered only by the previous-chain predictor.  The
 * historical do_cmd_buffer and linked-list walker stay in gpu.cpp; this
 * object contains only the deferred parser/finalizer so enabling the
 * experiment does not move the normal hot code. */
#include "gpu.h"
#include "gpu_dma_chain_adaptive.h"
#include "profiler.h"
#include "qpsx_phase_metrics.h"

#ifndef QPSX_GPU_DMA_CHAIN_ADAPTIVE_DEFER_PREFETCH
#define QPSX_GPU_DMA_CHAIN_ADAPTIVE_DEFER_PREFETCH 0
#endif

/* These operations are unreachable on the common deferred path, but are
 * needed to preserve exact A0/C0 and continued-write behaviour before the
 * walker falls back to the historical parser. */
extern int qpsx_gpu_adaptive_do_vram_io(uint32_t *data, int count,
                                        int is_read);
extern void qpsx_gpu_adaptive_start_vram_transfer(uint32_t pos_word,
                                                  uint32_t size_word,
                                                  int is_read);
extern int qpsx_gpu_adaptive_legacy_cmd_buffer(uint32_t *data, int count);
extern int do_cmd_list(uint32_t *data, int count, int *last_cmd);

QPSX_GPU_ADAPTIVE_SECTION QPSX_GPU_ADAPTIVE_NOINLINE
int qpsx_gpu_adaptive_cmd_buffer(uint32_t *data, int count, int *vram_dirty)
{
  int cmd, pos;

  *vram_dirty = 0;
  for (pos = 0; pos < count; ) {
    if (gpu.dma.h && !gpu.dma_start.is_read) {
      *vram_dirty = 1;
      pos += qpsx_gpu_adaptive_do_vram_io(data + pos, count - pos, 0);
      if (pos == count)
        break;
    }

    cmd = data[pos] >> 24;
    if (0xa0 <= cmd && cmd <= 0xdf) {
      qpsx_gpu_adaptive_start_vram_transfer(
        data[pos + 1], data[pos + 2], (cmd & 0xe0) == 0xc0);
      pos += 3;
      continue;
    }

    /* Entry is made only while frameskip is inactive.  GP0 parsing cannot
     * make it active, so this is the non-skipping arm of do_cmd_buffer. */
    pos += do_cmd_list(data + pos, count - pos, &cmd);
    *vram_dirty = 1;
    if (cmd == -1)
      break;
  }

  return count - pos;
}

QPSX_GPU_ADAPTIVE_SECTION QPSX_GPU_ADAPTIVE_NOINLINE
void qpsx_gpu_adaptive_finish(int vram_dirty, uint32_t old_e3)
{
  gpu.status.reg &= ~0x1fff;
  gpu.status.reg |= gpu.ex_regs[1] & 0x7ff;
  gpu.status.reg |= (gpu.ex_regs[6] & 3) << 11;
  gpu.state.fb_dirty |= vram_dirty;

  if (old_e3 != gpu.ex_regs[3]) {
    uint32_t x = gpu.ex_regs[3] & 0x3ff;
    uint32_t y = (gpu.ex_regs[3] >> 10) & 0x3ff;
    gpu.frameskip.allow = gpu.status.interlace ||
      (uint32_t)(x - gpu.screen.x) >= (uint32_t)gpu.screen.w ||
      (uint32_t)(y - gpu.screen.y) >= (uint32_t)gpu.screen.h;
  }
}

/* Complete linked-list walker for predicted-heavy frames.  Keeping the
 * traversal out of gpu.o is important: a predicted-light frame executes the
 * original GPU_dmaChain loop and never fetches this text.  The deferred
 * parser is used only while the chain is ordinary; one malformed command or
 * image transfer finalizes the accumulated state and irreversibly falls back
 * to the original per-node parser for the remainder. */
QPSX_GPU_ADAPTIVE_SECTION QPSX_GPU_ADAPTIVE_NOINLINE
long qpsx_gpu_adaptive_dma_chain(uint32_t *rambase, uint32_t start_addr)
{
  uint32_t addr, *list, ld_addr = 0;
  int len, left, count;
  long cpu_cycles = 0;
  int fast = 1;
  int saw_node = 0;
  int dirty = 0;
  uint32_t old_e3 = gpu.ex_regs[3];
#if QPSX_PHASE_METRICS
  unsigned dma_nodes = 0;
  unsigned dma_nonempty = 0;
  unsigned dma_words = 0;
  unsigned dma_batched = 0;
  unsigned dma_fallback = 0;
#endif

#if defined(__GNUC__) && !QPSX_GPU_DMA_CHAIN_ADAPTIVE_DEFER_PREFETCH
  /* GPU_dmaChain's caller already issued this hint on the opt-in path. */
  __builtin_prefetch(rambase + (start_addr & 0x1fffff) / 4);
#endif
  for (count = 0, addr = start_addr & 0xffffff;
       (addr & 0x800000) == 0; count++) {
    int segment_dirty;

    list = rambase + (addr & 0x1fffff) / 4;
    len = list[0] >> 24;
    addr = list[0] & 0xffffff;
#if QPSX_PHASE_METRICS
    dma_nodes++;
    if (len)
      dma_nonempty++;
    dma_words += (unsigned)len;
#endif
#if defined(__GNUC__)
    __builtin_prefetch(rambase + (addr & 0x1fffff) / 4);
#endif

    cpu_cycles += 10;
    if (len > 0)
      cpu_cycles += 5 + len;

    if (len) {
      if (fast) {
        if (!saw_node)
          PROFILE_START(PROF_GPU_TOTAL);
        saw_node = 1;
        left = qpsx_gpu_adaptive_cmd_buffer(list + 1, len,
                                            &segment_dirty);
        dirty |= segment_dirty;
#if QPSX_PHASE_METRICS
        dma_batched++;
#endif
        if (left || gpu.dma.h || gpu.frameskip.active) {
          qpsx_gpu_adaptive_finish(dirty, old_e3);
          PROFILE_END(PROF_GPU_TOTAL);
          fast = 0;
#if QPSX_PHASE_METRICS
          dma_fallback++;
#endif
        }
      } else {
        left = qpsx_gpu_adaptive_legacy_cmd_buffer(list + 1, len);
      }
    }

    /* Keep the original 8K loop-marker protocol exactly. */
    if (count >= 8 * 1024) {
      if (count == 8 * 1024) {
        ld_addr = addr;
        continue;
      }
      list[0] |= 0x800000;
    }
  }

  if (ld_addr != 0) {
    count -= 8 * 1024 + 2;
    addr = ld_addr & 0x1fffff;
    while (count-- > 0) {
      list = rambase + addr / 4;
      addr = list[0] & 0x1fffff;
      list[0] &= ~0x800000;
    }
  }

  if (fast && saw_node) {
    qpsx_gpu_adaptive_finish(dirty, old_e3);
    PROFILE_END(PROF_GPU_TOTAL);
  }

#if QPSX_PHASE_METRICS
  QPSX_PHASE_DMA(dma_nodes, dma_nonempty, dma_words, (unsigned)cpu_cycles,
                 1, dma_batched, dma_fallback);
#endif
  gpu.state.last_list.frame = *gpu.state.frame_count;
  gpu.state.last_list.hcnt = *gpu.state.hcnt;
  gpu.state.last_list.cycles = cpu_cycles;
  gpu.state.last_list.addr = start_addr;
  return cpu_cycles;
}

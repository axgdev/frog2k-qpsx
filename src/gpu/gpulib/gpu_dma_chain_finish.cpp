/* Optional DMA-chain finalization kept in its own object.  This prevents the
 * guarded build from shifting gpu.o's parser text and lets the 16 KiB I-cache
 * see the same command-processing layout as the control build. */
#include "gpu.h"
#include "gpu_dma_chain_fast.h"

extern int decide_frameskip_allow(uint32_t cmd_e3);

void qpsx_gpu_finish_cmd_buffer(int vram_dirty, uint32_t old_e3)
{
  gpu.status.reg &= ~0x1fff;
  gpu.status.reg |= gpu.ex_regs[1] & 0x7ff;
  gpu.status.reg |= (gpu.ex_regs[6] & 3) << 11;

  gpu.state.fb_dirty |= vram_dirty;

  if (qpsx_gpu_dma_chain_finish_needed(old_e3, gpu.ex_regs[3]))
    decide_frameskip_allow(gpu.ex_regs[3]);
}

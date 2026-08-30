/* Optional DMA-chain finalization kept in its own object.  This prevents the
 * guarded build from shifting gpu.o's parser text and lets the 16 KiB I-cache
 * see the same command-processing layout as the control build. */
#include "gpu.h"
#include "gpu_dma_chain_fast.h"

void qpsx_gpu_finish_cmd_buffer(int vram_dirty, uint32_t old_e3)
{
  gpu.status.reg &= ~0x1fff;
  gpu.status.reg |= gpu.ex_regs[1] & 0x7ff;
  gpu.status.reg |= (gpu.ex_regs[6] & 3) << 11;

  gpu.state.fb_dirty |= vram_dirty;

  if (qpsx_gpu_dma_chain_finish_needed(old_e3, gpu.ex_regs[3])) {
    /* Keep this expression identical to gpu.cpp's static
     * decide_frameskip_allow(). It is out-of-line here only to keep the
     * optional finalizer independent of gpu.o's text layout. */
    gpu.frameskip.allow = qpsx_gpu_frameskip_allow_value(
      gpu.status.interlace, gpu.ex_regs[3], gpu.screen.x, gpu.screen.y,
      gpu.screen.w, gpu.screen.h);
  }
}

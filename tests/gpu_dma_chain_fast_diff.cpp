/*
 * The optional DMA-chain path deliberately keeps its state as scalar locals
 * in GPU_dmaChain. This host test proves the exact shared E3 finalization
 * predicate without linking the whole MIPS GPU, and exercises the values that
 * previously caused the state-machine implementation to over-commit.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/gpu/gpulib/gpu_dma_chain_fast.h"

struct Case {
  uint32_t old_e3;
  uint32_t new_e3;
  int expected;
};

int main(void)
{
  static const Case cases[] = {
    {0x00000000u, 0x00000000u, 0},
    {0x00000010u, 0x00000010u, 0},
    {0xffffffffu, 0xffffffffu, 0},
    {0x00000010u, 0x00000020u, 1},
    {0x00000020u, 0x00000010u, 1},
    {0x00000000u, 0xffffffffu, 1},
    {0xffffffffu, 0x00000000u, 1},
    /* E3 changes and changes back across two ordinary nodes: the production
     * chain commits once at the end because only the final value matters. */
    {0x00000010u, 0x00000020u, 1},
    {0x00000020u, 0x00000010u, 1},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    int got = qpsx_gpu_dma_chain_finish_needed(cases[i].old_e3,
                                                cases[i].new_e3);
    if (got != cases[i].expected) {
      fprintf(stderr, "case %zu: expected %d, got %d\n",
              i, cases[i].expected, got);
      return 1;
    }
  }

  puts("gpu_dma_chain_fast_diff: PASS (9 finalization cases)");
  return 0;
}

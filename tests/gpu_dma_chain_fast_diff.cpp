/*
 * Differential state-machine proof for the optional GPU DMA-chain
 * orchestration.  The parser itself is deliberately not duplicated here:
 * production's exact boundary decision is included from
 * gpu_dma_chain_fast.h, while this test supplies independent node results and
 * compares them with the historical per-node finalization model.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/gpu/gpulib/gpu_dma_chain_fast.h"

struct Node {
  int words;
  int dirty;
  int left;
  int dma_active;
  int frameskip_active;
  uint32_t ex3_after;
  int allow_after;
};

struct Result {
  int fb_dirty;
  int finalizations;
  int allow;
  uint32_t ex3;
};

static void finalize(Result *result, int pending_dirty, uint32_t old_e3,
                     int allow_after)
{
  result->fb_dirty |= pending_dirty;
  result->finalizations++;
  if (old_e3 != result->ex3)
    result->allow = allow_after;
}

static Result run_legacy(int cmd_len, int frameskip_active, int dma_active,
                         uint32_t ex3, const Node *nodes, size_t count)
{
  Result result = {0, 0, 0, ex3};
  if (cmd_len || frameskip_active || dma_active) {
    /* These inputs affect only fast-path eligibility; the legacy path still
     * processes every node exactly as usual. */
  }
  for (size_t i = 0; i < count; ++i) {
    const Node &node = nodes[i];
    if (!node.words)
      continue;
    uint32_t old_e3 = result.ex3;
    result.ex3 = node.ex3_after;
    finalize(&result, node.dirty, old_e3, node.allow_after);
  }
  return result;
}

static Result run_fast(int cmd_len, int frameskip_active, int dma_active,
                       uint32_t ex3, const Node *nodes, size_t count)
{
  Result result = {0, 0, 0, ex3};
  qpsx_gpu_dma_chain_fast_state state;
  int final_allow = result.allow;
  int chain_fast = qpsx_gpu_dma_chain_fast_begin(
      &state, cmd_len, frameskip_active, dma_active, ex3);

  for (size_t i = 0; i < count; ++i) {
    const Node &node = nodes[i];
    if (!node.words)
      continue;

    if (!chain_fast) {
      uint32_t old_e3 = result.ex3;
      result.ex3 = node.ex3_after;
      final_allow = node.allow_after;
      finalize(&result, node.dirty, old_e3, node.allow_after);
      continue;
    }

    int commit = qpsx_gpu_dma_chain_fast_node(
        &state, node.dirty, node.left, node.dma_active,
        node.frameskip_active);
    result.ex3 = node.ex3_after;
    final_allow = node.allow_after;
    if (commit) {
      finalize(&result, state.pending_dirty, state.old_e3,
               node.allow_after);
      qpsx_gpu_dma_chain_fast_commit(&state);
      if (!state.active) {
        chain_fast = 0;
      }
    }
  }

  if (chain_fast && state.saw_node)
    finalize(&result, state.pending_dirty, state.old_e3,
             final_allow);
  return result;
}

static void compare_state(const char *name, int cmd_len,
                          int frameskip_active, int dma_active, uint32_t ex3,
                          const Node *nodes, size_t count,
                          int expected_fast_finalizations)
{
  Result legacy = run_legacy(cmd_len, frameskip_active, dma_active,
                             ex3, nodes, count);
  Result fast = run_fast(cmd_len, frameskip_active, dma_active,
                         ex3, nodes, count);
  if (legacy.fb_dirty != fast.fb_dirty || legacy.allow != fast.allow ||
      legacy.ex3 != fast.ex3) {
    fprintf(stderr, "%s: state mismatch (legacy dirty=%d allow=%d ex3=%08x; "
            "fast dirty=%d allow=%d ex3=%08x)\n", name,
            legacy.fb_dirty, legacy.allow, legacy.ex3,
            fast.fb_dirty, fast.allow, fast.ex3);
    exit(1);
  }
  if (fast.finalizations != expected_fast_finalizations) {
    fprintf(stderr, "%s: expected %d fast finalizations, got %d\n", name,
            expected_fast_finalizations, fast.finalizations);
    exit(1);
  }
}

int main(void)
{
  static const Node ordinary[] = {
    {8, 1, 0, 0, 0, 0x10, 0}, {5, 1, 0, 0, 0, 0x10, 0},
    {12, 1, 0, 0, 0, 0x10, 0}, {3, 1, 0, 0, 0, 0x10, 0}
  };
  static const Node e3_boundary[] = {
    {4, 1, 0, 0, 0, 0x10, 0}, {4, 1, 0, 0, 0, 0x20, 1},
    {4, 1, 0, 0, 0, 0x20, 1}
  };
  static const Node e3_change_back[] = {
    {4, 1, 0, 0, 0, 0x20, 1}, {4, 1, 0, 0, 0, 0x10, 0}
  };
  static const Node incomplete[] = {
    {7, 1, 1, 0, 0, 0x10, 0}, {7, 1, 0, 0, 0, 0x10, 0},
    {7, 1, 0, 0, 0, 0x10, 0}
  };
  static const Node image_write[] = {
    /* A0 starts a write transfer but contributes no dirty word until the
     * following payload is consumed by do_vram_io. */
    {3, 0, 0, 1, 0, 0x10, 0}, {8, 1, 0, 0, 0, 0x10, 0},
    {8, 1, 0, 0, 0, 0x10, 0}
  };
  static const Node image_read[] = {
    /* C0 starts a read transfer; command-buffer processing does not mark the
     * framebuffer dirty for that start, and read data is consumed by the
     * separate GPU_readDataMem path. */
    {3, 0, 0, 1, 0, 0x10, 0}, {2, 1, 0, 1, 0, 0x10, 0}
  };
  static const Node zero_and_terminal[] = {
    {0, 0, 0, 0, 0, 0x10, 0}, {5, 1, 0, 0, 0, 0x10, 0},
    {0, 0, 0, 0, 0, 0xffffff, 0}
  };
  static const Node state_commands[] = {
    {2, 1, 0, 0, 0, 0x10, 0}, {2, 1, 0, 0, 0, 0x10, 0},
    {2, 1, 0, 0, 0, 0x30, 1}
  };

  compare_state("ordinary mixed nodes", 0, 0, 0, 0x10,
                ordinary, sizeof(ordinary) / sizeof(ordinary[0]), 1);
  compare_state("E3 boundary", 0, 0, 0, 0x10,
                e3_boundary, sizeof(e3_boundary) / sizeof(e3_boundary[0]), 1);
  compare_state("E3 change back", 0, 0, 0, 0x10,
                e3_change_back,
                sizeof(e3_change_back) / sizeof(e3_change_back[0]), 1);
  compare_state("incomplete command fallback", 0, 0, 0, 0x10,
                incomplete, sizeof(incomplete) / sizeof(incomplete[0]), 3);
  compare_state("A0 write and continuation", 0, 0, 0, 0x10,
                image_write, sizeof(image_write) / sizeof(image_write[0]), 3);
  compare_state("C0/read transfer fallback", 0, 0, 0, 0x10,
                image_read, sizeof(image_read) / sizeof(image_read[0]), 2);
  compare_state("zero-length and terminal", 0, 0, 0, 0x10,
                zero_and_terminal,
                sizeof(zero_and_terminal) / sizeof(zero_and_terminal[0]), 1);
  compare_state("E1-E6 state stream", 0, 0, 0, 0x10,
                state_commands,
                sizeof(state_commands) / sizeof(state_commands[0]), 1);
  compare_state("initial pending command", 1, 0, 0, 0x10,
                ordinary, sizeof(ordinary) / sizeof(ordinary[0]), 4);
  compare_state("initial frameskip", 0, 1, 0, 0x10,
                ordinary, sizeof(ordinary) / sizeof(ordinary[0]), 4);
  compare_state("initial transfer", 0, 0, 1, 0x10,
                ordinary, sizeof(ordinary) / sizeof(ordinary[0]), 4);

  puts("gpu_dma_chain_fast_diff: PASS (11 boundary/state cases)");
  return 0;
}

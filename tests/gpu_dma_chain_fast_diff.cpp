/*
 * Host-only state-machine proof for the optional GPU_dmaChain bookkeeping
 * path.  The parser/renderer are intentionally not linked here: this model
 * isolates the production orchestration rule (which nodes can be coalesced,
 * and when the legacy finalizer must be used) while the pure predicates and
 * frameskip expression are shared with production code.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/gpu/gpulib/gpu_dma_chain_fast.h"
#include "../src/gpu/gpulib/gpu_dma_chain_adaptive.h"

enum NodeKind {
  NODE_EMPTY,
  NODE_NORMAL,
  NODE_REGS,
  NODE_E3,
  NODE_INCOMPLETE,
  NODE_A0_WRITE,
  NODE_C0_READ,
  NODE_TRANSFER
};

struct Node {
  NodeKind kind;
  uint32_t value;
  uint32_t ex1;
  uint32_t ex6;
  int len;
};

struct State {
  uint32_t status;
  uint32_t ex1;
  uint32_t ex6;
  uint32_t e3;
  int cmd_len;
  int dma_h;
  int dma_read;
  int frameskip_active;
  uint32_t allow;
  uint32_t dirty;
  uint32_t frame;
  uint32_t hcnt;
  uint32_t last_addr;
  uint32_t cycles;
  int loop_marker;
  int finish_calls;
};

struct Result {
  int left;
  int dirty;
};

static uint32_t independent_allow(uint32_t interlace, uint32_t e3,
                                  int sx, int sy, int sw, int sh)
{
  uint32_t x = e3 & 0x3ff;
  uint32_t y = (e3 >> 10) & 0x3ff;
  return interlace ||
    (uint32_t)(x - sx) >= (uint32_t)sw ||
    (uint32_t)(y - sy) >= (uint32_t)sh;
}

static void finish(State *s, int dirty, uint32_t old_e3)
{
  s->status = (s->status & ~0x1fffU) |
    (s->ex1 & 0x7ffU) | ((s->ex6 & 3U) << 11);
  s->dirty |= (uint32_t)dirty;
  if (qpsx_gpu_dma_chain_finish_needed(old_e3, s->e3)) {
    s->allow = qpsx_gpu_frameskip_allow_value(
      (s->status >> 22) & 1U, s->e3, 10, 20, 100, 80);
  }
  s->finish_calls++;
}

/* This models only externally relevant parser effects.  A0/C0 deliberately
 * leave a transfer active; the next node exercises the legacy continuation
 * path.  A write continuation dirties VRAM, while a read continuation does
 * not, matching do_vram_io(). */
static Result process(State *s, const Node *n)
{
  Result r = { 0, 0 };

  if (s->dma_h) {
    if (n->kind == NODE_TRANSFER) {
      s->dma_h = 0;
      r.dirty = !s->dma_read;
      return r;
    }
    r.left = n->len;
    return r;
  }

  switch (n->kind) {
    case NODE_EMPTY:
      break;
    case NODE_NORMAL:
      r.dirty = 1;
      break;
    case NODE_REGS:
      s->ex1 = n->ex1;
      s->ex6 = n->ex6;
      r.dirty = 1;
      break;
    case NODE_E3:
      s->e3 = n->value;
      r.dirty = 1;
      break;
    case NODE_INCOMPLETE:
      r.left = n->len;
      break;
    case NODE_A0_WRITE:
      s->dma_h = 2;
      s->dma_read = 0;
      break;
    case NODE_C0_READ:
      s->dma_h = 2;
      s->dma_read = 1;
      break;
    case NODE_TRANSFER:
      /* A transfer without an active A0/C0 is malformed input. */
      r.left = n->len;
      break;
  }
  return r;
}

static void finish_chain_metadata(State *s, const Node *nodes, size_t count,
                                  uint32_t start_addr, int long_chain)
{
  size_t i;
  s->cycles = 0;
  for (i = 0; i < count; ++i) {
    if (nodes[i].len > 0)
      s->cycles += (uint32_t)(5 + nodes[i].len);
    s->cycles += 10;
  }
  s->frame = 77;
  s->hcnt = 123;
  s->last_addr = start_addr;
  if (long_chain)
    s->loop_marker = 0;
}

static void run_legacy(State *s, const Node *nodes, size_t count,
                       uint32_t start_addr, int long_chain)
{
  size_t i;
  for (i = 0; i < count; ++i) {
    Result r;
    uint32_t old_e3;
    if (nodes[i].len == 0)
      continue;
    old_e3 = s->e3;
    r = process(s, &nodes[i]);
    finish(s, r.dirty, old_e3);
  }
  finish_chain_metadata(s, nodes, count, start_addr, long_chain);
}

static void run_fast(State *s, const Node *nodes, size_t count,
                     uint32_t start_addr, int long_chain)
{
  size_t i;
  int fast = qpsx_gpu_dma_chain_can_fast(
    s->cmd_len, (uint32_t)s->frameskip_active, s->dma_h);
  int saw_node = 0;
  int chain_dirty = 0;
  uint32_t old_e3 = s->e3;

  for (i = 0; i < count; ++i) {
    Result r;
    if (nodes[i].len == 0)
      continue;
    if (fast) {
      saw_node = 1;
      r = process(s, &nodes[i]);
      chain_dirty |= r.dirty;
      if (r.left || s->dma_h || s->frameskip_active) {
        finish(s, chain_dirty, old_e3);
        chain_dirty = 0;
        fast = 0;
      }
    } else {
      uint32_t node_old_e3 = s->e3;
      r = process(s, &nodes[i]);
      finish(s, r.dirty, node_old_e3);
    }
  }
  if (fast && saw_node)
    finish(s, chain_dirty, old_e3);
  finish_chain_metadata(s, nodes, count, start_addr, long_chain);
}

static int same_state(const State *a, const State *b)
{
  return a->status == b->status && a->ex1 == b->ex1 &&
    a->ex6 == b->ex6 && a->e3 == b->e3 && a->cmd_len == b->cmd_len &&
    a->dma_h == b->dma_h && a->dma_read == b->dma_read &&
    a->frameskip_active == b->frameskip_active && a->allow == b->allow &&
    a->dirty == b->dirty && a->frame == b->frame && a->hcnt == b->hcnt &&
    a->last_addr == b->last_addr && a->cycles == b->cycles &&
    a->loop_marker == b->loop_marker;
}

static State initial_state(void)
{
  State s;
  memset(&s, 0, sizeof(s));
  s.status = 0x14802000U;
  s.ex1 = 0x123U;
  s.ex6 = 2;
  s.e3 = (20U << 10) | 10U;
  /* Initial allow corresponds to the initial E3; this is what makes the
   * change-then-change-back case compare the final externally visible state,
   * rather than an intentionally stale fixture. */
  s.allow = independent_allow((s.status >> 22) & 1U, s.e3,
                               10, 20, 100, 80);
  s.frame = 77;
  s.hcnt = 123;
  s.loop_marker = 1;
  return s;
}

static int compare_case(const char *name, State s, const Node *nodes,
                        size_t count, int expect_fast)
{
  State legacy = s;
  State fast = s;
  run_legacy(&legacy, nodes, count, 0x1000, 1);
  run_fast(&fast, nodes, count, 0x1000, 1);
  if (!same_state(&legacy, &fast)) {
    fprintf(stderr, "%s: final state mismatch\n", name);
    return 1;
  }
  if (!!(fast.finish_calls < legacy.finish_calls) != !!expect_fast) {
    fprintf(stderr, "%s: unexpected finalizer count (%d vs %d)\n",
            name, legacy.finish_calls, fast.finish_calls);
    return 1;
  }
  return 0;
}

/* The production entry gate uses gpu.state.last_list directly.  Keep this
 * test focused on that exact inline predicate: no frame accumulator exists
 * in the performance path, so stale or same-frame records are the only
 * predictor state that can affect dispatch.  Parser/state equivalence and
 * irreversible fallback are covered by the state-machine cases above. */
static int check_previous_chain_gate(void)
{
  const unsigned thresholds[] = { 2048, 4096, 8192 };
  size_t i;

  for (i = 0; i < sizeof(thresholds) / sizeof(thresholds[0]); ++i) {
    unsigned threshold = thresholds[i];
    if (qpsx_gpu_dma_chain_adaptive_prev_ready(8, 10, threshold, threshold,
                                               0, 0))
      return 1; /* a record older than one frame is stale */
    if (qpsx_gpu_dma_chain_adaptive_prev_ready(9, 10, threshold - 1,
                                               threshold, 0, 0))
      return 1; /* exact threshold is inclusive, but threshold-1 is not */
    if (!qpsx_gpu_dma_chain_adaptive_prev_ready(9, 10, threshold, threshold,
                                                0, 0))
      return 1; /* immediately preceding frame, heavy chain */
    if (!qpsx_gpu_dma_chain_adaptive_prev_ready(10, 10, threshold, threshold,
                                                0, 0))
      return 1; /* same-frame subsequent chain is also usable */
    if (qpsx_gpu_dma_chain_adaptive_prev_ready(9, 10, threshold, threshold,
                                               1, 0) ||
        qpsx_gpu_dma_chain_adaptive_prev_ready(9, 10, threshold, threshold,
                                               0, 1))
      return 1; /* unsafe entry must stay on legacy traversal */
  }

  /* Frame zero has no representable preceding frame.  A same-frame record is
   * valid, while UINT_MAX must not wrap into frame zero. */
  if (qpsx_gpu_dma_chain_adaptive_prev_ready(~0u, 0, 4096, 4096, 0, 0) ||
      !qpsx_gpu_dma_chain_adaptive_prev_ready(0, 0, 4096, 4096, 0, 0))
    return 1;
  return 0;
}

int main(void)
{
  static const Node ordinary[] = {
    { NODE_EMPTY, 0, 0, 0, 0 },
    { NODE_NORMAL, 0, 0, 0, 3 },
    { NODE_REGS, 0, 0x456, 1, 2 },
    { NODE_NORMAL, 0, 0, 0, 1 }
  };
  static const Node e3_back[] = {
    { NODE_E3, (20U << 10) | 110U, 0, 0, 1 },
    { NODE_E3, (20U << 10) | 10U, 0, 0, 1 }
  };
  static const Node incomplete[] = {
    { NODE_NORMAL, 0, 0, 0, 1 },
    { NODE_INCOMPLETE, 0, 0, 0, 1 },
    { NODE_NORMAL, 0, 0, 0, 1 }
  };
  static const Node write_transfer[] = {
    { NODE_A0_WRITE, 0, 0, 0, 3 },
    { NODE_TRANSFER, 0, 0, 0, 2 },
    { NODE_NORMAL, 0, 0, 0, 1 }
  };
  static const Node read_transfer[] = {
    { NODE_C0_READ, 0, 0, 0, 3 },
    { NODE_TRANSFER, 0, 0, 0, 2 },
    { NODE_NORMAL, 0, 0, 0, 1 }
  };
  static const Node zero_nodes[] = {
    { NODE_EMPTY, 0, 0, 0, 0 },
    { NODE_EMPTY, 0, 0, 0, 0 }
  };
  static const Node one_normal[] = {
    { NODE_NORMAL, 0, 0, 0, 1 }
  };
  struct {
    const char *name;
    const Node *nodes;
    size_t count;
    int cmd_len;
    int dma_h;
    int frameskip_active;
    int expect_fast;
  } cases[] = {
    { "ordinary", ordinary, sizeof(ordinary) / sizeof(ordinary[0]), 0, 0, 0, 1 },
    { "e3-change-back", e3_back, sizeof(e3_back) / sizeof(e3_back[0]), 0, 0, 0, 1 },
    { "incomplete-fallback", incomplete, sizeof(incomplete) / sizeof(incomplete[0]), 0, 0, 0, 1 },
    { "a0-write-fallback", write_transfer, sizeof(write_transfer) / sizeof(write_transfer[0]), 0, 0, 0, 0 },
    { "c0-read-fallback", read_transfer, sizeof(read_transfer) / sizeof(read_transfer[0]), 0, 0, 0, 0 },
    { "zero-nodes", zero_nodes, sizeof(zero_nodes) / sizeof(zero_nodes[0]), 0, 0, 0, 0 },
    { "pending-command-tail", one_normal, 1, 1, 0, 0, 0 },
    { "frameskip-active", one_normal, 1, 0, 0, 1, 0 },
    { "transfer-active", one_normal, 1, 0, 2, 0, 0 }
  };
  size_t i;
  int failures = 0;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    State s = initial_state();
    s.cmd_len = cases[i].cmd_len;
    s.dma_h = cases[i].dma_h;
    s.frameskip_active = cases[i].frameskip_active;
    failures += compare_case(cases[i].name, s, cases[i].nodes,
                             cases[i].count, cases[i].expect_fast);
  }

  /* The production walker changes loop-marker behaviour at 8K entries.  Keep
   * the legacy and deferred orchestration models compared on both sides of
   * that boundary. */
  {
    static Node boundary_nodes[8193];
    const size_t boundary_counts[] = { 8191, 8192, 8193 };
    size_t b, n;
    for (n = 0; n < sizeof(boundary_nodes) / sizeof(boundary_nodes[0]); ++n)
      boundary_nodes[n] = (Node){ NODE_NORMAL, 0, 0, 0, 1 };
    for (b = 0; b < sizeof(boundary_counts) / sizeof(boundary_counts[0]); ++b)
      failures += compare_case("loop-marker-boundary", initial_state(),
                               boundary_nodes, boundary_counts[b], 1);
  }

  /* Verify the exact unsigned edge behaviour independently at both display
   * boundaries, including the interlace override. */
  {
    const uint32_t e3_values[] = {
      (20U << 10) | 10U, (20U << 10) | 109U,
      (20U << 10) | 110U, (19U << 10) | 10U,
      (100U << 10) | 10U, 0xffffffffU
    };
    size_t n;
    for (n = 0; n < sizeof(e3_values) / sizeof(e3_values[0]); ++n) {
      uint32_t ref = independent_allow(0, e3_values[n], 10, 20, 100, 80);
      uint32_t got = qpsx_gpu_frameskip_allow_value(
        0, e3_values[n], 10, 20, 100, 80);
      if (ref != got) {
        fprintf(stderr, "allow boundary %zu: expected %u, got %u\n",
                n, ref, got);
        failures++;
      }
    }
    if (!qpsx_gpu_frameskip_allow_value(1, (20U << 10) | 10U,
                                        10, 20, 100, 80)) {
      fprintf(stderr, "allow interlace override failed\n");
      failures++;
    }
  }

  if (check_previous_chain_gate()) {
    fprintf(stderr, "previous-chain predictor gate boundary failure\n");
    failures++;
  }

  if (failures)
    return 1;
  puts("gpu_dma_chain_fast_diff: PASS (state, fallback, transfer, boundary, and previous-chain gate cases)");
  return 0;
}

#include "qpsx_phase_metrics.h"

#if QPSX_PHASE_METRICS

#include <stdint.h>
#include <stdio.h>

#ifdef SF2000
extern void xlog(const char *fmt, ...);
#define QPSX_PHASE_LOG xlog
#else
#define QPSX_PHASE_LOG printf
#endif

static uint64_t gpu_words;
static uint64_t spu_cycles;
static uint32_t gpu_calls;
static uint32_t spu_calls;
static uint32_t cd_calls;
static uint32_t rec_calls;
static uint32_t dma_chains;
static uint32_t dma_nodes;
static uint32_t dma_nonempty;
static uint64_t dma_words;
static uint64_t dma_work;
static uint32_t dma_eligible;
static uint32_t dma_batched;
static uint32_t dma_fallback;
static uint32_t dma_hist[5];
static uint32_t dma_nonempty_hist[5];
static uint32_t dma_work_hist[5];

void qpsx_phase_metrics_frame(unsigned frame)
{
    /* The hook is placed immediately after the exact frame marker.  Thus the
     * values belong to the preceding frame, while the frame number is the
     * same one visible in the frontend/QEMU logs. */
    if (!frame || (frame % 300) != 0) {
        return;
    }
    QPSX_PHASE_LOG("QPSX: phase frame=%u gpu_calls=%u gpu_words=%llu "
           "spu_calls=%u spu_cycles=%llu cd_irqs=%u rec_dispatches=%u\n",
                   frame, gpu_calls, (unsigned long long)gpu_words,
                   spu_calls, (unsigned long long)spu_cycles,
                   cd_calls, rec_calls);
    QPSX_PHASE_LOG("QPSX: phase_dma frame=%u chains=%u nodes=%u nonempty=%u words=%llu "
           "work=%llu eligible=%u batched=%u fallback=%u "
           "hist=%u,%u,%u,%u,%u nonempty_hist=%u,%u,%u,%u,%u "
           "work_hist=%u,%u,%u,%u,%u\n",
           frame, dma_chains, dma_nodes, dma_nonempty,
           (unsigned long long)dma_words,
           (unsigned long long)dma_work, dma_eligible, dma_batched,
           dma_fallback, dma_hist[0], dma_hist[1], dma_hist[2], dma_hist[3],
           dma_hist[4], dma_nonempty_hist[0], dma_nonempty_hist[1],
           dma_nonempty_hist[2], dma_nonempty_hist[3], dma_nonempty_hist[4],
           dma_work_hist[0], dma_work_hist[1], dma_work_hist[2],
           dma_work_hist[3], dma_work_hist[4]);
    gpu_words = 0;
    spu_cycles = 0;
    gpu_calls = 0;
    spu_calls = 0;
    cd_calls = 0;
    rec_calls = 0;
    dma_chains = 0;
    dma_nodes = 0;
    dma_nonempty = 0;
    dma_words = 0;
    dma_work = 0;
    dma_eligible = 0;
    dma_batched = 0;
    dma_fallback = 0;
    dma_hist[0] = dma_hist[1] = dma_hist[2] = dma_hist[3] = dma_hist[4] = 0;
    dma_nonempty_hist[0] = dma_nonempty_hist[1] = dma_nonempty_hist[2] =
        dma_nonempty_hist[3] = dma_nonempty_hist[4] = 0;
    dma_work_hist[0] = dma_work_hist[1] = dma_work_hist[2] =
        dma_work_hist[3] = dma_work_hist[4] = 0;
}

void qpsx_phase_metrics_gpu(unsigned words)
{
    gpu_calls++;
    gpu_words += words;
}

void qpsx_phase_metrics_dma(unsigned nodes, unsigned nonempty,
                            unsigned words, unsigned work, unsigned eligible,
                            unsigned batched, unsigned fallback)
{
    unsigned bucket, work_bucket;

    dma_chains++;
    dma_nodes += nodes;
    dma_nonempty += nonempty;
    dma_words += words;
    dma_work += work;
    dma_eligible += eligible;
    dma_batched += batched;
    dma_fallback += fallback;
    if (nodes == 0)
        bucket = 0;
    else if (nodes == 1)
        bucket = 1;
    else if (nodes <= 4)
        bucket = 2;
    else if (nodes <= 8)
        bucket = 3;
    else
        bucket = 4;
    dma_hist[bucket]++;

    if (nonempty == 0)
        bucket = 0;
    else if (nonempty == 1)
        bucket = 1;
    else if (nonempty <= 4)
        bucket = 2;
    else if (nonempty <= 8)
        bucket = 3;
    else
        bucket = 4;
    dma_nonempty_hist[bucket]++;

    if (work < 2048)
        work_bucket = 0;
    else if (work < 4096)
        work_bucket = 1;
    else if (work < 6144)
        work_bucket = 2;
    else if (work < 8192)
        work_bucket = 3;
    else
        work_bucket = 4;
    dma_work_hist[work_bucket]++;
}

void qpsx_phase_metrics_spu(unsigned cycles)
{
    spu_calls++;
    spu_cycles += cycles;
}

void qpsx_phase_metrics_cd(void)
{
    cd_calls++;
}

void qpsx_phase_metrics_rec(void)
{
    rec_calls++;
}

#else

/* Keep the translation unit non-empty for toolchains that warn on an empty
 * archive member; all call sites are compiled out by the header. */
const int qpsx_phase_metrics_disabled = 1;

#endif

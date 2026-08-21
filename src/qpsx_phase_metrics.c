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
    gpu_words = 0;
    spu_cycles = 0;
    gpu_calls = 0;
    spu_calls = 0;
    cd_calls = 0;
    rec_calls = 0;
}

void qpsx_phase_metrics_gpu(unsigned words)
{
    gpu_calls++;
    gpu_words += words;
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

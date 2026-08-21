/* Optional coarse phase counters for physical-device experiments.  The
 * normal core expands every macro to nothing, so production builds pay no
 * call or data cost. */
#ifndef QPSX_PHASE_METRICS_H
#define QPSX_PHASE_METRICS_H

#ifndef QPSX_PHASE_METRICS
#define QPSX_PHASE_METRICS 0
#endif

#if QPSX_PHASE_METRICS
#ifdef __cplusplus
extern "C" {
#endif
void qpsx_phase_metrics_frame(unsigned frame);
void qpsx_phase_metrics_gpu(unsigned words);
void qpsx_phase_metrics_spu(unsigned cycles);
void qpsx_phase_metrics_cd(void);
void qpsx_phase_metrics_rec(void);
#ifdef __cplusplus
}
#endif

#define QPSX_PHASE_FRAME(frame) qpsx_phase_metrics_frame((unsigned)(frame))
#define QPSX_PHASE_GPU(words) qpsx_phase_metrics_gpu((unsigned)(words))
#define QPSX_PHASE_SPU(cycles) qpsx_phase_metrics_spu((unsigned)(cycles))
#define QPSX_PHASE_CD() qpsx_phase_metrics_cd()
#define QPSX_PHASE_REC() qpsx_phase_metrics_rec()
#else
#define QPSX_PHASE_FRAME(frame) do { (void)(frame); } while (0)
#define QPSX_PHASE_GPU(words) do { (void)(words); } while (0)
#define QPSX_PHASE_SPU(cycles) do { (void)(cycles); } while (0)
#define QPSX_PHASE_CD() do { } while (0)
#define QPSX_PHASE_REC() do { } while (0)
#endif

#endif

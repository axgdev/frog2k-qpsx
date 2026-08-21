/* Build-time switches shared by the small number of translation units that
 * form the SF2000 hot path.  Keep this header dependency-free: the qpsx fork
 * is also built directly by Makefile.libretro, outside the frontend tree. */
#ifndef QPSX_BUILD_CONFIG_H
#define QPSX_BUILD_CONFIG_H

#ifndef QPSX_HOT_LAYOUT
#define QPSX_HOT_LAYOUT 0
#endif

/* GCC's hot attribute puts a function in .text.hot and applies the normal
 * speed-oriented scheduling to it.  The first experiment marked every entry
 * point, but that moved enough ordinary frontend text to increase conflicts in
 * the 16 KiB I-cache.  Keep the knob a small bit mask so we can isolate the
 * recompiler (bit 1), GPU (bit 2), SPU (bit 4), and CD (bit 8) working sets.
 * Layout 1 retains the all-hot experiment for reproducibility. */
#if defined(__GNUC__)
#define QPSX_HOT_ATTRIBUTE __attribute__((hot))
#else
#define QPSX_HOT_ATTRIBUTE
#endif

#if QPSX_HOT_LAYOUT == 1
#define QPSX_HOT_ALL QPSX_HOT_ATTRIBUTE
#define QPSX_HOT_REC QPSX_HOT_ATTRIBUTE
#define QPSX_HOT_GPU QPSX_HOT_ATTRIBUTE
#define QPSX_HOT_SPU QPSX_HOT_ATTRIBUTE
#define QPSX_HOT_CD QPSX_HOT_ATTRIBUTE
#else
#define QPSX_HOT_ALL
#if (QPSX_HOT_LAYOUT & 2)
#define QPSX_HOT_REC QPSX_HOT_ATTRIBUTE
#else
#define QPSX_HOT_REC
#endif
#if (QPSX_HOT_LAYOUT & 4)
#define QPSX_HOT_GPU QPSX_HOT_ATTRIBUTE
#else
#define QPSX_HOT_GPU
#endif
#if (QPSX_HOT_LAYOUT & 8)
#define QPSX_HOT_SPU QPSX_HOT_ATTRIBUTE
#else
#define QPSX_HOT_SPU
#endif
#if (QPSX_HOT_LAYOUT & 16)
#define QPSX_HOT_CD QPSX_HOT_ATTRIBUTE
#else
#define QPSX_HOT_CD
#endif
#endif

/* Preserve the original source spelling for out-of-tree users. */
#define QPSX_HOT_FUNCTION QPSX_HOT_ALL

#endif

/*
 * PCSX4ALL libretro port implementation
 * QPSX_295 - Dual FPS counter (retro_run calls + real GPU frames)
 *
 * CRITICAL: This file does NOT call any SF2000 input functions!
 * All input is read from the cache set by update_input_cache() in libretro-core.cpp.
 */

#include "port.h"
#include "libretro.h"
#include <string.h>
#include <stdio.h>

/*
 * Debug logging - uses the file logging system from libretro-core.cpp
 * When debug_log is enabled, writes to /mnt/sda1/log.txt
 */
extern "C" void port_debug_log(const char *fmt, ...);

/* Debug counter */
static int port_debug_counter = 0;
#define PORT_DEBUG_INTERVAL 60  /* Log every 60 frames */

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

static uint16_t static_screen_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

unsigned short *SCREEN = static_screen_buffer;

extern retro_video_refresh_t video_cb;
extern retro_audio_sample_batch_t audio_batch_cb;

extern volatile int skip_video_output;

/* v295: GPU frame counter - counts ACTUAL rendered frames (not skipped/duped) */
volatile int gpu_frame_count = 0;

/* QPSX half-res: line-skip mask applied by vout_update() and the height of
 * the compacted output rows it produced. video_flip() presents only the
 * compacted rows; the host upscaler restores full screen size. */
volatile int gpu_out_ilace_mask = 0;
volatile int gpu_out_height = SCREEN_HEIGHT;

static unsigned tick_counter = 0;

unsigned get_ticks(void) { return tick_counter++; }
void wait_ticks(unsigned s) { (void)s; }

static uint16_t libretro_pad1 = 0xFFFF;
static uint16_t libretro_pad2 = 0xFFFF;

extern "C" void set_pad_state(int num, uint16_t state)
{
    if (num == 0) libretro_pad1 = state;
    else libretro_pad2 = state;
}

/* Import the cached input getter from libretro-core.cpp */
extern "C" uint16_t get_cached_pad(int num);

void pad_update(void)
{
    /*
     * QPSX_073 DEBUG: This is called by EmuUpdate() at VBlank.
     * We copy the cached input (from retro_run) to local state.
     * The game then reads it via pad_read().
     *
     * DO NOT call any SF2000 input functions here!
     */
    uint16_t old_pad1 = libretro_pad1;

    libretro_pad1 = get_cached_pad(0);
    libretro_pad2 = get_cached_pad(1);

    port_debug_counter++;

    /* DEBUG: Log when buttons are pressed during pad_update (only when debug_log enabled) */
    if (libretro_pad1 != 0xFFFF && (port_debug_counter % PORT_DEBUG_INTERVAL) == 0) {
        port_debug_log("[PAD-UPDATE] pad1=0x%04X old=0x%04X (VBlank copy)", libretro_pad1, old_pad1);
    }
}

unsigned short pad_read(int num)
{
    uint16_t val = (num == 0) ? libretro_pad1 : libretro_pad2;

    /* DEBUG: Log when game actually reads controller with buttons pressed (only when debug_log enabled) */
    if (val != 0xFFFF && (port_debug_counter % PORT_DEBUG_INTERVAL) == 0) {
        port_debug_log("[PAD-READ] num=%d val=0x%04X (game SIO read)", num, val);
    }

    return val;
}

void video_flip(void)
{
    if (!video_cb) return;

    /*
     * v108: Proper libretro frame duping
     * When skip_video_output is set, call video_cb(NULL) to tell frontend
     * to reuse previous frame. This is faster than sending same data again.
     * IMPORTANT: We must still call video_cb for profiler timing to work!
     */
    int out_height = gpu_out_height;
    if (out_height <= 0 || out_height > SCREEN_HEIGHT)
        out_height = SCREEN_HEIGHT;
    if (skip_video_output) {
        video_cb(NULL, SCREEN_WIDTH, out_height, SCREEN_WIDTH * 2);
    } else if (SCREEN) {
        /* v295: Count ACTUAL rendered frames (not skipped) */
        gpu_frame_count++;
        video_cb(SCREEN, SCREEN_WIDTH, out_height, SCREEN_WIDTH * 2);
    }
}

void video_clear(void) { memset(SCREEN, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 2); }
void sound_set(int frequency) { (void)frequency; }
void sound_close(void) { }
void sound_send(void) { }

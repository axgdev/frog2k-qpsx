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
extern retro_environment_t environ_cb;

extern volatile int skip_video_output;

/* Linux's static frontend exports this optional zero-copy presenter.  The
 * weak reference keeps the ordinary libretro port usable on UniFrog and on
 * host builds where no GE presenter exists. */
extern "C" void sf2000_video_vram(const void *data, unsigned width,
                                  unsigned height, size_t pitch)
    __attribute__((weak));

/* v295: GPU frame counter - counts ACTUAL rendered frames (not skipped/duped) */
volatile int gpu_frame_count = 0;

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

unsigned short *video_acquire_framebuffer(void)
{
    struct retro_framebuffer framebuffer;

    if (!environ_cb)
        return SCREEN;

    memset(&framebuffer, 0, sizeof(framebuffer));
    framebuffer.width = SCREEN_WIDTH;
    framebuffer.height = SCREEN_HEIGHT;
    framebuffer.access_flags = RETRO_MEMORY_ACCESS_WRITE;
    if (!environ_cb(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER,
                    &framebuffer) ||
        !framebuffer.data ||
        framebuffer.format != RETRO_PIXEL_FORMAT_RGB565 ||
        framebuffer.pitch != SCREEN_WIDTH * sizeof(uint16_t))
        return SCREEN;

    return (unsigned short *)framebuffer.data;
}

void video_flip_framebuffer(const unsigned short *buffer)
{
    if (!video_cb) return;

    /*
     * v108: Proper libretro frame duping
     * When skip_video_output is set, call video_cb(NULL) to tell frontend
     * to reuse previous frame. This is faster than sending same data again.
     * IMPORTANT: We must still call video_cb for profiler timing to work!
     */
    if (skip_video_output) {
        video_cb(NULL, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH * 2);
    } else if (buffer) {
        /* v295: Count ACTUAL rendered frames (not skipped) */
        gpu_frame_count++;
        video_cb(buffer, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH * 2);
    }
}

int video_flip_vram(const unsigned short *buffer, unsigned width,
                    unsigned height, size_t pitch)
{
    if (!sf2000_video_vram || !buffer || !width || !height)
        return 0;
    if (skip_video_output) {
        /* Keep libretro's frame-duplication accounting and pacing semantics;
         * the frontend has no source surface to submit for this frame. */
        if (video_cb)
            video_cb(NULL, width, height, pitch);
        return 1;
    }
    gpu_frame_count++;
    sf2000_video_vram(buffer, width, height, pitch);
    return 1;
}

void video_flip(void)
{
    video_flip_framebuffer(SCREEN);
}

void video_clear(void) { memset(SCREEN, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 2); }
void sound_set(int frequency) { (void)frequency; }
void sound_close(void) { }
void sound_send(void) { }

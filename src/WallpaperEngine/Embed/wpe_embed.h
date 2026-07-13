#pragma once

/**
 * Embedding API for linux-wallpaperengine.
 *
 * Lets a host application render Wallpaper Engine backgrounds into its own
 * OpenGL context / framebuffer, in the style of libmpv's render API:
 * the host owns the GL context, the frame clock and the target FBO;
 * the engine only draws when asked to.
 *
 * Threading/context rules:
 *  - wpe_context_create, wpe_context_render and wpe_context_destroy must be
 *    called with the host's GL context current on the calling thread.
 *  - All calls for one wpe_context must come from the same thread.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wpe_context wpe_context;

/** Resolves an OpenGL function by name (same contract as libmpv). */
typedef void* (*wpe_get_proc_address_fn) (void* userdata, const char* name);

typedef struct wpe_init_params {
    /** Path to the Wallpaper Engine "assets" directory. NULL/empty auto-detects a Steam install. */
    const char* assets_dir;
    /** Path to the background directory (contains project.json) or a workshop id. Required. */
    const char* background;
    /** Initial framebuffer size in pixels. */
    int width;
    int height;
    /** Flip the final blit vertically (depends on how the host presents the FBO). */
    int vflip;
    /** Disable mouse-driven interaction/parallax. */
    int disable_mouse;
    int disable_parallax;
    /** Disable sound playback entirely. */
    int disable_audio;
    /** Disable the audio-visualizer capture (PulseAudio + FFT). */
    int disable_audio_processing;
    /** Initial volume, 0-128. */
    int volume;
    /** Viewport scaling mode: "stretch", "fit", "fill" or NULL for the scene default. */
    const char* scaling;
    /** Optional NULL-terminated array of "name=value" wallpaper property overrides. */
    const char* const* properties;
} wpe_init_params;

/**
 * Creates an engine instance and loads the background.
 * The host GL context must be current. Returns NULL on failure; if error_out
 * is non-NULL it receives a malloc'd error message the caller must free().
 */
wpe_context* wpe_context_create (
    const wpe_init_params* params, wpe_get_proc_address_fn get_proc_address, void* userdata, char** error_out
);

/**
 * Renders one frame into the given framebuffer object.
 * time_seconds is the host's monotonic clock; the engine derives the scene
 * clock from consecutive values (a paused context does not advance it, so
 * re-rendering while paused yields a still frame).
 */
void wpe_context_render (wpe_context* ctx, unsigned int framebuffer, int width, int height, double time_seconds);

/** Freezes/unfreezes the scene clock. Rendering while paused repeats the frame. */
void wpe_context_set_paused (wpe_context* ctx, int paused);

/** Sets playback volume, 0-128. */
void wpe_context_set_volume (wpe_context* ctx, int volume);

/** Enables/disables audio playback at runtime. */
void wpe_context_set_audio_enabled (wpe_context* ctx, int enabled);

/**
 * Feeds the pointer position in window coordinates (origin top-left, pixels)
 * plus button state (non-zero = pressed).
 */
void wpe_context_set_mouse (wpe_context* ctx, double x, double y, int left_pressed, int right_pressed);

/** Destroys the instance and frees all GL resources. The GL context must be current. */
void wpe_context_destroy (wpe_context* ctx);

#ifdef __cplusplus
}
#endif

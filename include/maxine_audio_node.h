// Generic audio effect node using PipeWire's pw_filter API
// Each node wraps a single Maxine AFX effect with input/output ports.

#pragma once

#include <pipewire/pipewire.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

struct maxine_sdk;  // forward decl from maxine_loader.h

struct maxine_audio_node_config {
    const char *effect_id;        // short name, e.g. "denoiser"
    const char *effect_selector;  // SDK selector, e.g. NVAFX_EFFECT_DENOISER
    const char *model_subdir;     // subdirectory under features/, e.g. "denoiser"
    const char *model_path;       // base path to features/ directory
    const char *sdk_lib_path;     // path to SDK libs (or NULL for default)
    uint32_t sample_rate;         // 48000 or 16000
    float intensity;              // 0.0-1.0
    bool enable_vad;
};

struct maxine_audio_node {
    struct pw_filter *filter;
    struct maxine_sdk *sdk;       // shared SDK handle (not owned)
    void *fx_handle;              // NvAFX_Handle

    struct maxine_audio_node_config config;
    char effect_id[64];           // owned copy
    char effect_selector[128];
    char model_subdir[128];
    char model_path[4096];
    atomic_bool enabled;          // written from main thread, read from RT thread
    uint32_t frame_size;          // samples per SDK frame

    // Internal processing buffers
    float *in_buf;
    float *out_buf;
    uint32_t buf_pos;             // current position in accumulation buffer

    // Port data
    void *input_port;
    void *output_port;

    // Stats
    uint64_t frames_processed;
    double avg_process_time_us;
};

// Create a new audio effect node. The node creates a pw_filter with input/output ports.
// sdk must be already loaded. Returns NULL on failure.
struct maxine_audio_node *maxine_audio_node_create(
    struct pw_loop *loop,
    struct maxine_sdk *sdk,
    const struct maxine_audio_node_config *config);

// Destroy node and release all resources.
void maxine_audio_node_destroy(struct maxine_audio_node *node);

// Enable or disable processing (disabled = passthrough).
void maxine_audio_node_set_enabled(struct maxine_audio_node *node, bool enabled);

// Set effect intensity (0.0-1.0). Takes effect immediately.
void maxine_audio_node_set_intensity(struct maxine_audio_node *node, float intensity);

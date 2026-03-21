// Effect registry — metadata for every available NVIDIA Maxine effect.
// Provides lookup by short ID and SDK effect selector string.

#pragma once

#include <stdbool.h>
#include <stdint.h>

enum maxine_effect_type {
    MAXINE_EFFECT_AFX,   // Audio effect
    MAXINE_EFFECT_VFX,   // Video effect (future)
};

struct maxine_effect_info {
    const char *id;              // short name for CLI, e.g. "denoiser"
    const char *effect_selector; // SDK selector string, e.g. "denoiser"
    const char *display_name;    // human readable, e.g. "Noise Removal"
    const char *description;     // one-line description for --help
    enum maxine_effect_type type;
    bool supports_48k;
    bool supports_16k;
    bool supports_8k;
    bool has_intensity;          // supports 0.0-1.0 intensity
    bool has_vad;                // supports voice activity detection
    bool is_chained;             // uses CreateChainedEffect
    bool needs_reference_audio;  // voice font needs reference wav
    uint32_t input_channels;     // typically 1 (mono), 2 for AEC (near+far)
    uint32_t output_channels;    // typically 1
    const char *model_subdir;    // e.g. "denoiser", "aec"
    const char *category;        // "noise", "echo", "enhancement", "voice"
};

// Get info for a specific effect by id (e.g. "denoiser"). Returns NULL if not found.
const struct maxine_effect_info *maxine_effect_find(const char *id);

// Get the full list of all effects. Returns count.
int maxine_effect_list_all(const struct maxine_effect_info **out_list);

// Get info by SDK effect selector string (e.g. NVAFX_EFFECT_DENOISER).
const struct maxine_effect_info *maxine_effect_find_by_selector(const char *selector);

// NVIDIA Maxine AFX SDK type definitions
// Extracted from nvAudioEffects.h for dynamic loading (dlopen)
// This avoids requiring the SDK headers at compile time.

#pragma once

#include <stdint.h>

typedef void *NvAFX_Handle;
typedef const char *NvAFX_EffectSelector;
typedef const char *NvAFX_ParameterSelector;
typedef char NvAFX_Bool;

#define NVAFX_TRUE  1
#define NVAFX_FALSE 0

// Effect selectors
#define NVAFX_EFFECT_DENOISER           "denoiser"
#define NVAFX_EFFECT_DEREVERB           "dereverb"
#define NVAFX_EFFECT_DEREVERB_DENOISER  "dereverb_denoiser"
#define NVAFX_EFFECT_AEC                "aec"
#define NVAFX_EFFECT_SUPERRES           "superres"
#define NVAFX_EFFECT_STUDIO_VOICE_HQ    "studio_voice_high_quality"
#define NVAFX_EFFECT_STUDIO_VOICE_LL    "studio_voice_low_latency"
#define NVAFX_EFFECT_SPEAKER_FOCUS      "speaker_focus"
#define NVAFX_EFFECT_VOICE_FONT_HIGH_QUALITY "voice_font_high_quality"
#define NVAFX_EFFECT_VOICE_FONT_LOW_LATENCY  "voice_font_low_latency"

// Chained effect selectors
#define NVAFX_CHAINED_EFFECT_DENOISER_16k_SUPERRES_16k_TO_48k \
    "denoiser16k_superres16kto48k"
#define NVAFX_CHAINED_EFFECT_DEREVERB_16k_SUPERRES_16k_TO_48k \
    "dereverb16k_superres16kto48k"
#define NVAFX_CHAINED_EFFECT_DEREVERB_DENOISER_16k_SUPERRES_16k_TO_48k \
    "dereverb_denoiser16k_superres16kto48k"
#define NVAFX_CHAINED_EFFECT_SUPERRES_8k_TO_16k_DENOISER_16k \
    "superres8kto16k_denoiser16k"
#define NVAFX_CHAINED_EFFECT_SUPERRES_8k_TO_16k_DEREVERB_16k \
    "superres8kto16k_dereverb16k"
#define NVAFX_CHAINED_EFFECT_SUPERRES_8k_TO_16k_DEREVERB_DENOISER_16k \
    "superres8kto16k_dereverb_denoiser16k"
#define NVAFX_CHAINED_EFFECT_DENOISER_16k_SPEAKER_FOCUS_16k \
    "denoiser16k_speaker_focus16k"
#define NVAFX_CHAINED_EFFECT_DENOISER_48k_SPEAKER_FOCUS_48k \
    "denoiser48k_speaker_focus48k"

// Parameter selectors
#define NVAFX_PARAM_MODEL_PATH                     "model_path"
#define NVAFX_PARAM_INPUT_SAMPLE_RATE              "input_sample_rate"
#define NVAFX_PARAM_OUTPUT_SAMPLE_RATE             "output_sample_rate"
#define NVAFX_PARAM_NUM_INPUT_CHANNELS             "num_input_channels"
#define NVAFX_PARAM_NUM_OUTPUT_CHANNELS             "num_output_channels"
#define NVAFX_PARAM_NUM_STREAMS                    "num_streams"
#define NVAFX_PARAM_INTENSITY_RATIO                "intensity_ratio"
#define NVAFX_PARAM_ENABLE_VAD                     "enable_vad"
#define NVAFX_PARAM_NUM_SAMPLES_PER_INPUT_FRAME    "num_samples_per_input_frame"
#define NVAFX_PARAM_NUM_SAMPLES_PER_OUTPUT_FRAME   "num_samples_per_output_frame"
#define NVAFX_PARAM_SUPPORTED_NUM_SAMPLES_PER_FRAME "supported_num_samples_per_frame"
#define NVAFX_PARAM_USE_DEFAULT_GPU                "use_default_gpu"
#define NVAFX_PARAM_REFERENCE_AUDIO                "reference_audio"
#define NVAFX_PARAM_ACTIVE_STREAMS                 "active_streams"
#define NVAFX_PARAM_CHAINED_EFFECT_GPU_LIST        "chained_effect_gpu_list"
#define NVAFX_PARAM_EFFECT_VERSION                 "effect_version"
#define NVAFX_PARAM_DISABLE_CUDA_GRAPH             "disable_cuda_graph"

// Status codes
typedef enum {
    NVAFX_STATUS_SUCCESS                     = 0,
    NVAFX_STATUS_FAILED                      = 1,
    NVAFX_STATUS_INVALID_HANDLE              = 2,
    NVAFX_STATUS_INVALID_PARAM               = 3,
    NVAFX_STATUS_IMMUTABLE_PARAM             = 4,
    NVAFX_STATUS_INSUFFICIENT_DATA           = 5,
    NVAFX_STATUS_EFFECT_NOT_AVAILABLE        = 6,
    NVAFX_STATUS_OUTPUT_BUFFER_TOO_SMALL     = 7,
    NVAFX_STATUS_MODEL_LOAD_FAILED           = 8,
    NVAFX_STATUS_MODEL_NOT_LOADED            = 9,
    NVAFX_STATUS_INCOMPATIBLE_MODEL          = 10,
    NVAFX_STATUS_GPU_UNSUPPORTED             = 11,
    NVAFX_STATUS_CUDA_CONTEXT_CREATION_FAILED = 12,
    NVAFX_STATUS_NO_SUPPORTED_GPU_FOUND      = 13,
    NVAFX_STATUS_WRONG_GPU                   = 14,
    NVAFX_STATUS_CUDA_ERROR                  = 15,
    NVAFX_STATUS_INVALID_OPERATION           = 16,
} NvAFX_Status;

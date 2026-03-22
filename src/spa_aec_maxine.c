// PipeWire SPA AEC Plugin using NVIDIA Maxine AFX SDK
// This implements the spa_audio_aec interface so PipeWire's
// libpipewire-module-echo-cancel can use Maxine for echo cancellation.
//
// Install: copy built .so to /usr/lib/spa-0.2/aec/
// Config:  library.name = "aec/libspa-aec-maxine"

#include <spa/interfaces/audio/aec.h>
#include <spa/support/log.h>
#include <spa/support/plugin.h>
#include <spa/utils/dict.h>
#include <spa/utils/hook.h>
#include <spa/utils/string.h>
#include <spa/utils/type.h>

#include <stdlib.h>
#include <string.h>

#include "maxine_loader.h"

#define DEFAULT_MODEL_PATH "/opt/maxine-afx/features"
#define DEFAULT_INTENSITY  1.0f
#define MAX_CHANNELS       2

struct impl {
    struct spa_handle handle;
    struct spa_audio_aec aec;
    struct spa_hook_list hooks;
    struct spa_log *log;

    struct maxine_sdk sdk;
    NvAFX_Handle fx_aec;
    NvAFX_Handle fx_denoise;

    char model_path[4096];
    char sdk_lib_path[4096];
    float intensity;
    bool use_denoise;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t frame_size;  // samples per frame (from SDK after Load)

    // Intermediate buffers for mono processing
    float *mono_rec;
    float *mono_play;
    float *mono_out;
    float *mono_denoised;
};

#define spa_log_trace_maxine(impl, ...) spa_log_trace((impl)->log, "maxine: " __VA_ARGS__)
#define spa_log_info_maxine(impl, ...)  spa_log_info((impl)->log, "maxine: " __VA_ARGS__)
#define spa_log_warn_maxine(impl, ...)  spa_log_warn((impl)->log, "maxine: " __VA_ARGS__)
#define spa_log_error_maxine(impl, ...) spa_log_error((impl)->log, "maxine: " __VA_ARGS__)

static int impl_add_listener(void *object, struct spa_hook *listener,
                             const struct spa_audio_aec_events *events, void *data)
{
    struct impl *impl = object;
    spa_hook_list_append(&impl->hooks, listener, events, data);
    return 0;
}

static void free_buffers(struct impl *impl)
{
    free(impl->mono_rec);
    free(impl->mono_play);
    free(impl->mono_out);
    free(impl->mono_denoised);
    impl->mono_rec = NULL;
    impl->mono_play = NULL;
    impl->mono_out = NULL;
    impl->mono_denoised = NULL;
}

static bool alloc_buffers(struct impl *impl, uint32_t frame_size)
{
    free_buffers(impl);
    impl->mono_rec      = calloc(frame_size, sizeof(float));
    impl->mono_play     = calloc(frame_size, sizeof(float));
    impl->mono_out      = calloc(frame_size, sizeof(float));
    impl->mono_denoised = calloc(frame_size, sizeof(float));
    return impl->mono_rec && impl->mono_play && impl->mono_out && impl->mono_denoised;
}

static int load_effect(struct impl *impl, NvAFX_Handle *handle,
                       NvAFX_EffectSelector effect, const char *model_subdir)
{
    NvAFX_Status st;
    char model[8192];

    st = impl->sdk.CreateEffect(effect, handle);
    if (st != NVAFX_STATUS_SUCCESS) {
        spa_log_error_maxine(impl, "CreateEffect(%s) failed: %s", effect, nvafx_status_str(st));
        *handle = NULL;
        return -1;
    }

    {
        const char *rate_suffix = (impl->sample_rate == 48000) ? "48k" : "16k";
        snprintf(model, sizeof(model), "%s/%s/models/sm_89/%s_%s.trtpkg",
                 impl->model_path, model_subdir, model_subdir, rate_suffix);
    }
    if (impl->sdk.SetStringList) {
        const char *model_list[1] = { model };
        st = impl->sdk.SetStringList(*handle, NVAFX_PARAM_MODEL_PATH, model_list, 1);
    } else {
        st = impl->sdk.SetString(*handle, NVAFX_PARAM_MODEL_PATH, model);
    }
    if (st != NVAFX_STATUS_SUCCESS) {
        spa_log_error_maxine(impl, "SetModel(%s) failed: %s", model, nvafx_status_str(st));
        goto fail;
    }

    st = impl->sdk.SetU32(*handle, NVAFX_PARAM_INPUT_SAMPLE_RATE, impl->sample_rate);
    if (st != NVAFX_STATUS_SUCCESS) {
        spa_log_error_maxine(impl, "SetU32(sample_rate, %u) failed: %s", impl->sample_rate, nvafx_status_str(st));
        goto fail;
    }

    st = impl->sdk.SetU32(*handle, NVAFX_PARAM_NUM_STREAMS, 1);
    if (st != NVAFX_STATUS_SUCCESS)
        spa_log_warn_maxine(impl, "SetU32(num_streams) failed: %s (may be OK)", nvafx_status_str(st));

    st = impl->sdk.SetFloat(*handle, NVAFX_PARAM_INTENSITY_RATIO, impl->intensity);
    if (st != NVAFX_STATUS_SUCCESS)
        spa_log_warn_maxine(impl, "SetFloat(intensity) failed: %s (may be OK)", nvafx_status_str(st));

    spa_log_info_maxine(impl, "loading model from %s ...", model);

    st = impl->sdk.Load(*handle);
    if (st != NVAFX_STATUS_SUCCESS) {
        spa_log_error_maxine(impl, "Load() failed: %s", nvafx_status_str(st));
        goto fail;
    }

    spa_log_info_maxine(impl, "model loaded successfully");
    return 0;

fail:
    impl->sdk.DestroyEffect(*handle);
    *handle = NULL;
    return -1;
}

static int impl_init(void *object, const struct spa_dict *args,
                     const struct spa_audio_info_raw *info)
{
    struct impl *impl = object;
    const char *val;

    impl->sample_rate = info->rate;
    impl->channels = info->channels;

    // Validate channel count
    if (impl->channels == 0 || impl->channels > MAX_CHANNELS) {
        spa_log_error_maxine(impl, "unsupported channel count %u (max %u)",
                             impl->channels, MAX_CHANNELS);
        return -EINVAL;
    }

    // Read config from PipeWire args
    val = spa_dict_lookup(args, "maxine.model-path");
    if (val)
        snprintf(impl->model_path, sizeof(impl->model_path), "%s", val);

    val = spa_dict_lookup(args, "maxine.sdk-path");
    if (val)
        snprintf(impl->sdk_lib_path, sizeof(impl->sdk_lib_path), "%s", val);

    val = spa_dict_lookup(args, "maxine.intensity");
    if (val) {
        float v = (float)atof(val);
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        impl->intensity = v;
    }

    val = spa_dict_lookup(args, "maxine.denoise");
    if (val)
        impl->use_denoise = spa_atob(val);

    spa_log_info_maxine(impl, "init: rate=%u channels=%u intensity=%.2f denoise=%d",
                        impl->sample_rate, impl->channels, impl->intensity, impl->use_denoise);

    // Maxine only supports 16kHz and 48kHz
    if (impl->sample_rate != 16000 && impl->sample_rate != 48000) {
        spa_log_error_maxine(impl, "unsupported sample rate %u (need 16000 or 48000)", impl->sample_rate);
        return -EINVAL;
    }

    // Load SDK
    const char *sdk_path = impl->sdk_lib_path[0] ? impl->sdk_lib_path : NULL;
    if (!maxine_sdk_load(&impl->sdk, sdk_path)) {
        spa_log_error_maxine(impl, "failed to load Maxine SDK");
        return -ENOENT;
    }
    spa_log_info_maxine(impl, "SDK loaded");

    // Load AEC effect
    if (load_effect(impl, &impl->fx_aec, NVAFX_EFFECT_AEC, "aec") < 0) {
        maxine_sdk_unload(&impl->sdk);
        return -EIO;
    }

    // Query frame size from SDK
    unsigned int fs = 0;
    impl->sdk.GetU32(impl->fx_aec, NVAFX_PARAM_NUM_SAMPLES_PER_INPUT_FRAME, &fs);
    impl->frame_size = fs > 0 ? fs : (impl->sample_rate == 48000 ? 480 : 160);
    spa_log_info_maxine(impl, "frame size: %u samples (%.1f ms)",
                        impl->frame_size, (float)impl->frame_size / impl->sample_rate * 1000.0f);

    // Optionally load denoiser on top
    if (impl->use_denoise) {
        if (load_effect(impl, &impl->fx_denoise, NVAFX_EFFECT_DENOISER, "denoiser") < 0) {
            spa_log_warn_maxine(impl, "denoiser failed to load, continuing with AEC only");
            impl->use_denoise = false;
        } else {
            spa_log_info_maxine(impl, "denoiser loaded");
        }
    }

    if (!alloc_buffers(impl, impl->frame_size)) {
        spa_log_error_maxine(impl, "failed to allocate buffers");
        if (impl->fx_denoise) {
            impl->sdk.DestroyEffect(impl->fx_denoise);
            impl->fx_denoise = NULL;
        }
        impl->sdk.DestroyEffect(impl->fx_aec);
        impl->fx_aec = NULL;
        maxine_sdk_unload(&impl->sdk);
        return -ENOMEM;
    }

    return 0;
}

static int impl_run(void *object, const float *rec[], const float *play[],
                    float *out[], uint32_t n_samples)
{
    struct impl *impl = object;
    NvAFX_Status st;
    uint32_t channels = impl->channels;
    uint32_t fs = impl->frame_size;

    if (channels == 0 || channels > MAX_CHANNELS || fs == 0 || !impl->mono_rec)
        return -EINVAL;

    // Process in frame_size chunks
    for (uint32_t offset = 0; offset < n_samples; offset += fs) {
        uint32_t chunk = (n_samples - offset < fs) ? n_samples - offset : fs;

        // Downmix to mono if stereo (Maxine only supports mono)
        if (channels == 1) {
            memcpy(impl->mono_rec, rec[0] + offset, chunk * sizeof(float));
            memcpy(impl->mono_play, play[0] + offset, chunk * sizeof(float));
        } else {
            for (uint32_t i = 0; i < chunk; i++) {
                impl->mono_rec[i] = 0;
                impl->mono_play[i] = 0;
                for (uint32_t ch = 0; ch < channels; ch++) {
                    impl->mono_rec[i] += rec[ch][offset + i];
                    impl->mono_play[i] += play[ch][offset + i];
                }
                impl->mono_rec[i] /= channels;
                impl->mono_play[i] /= channels;
            }
        }

        // Pad with zeros if we have less than frame_size
        if (chunk < fs) {
            memset(impl->mono_rec + chunk, 0, (fs - chunk) * sizeof(float));
            memset(impl->mono_play + chunk, 0, (fs - chunk) * sizeof(float));
        }

        // AEC: input is [near-end (mic), far-end (speaker)] as separate channels
        const float *aec_in[2] = { impl->mono_rec, impl->mono_play };
        float *aec_out[1] = { impl->mono_out };

        st = impl->sdk.Run(impl->fx_aec, aec_in, aec_out, fs, 1);
        if (st != NVAFX_STATUS_SUCCESS) {
            spa_log_warn_maxine(impl, "AEC Run failed: %s", nvafx_status_str(st));
            // Fallback: pass through unprocessed
            memcpy(impl->mono_out, impl->mono_rec, chunk * sizeof(float));
        }

        // Optional denoiser pass
        float *result = impl->mono_out;
        if (impl->use_denoise && impl->fx_denoise) {
            const float *dn_in[1] = { impl->mono_out };
            float *dn_out[1] = { impl->mono_denoised };

            st = impl->sdk.Run(impl->fx_denoise, dn_in, dn_out, fs, 1);
            if (st == NVAFX_STATUS_SUCCESS)
                result = impl->mono_denoised;
        }

        // Copy mono result to all output channels
        for (uint32_t ch = 0; ch < channels; ch++)
            memcpy(out[ch] + offset, result, chunk * sizeof(float));
    }

    return 0;
}

static int impl_set_props(void *object, const struct spa_dict *args)
{
    struct impl *impl = object;
    const char *val;

    val = spa_dict_lookup(args, "maxine.intensity");
    if (val) {
        impl->intensity = atof(val);
        if (impl->fx_aec)
            impl->sdk.SetFloat(impl->fx_aec, NVAFX_PARAM_INTENSITY_RATIO, impl->intensity);
        if (impl->fx_denoise)
            impl->sdk.SetFloat(impl->fx_denoise, NVAFX_PARAM_INTENSITY_RATIO, impl->intensity);
    }

    return 0;
}

static int impl_activate(void *object)  { return 0; }
static int impl_deactivate(void *object) { return 0; }
static int impl_enum_props(void *object, int index, struct spa_pod_builder *builder) { return 0; }
static int impl_get_params(void *object, struct spa_pod_builder *builder) { return 0; }
static int impl_set_params(void *object, const struct spa_pod *args)
{
    (void)object; (void)args;
    return 0;
}

static const struct spa_audio_aec_methods impl_methods = {
    SPA_VERSION_AUDIO_AEC_METHODS,
    .add_listener = impl_add_listener,
    .init = impl_init,
    .run = impl_run,
    .set_props = impl_set_props,
    .activate = impl_activate,
    .deactivate = impl_deactivate,
    .enum_props = impl_enum_props,
    .get_params = impl_get_params,
};

// --- SPA Plugin boilerplate ---

static int impl_get_interface(struct spa_handle *handle, const char *type, void **interface)
{
    struct impl *impl = (struct impl *)handle;

    if (spa_streq(type, SPA_TYPE_INTERFACE_AUDIO_AEC))
        *interface = &impl->aec;
    else
        return -ENOENT;

    return 0;
}

static int impl_clear(struct spa_handle *handle)
{
    struct impl *impl = (struct impl *)handle;

    if (impl->fx_aec) {
        impl->sdk.DestroyEffect(impl->fx_aec);
        impl->fx_aec = NULL;
    }
    if (impl->fx_denoise) {
        impl->sdk.DestroyEffect(impl->fx_denoise);
        impl->fx_denoise = NULL;
    }

    free_buffers(impl);
    maxine_sdk_unload(&impl->sdk);

    return 0;
}

static size_t impl_get_size(const struct spa_handle_factory *factory, const struct spa_dict *params)
{
    return sizeof(struct impl);
}

static int impl_init_factory(const struct spa_handle_factory *factory, struct spa_handle *handle,
                             const struct spa_dict *info, const struct spa_support *support,
                             uint32_t n_support)
{
    struct impl *impl = (struct impl *)handle;

    handle->get_interface = impl_get_interface;
    handle->clear = impl_clear;

    impl->aec.iface = SPA_INTERFACE_INIT(SPA_TYPE_INTERFACE_AUDIO_AEC,
                                          SPA_VERSION_AUDIO_AEC, &impl_methods, impl);
    spa_hook_list_init(&impl->hooks);

    // Find log support
    for (uint32_t i = 0; i < n_support; i++) {
        if (spa_streq(support[i].type, SPA_TYPE_INTERFACE_Log))
            impl->log = support[i].data;
    }

    // Defaults
    snprintf(impl->model_path, sizeof(impl->model_path), "%s", DEFAULT_MODEL_PATH);
    impl->sdk_lib_path[0] = '\0';
    impl->intensity = DEFAULT_INTENSITY;
    impl->use_denoise = true;

    return 0;
}

static const struct spa_interface_info impl_interfaces[] = {
    { SPA_TYPE_INTERFACE_AUDIO_AEC, },
};

static int impl_enum_interface_info(const struct spa_handle_factory *factory,
                                    const struct spa_interface_info **info,
                                    uint32_t *index)
{
    if (*index >= SPA_N_ELEMENTS(impl_interfaces))
        return 0;
    *info = &impl_interfaces[(*index)++];
    return 1;
}

static const struct spa_handle_factory spa_aec_maxine_factory = {
    .version = SPA_VERSION_HANDLE_FACTORY,
    .name = "aec/maxine",
    .info = NULL,
    .get_size = impl_get_size,
    .init = impl_init_factory,
    .enum_interface_info = impl_enum_interface_info,
};

SPA_EXPORT
int spa_handle_factory_enum(const struct spa_handle_factory **factory, uint32_t *index)
{
    if (*index >= 1)
        return 0;
    *factory = &spa_aec_maxine_factory;
    (*index)++;
    return 1;
}

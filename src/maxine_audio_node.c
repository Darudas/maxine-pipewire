#include "maxine_audio_node.h"
#include "maxine_loader.h"
#include "nvafx_types.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// PipeWire filter port data - kept in filter port user_data
struct port_data {
    struct maxine_audio_node *node;
};

static double timespec_diff_us(struct timespec *start, struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1e6 +
           (end->tv_nsec - start->tv_nsec) / 1e3;
}

static void on_process(void *userdata, struct spa_io_position *position)
{
    struct maxine_audio_node *node = userdata;

    struct pw_buffer *b_in = pw_filter_dequeue_buffer(node->input_port);
    if (!b_in)
        return;

    struct pw_buffer *b_out = pw_filter_dequeue_buffer(node->output_port);
    if (!b_out) {
        pw_filter_queue_buffer(node->input_port, b_in);
        return;
    }

    struct spa_buffer *buf_in = b_in->buffer;
    struct spa_buffer *buf_out = b_out->buffer;

    float *src = NULL;
    float *dst = NULL;
    uint32_t n_samples = 0;

    if (buf_in->n_datas > 0 && buf_in->datas[0].data) {
        src = buf_in->datas[0].data;
        n_samples = buf_in->datas[0].chunk->size / sizeof(float);
    }

    if (buf_out->n_datas > 0 && buf_out->datas[0].data)
        dst = buf_out->datas[0].data;

    if (!src || !dst || n_samples == 0) {
        pw_filter_queue_buffer(node->input_port, b_in);
        pw_filter_queue_buffer(node->output_port, b_out);
        return;
    }

    // Passthrough when disabled
    if (!node->enabled || !node->fx_handle) {
        memcpy(dst, src, n_samples * sizeof(float));
        buf_out->datas[0].chunk->size = n_samples * sizeof(float);
        buf_out->datas[0].chunk->stride = sizeof(float);
        buf_out->datas[0].chunk->offset = 0;
        pw_filter_queue_buffer(node->input_port, b_in);
        pw_filter_queue_buffer(node->output_port, b_out);
        return;
    }

    uint32_t out_pos = 0;
    uint32_t fs = node->frame_size;
    struct timespec t_start, t_end;

    for (uint32_t i = 0; i < n_samples; i++) {
        node->in_buf[node->buf_pos] = src[i];
        node->buf_pos++;

        if (node->buf_pos >= fs) {
            // We have a full frame, run the effect
            clock_gettime(CLOCK_MONOTONIC, &t_start);

            const float *in_ptr[1] = { node->in_buf };
            float *out_ptr[1] = { node->out_buf };

            NvAFX_Status st = node->sdk->Run(
                node->fx_handle, in_ptr, out_ptr, fs, 1);

            clock_gettime(CLOCK_MONOTONIC, &t_end);

            if (st != NVAFX_STATUS_SUCCESS) {
                // On error, pass through
                memcpy(node->out_buf, node->in_buf, fs * sizeof(float));
            }

            // Update stats
            double elapsed = timespec_diff_us(&t_start, &t_end);
            node->_time_accum += elapsed;
            node->_time_count++;
            if (node->_time_count > 0)
                node->avg_process_time_us = node->_time_accum / node->_time_count;
            node->frames_processed++;

            // Copy processed samples to output
            uint32_t copy = fs;
            if (out_pos + copy > n_samples)
                copy = n_samples - out_pos;
            memcpy(dst + out_pos, node->out_buf, copy * sizeof(float));
            out_pos += copy;

            node->buf_pos = 0;
        }
    }

    // If we haven't filled the output completely (partial frame left),
    // fill remaining with zeros to avoid garbage
    if (out_pos < n_samples)
        memset(dst + out_pos, 0, (n_samples - out_pos) * sizeof(float));

    buf_out->datas[0].chunk->size = n_samples * sizeof(float);
    buf_out->datas[0].chunk->stride = sizeof(float);
    buf_out->datas[0].chunk->offset = 0;

    pw_filter_queue_buffer(node->input_port, b_in);
    pw_filter_queue_buffer(node->output_port, b_out);
}

static void on_state_changed(void *userdata, enum pw_filter_state old,
                             enum pw_filter_state state, const char *error)
{
    struct maxine_audio_node *node = userdata;
    pw_log_info("maxine node '%s': state %s -> %s%s%s",
                node->effect_id,
                pw_filter_state_as_string(old),
                pw_filter_state_as_string(state),
                error ? " error: " : "",
                error ? error : "");
}

static const struct pw_filter_events filter_events = {
    PW_VERSION_FILTER_EVENTS,
    .process = on_process,
    .state_changed = on_state_changed,
};

static int init_effect(struct maxine_audio_node *node)
{
    NvAFX_Status st;
    char model[8192];

    st = node->sdk->CreateEffect(node->effect_selector, &node->fx_handle);
    if (st != NVAFX_STATUS_SUCCESS) {
        fprintf(stderr, "maxine: CreateEffect(%s) failed: %s\n",
                node->effect_selector, nvafx_status_str(st));
        return -1;
    }

    snprintf(model, sizeof(model), "%s/%s/models/sm_89/",
             node->model_path, node->model_subdir);

    st = node->sdk->SetString(node->fx_handle, NVAFX_PARAM_MODEL_PATH, model);
    if (st != NVAFX_STATUS_SUCCESS) {
        fprintf(stderr, "maxine: SetString(model_path, %s) failed: %s\n",
                model, nvafx_status_str(st));
        goto fail;
    }

    st = node->sdk->SetU32(node->fx_handle, NVAFX_PARAM_INPUT_SAMPLE_RATE,
                           node->config.sample_rate);
    if (st != NVAFX_STATUS_SUCCESS) {
        fprintf(stderr, "maxine: SetU32(input_sample_rate, %u) failed: %s\n",
                node->config.sample_rate, nvafx_status_str(st));
        goto fail;
    }

    st = node->sdk->SetU32(node->fx_handle, NVAFX_PARAM_OUTPUT_SAMPLE_RATE,
                           node->config.sample_rate);
    if (st != NVAFX_STATUS_SUCCESS) {
        // Some effects don't support output_sample_rate, that's OK
        pw_log_debug("maxine: SetU32(output_sample_rate) failed (may be OK): %s",
                     nvafx_status_str(st));
    }

    st = node->sdk->SetU32(node->fx_handle, NVAFX_PARAM_NUM_STREAMS, 1);
    if (st != NVAFX_STATUS_SUCCESS)
        pw_log_debug("maxine: SetU32(num_streams) failed (may be OK): %s",
                     nvafx_status_str(st));

    st = node->sdk->SetU32(node->fx_handle, NVAFX_PARAM_NUM_INPUT_CHANNELS, 1);
    if (st != NVAFX_STATUS_SUCCESS)
        pw_log_debug("maxine: SetU32(num_input_channels) failed (may be OK): %s",
                     nvafx_status_str(st));

    st = node->sdk->SetU32(node->fx_handle, NVAFX_PARAM_NUM_OUTPUT_CHANNELS, 1);
    if (st != NVAFX_STATUS_SUCCESS)
        pw_log_debug("maxine: SetU32(num_output_channels) failed (may be OK): %s",
                     nvafx_status_str(st));

    st = node->sdk->SetFloat(node->fx_handle, NVAFX_PARAM_INTENSITY_RATIO,
                             node->config.intensity);
    if (st != NVAFX_STATUS_SUCCESS)
        pw_log_debug("maxine: SetFloat(intensity) failed (may be OK): %s",
                     nvafx_status_str(st));

    if (node->config.enable_vad) {
        st = node->sdk->SetU32(node->fx_handle, NVAFX_PARAM_ENABLE_VAD, 1);
        if (st != NVAFX_STATUS_SUCCESS)
            pw_log_debug("maxine: SetU32(enable_vad) failed (may be OK): %s",
                         nvafx_status_str(st));
    }

    fprintf(stderr, "maxine: loading model from %s ...\n", model);

    st = node->sdk->Load(node->fx_handle);
    if (st != NVAFX_STATUS_SUCCESS) {
        fprintf(stderr, "maxine: Load() failed: %s\n", nvafx_status_str(st));
        goto fail;
    }

    // Query frame size
    unsigned int fs = 0;
    st = node->sdk->GetU32(node->fx_handle,
                           NVAFX_PARAM_NUM_SAMPLES_PER_INPUT_FRAME, &fs);
    if (st == NVAFX_STATUS_SUCCESS && fs > 0) {
        node->frame_size = fs;
    } else {
        node->frame_size = (node->config.sample_rate == 48000) ? 480 : 160;
    }

    fprintf(stderr, "maxine: '%s' loaded, frame_size=%u (%.1f ms)\n",
            node->effect_id, node->frame_size,
            (float)node->frame_size / node->config.sample_rate * 1000.0f);

    return 0;

fail:
    node->sdk->DestroyEffect(node->fx_handle);
    node->fx_handle = NULL;
    return -1;
}

struct maxine_audio_node *maxine_audio_node_create(
    struct pw_loop *loop,
    struct maxine_sdk *sdk,
    const struct maxine_audio_node_config *config)
{
    struct maxine_audio_node *node = calloc(1, sizeof(*node));
    if (!node)
        return NULL;

    node->sdk = sdk;
    node->config = *config;
    node->enabled = true;

    // Copy strings into owned storage
    snprintf(node->effect_id, sizeof(node->effect_id), "%s",
             config->effect_id ? config->effect_id : "unknown");
    snprintf(node->effect_selector, sizeof(node->effect_selector), "%s",
             config->effect_selector ? config->effect_selector : "");
    snprintf(node->model_subdir, sizeof(node->model_subdir), "%s",
             config->model_subdir ? config->model_subdir : "");
    snprintf(node->model_path, sizeof(node->model_path), "%s",
             config->model_path ? config->model_path : "/opt/maxine-afx/features");

    // Update config pointers to owned storage
    node->config.effect_id = node->effect_id;
    node->config.effect_selector = node->effect_selector;
    node->config.model_subdir = node->model_subdir;
    node->config.model_path = node->model_path;

    // Initialize the SDK effect
    if (init_effect(node) < 0) {
        free(node);
        return NULL;
    }

    // Allocate processing buffers
    node->in_buf = calloc(node->frame_size, sizeof(float));
    node->out_buf = calloc(node->frame_size, sizeof(float));
    if (!node->in_buf || !node->out_buf) {
        fprintf(stderr, "maxine: failed to allocate processing buffers\n");
        maxine_audio_node_destroy(node);
        return NULL;
    }

    // Create PipeWire filter
    char node_name[256];
    snprintf(node_name, sizeof(node_name), "maxine_%s", node->effect_id);

    struct pw_properties *props = pw_properties_new(
        PW_KEY_NODE_NAME, node_name,
        PW_KEY_NODE_DESCRIPTION, node->effect_id,
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Filter",
        PW_KEY_MEDIA_ROLE, "DSP",
        NULL);

    node->filter = pw_filter_new_simple(
        loop,
        node_name,
        props,
        &filter_events,
        node);

    if (!node->filter) {
        fprintf(stderr, "maxine: failed to create pw_filter for '%s'\n",
                node->effect_id);
        maxine_audio_node_destroy(node);
        return NULL;
    }

    // Build port format params
    uint8_t pod_buf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(pod_buf, sizeof(pod_buf));

    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(
        &b, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_F32,
            .channels = 1,
            .rate = config->sample_rate));

    // Add input port
    node->input_port = pw_filter_add_port(node->filter,
        PW_DIRECTION_INPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port_data),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "input",
            NULL),
        params, 1);

    if (!node->input_port) {
        fprintf(stderr, "maxine: failed to add input port for '%s'\n",
                node->effect_id);
        maxine_audio_node_destroy(node);
        return NULL;
    }

    // pw_filter_add_port returns a pointer to the port_data struct
    struct port_data *pd_in = node->input_port;
    pd_in->node = node;

    // Build params again for output port (builder was consumed)
    b = SPA_POD_BUILDER_INIT(pod_buf, sizeof(pod_buf));
    params[0] = spa_format_audio_raw_build(
        &b, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_F32,
            .channels = 1,
            .rate = config->sample_rate));

    // Add output port
    node->output_port = pw_filter_add_port(node->filter,
        PW_DIRECTION_OUTPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port_data),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "output",
            NULL),
        params, 1);

    if (!node->output_port) {
        fprintf(stderr, "maxine: failed to add output port for '%s'\n",
                node->effect_id);
        maxine_audio_node_destroy(node);
        return NULL;
    }

    struct port_data *pd_out = node->output_port;
    pd_out->node = node;

    // Connect the filter
    if (pw_filter_connect(node->filter, PW_FILTER_FLAG_RT_PROCESS, NULL, 0) < 0) {
        fprintf(stderr, "maxine: failed to connect filter for '%s'\n",
                node->effect_id);
        maxine_audio_node_destroy(node);
        return NULL;
    }

    fprintf(stderr, "maxine: audio node '%s' created and connected\n",
            node->effect_id);
    return node;
}

void maxine_audio_node_destroy(struct maxine_audio_node *node)
{
    if (!node)
        return;

    if (node->filter) {
        pw_filter_destroy(node->filter);
        node->filter = NULL;
    }

    if (node->fx_handle && node->sdk) {
        node->sdk->DestroyEffect(node->fx_handle);
        node->fx_handle = NULL;
    }

    free(node->in_buf);
    free(node->out_buf);
    node->in_buf = NULL;
    node->out_buf = NULL;

    free(node);
}

void maxine_audio_node_set_enabled(struct maxine_audio_node *node, bool enabled)
{
    if (!node)
        return;
    node->enabled = enabled;
    fprintf(stderr, "maxine: '%s' %s\n", node->effect_id,
            enabled ? "enabled" : "disabled");
}

void maxine_audio_node_set_intensity(struct maxine_audio_node *node, float intensity)
{
    if (!node)
        return;

    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    node->config.intensity = intensity;

    if (node->fx_handle && node->sdk) {
        NvAFX_Status st = node->sdk->SetFloat(
            node->fx_handle, NVAFX_PARAM_INTENSITY_RATIO, intensity);
        if (st != NVAFX_STATUS_SUCCESS)
            fprintf(stderr, "maxine: SetFloat(intensity) for '%s' failed: %s\n",
                    node->effect_id, nvafx_status_str(st));
    }

    fprintf(stderr, "maxine: '%s' intensity set to %.2f\n",
            node->effect_id, intensity);
}

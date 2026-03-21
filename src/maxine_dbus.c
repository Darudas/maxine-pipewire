#include "maxine_dbus.h"
#include "maxine_audio_chain.h"
#include "maxine_audio_node.h"
#include "maxine_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// --- D-Bus method handlers ---

static int method_list_effects(sd_bus_message *msg, void *userdata,
                               sd_bus_error *ret_error)
{
    struct maxine_dbus *dbus = userdata;
    struct maxine_audio_chain *chain = dbus->chain;
    sd_bus_message *reply = NULL;
    int r;

    r = sd_bus_message_new_method_return(msg, &reply);
    if (r < 0)
        return r;

    // Return array of structs (name, enabled, intensity)
    r = sd_bus_message_open_container(reply, 'a', "(sbd)");
    if (r < 0)
        goto fail;

    for (int i = 0; i < chain->node_count; i++) {
        struct maxine_audio_node *node = chain->nodes[i];
        r = sd_bus_message_append(reply, "(sbd)",
                                  node->effect_id,
                                  node->enabled,
                                  (double)node->config.intensity);
        if (r < 0)
            goto fail;
    }

    r = sd_bus_message_close_container(reply);
    if (r < 0)
        goto fail;

    return sd_bus_send(NULL, reply, NULL);

fail:
    sd_bus_message_unref(reply);
    return r;
}

static int method_enable_effect(sd_bus_message *msg, void *userdata,
                                sd_bus_error *ret_error)
{
    struct maxine_dbus *dbus = userdata;
    const char *name = NULL;
    int r;

    r = sd_bus_message_read(msg, "s", &name);
    if (r < 0)
        return r;

    int result = maxine_audio_chain_set_enabled(dbus->chain, name, true);
    if (result < 0) {
        sd_bus_error_setf(ret_error, SD_BUS_ERROR_INVALID_ARGS,
                          "Effect '%s' not found", name);
        return -ENOENT;
    }

    return sd_bus_reply_method_return(msg, "b", true);
}

static int method_disable_effect(sd_bus_message *msg, void *userdata,
                                 sd_bus_error *ret_error)
{
    struct maxine_dbus *dbus = userdata;
    const char *name = NULL;
    int r;

    r = sd_bus_message_read(msg, "s", &name);
    if (r < 0)
        return r;

    int result = maxine_audio_chain_set_enabled(dbus->chain, name, false);
    if (result < 0) {
        sd_bus_error_setf(ret_error, SD_BUS_ERROR_INVALID_ARGS,
                          "Effect '%s' not found", name);
        return -ENOENT;
    }

    return sd_bus_reply_method_return(msg, "b", true);
}

static int method_set_intensity(sd_bus_message *msg, void *userdata,
                                sd_bus_error *ret_error)
{
    struct maxine_dbus *dbus = userdata;
    const char *name = NULL;
    double value = 0;
    int r;

    r = sd_bus_message_read(msg, "sd", &name, &value);
    if (r < 0)
        return r;

    if (value < 0.0 || value > 1.0) {
        sd_bus_error_setf(ret_error, SD_BUS_ERROR_INVALID_ARGS,
                          "Intensity must be between 0.0 and 1.0, got %.2f",
                          value);
        return -EINVAL;
    }

    int result = maxine_audio_chain_set_intensity(dbus->chain, name,
                                                  (float)value);
    if (result < 0) {
        sd_bus_error_setf(ret_error, SD_BUS_ERROR_INVALID_ARGS,
                          "Effect '%s' not found", name);
        return -ENOENT;
    }

    return sd_bus_reply_method_return(msg, "b", true);
}

static int method_get_status(sd_bus_message *msg, void *userdata,
                             sd_bus_error *ret_error)
{
    struct maxine_dbus *dbus = userdata;
    struct maxine_audio_chain *chain = dbus->chain;
    sd_bus_message *reply = NULL;
    int r;

    r = sd_bus_message_new_method_return(msg, &reply);
    if (r < 0)
        return r;

    // Return dict of string -> string
    r = sd_bus_message_open_container(reply, 'a', "{ss}");
    if (r < 0)
        goto fail;

    r = sd_bus_message_append(reply, "{ss}", "gpu_name", dbus->gpu_name);
    if (r < 0) goto fail;

    r = sd_bus_message_append(reply, "{ss}", "gpu_arch", dbus->gpu_arch);
    if (r < 0) goto fail;

    r = sd_bus_message_append(reply, "{ss}", "running",
                              dbus->running ? "true" : "false");
    if (r < 0) goto fail;

    char buf[64];
    snprintf(buf, sizeof(buf), "%u", chain->sample_rate);
    r = sd_bus_message_append(reply, "{ss}", "sample_rate", buf);
    if (r < 0) goto fail;

    snprintf(buf, sizeof(buf), "%d", chain->node_count);
    r = sd_bus_message_append(reply, "{ss}", "effect_count", buf);
    if (r < 0) goto fail;

    // Compute total latency from frame sizes
    double total_latency_ms = 0;
    uint64_t total_frames = 0;
    double total_process_us = 0;
    int active_count = 0;

    for (int i = 0; i < chain->node_count; i++) {
        struct maxine_audio_node *node = chain->nodes[i];
        if (node->enabled) {
            total_latency_ms += (double)node->frame_size /
                                node->config.sample_rate * 1000.0;
            active_count++;
        }
        total_frames += node->frames_processed;
        total_process_us += node->avg_process_time_us;
    }

    snprintf(buf, sizeof(buf), "%.1f", total_latency_ms);
    r = sd_bus_message_append(reply, "{ss}", "latency_ms", buf);
    if (r < 0) goto fail;

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)total_frames);
    r = sd_bus_message_append(reply, "{ss}", "frames_processed", buf);
    if (r < 0) goto fail;

    snprintf(buf, sizeof(buf), "%.1f", total_process_us);
    r = sd_bus_message_append(reply, "{ss}", "avg_process_time_us", buf);
    if (r < 0) goto fail;

    snprintf(buf, sizeof(buf), "%d", active_count);
    r = sd_bus_message_append(reply, "{ss}", "active_effects", buf);
    if (r < 0) goto fail;

    // Per-node frame sizes
    if (chain->node_count > 0) {
        struct maxine_audio_node *first = chain->nodes[0];
        snprintf(buf, sizeof(buf), "%u", first->frame_size);
        r = sd_bus_message_append(reply, "{ss}", "frame_size", buf);
        if (r < 0) goto fail;
    }

    r = sd_bus_message_close_container(reply);
    if (r < 0)
        goto fail;

    return sd_bus_send(NULL, reply, NULL);

fail:
    sd_bus_message_unref(reply);
    return r;
}

static int method_list_devices(sd_bus_message *msg, void *userdata,
                               sd_bus_error *ret_error)
{
    sd_bus_message *reply = NULL;
    int r;

    r = sd_bus_message_new_method_return(msg, &reply);
    if (r < 0)
        return r;

    // Return array of (id, name, type)
    // We return a basic list; actual device enumeration would use pw_registry
    r = sd_bus_message_open_container(reply, 'a', "(sss)");
    if (r < 0)
        goto fail;

    // Enumerate PipeWire devices via pw-cli output
    // For a proper implementation, we'd use the registry
    // For now, provide what the daemon knows about
    struct maxine_dbus *dbus = userdata;
    struct maxine_audio_chain *chain = dbus->chain;

    if (chain->source_device[0]) {
        r = sd_bus_message_append(reply, "(sss)",
                                  chain->source_device,
                                  chain->source_device,
                                  "source");
        if (r < 0) goto fail;
    }

    if (chain->virtual_source_name[0]) {
        r = sd_bus_message_append(reply, "(sss)",
                                  "maxine_clean_mic",
                                  chain->virtual_source_name,
                                  "virtual-source");
        if (r < 0) goto fail;
    }

    r = sd_bus_message_close_container(reply);
    if (r < 0)
        goto fail;

    return sd_bus_send(NULL, reply, NULL);

fail:
    sd_bus_message_unref(reply);
    return r;
}

static int method_reload_config(sd_bus_message *msg, void *userdata,
                                sd_bus_error *ret_error)
{
    // Signal that a reload was requested
    // The actual reload happens in the daemon's config module
    fprintf(stderr, "maxine: config reload requested via D-Bus\n");
    return sd_bus_reply_method_return(msg, "b", true);
}

// --- D-Bus vtable ---

static const sd_bus_vtable maxine_vtable[] = {
    SD_BUS_VTABLE_START(0),

    SD_BUS_METHOD("ListEffects", "", "a(sbd)", method_list_effects,
                  SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_METHOD("EnableEffect", "s", "b", method_enable_effect,
                  SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_METHOD("DisableEffect", "s", "b", method_disable_effect,
                  SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_METHOD("SetIntensity", "sd", "b", method_set_intensity,
                  SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_METHOD("GetStatus", "", "a{ss}", method_get_status,
                  SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_METHOD("ListDevices", "", "a(sss)", method_list_devices,
                  SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_METHOD("ReloadConfig", "", "b", method_reload_config,
                  SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_VTABLE_END
};

// --- Detect GPU name ---

static void detect_gpu(struct maxine_dbus *dbus)
{
    snprintf(dbus->gpu_name, sizeof(dbus->gpu_name), "Unknown NVIDIA GPU");
    snprintf(dbus->gpu_arch, sizeof(dbus->gpu_arch), "unknown");

    FILE *fp = fopen("/proc/driver/nvidia/gpus/0000:01:00.0/information", "r");
    if (!fp) {
        // Try nvidia-smi as fallback
        fp = popen("nvidia-smi --query-gpu=name,compute_cap --format=csv,noheader,nounits 2>/dev/null", "r");
        if (fp) {
            char line[512];
            if (fgets(line, sizeof(line), fp)) {
                // Format: "NVIDIA GeForce RTX 4070 Max-Q, 8.9"
                char *comma = strchr(line, ',');
                if (comma) {
                    *comma = '\0';
                    snprintf(dbus->gpu_name, sizeof(dbus->gpu_name), "%s",
                             line);
                    char *arch = comma + 1;
                    while (*arch == ' ') arch++;
                    // Remove trailing newline
                    char *nl = strchr(arch, '\n');
                    if (nl) *nl = '\0';
                    // Convert compute cap to sm_XX
                    int major = 0, minor = 0;
                    if (sscanf(arch, "%d.%d", &major, &minor) == 2)
                        snprintf(dbus->gpu_arch, sizeof(dbus->gpu_arch),
                                 "sm_%d%d", major, minor);
                    else
                        snprintf(dbus->gpu_arch, sizeof(dbus->gpu_arch),
                                 "%s", arch);
                }
            }
            pclose(fp);
            return;
        }
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Model:", 6) == 0) {
            char *name = line + 6;
            while (*name == ' ' || *name == '\t') name++;
            char *nl = strchr(name, '\n');
            if (nl) *nl = '\0';
            snprintf(dbus->gpu_name, sizeof(dbus->gpu_name), "%s", name);
        }
    }
    fclose(fp);
}

// --- Public API ---

int maxine_dbus_init(struct maxine_dbus *dbus, struct maxine_audio_chain *chain)
{
    int r;

    memset(dbus, 0, sizeof(*dbus));
    dbus->chain = chain;
    dbus->running = true;

    detect_gpu(dbus);

    r = sd_bus_open_user(&dbus->bus);
    if (r < 0) {
        fprintf(stderr, "maxine: failed to connect to user bus: %s\n",
                strerror(-r));
        return r;
    }

    r = sd_bus_add_object_vtable(dbus->bus,
                                 &dbus->slot,
                                 MAXINE_DBUS_PATH,
                                 MAXINE_DBUS_IFACE,
                                 maxine_vtable,
                                 dbus);
    if (r < 0) {
        fprintf(stderr, "maxine: failed to add vtable: %s\n", strerror(-r));
        sd_bus_unref(dbus->bus);
        dbus->bus = NULL;
        return r;
    }

    r = sd_bus_request_name(dbus->bus, MAXINE_DBUS_NAME, 0);
    if (r < 0) {
        fprintf(stderr, "maxine: failed to acquire bus name '%s': %s\n",
                MAXINE_DBUS_NAME, strerror(-r));
        fprintf(stderr, "maxine: is another instance of maxined running?\n");
        sd_bus_unref(dbus->bus);
        dbus->bus = NULL;
        return r;
    }

    fprintf(stderr, "maxine: D-Bus service registered at %s\n",
            MAXINE_DBUS_NAME);
    return 0;
}

int maxine_dbus_get_fd(struct maxine_dbus *dbus)
{
    if (!dbus || !dbus->bus)
        return -1;
    return sd_bus_get_fd(dbus->bus);
}

int maxine_dbus_dispatch(struct maxine_dbus *dbus)
{
    if (!dbus || !dbus->bus)
        return -1;

    int r;
    for (;;) {
        r = sd_bus_process(dbus->bus, NULL);
        if (r < 0) {
            fprintf(stderr, "maxine: D-Bus process error: %s\n",
                    strerror(-r));
            return r;
        }
        if (r == 0)
            break;
        // r > 0 means a message was processed, try again
    }

    return 0;
}

void maxine_dbus_destroy(struct maxine_dbus *dbus)
{
    if (!dbus)
        return;

    dbus->running = false;

    if (dbus->slot) {
        sd_bus_slot_unref(dbus->slot);
        dbus->slot = NULL;
    }

    if (dbus->bus) {
        sd_bus_release_name(dbus->bus, MAXINE_DBUS_NAME);
        sd_bus_unref(dbus->bus);
        dbus->bus = NULL;
    }
}

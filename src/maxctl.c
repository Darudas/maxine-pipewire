// maxctl - CLI tool for controlling the Maxine PipeWire audio effects daemon
//
// Communicates with maxined via D-Bus (org.maxine.Effects).
//
// Usage: maxctl <command> [args...]

#include <systemd/sd-bus.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DBUS_NAME  "org.maxine.Effects"
#define DBUS_PATH  "/org/maxine/Effects"
#define DBUS_IFACE "org.maxine.Effects"

// --- ANSI colors ---

#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_DIM     "\033[2m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN    "\033[36m"
#define C_WHITE   "\033[37m"

static bool use_color = true;

#define COL(c) (use_color ? (c) : "")

// --- Effect catalog (static metadata for display, independent of daemon) ---

struct effect_info {
    const char *id;
    const char *name;
    const char *description;
    const char *category;
};

static const struct effect_info effect_catalog[] = {
    { "denoiser",       "Noise Removal",
      "Remove background noise (typing, fans, etc.)",        "noise" },
    { "dereverb",       "Room Echo Removal",
      "Remove room reverberation/echo",                      "echo" },
    { "dereverb-denoiser", "Noise + Room Echo Removal",
      "Combined single-pass (lower latency)",                "noise" },
    { "aec",            "Acoustic Echo Cancellation",
      "Remove speaker playback from microphone",             "echo" },
    { "superres",       "Audio Super Resolution",
      "Upscale low-quality audio (8/16kHz -> 48kHz)",        "enhancement" },
    { "studio-voice-hq", "Studio Voice (High Quality)",
      "Make any mic sound professional",                     "enhancement" },
    { "studio-voice-ll", "Studio Voice (Low Latency)",
      "Studio quality with lower latency",                   "enhancement" },
    { "speaker-focus",  "Speaker Focus",
      "Isolate main speaker, remove others",                 "voice" },
    { "voice-font-hq",  "Voice Font (High Quality)",
      "Change voice to match reference",                     "voice" },
    { "voice-font-ll",  "Voice Font (Low Latency)",
      "Voice conversion with lower latency",                 "voice" },
    // Chained effects
    { "denoiser-16k-superres-16k-to-48k",
      "Denoise 16k + Super Resolution to 48k",
      "Denoise at 16kHz then upscale to 48kHz",             "noise" },
    { "dereverb-16k-superres-16k-to-48k",
      "Dereverb 16k + Super Resolution to 48k",
      "Remove reverb at 16kHz then upscale to 48kHz",       "echo" },
    { "dereverb-denoiser-16k-superres-16k-to-48k",
      "Dereverb + Denoise 16k + Super Resolution to 48k",
      "Combined dereverb/denoise at 16kHz then upscale",    "noise" },
    { "superres-8k-to-16k-denoiser-16k",
      "Super Resolution 8k to 16k + Denoise",
      "Upscale 8kHz to 16kHz then denoise",                 "enhancement" },
    { "superres-8k-to-16k-dereverb-16k",
      "Super Resolution 8k to 16k + Dereverb",
      "Upscale 8kHz to 16kHz then remove reverb",           "enhancement" },
    { "superres-8k-to-16k-dereverb-denoiser-16k",
      "Super Resolution 8k to 16k + Dereverb + Denoise",
      "Upscale 8kHz to 16kHz then dereverb/denoise",        "enhancement" },
    { "denoiser-16k-speaker-focus-16k",
      "Denoise 16k + Speaker Focus 16k",
      "Denoise then isolate primary speaker at 16kHz",       "voice" },
    { "denoiser-48k-speaker-focus-48k",
      "Denoise 48k + Speaker Focus 48k",
      "Denoise then isolate primary speaker at 48kHz",       "voice" },
    { NULL, NULL, NULL, NULL }
};

static const struct effect_info *find_effect_info(const char *id)
{
    for (int i = 0; effect_catalog[i].id; i++) {
        if (strcmp(effect_catalog[i].id, id) == 0)
            return &effect_catalog[i];
    }
    return NULL;
}

// --- D-Bus connection helper ---

static sd_bus *bus_connect(void)
{
    sd_bus *bus = NULL;
    int r = sd_bus_open_user(&bus);
    if (r < 0) {
        fprintf(stderr, "%serror:%s failed to connect to user D-Bus: %s\n",
                COL(C_RED), COL(C_RESET), strerror(-r));
        return NULL;
    }
    return bus;
}

static bool daemon_is_running(sd_bus *bus)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;

    int r = sd_bus_call_method(bus, DBUS_NAME, DBUS_PATH, DBUS_IFACE,
                               "GetStatus", &error, &reply, "");

    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);

    return r >= 0;
}

// --- Commands ---

static int cmd_status(sd_bus *bus)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;

    // Get status dict
    r = sd_bus_call_method(bus, DBUS_NAME, DBUS_PATH, DBUS_IFACE,
                           "GetStatus", &error, &reply, "");
    if (r < 0) {
        fprintf(stderr, "%serror:%s daemon not responding: %s\n",
                COL(C_RED), COL(C_RESET), error.message);
        sd_bus_error_free(&error);
        return 1;
    }

    // Parse the status dict
    char gpu_name[256] = "Unknown";
    char gpu_arch[32] = "";
    char latency[32] = "?";
    char frames[32] = "0";
    char process_us[32] = "0";
    char sample_rate_str[16] = "48000";
    char frame_size_str[16] = "480";
    char active_str[8] = "0";
    bool running = false;

    r = sd_bus_message_enter_container(reply, 'a', "{ss}");
    if (r < 0) {
        sd_bus_message_unref(reply);
        sd_bus_error_free(&error);
        return 1;
    }

    while ((r = sd_bus_message_enter_container(reply, 'e', "ss")) > 0) {
        const char *key = NULL, *val = NULL;
        r = sd_bus_message_read(reply, "ss", &key, &val);
        if (r < 0) break;

        if (strcmp(key, "gpu_name") == 0)
            snprintf(gpu_name, sizeof(gpu_name), "%s", val);
        else if (strcmp(key, "gpu_arch") == 0)
            snprintf(gpu_arch, sizeof(gpu_arch), "%s", val);
        else if (strcmp(key, "running") == 0)
            running = (strcmp(val, "true") == 0);
        else if (strcmp(key, "latency_ms") == 0)
            snprintf(latency, sizeof(latency), "%s", val);
        else if (strcmp(key, "frames_processed") == 0)
            snprintf(frames, sizeof(frames), "%s", val);
        else if (strcmp(key, "avg_process_time_us") == 0)
            snprintf(process_us, sizeof(process_us), "%s", val);
        else if (strcmp(key, "sample_rate") == 0)
            snprintf(sample_rate_str, sizeof(sample_rate_str), "%s", val);
        else if (strcmp(key, "frame_size") == 0)
            snprintf(frame_size_str, sizeof(frame_size_str), "%s", val);
        else if (strcmp(key, "active_effects") == 0)
            snprintf(active_str, sizeof(active_str), "%s", val);

        sd_bus_message_exit_container(reply);
    }
    sd_bus_message_exit_container(reply);

    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);

    // Get effects list
    error = (sd_bus_error)SD_BUS_ERROR_NULL;
    reply = NULL;

    r = sd_bus_call_method(bus, DBUS_NAME, DBUS_PATH, DBUS_IFACE,
                           "ListEffects", &error, &reply, "");
    if (r < 0) {
        fprintf(stderr, "%serror:%s failed to list effects: %s\n",
                COL(C_RED), COL(C_RESET), error.message);
        sd_bus_error_free(&error);
        return 1;
    }

    // Print header
    printf("\n%s%sMaxine PipeWire Audio Effects%s\n", COL(C_BOLD), COL(C_CYAN),
           COL(C_RESET));
    printf("%sGPU:%s %s", COL(C_BOLD), COL(C_RESET), gpu_name);
    if (gpu_arch[0])
        printf(" %s(%s)%s", COL(C_DIM), gpu_arch, COL(C_RESET));
    printf("\n");

    printf("%sStatus:%s %s%s%s\n\n", COL(C_BOLD), COL(C_RESET),
           running ? COL(C_GREEN) : COL(C_RED),
           running ? "Active" : "Inactive",
           COL(C_RESET));

    // Print effects
    printf("%sEffects:%s\n", COL(C_BOLD), COL(C_RESET));

    r = sd_bus_message_enter_container(reply, 'a', "(sbd)");
    if (r >= 0) {
        while ((r = sd_bus_message_enter_container(reply, 'r', "sbd")) > 0) {
            const char *name = NULL;
            int enabled = 0;
            double intensity = 0;

            r = sd_bus_message_read(reply, "sbd", &name, &enabled, &intensity);
            if (r < 0) break;

            const struct effect_info *info = find_effect_info(name);
            const char *display_name = info ? info->name : name;

            if (enabled) {
                printf("  %s\xe2\x97\x8f%s %-28s%senabled%s",
                       COL(C_GREEN), COL(C_RESET),
                       display_name,
                       COL(C_GREEN), COL(C_RESET));
                printf("   intensity: %s%.2f%s\n", COL(C_YELLOW),
                       intensity, COL(C_RESET));
            } else {
                printf("  %s\xe2\x97\x8b %-28s%sdisabled%s\n",
                       COL(C_DIM),
                       display_name, COL(C_DIM), COL(C_RESET));
            }

            sd_bus_message_exit_container(reply);
        }
        sd_bus_message_exit_container(reply);
    }

    // Print audio info
    uint32_t sr = (uint32_t)atoi(sample_rate_str);
    uint32_t fs = (uint32_t)atoi(frame_size_str);
    double frame_ms = sr > 0 ? (double)fs / sr * 1000.0 : 0;

    printf("\n%sAudio:%s %s Hz, %s samples/frame (%.0fms)\n",
           COL(C_BOLD), COL(C_RESET), sample_rate_str, frame_size_str, frame_ms);
    printf("%sLatency:%s ~%sms\n", COL(C_BOLD), COL(C_RESET), latency);
    printf("%sFrames processed:%s %s\n", COL(C_BOLD), COL(C_RESET), frames);
    printf("\n");

    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    return 0;
}

static int cmd_effects(void)
{
    printf("\n%s%sAvailable Audio Effects:%s\n\n", COL(C_BOLD), COL(C_CYAN),
           COL(C_RESET));

    // Main effects (single-pass)
    printf("  %s%-22s %-34s %s%s\n", COL(C_DIM), "ID", "NAME", "DESCRIPTION",
           COL(C_RESET));
    printf("  %s%-22s %-34s %s%s\n", COL(C_DIM),
           "----------------------", "----------------------------------",
           "-------------------------------------------", COL(C_RESET));

    const char *last_category = "";
    bool printed_chained_header = false;

    for (int i = 0; effect_catalog[i].id; i++) {
        // Separate chained effects with a header
        if (i >= 10 && !printed_chained_header) {
            printf("\n  %s%sChained Effects:%s\n", COL(C_BOLD), COL(C_DIM),
                   COL(C_RESET));
            printed_chained_header = true;
            last_category = "";
        }

        // Category separators for main effects
        if (effect_catalog[i].category &&
            strcmp(effect_catalog[i].category, last_category) != 0 && i > 0)
            printf("\n");
        last_category = effect_catalog[i].category ? effect_catalog[i].category : "";

        printf("  %s%-42s%s %-34s %s%s%s\n",
               COL(C_YELLOW), effect_catalog[i].id, COL(C_RESET),
               effect_catalog[i].name,
               COL(C_DIM), effect_catalog[i].description, COL(C_RESET));
    }

    printf("\n");
    return 0;
}

static int cmd_devices(sd_bus *bus)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;

    r = sd_bus_call_method(bus, DBUS_NAME, DBUS_PATH, DBUS_IFACE,
                           "ListDevices", &error, &reply, "");
    if (r < 0) {
        fprintf(stderr, "%serror:%s failed to list devices: %s\n",
                COL(C_RED), COL(C_RESET), error.message);
        sd_bus_error_free(&error);
        return 1;
    }

    printf("\n%s%sAudio Devices:%s\n\n", COL(C_BOLD), COL(C_CYAN),
           COL(C_RESET));
    printf("  %s%-30s %-30s %s%s\n", COL(C_DIM), "ID", "NAME", "TYPE",
           COL(C_RESET));
    printf("  %s%-30s %-30s %s%s\n", COL(C_DIM),
           "------------------------------", "------------------------------",
           "----------------", COL(C_RESET));

    r = sd_bus_message_enter_container(reply, 'a', "(sss)");
    if (r < 0) goto done;

    while ((r = sd_bus_message_enter_container(reply, 'r', "sss")) > 0) {
        const char *id = NULL, *name = NULL, *type = NULL;
        r = sd_bus_message_read(reply, "sss", &id, &name, &type);
        if (r < 0) break;

        const char *type_color = COL(C_WHITE);
        if (strcmp(type, "virtual-source") == 0)
            type_color = COL(C_GREEN);
        else if (strcmp(type, "source") == 0)
            type_color = COL(C_CYAN);

        printf("  %-30s %-30s %s%s%s\n", id, name, type_color, type,
               COL(C_RESET));

        sd_bus_message_exit_container(reply);
    }
    sd_bus_message_exit_container(reply);

    printf("\n");

done:
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    return 0;
}

static int cmd_enable(sd_bus *bus, const char *effect)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;

    r = sd_bus_call_method(bus, DBUS_NAME, DBUS_PATH, DBUS_IFACE,
                           "EnableEffect", &error, &reply, "s", effect);
    if (r < 0) {
        fprintf(stderr, "%serror:%s failed to enable '%s': %s\n",
                COL(C_RED), COL(C_RESET), effect, error.message);
        sd_bus_error_free(&error);
        return 1;
    }

    const struct effect_info *info = find_effect_info(effect);
    printf("%s\xe2\x9c\x93%s %s%s%s enabled\n",
           COL(C_GREEN), COL(C_RESET),
           COL(C_BOLD), info ? info->name : effect, COL(C_RESET));

    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    return 0;
}

static int cmd_disable(sd_bus *bus, const char *effect)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;

    r = sd_bus_call_method(bus, DBUS_NAME, DBUS_PATH, DBUS_IFACE,
                           "DisableEffect", &error, &reply, "s", effect);
    if (r < 0) {
        fprintf(stderr, "%serror:%s failed to disable '%s': %s\n",
                COL(C_RED), COL(C_RESET), effect, error.message);
        sd_bus_error_free(&error);
        return 1;
    }

    const struct effect_info *info = find_effect_info(effect);
    printf("%s\xe2\x97\x8b%s %s%s%s disabled\n",
           COL(C_YELLOW), COL(C_RESET),
           COL(C_BOLD), info ? info->name : effect, COL(C_RESET));

    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    return 0;
}

static int cmd_set(sd_bus *bus, const char *effect, const char *param,
                   const char *value)
{
    if (strcmp(param, "intensity") != 0) {
        fprintf(stderr, "%serror:%s unknown parameter '%s' (supported: intensity)\n",
                COL(C_RED), COL(C_RESET), param);
        return 1;
    }

    double intensity = atof(value);
    if (intensity < 0.0 || intensity > 1.0) {
        fprintf(stderr, "%serror:%s intensity must be between 0.0 and 1.0\n",
                COL(C_RED), COL(C_RESET));
        return 1;
    }

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;

    r = sd_bus_call_method(bus, DBUS_NAME, DBUS_PATH, DBUS_IFACE,
                           "SetIntensity", &error, &reply, "sd",
                           effect, intensity);
    if (r < 0) {
        fprintf(stderr, "%serror:%s failed to set intensity for '%s': %s\n",
                COL(C_RED), COL(C_RESET), effect, error.message);
        sd_bus_error_free(&error);
        return 1;
    }

    const struct effect_info *info = find_effect_info(effect);
    printf("%s\xe2\x9c\x93%s %s%s%s intensity set to %s%.2f%s\n",
           COL(C_GREEN), COL(C_RESET),
           COL(C_BOLD), info ? info->name : effect, COL(C_RESET),
           COL(C_YELLOW), intensity, COL(C_RESET));

    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    return 0;
}

static int cmd_monitor(sd_bus *bus)
{
    printf("%s%sMaxine Audio Effects Monitor%s\n", COL(C_BOLD), COL(C_CYAN),
           COL(C_RESET));
    printf("%sPress Ctrl+C to stop%s\n\n", COL(C_DIM), COL(C_RESET));

    while (1) {
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *status_reply = NULL;
        sd_bus_message *effects_reply = NULL;
        int r;

        // Clear screen and move cursor to top
        printf("\033[H\033[2J");
        printf("%s%sMaxine Audio Effects Monitor%s  %s[Ctrl+C to stop]%s\n\n",
               COL(C_BOLD), COL(C_CYAN), COL(C_RESET),
               COL(C_DIM), COL(C_RESET));

        // Get status
        r = sd_bus_call_method(bus, DBUS_NAME, DBUS_PATH, DBUS_IFACE,
                               "GetStatus", &error, &status_reply, "");
        if (r < 0) {
            printf("  %s%sDaemon not responding%s\n", COL(C_BOLD), COL(C_RED),
                   COL(C_RESET));
            sd_bus_error_free(&error);
            sleep(1);
            continue;
        }

        // Parse status
        char frames[32] = "0";
        char process_us[32] = "0";
        char latency[32] = "0";

        r = sd_bus_message_enter_container(status_reply, 'a', "{ss}");
        if (r >= 0) {
            while ((r = sd_bus_message_enter_container(status_reply, 'e', "ss")) > 0) {
                const char *key = NULL, *val = NULL;
                sd_bus_message_read(status_reply, "ss", &key, &val);
                if (key && val) {
                    if (strcmp(key, "frames_processed") == 0)
                        snprintf(frames, sizeof(frames), "%s", val);
                    else if (strcmp(key, "avg_process_time_us") == 0)
                        snprintf(process_us, sizeof(process_us), "%s", val);
                    else if (strcmp(key, "latency_ms") == 0)
                        snprintf(latency, sizeof(latency), "%s", val);
                }
                sd_bus_message_exit_container(status_reply);
            }
            sd_bus_message_exit_container(status_reply);
        }

        sd_bus_message_unref(status_reply);
        sd_bus_error_free(&error);

        // Get effects
        error = (sd_bus_error)SD_BUS_ERROR_NULL;
        r = sd_bus_call_method(bus, DBUS_NAME, DBUS_PATH, DBUS_IFACE,
                               "ListEffects", &error, &effects_reply, "");
        if (r < 0) {
            sd_bus_error_free(&error);
            sleep(1);
            continue;
        }

        printf("  %s%-28s %-10s %-12s%s\n",
               COL(C_DIM), "EFFECT", "STATUS", "INTENSITY", COL(C_RESET));
        printf("  %s%-28s %-10s %-12s%s\n",
               COL(C_DIM),
               "----------------------------", "----------", "------------",
               COL(C_RESET));

        r = sd_bus_message_enter_container(effects_reply, 'a', "(sbd)");
        if (r >= 0) {
            while ((r = sd_bus_message_enter_container(effects_reply, 'r', "sbd")) > 0) {
                const char *name = NULL;
                int enabled = 0;
                double intensity = 0;

                sd_bus_message_read(effects_reply, "sbd", &name, &enabled, &intensity);

                const struct effect_info *info = name ? find_effect_info(name) : NULL;
                const char *display = info ? info->name : (name ? name : "?");

                if (enabled) {
                    printf("  %s%-28s%s %s%-10s%s %s%.2f%s\n",
                           COL(C_WHITE), display, COL(C_RESET),
                           COL(C_GREEN), "active", COL(C_RESET),
                           COL(C_YELLOW), intensity, COL(C_RESET));
                } else {
                    printf("  %s%-28s %-10s%s\n",
                           COL(C_DIM), display, "inactive", COL(C_RESET));
                }

                sd_bus_message_exit_container(effects_reply);
            }
            sd_bus_message_exit_container(effects_reply);
        }

        printf("\n");
        printf("  %sFrames:%s %-16s", COL(C_BOLD), COL(C_RESET), frames);
        printf("  %sLatency:%s ~%sms", COL(C_BOLD), COL(C_RESET), latency);
        printf("  %sProcess:%s %s us/frame\n", COL(C_BOLD), COL(C_RESET),
               process_us);

        sd_bus_message_unref(effects_reply);
        sd_bus_error_free(&error);

        sleep(1);
    }

    return 0;
}

static void print_help(void)
{
    printf("\n%s%smaxctl%s - Control NVIDIA Maxine Audio Effects\n\n",
           COL(C_BOLD), COL(C_CYAN), COL(C_RESET));

    printf("%sUsage:%s maxctl <command> [arguments]\n\n", COL(C_BOLD),
           COL(C_RESET));

    printf("%sCommands:%s\n", COL(C_BOLD), COL(C_RESET));
    printf("  %sstatus%s                          Show daemon status, GPU info, active effects\n",
           COL(C_YELLOW), COL(C_RESET));
    printf("  %seffects%s                         List all available effects with descriptions\n",
           COL(C_YELLOW), COL(C_RESET));
    printf("  %sdevices%s                         List PipeWire audio devices\n",
           COL(C_YELLOW), COL(C_RESET));
    printf("  %senable%s <effect>                 Enable an effect in the chain\n",
           COL(C_YELLOW), COL(C_RESET));
    printf("  %sdisable%s <effect>                Disable an effect (passthrough)\n",
           COL(C_YELLOW), COL(C_RESET));
    printf("  %sset%s <effect> intensity <0-1>    Set effect intensity\n",
           COL(C_YELLOW), COL(C_RESET));
    printf("  %smonitor%s                         Live stats (refresh every second)\n",
           COL(C_YELLOW), COL(C_RESET));
    printf("  %shelp%s                            Show this help\n",
           COL(C_YELLOW), COL(C_RESET));

    printf("\n%sExamples:%s\n", COL(C_BOLD), COL(C_RESET));
    printf("  maxctl enable denoiser\n");
    printf("  maxctl set denoiser intensity 0.8\n");
    printf("  maxctl disable dereverb\n");
    printf("  maxctl monitor\n");

    printf("\n%sEffect IDs:%s (use 'maxctl effects' for full list)\n",
           COL(C_BOLD), COL(C_RESET));
    // Show just the main (non-chained) effects for brevity
    for (int i = 0; i < 10 && effect_catalog[i].id; i++) {
        printf("  %s%-20s%s %s\n", COL(C_YELLOW), effect_catalog[i].id,
               COL(C_RESET), effect_catalog[i].name);
    }

    printf("\n");
}

int main(int argc, char *argv[])
{
    // Detect if output is a terminal
    use_color = isatty(fileno(stdout));

    if (argc < 2) {
        print_help();
        return 1;
    }

    const char *cmd = argv[1];

    // Commands that don't need D-Bus
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 ||
        strcmp(cmd, "-h") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(cmd, "effects") == 0)
        return cmd_effects();

    // Commands that need D-Bus
    sd_bus *bus = bus_connect();
    if (!bus)
        return 1;

    if (!daemon_is_running(bus)) {
        fprintf(stderr, "%serror:%s maxined daemon is not running\n",
                COL(C_RED), COL(C_RESET));
        fprintf(stderr, "%shint:%s  start it with: systemctl --user start maxine-pipewire\n",
                COL(C_DIM), COL(C_RESET));
        fprintf(stderr, "%shint:%s  or run directly: maxined --foreground\n",
                COL(C_DIM), COL(C_RESET));
        sd_bus_unref(bus);
        return 1;
    }

    int ret = 0;

    if (strcmp(cmd, "status") == 0) {
        ret = cmd_status(bus);
    } else if (strcmp(cmd, "devices") == 0) {
        ret = cmd_devices(bus);
    } else if (strcmp(cmd, "enable") == 0) {
        if (argc < 3) {
            fprintf(stderr, "%serror:%s missing effect name\n",
                    COL(C_RED), COL(C_RESET));
            fprintf(stderr, "usage: maxctl enable <effect>\n");
            fprintf(stderr, "run 'maxctl effects' to see available effects\n");
            ret = 1;
        } else {
            ret = cmd_enable(bus, argv[2]);
        }
    } else if (strcmp(cmd, "disable") == 0) {
        if (argc < 3) {
            fprintf(stderr, "%serror:%s missing effect name\n",
                    COL(C_RED), COL(C_RESET));
            fprintf(stderr, "usage: maxctl disable <effect>\n");
            ret = 1;
        } else {
            ret = cmd_disable(bus, argv[2]);
        }
    } else if (strcmp(cmd, "set") == 0) {
        if (argc < 5) {
            fprintf(stderr, "%serror:%s not enough arguments\n",
                    COL(C_RED), COL(C_RESET));
            fprintf(stderr, "usage: maxctl set <effect> intensity <0.0-1.0>\n");
            ret = 1;
        } else {
            ret = cmd_set(bus, argv[2], argv[3], argv[4]);
        }
    } else if (strcmp(cmd, "monitor") == 0) {
        ret = cmd_monitor(bus);
    } else {
        fprintf(stderr, "%serror:%s unknown command '%s'\n",
                COL(C_RED), COL(C_RESET), cmd);
        fprintf(stderr, "run 'maxctl help' for available commands\n");
        ret = 1;
    }

    sd_bus_unref(bus);
    return ret;
}

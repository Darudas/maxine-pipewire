// maxined - NVIDIA Maxine Audio Effects Daemon
//
// Manages a chain of Maxine AFX audio effects as PipeWire filter nodes.
// Exposes D-Bus interface for runtime control via maxctl.
//
// Usage: maxined [--config PATH] [--verbose] [--foreground]

#include "maxine_loader.h"
#include "maxine_config.h"
#include "maxine_effect_registry.h"
#include "maxine_audio_chain.h"
#include "maxine_audio_node.h"
#include "maxine_dbus.h"
#include "nvafx_types.h"

#include <pipewire/pipewire.h>

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_MODEL_PATH  "/opt/maxine-afx/features"
#define DEFAULT_SAMPLE_RATE 48000

// --- PipeWire loop integration for D-Bus fd ---

struct dbus_source {
    struct maxine_dbus *dbus;
    struct spa_source *source;
};

static void on_dbus_io(void *data, int fd, uint32_t mask)
{
    struct dbus_source *ds = data;
    (void)fd;
    (void)mask;
    maxine_dbus_dispatch(ds->dbus);
}

// --- Signal handling via PipeWire loop ---

static void on_signal_quit(void *data, int signal_number)
{
    struct pw_main_loop *loop = data;
    (void)signal_number;
    pw_main_loop_quit(loop);
}

// --- Usage ---

static void print_usage(const char *progname)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "\n"
        "NVIDIA Maxine Audio Effects Daemon\n"
        "\n"
        "Options:\n"
        "  -c, --config PATH   Config file path\n"
        "                      (default: ~/.config/maxine-pipewire/config.toml)\n"
        "  -v, --verbose       Enable verbose/debug logging\n"
        "  -f, --foreground    Run in foreground (default, for systemd)\n"
        "  -h, --help          Show this help\n",
        progname);
}

// --- Main ---

int main(int argc, char *argv[])
{
    char config_path[4096] = "";
    bool verbose = false;

    static const struct option long_options[] = {
        { "config",     required_argument, NULL, 'c' },
        { "verbose",    no_argument,       NULL, 'v' },
        { "foreground", no_argument,       NULL, 'f' },
        { "help",       no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:vfh", long_options, NULL)) != -1) {
        switch (opt) {
        case 'c':
            snprintf(config_path, sizeof(config_path), "%s", optarg);
            break;
        case 'v':
            verbose = true;
            break;
        case 'f':
            // Already foreground by default
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    // Default config path
    if (config_path[0] == '\0') {
        if (!maxine_config_default_path(config_path, sizeof(config_path))) {
            fprintf(stderr, "maxined: HOME not set, cannot determine config path\n");
            return 1;
        }
    }

    // Ensure config exists (create default if needed)
    if (maxine_config_write_default(config_path) < 0 &&
        access(config_path, F_OK) != 0) {
        fprintf(stderr, "maxined: no config file, using built-in defaults\n");
    }

    // Parse config
    struct maxine_config cfg;
    maxine_config_init(&cfg);
    if (access(config_path, F_OK) == 0) {
        if (maxine_config_parse(&cfg, config_path) < 0)
            fprintf(stderr, "maxined: config parse failed, using defaults\n");
        else
            fprintf(stderr, "maxined: loaded config from %s\n", config_path);
    }

    // Read general settings
    const char *model_path = maxine_config_get(&cfg, "general", "model_path");
    if (!model_path)
        model_path = DEFAULT_MODEL_PATH;

    const char *sdk_lib_path = maxine_config_get(&cfg, "general", "sdk_lib_path");

    uint32_t sample_rate = maxine_config_get_uint(&cfg, "general", "sample_rate",
                                                  DEFAULT_SAMPLE_RATE);
    if (sample_rate != 16000 && sample_rate != 48000) {
        fprintf(stderr, "maxined: unsupported sample_rate %u (need 16000 or 48000), using %u\n",
                sample_rate, DEFAULT_SAMPLE_RATE);
        sample_rate = DEFAULT_SAMPLE_RATE;
    }

    const char *source_device = maxine_config_get(&cfg, "general", "source_device");
    const char *virtual_name = maxine_config_get(&cfg, "general", "virtual_source_name");
    if (!virtual_name)
        virtual_name = "Maxine Clean Mic";

    fprintf(stderr, "maxined: starting (model_path=%s, rate=%u)\n",
            model_path, sample_rate);

    // Initialize PipeWire
    pw_init(&argc, &argv);

    if (verbose)
        pw_log_set_level(SPA_LOG_LEVEL_DEBUG);

    struct pw_main_loop *main_loop = pw_main_loop_new(NULL);
    if (!main_loop) {
        fprintf(stderr, "maxined: failed to create PipeWire main loop\n");
        pw_deinit();
        return 1;
    }

    struct pw_loop *loop = pw_main_loop_get_loop(main_loop);

    struct pw_context *context = pw_context_new(loop, NULL, 0);
    if (!context) {
        fprintf(stderr, "maxined: failed to create PipeWire context\n");
        pw_main_loop_destroy(main_loop);
        pw_deinit();
        return 1;
    }

    struct pw_core *core = pw_context_connect(context, NULL, 0);
    if (!core) {
        fprintf(stderr, "maxined: cannot connect to PipeWire -- is pipewire running?\n");
        pw_context_destroy(context);
        pw_main_loop_destroy(main_loop);
        pw_deinit();
        return 1;
    }

    fprintf(stderr, "maxined: connected to PipeWire\n");

    // Load Maxine SDK
    struct maxine_sdk sdk;
    if (!maxine_sdk_load(&sdk, sdk_lib_path)) {
        fprintf(stderr, "maxined: failed to load Maxine SDK\n");
        fprintf(stderr, "maxined: ensure libnv_audiofx.so is in LD_LIBRARY_PATH\n");
        fprintf(stderr, "maxined: or set sdk_lib_path in %s\n", config_path);
        pw_core_disconnect(core);
        pw_context_destroy(context);
        pw_main_loop_destroy(main_loop);
        pw_deinit();
        return 1;
    }

    fprintf(stderr, "maxined: Maxine SDK loaded\n");

    // Create audio chain
    struct maxine_audio_chain *chain = maxine_audio_chain_create(loop, &sdk);
    if (!chain) {
        fprintf(stderr, "maxined: failed to create audio chain\n");
        maxine_sdk_unload(&sdk);
        pw_core_disconnect(core);
        pw_context_destroy(context);
        pw_main_loop_destroy(main_loop);
        pw_deinit();
        return 1;
    }

    chain->sample_rate = sample_rate;
    snprintf(chain->model_path, sizeof(chain->model_path), "%s", model_path);
    if (source_device)
        snprintf(chain->source_device, sizeof(chain->source_device), "%s",
                 source_device);
    snprintf(chain->virtual_source_name, sizeof(chain->virtual_source_name),
             "%s", virtual_name);

    // Load effects from config using the effect registry
    const struct maxine_effect_info *all_effects = NULL;
    int n_effects = maxine_effect_list_all(&all_effects);
    int effects_loaded = 0;

    for (int i = 0; i < n_effects; i++) {
        const struct maxine_effect_info *info = &all_effects[i];

        // Only load audio effects (skip video)
        if (info->type != MAXINE_EFFECT_AFX)
            continue;

        // Check if this effect has a config section
        char section[128];
        snprintf(section, sizeof(section), "effect.%s", info->id);

        bool enabled = maxine_config_get_bool(&cfg, section, "enabled", false);
        if (!enabled)
            continue;

        // Verify sample rate compatibility
        if (sample_rate == 48000 && !info->supports_48k) {
            fprintf(stderr, "maxined: effect '%s' does not support 48kHz, skipping\n",
                    info->id);
            continue;
        }
        if (sample_rate == 16000 && !info->supports_16k) {
            fprintf(stderr, "maxined: effect '%s' does not support 16kHz, skipping\n",
                    info->id);
            continue;
        }

        float intensity = maxine_config_get_float(&cfg, section, "intensity",
                                                  info->has_intensity ? 1.0f : 0.0f);
        bool enable_vad = maxine_config_get_bool(&cfg, section, "enable_vad", false);

        struct maxine_audio_node_config node_cfg = {
            .effect_id       = info->id,
            .effect_selector = info->effect_selector,
            .model_subdir    = info->model_subdir,
            .model_path      = model_path,
            .sdk_lib_path    = sdk_lib_path,
            .sample_rate     = sample_rate,
            .intensity       = intensity,
            .enable_vad      = enable_vad && info->has_vad,
        };

        if (maxine_audio_chain_add_effect(chain, &node_cfg) == 0) {
            effects_loaded++;
            fprintf(stderr, "maxined: loaded '%s' (%s) intensity=%.2f\n",
                    info->id, info->display_name, intensity);
        } else {
            fprintf(stderr, "maxined: failed to load effect '%s'\n", info->id);
        }
    }

    fprintf(stderr, "maxined: %d effect(s) loaded into chain\n", effects_loaded);

    // Set up D-Bus
    struct maxine_dbus dbus;
    int dbus_ok = maxine_dbus_init(&dbus, chain);
    if (dbus_ok < 0) {
        fprintf(stderr, "maxined: D-Bus init failed (continuing without D-Bus control)\n");
    }

    // Integrate D-Bus fd into PipeWire loop
    struct dbus_source dbus_src = { .dbus = &dbus, .source = NULL };
    if (dbus_ok >= 0) {
        int dbus_fd = maxine_dbus_get_fd(&dbus);
        if (dbus_fd >= 0) {
            dbus_src.source = pw_loop_add_io(loop, dbus_fd,
                                              SPA_IO_IN | SPA_IO_ERR | SPA_IO_HUP,
                                              false, on_dbus_io, &dbus_src);
            if (!dbus_src.source)
                fprintf(stderr, "maxined: failed to add D-Bus fd to event loop\n");
        }
    }

    // Register signal handlers via PipeWire loop
    pw_loop_add_signal(loop, SIGINT, on_signal_quit, main_loop);
    pw_loop_add_signal(loop, SIGTERM, on_signal_quit, main_loop);

    fprintf(stderr, "maxined: running (PID %d)\n", getpid());

    // Run main loop (blocks until quit)
    pw_main_loop_run(main_loop);

    fprintf(stderr, "maxined: shutting down...\n");

    // Cleanup
    if (dbus_src.source)
        pw_loop_destroy_source(loop, dbus_src.source);

    if (dbus_ok >= 0)
        maxine_dbus_destroy(&dbus);

    maxine_audio_chain_destroy(chain);
    maxine_sdk_unload(&sdk);

    pw_core_disconnect(core);
    pw_context_destroy(context);
    pw_main_loop_destroy(main_loop);
    pw_deinit();

    fprintf(stderr, "maxined: goodbye\n");
    return 0;
}

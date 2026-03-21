#include "maxine_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void maxine_config_init(struct maxine_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
}

int maxine_config_parse(struct maxine_config *cfg, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "maxine: cannot open config '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    char line[8192];
    char current_section[64] = "";

    while (fgets(line, sizeof(line), fp)) {
        // Strip trailing whitespace
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' ||
                           line[len - 1] == ' ' || line[len - 1] == '\t'))
            line[--len] = '\0';

        // Skip empty lines and comments
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#')
            continue;

        // Section header [section]
        if (*p == '[') {
            char *end = strchr(p, ']');
            if (end) {
                *end = '\0';
                snprintf(current_section, sizeof(current_section), "%s", p + 1);
            }
            continue;
        }

        // Key = value
        char *eq = strchr(p, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key = p;
        char *val = eq + 1;

        // Trim key trailing whitespace
        size_t klen = strlen(key);
        while (klen > 0 && (key[klen - 1] == ' ' || key[klen - 1] == '\t'))
            key[--klen] = '\0';

        // Trim value leading whitespace
        while (*val == ' ' || *val == '\t') val++;

        // Strip quotes from value
        size_t vlen = strlen(val);
        if (vlen >= 2 && val[0] == '"' && val[vlen - 1] == '"') {
            val[vlen - 1] = '\0';
            val++;
        }

        if (cfg->count < MAXINE_CONFIG_MAX_ENTRIES) {
            struct maxine_config_entry *e = &cfg->entries[cfg->count++];
            snprintf(e->section, sizeof(e->section), "%s", current_section);
            snprintf(e->key, sizeof(e->key), "%s", key);
            snprintf(e->value, sizeof(e->value), "%s", val);
        }
    }

    fclose(fp);
    return 0;
}

static const char default_config_text[] =
    "# Maxine PipeWire Audio Effects Configuration\n"
    "#\n"
    "# See 'maxctl effects' for available effects and descriptions.\n"
    "# After changes: systemctl --user restart maxine-pipewire\n"
    "\n"
    "[general]\n"
    "# Path to Maxine AFX features/ directory\n"
    "model_path = \"/opt/maxine-afx/features\"\n"
    "\n"
    "# Path to directory containing libnv_audiofx.so\n"
    "# Leave commented out to use LD_LIBRARY_PATH instead.\n"
    "# sdk_lib_path = \"/opt/maxine-afx/nvafx/lib\"\n"
    "\n"
    "# Sample rate: 48000 (high quality) or 16000 (low latency)\n"
    "sample_rate = 48000\n"
    "\n"
    "# PipeWire source device to capture from (empty = default mic)\n"
    "# Find names with: pw-cli list-objects Node\n"
    "# source_device = \"alsa_input.pci-0000_c1_00.6.analog-stereo\"\n"
    "\n"
    "# Virtual source name (select this in apps as input device)\n"
    "virtual_source_name = \"Maxine Clean Mic\"\n"
    "\n"
    "\n"
    "# ============================================================================\n"
    "# Effects -- each [effect.ID] section configures one effect.\n"
    "# ID must match a valid effect identifier (see 'maxctl effects').\n"
    "# Effects process audio in the order listed here.\n"
    "# ============================================================================\n"
    "\n"
    "# Noise Removal -- removes background noise (typing, fans, etc.)\n"
    "[effect.denoiser]\n"
    "enabled = true\n"
    "intensity = 0.8\n"
    "enable_vad = false\n"
    "\n"
    "# Room Echo Removal -- removes room reverberation\n"
    "# [effect.dereverb]\n"
    "# enabled = false\n"
    "# intensity = 0.6\n"
    "\n"
    "# Combined Noise + Room Echo Removal (single pass, lower latency)\n"
    "# [effect.dereverb-denoiser]\n"
    "# enabled = false\n"
    "# intensity = 0.8\n"
    "\n"
    "# Acoustic Echo Cancellation\n"
    "# [effect.aec]\n"
    "# enabled = false\n"
    "# intensity = 1.0\n"
    "\n"
    "# Audio Super Resolution -- upscale low-quality audio\n"
    "# [effect.superres]\n"
    "# enabled = false\n"
    "\n"
    "# Studio Voice (High Quality)\n"
    "# [effect.studio-voice-hq]\n"
    "# enabled = false\n"
    "\n"
    "# Studio Voice (Low Latency)\n"
    "# [effect.studio-voice-ll]\n"
    "# enabled = false\n"
    "\n"
    "# Speaker Focus -- isolate primary speaker\n"
    "# [effect.speaker-focus]\n"
    "# enabled = false\n"
    "\n"
    "# Voice Font (High Quality) -- voice conversion\n"
    "# [effect.voice-font-hq]\n"
    "# enabled = false\n"
    "\n"
    "# Voice Font (Low Latency)\n"
    "# [effect.voice-font-ll]\n"
    "# enabled = false\n";

// mkdir -p helper
static int mkdirp(const char *path, mode_t mode)
{
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, mode);
            *p = '/';
        }
    }
    return mkdir(tmp, mode);
}

int maxine_config_write_default(const char *path)
{
    if (access(path, F_OK) == 0)
        return 0;  // already exists

    // Create parent directory
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdirp(dir, 0755);
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "maxine: failed to create default config '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    fputs(default_config_text, fp);
    fclose(fp);

    fprintf(stderr, "maxine: created default config at %s\n", path);
    return 0;
}

const char *maxine_config_get(const struct maxine_config *cfg,
                              const char *section, const char *key)
{
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].section, section) == 0 &&
            strcmp(cfg->entries[i].key, key) == 0)
            return cfg->entries[i].value;
    }
    return NULL;
}

bool maxine_config_get_bool(const struct maxine_config *cfg,
                            const char *section, const char *key, bool def)
{
    const char *val = maxine_config_get(cfg, section, key);
    if (!val)
        return def;
    return (strcmp(val, "true") == 0 || strcmp(val, "1") == 0 ||
            strcmp(val, "yes") == 0);
}

float maxine_config_get_float(const struct maxine_config *cfg,
                              const char *section, const char *key, float def)
{
    const char *val = maxine_config_get(cfg, section, key);
    if (!val)
        return def;
    return (float)atof(val);
}

uint32_t maxine_config_get_uint(const struct maxine_config *cfg,
                                const char *section, const char *key,
                                uint32_t def)
{
    const char *val = maxine_config_get(cfg, section, key);
    if (!val)
        return def;
    return (uint32_t)strtoul(val, NULL, 10);
}

char *maxine_config_default_path(char *buf, size_t buf_size)
{
    const char *home = getenv("HOME");
    if (!home)
        return NULL;

    snprintf(buf, buf_size, "%s/.config/maxine-pipewire/config.toml", home);
    return buf;
}

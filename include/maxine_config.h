// Configuration parser for maxine-pipewire
// Simple TOML-subset parser: [section], key = "value", key = number, key = true/false

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAXINE_CONFIG_MAX_ENTRIES 256

struct maxine_config_entry {
    char section[64];
    char key[128];
    char value[4096];
};

struct maxine_config {
    struct maxine_config_entry entries[MAXINE_CONFIG_MAX_ENTRIES];
    int count;
};

// Initialize an empty config.
void maxine_config_init(struct maxine_config *cfg);

// Parse a TOML file. Returns 0 on success, -1 on error.
int maxine_config_parse(struct maxine_config *cfg, const char *path);

// Look up a string value. Returns NULL if not found.
const char *maxine_config_get(const struct maxine_config *cfg,
                              const char *section, const char *key);

// Look up a boolean value. Returns def if not found.
bool maxine_config_get_bool(const struct maxine_config *cfg,
                            const char *section, const char *key, bool def);

// Look up a float value. Returns def if not found.
float maxine_config_get_float(const struct maxine_config *cfg,
                              const char *section, const char *key, float def);

// Look up an unsigned int value. Returns def if not found.
uint32_t maxine_config_get_uint(const struct maxine_config *cfg,
                                const char *section, const char *key,
                                uint32_t def);

// Get the default config file path (~/.config/maxine-pipewire/config.toml).
// Writes into buf, returns buf on success, NULL on failure.
char *maxine_config_default_path(char *buf, size_t buf_size);

// Write the default config template to the given path, creating directories.
// Returns 0 on success, -1 on error.
int maxine_config_write_default(const char *path);

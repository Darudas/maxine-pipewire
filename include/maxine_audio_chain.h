// Manages an ordered chain of audio effect nodes.
// Each node is a PipeWire filter; the chain connects them in series.

#pragma once

#include "maxine_audio_node.h"

#define MAXINE_MAX_CHAIN_NODES 16

struct maxine_audio_chain {
    struct pw_loop *loop;
    struct maxine_sdk *sdk;
    struct maxine_audio_node *nodes[MAXINE_MAX_CHAIN_NODES];
    int node_count;
    char source_device[256];         // target mic device name
    char virtual_source_name[128];   // name of virtual source
    uint32_t sample_rate;
    char model_path[4096];
};

// Create an empty audio chain.
struct maxine_audio_chain *maxine_audio_chain_create(
    struct pw_loop *loop,
    struct maxine_sdk *sdk);

// Destroy chain and all nodes.
void maxine_audio_chain_destroy(struct maxine_audio_chain *chain);

// Add an effect to the end of the chain. Returns 0 on success.
int maxine_audio_chain_add_effect(
    struct maxine_audio_chain *chain,
    const struct maxine_audio_node_config *config);

// Remove an effect by ID. Returns 0 on success, -1 if not found.
int maxine_audio_chain_remove_effect(
    struct maxine_audio_chain *chain,
    const char *effect_id);

// Find a node by effect ID. Returns NULL if not found.
struct maxine_audio_node *maxine_audio_chain_find(
    struct maxine_audio_chain *chain,
    const char *effect_id);

// Enable or disable an effect by ID. Returns 0 on success, -1 if not found.
int maxine_audio_chain_set_enabled(
    struct maxine_audio_chain *chain,
    const char *effect_id,
    bool enabled);

// Set intensity for an effect by ID. Returns 0 on success, -1 if not found.
int maxine_audio_chain_set_intensity(
    struct maxine_audio_chain *chain,
    const char *effect_id,
    float intensity);

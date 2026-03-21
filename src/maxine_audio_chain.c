#include "maxine_audio_chain.h"
#include "maxine_loader.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct maxine_audio_chain *maxine_audio_chain_create(
    struct pw_loop *loop,
    struct maxine_sdk *sdk)
{
    struct maxine_audio_chain *chain = calloc(1, sizeof(*chain));
    if (!chain)
        return NULL;

    chain->loop = loop;
    chain->sdk = sdk;
    chain->node_count = 0;
    chain->sample_rate = 48000;

    snprintf(chain->model_path, sizeof(chain->model_path),
             "/opt/maxine-afx/features");
    snprintf(chain->virtual_source_name, sizeof(chain->virtual_source_name),
             "Maxine Clean Mic");

    return chain;
}

void maxine_audio_chain_destroy(struct maxine_audio_chain *chain)
{
    if (!chain)
        return;

    for (int i = 0; i < chain->node_count; i++) {
        maxine_audio_node_destroy(chain->nodes[i]);
        chain->nodes[i] = NULL;
    }
    chain->node_count = 0;

    free(chain);
}

int maxine_audio_chain_add_effect(
    struct maxine_audio_chain *chain,
    const struct maxine_audio_node_config *config)
{
    if (!chain || !config)
        return -1;

    if (chain->node_count >= MAXINE_MAX_CHAIN_NODES) {
        fprintf(stderr, "maxine: chain full (%d/%d nodes)\n",
                chain->node_count, MAXINE_MAX_CHAIN_NODES);
        return -1;
    }

    // Check for duplicate effect_id
    if (maxine_audio_chain_find(chain, config->effect_id)) {
        fprintf(stderr, "maxine: effect '%s' already in chain\n",
                config->effect_id);
        return -1;
    }

    struct maxine_audio_node *node = maxine_audio_node_create(
        chain->loop, chain->sdk, config);
    if (!node) {
        fprintf(stderr, "maxine: failed to create node for '%s'\n",
                config->effect_id);
        return -1;
    }

    chain->nodes[chain->node_count++] = node;

    fprintf(stderr, "maxine: added '%s' to chain (position %d)\n",
            config->effect_id, chain->node_count - 1);
    return 0;
}

int maxine_audio_chain_remove_effect(
    struct maxine_audio_chain *chain,
    const char *effect_id)
{
    if (!chain || !effect_id)
        return -1;

    for (int i = 0; i < chain->node_count; i++) {
        if (strcmp(chain->nodes[i]->effect_id, effect_id) == 0) {
            maxine_audio_node_destroy(chain->nodes[i]);

            // Shift remaining nodes down
            for (int j = i; j < chain->node_count - 1; j++)
                chain->nodes[j] = chain->nodes[j + 1];

            chain->nodes[--chain->node_count] = NULL;

            fprintf(stderr, "maxine: removed '%s' from chain\n", effect_id);
            return 0;
        }
    }

    fprintf(stderr, "maxine: effect '%s' not found in chain\n", effect_id);
    return -1;
}

struct maxine_audio_node *maxine_audio_chain_find(
    struct maxine_audio_chain *chain,
    const char *effect_id)
{
    if (!chain || !effect_id)
        return NULL;

    for (int i = 0; i < chain->node_count; i++) {
        if (strcmp(chain->nodes[i]->effect_id, effect_id) == 0)
            return chain->nodes[i];
    }

    return NULL;
}

int maxine_audio_chain_set_enabled(
    struct maxine_audio_chain *chain,
    const char *effect_id,
    bool enabled)
{
    struct maxine_audio_node *node = maxine_audio_chain_find(chain, effect_id);
    if (!node) {
        fprintf(stderr, "maxine: effect '%s' not found\n", effect_id);
        return -1;
    }

    maxine_audio_node_set_enabled(node, enabled);
    return 0;
}

int maxine_audio_chain_set_intensity(
    struct maxine_audio_chain *chain,
    const char *effect_id,
    float intensity)
{
    struct maxine_audio_node *node = maxine_audio_chain_find(chain, effect_id);
    if (!node) {
        fprintf(stderr, "maxine: effect '%s' not found\n", effect_id);
        return -1;
    }

    maxine_audio_node_set_intensity(node, intensity);
    return 0;
}

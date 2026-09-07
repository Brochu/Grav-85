#pragma once
#include "qg_shared.hpp"
#include "qg_shared_types.hpp"
#include "qg_memory.hpp"

#define CONFIG_NUM_KEYS 128

struct config {
    const char *keys[CONFIG_NUM_KEYS];
    config_value *values[CONFIG_NUM_KEYS];
    u64 num_entries = 0;

    mem_arena _mem_vals;
};

void config_init(config *c, const char *file);
void config_free(config *c);

bool config_read(config *c, const char *key, config_value *out);

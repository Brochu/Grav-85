#pragma once
#include "shared_types.hpp"
#include "qg_memory.hpp"

enum class value_type : u8 {
    SINGLE,
    RANGE,
    ARRAY,
    STRING,
};

struct config_value {
    value_type type;

    union {
        i32 single;

        struct {
            i32 min;
            i32 max;
        } range;

        struct {
            i32 *arr;
            u64 len;
        } array;

        struct {
            const char *arr;
            u64 len;
        } str;
    };
};

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

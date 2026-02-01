#include "qg_config.hpp"
#include "qg_memory.hpp"
#include "qg_parse.hpp"

#include "SDL3/SDL.h"
#include <cstdlib>

#define CONFIG_ALLOC_SIZE 1024*2

void config_init(config *c, const char *file) {
    mem_arena_init(&c->_mem_vals, CONFIG_ALLOC_SIZE);

    u64 file_size = 0;
    char *content = (char *)SDL_LoadFile(file, &file_size);

    char *next = NULL;
    char *token = strtok_s(content, "\r\n", &next);
    while (token) {
        if (token[0] == '#') {
            token = strtok_s(NULL, "\r\n", &next);
            continue;
        }

        strview l, r;
        bool res = sv_split_once(sv(token), " = ", &l, &r);
        if (res) {
            assert(c->num_entries < CONFIG_NUM_KEYS);

            arena_ptr pname = mem_arena_alloc(&c->_mem_vals, l.len+1, 1);
            strncpy_s((char*)pname.p, l.len+1, l.ptr, l.len);
            c->keys[c->num_entries] = (const char*)pname.p;


            config_value *val = (config_value*)mem_arena_alloc(&c->_mem_vals, sizeof(config_value)).p;
            if (r.ptr[0] == '[') {
                val->type = value_type::RANGE;
                strview min, max;
                sv_split_once(r, ",", &min, &max);
                val->range.min = atoi(min.ptr+1);
                val->range.max = atoi(max.ptr);
            }
            else if (r.ptr[0] == '"') {
                val->type = value_type::STRING;
                arena_ptr str = mem_arena_alloc(&c->_mem_vals, r.len-1, 1);
                strncpy_s((char*)str.p, r.len-1, r.ptr+1, r.len-2);
                val->str.arr = (char*)str.p;
                val->str.len = r.len-2;
            }
            else if (sv_find(r, ",").ptr) {
                val->type = value_type::ARRAY;
                strview elem[64];
                u64 num = sv_split(r, ",", elem, 64);
                assert(num > 0);

                val->array.arr = (i32*)mem_arena_alloc(&c->_mem_vals, sizeof(i32) * num).p;
                for (int i = 0; i < num; i++) {
                    val->array.arr[i] = atoi(elem[i].ptr);
                }
                val->array.len = num;
            }
            else {
                val->type = value_type::SINGLE;
                val->single = atoi(r.ptr);
            }
            c->values[c->num_entries++] = val;
        }
        //TODO: Parse line by line
        // Split at '='
        // detect CSVs; split at ',' if the case

        token = strtok_s(NULL, "\r\n", &next);
    }

    SDL_free(content);
}

void config_free(config *c) {
    mem_arena_clear(&c->_mem_vals);
}

bool config_read(config *c, const char *key, config_value *out) {
    for (int i = 0; i < c->num_entries; i++) {
        if (strcmp(c->keys[i], key) == 0) {
            *out = *c->values[i];
            return true;
        }
    }

    return false;
}

#pragma once
#include "qg_shared.hpp"
#include "qg_shared_types.hpp"

#include <cassert>

void *qg_malloc(u64 sz);
void *qg_calloc(u64 count, u64 sz);
void *qg_realloc(void *ptr, u64 sz);
void qg_free(void *ptr);

struct mem_arena {
    u8 *base = nullptr;
    u64 next;
    u64 cap;
    u64 gen;

    //TODO: Maybe adding tracking information here, updated only for debug builds
};

mem_arena *mem_arena_create(u64 max_size);
void mem_arena_reset(mem_arena *arena);
void mem_arena_destroy(mem_arena *arena);

arena_ptr mem_arena_alloc(mem_arena *arena, u64 size, u64 align = sizeof(void *));
arena_off mem_arena_offloc(mem_arena *arena, u64 size, u64 align = sizeof(void *));

void *mem_arena_at(mem_arena *arena, arena_off off);

#pragma once
#include "shared_types.hpp"

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
};

struct arena_ptr {
    u8 *p;
    u64 gen;
};

struct arena_off {
    u64 off;
    u64 gen;
};

void mem_arena_init(mem_arena *arena, u64 max_size);
void mem_arena_reset(mem_arena *arena);
void mem_arena_clear(mem_arena *arena);

arena_ptr mem_arena_alloc(mem_arena *arena, u64 size, u64 align = sizeof(void *));
arena_off mem_arena_offloc(mem_arena *arena, u64 size, u64 align = sizeof(void *));

template<class T>
static inline T *mem_arena_at(mem_arena *arena, arena_off offset) {
    assert(arena->gen == offset.gen && "Trying to access stale offset in arena");
    return reinterpret_cast<T *>(arena->base + offset.off);
}

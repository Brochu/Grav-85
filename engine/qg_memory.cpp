#include "qg_memory.hpp"
#include <cstdlib>

//TODO: Add memory tracking w/ profiler? Tracy
void *qg_malloc(u64 size) {
    return malloc(size);
}

void *qg_calloc(u64 count, u64 size) {
    return calloc(count, size);
}

void *qg_realloc(void *ptr, u64 size) {
    return realloc(ptr, size);
}

void qg_free(void *ptr) {
    free(ptr);
}

// MEMORY ARENA -----------------------------------

static inline u64 align_fwd(u64 ptr, u64 align) {
    assert((align & (align - 1)) == 0 && "alignment must be power of two");

    u64 m = align - 1;
    return (ptr + m) & ~m;
}

void mem_arena_init(mem_arena *arena, u64 max_size) {
    arena->base = (u8*)qg_malloc(max_size);
    if (arena->base == nullptr) {
        assert(false && "ASSERT: Could not allocate new mem_arena");
    }
    arena->next = 0;
    arena->cap = max_size;
}

void mem_arena_reset(mem_arena *arena) {
    //TODO: Maybe adding tracking information here, updated only for debug builds
    arena->next = 0;
    arena->gen++;
}

void mem_arena_clear(mem_arena *arena) {
    //TODO: Maybe adding tracking information here, updated only for debug builds
    qg_free(arena->base);
    arena->base = nullptr;

    arena->next = 0;
    arena->cap = 0;
    arena->gen++;
}

arena_ptr mem_arena_alloc(mem_arena *arena, u64 size, u64 align) {
    //TODO: Maybe adding tracking information here, updated only for debug builds
    u64 off = align_fwd(arena->next, align);

    if (off + size > arena->cap) {
        assert(false && "ASSERT: mem_arena ran out of allocated memory");
        return { nullptr, arena->gen };
    }
    u8 *ptr = arena->base + off;
    arena->next = off + size;
    return { ptr, arena->gen };
}

arena_off mem_arena_offloc(mem_arena *arena, u64 size, u64 align) {
    u64 off = align_fwd(arena->next, align);

    if (off + size > arena->cap) {
        assert(false && "ASSERT: mem_arena ran out of allocated memory");
        return { UINT64_MAX, arena->gen }; //TODO: This is dangerous, need to rethink invalid offsets
    }

    arena->next = off + size;
    return { off, arena->gen };
}

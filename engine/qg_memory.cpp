#include "qg_memory.hpp"
#include <cstdlib>
#include <cstddef>

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

static const u64 ARENA_HEADER_ALIGN = alignof(std::max_align_t);
static inline u64 align_fwd(u64 ptr, u64 align) {
    assert((align & (align - 1)) == 0 && "alignment must be power of two");

    u64 m = align - 1;
    return (ptr + m) & ~m;
}

mem_arena *mem_arena_create(u64 max_size) {
    u64 header_size = align_fwd(sizeof(mem_arena), ARENA_HEADER_ALIGN);
    u8 *alloc = (u8 *)qg_calloc(1, header_size + max_size);
    if (alloc == nullptr) {
        assert(false && "ASSERT: Could not allocate new mem_arena");
        return nullptr;
    }
    mem_arena *arena = (mem_arena *)alloc;

    arena->base = alloc + header_size;
    arena->next = 0;
    arena->cap = max_size;
    arena->gen = 1;
    return arena;
}

void mem_arena_reset(mem_arena *arena) {
    //TODO: Maybe adding tracking information here, updated only for debug builds
    arena->next = 0;
    arena->gen++;
}

void mem_arena_destroy(mem_arena *arena) {
    if (arena == nullptr) return;

    //TODO: Maybe adding tracking information here, updated only for debug builds
    arena->base = nullptr;
    arena->next = 0;
    arena->cap = 0;
    arena->gen++;
    qg_free(arena);
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

void *mem_arena_at(mem_arena *arena, arena_off off) {
    assert(arena->gen == off.gen && "Trying to access stale offset in arena");
    return reinterpret_cast<void *>(arena->base + off.off);
}

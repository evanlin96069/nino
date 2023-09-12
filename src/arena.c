#include "arena.h"

#include <stdlib.h>
#include <string.h>

#include "utils.h"

void arenaInit(Arena* arena, size_t first_block_size) {
    memset(arena, 0, sizeof(Arena));
    arena->first_block_size = first_block_size;
}

void arenaDeinit(Arena* arena) {
    for (size_t i = 0; i < arena->block_count; i++) {
        free(arena->blocks[i].data);
    }
    free(arena->blocks);
    memset(arena, 0, sizeof(Arena));
}

void arenaReset(Arena* arena) {
    for (size_t i = 0; i < arena->block_count; i++) {
        arena->blocks[i].size = 0;
    }
    arena->current_block = 0;
}

static inline size_t alignment_loss(size_t bytes_allocated, size_t alignment) {
    size_t offset = bytes_allocated & (alignment - 1);
    if (offset == 0)
        return 0;
    return alignment - offset;
}

static inline size_t block_bytes_left(ArenaBlock* blk) {
    size_t inc = alignment_loss(blk->size, ARENA_ALIGNMENT);
    return blk->capacity - (blk->size + inc);
}

static inline ArenaBlock* get_last_block(Arena* arena) {
    if (arena->block_count == 0)
        return NULL;
    return &arena->blocks[arena->block_count - 1];
}

static void alloc_new_block(Arena* arena, size_t requested_size) {
    size_t allocated_size;
    if (arena->block_count == 0) {
        allocated_size = arena->first_block_size;
    } else {
        allocated_size = get_last_block(arena)->capacity;
    }

    if (allocated_size < 1)
        allocated_size = 1;

    while (allocated_size < requested_size) {
        allocated_size *= 2;
    }

    if (allocated_size > UINT32_MAX)
        allocated_size = UINT32_MAX;

    arena->block_count++;
    arena->blocks =
        realloc_s(arena->blocks, sizeof(ArenaBlock) * arena->block_count);
    ArenaBlock* blk = get_last_block(arena);
    blk->data = malloc_s(allocated_size);
    blk->size = 0;
    blk->capacity = allocated_size;
}

void* arenaAlloc(Arena* arena, size_t size) {
    if (size == 0)
        return NULL;

    if (arena->block_count == 0)
        alloc_new_block(arena, size);

    while (block_bytes_left(&arena->blocks[arena->current_block]) < size) {
        arena->current_block++;
        if (arena->current_block >= arena->block_count) {
            alloc_new_block(arena, size);
            break;
        }
    }

    ArenaBlock* blk = &arena->blocks[arena->current_block];
    size_t inc = alignment_loss(blk->size, ARENA_ALIGNMENT);
    void* out = (uint8_t*)blk->data + blk->size + inc;
    blk->size += size + inc;
    return out;
}

void* arenaRealloc(Arena* arena, void* ptr, size_t old_size, size_t size) {
    if (!ptr || !arena->blocks)
        return arenaAlloc(arena, size);

    if (old_size >= size)
        return ptr;

    ArenaBlock* blk = &arena->blocks[arena->current_block];
    uint8_t* prev = (uint8_t*)blk->data + blk->size - old_size;
    uint32_t bytes_left = blk->capacity - blk->size + old_size;
    void* new_arr;
    if (prev == (uint8_t*)ptr && bytes_left >= size) {
        blk->size -= old_size;
        new_arr = arenaAlloc(arena, size);
    } else {
        new_arr = arenaAlloc(arena, size);
        memcpy(new_arr, ptr, old_size);
    }
    return new_arr;
}

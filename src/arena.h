#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

#define ARENA_ALIGNMENT 16

typedef struct ArenaBlock {
    uint32_t size;
    uint32_t capacity;
    void* data;
} ArenaBlock;

typedef struct Arena {
    ArenaBlock* blocks;
    uint32_t block_count;
    uint32_t first_block_size;
    uint32_t current_block;
} Arena;

void arenaInit(Arena* arena, size_t first_block_size);
void arenaDeinit(Arena* arena);
void arenaReset(Arena* arena);

void* arenaAlloc(Arena* arena, size_t size);
void* arenaRealloc(Arena* arena, void* ptr, size_t old_size, size_t size);

#endif

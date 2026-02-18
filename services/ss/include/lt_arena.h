/*  lt_arena.h - Single file library for my arena allocator implementation
    Copyright (C) 2026 splatte.dev

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#ifndef LT_ARENA_H
#define LT_ARENA_H

#include "lt_base.h"

/* API Definitions */
typedef struct {
    u64 capacity;
    u64 pos;
} mem_arena;

mem_arena* arena_create(u64 capacity);
void arena_destroy(mem_arena* arena);
u64 arena_align_forward(u64 pos, u64 alignment);
void* arena_push(mem_arena* arena, u64 size);
void arena_clear(mem_arena* arena);
u64 arena_mark(mem_arena* arena);
void arena_pop(mem_arena* arena, u64 mark);

/* API Implementations */

// TODO(laith): add a debug flag that will fill unused memory with a byte like 0xCD
// and cleared memory with a byte like 0xDD

#if defined(LT_ARENA_IMPLEMENTATION)

#include <stdlib.h>
#include <string.h>

mem_arena* arena_create(u64 capacity) {
    mem_arena* arena = (mem_arena*)malloc(capacity);
    // TODO(laith): null check here?

    arena->capacity = capacity;
    // points the starting position to the end of the arena struct in address space
    arena->pos = sizeof(mem_arena);

    return arena;
}

void arena_destroy(mem_arena* arena) {
    free(arena);
}

u64 arena_align_forward(u64 pos, u64 alignment) {
    // align the current position up to a power of two, based on the alignment size
    return (pos + (alignment - 1)) & ~(alignment - 1);
}

void* arena_push(mem_arena* arena, u64 size) {
    u64 current_pos = arena->pos;
    // TODO (laith): look into cpu intricaces and why position alignment being in a power of 2
    // is necessary for performance
    u64 aligned_pos = arena_align_forward(current_pos, sizeof(void*));
    u64 new_pos = aligned_pos + size;

    if (new_pos > arena->capacity) {
        // TODO (laith): might want to handle this more gracefully, but if you get to this
        // point its too far gone
        return NULL;
    }

    arena->pos = new_pos;

    // the pointer to the arena itself is the start to our chunk of memory
    // returning where the new aligned position is now gives an empty chunk
    // of allocated memory up until the arena->new_pos
    u8* block = (u8*)arena + aligned_pos;
    // 0 out the memory from the aligned position up until arena->new_pos, which is
    // of length "size"
    memset(block, 0, size);

    return block;
}

void arena_clear(mem_arena* arena) {
    // capacity stays the same, as this is just moving the position pointer back to the start
    arena->pos = sizeof(mem_arena);
}

u64 arena_mark(mem_arena* arena) {
    return arena->pos;
}

void arena_pop(mem_arena* arena, u64 mark) {
    if (mark < sizeof(mem_arena)) {
        mark = sizeof(mem_arena);
    }

    arena->pos = mark;
}

#endif // LT_ARENA_IMPLEMENTATION
#endif // LT_ARENA_H

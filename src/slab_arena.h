#ifndef FOLDEN_SLAB_ARENA_H
#define FOLDEN_SLAB_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct folden_slab_arena folden_slab_arena_t;

folden_slab_arena_t *folden_arena_get_global(void);
void folden_arena_reset(folden_slab_arena_t *arena);

uint32_t folden_arena_alloc(folden_slab_arena_t *arena, const uint8_t *data,
                            size_t len);
uint8_t *folden_arena_ptr(folden_slab_arena_t *arena, uint32_t token);
size_t folden_arena_len(folden_slab_arena_t *arena, uint32_t token);

void folden_arena_borrow(folden_slab_arena_t *arena, uint32_t token);
void folden_arena_release_borrow(folden_slab_arena_t *arena, uint32_t token);
void folden_arena_free_token(folden_slab_arena_t *arena, uint32_t token);

#endif /* FOLDEN_SLAB_ARENA_H */

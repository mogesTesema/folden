#include "slab_arena.h"

#include <stdlib.h>
#include <string.h>

typedef struct folden_arena_block {
  uint32_t token;
  uint8_t *data;
  size_t len;
  uint32_t borrow_count;
  int live;
} folden_arena_block_t;

struct folden_slab_arena {
  folden_arena_block_t *blocks;
  size_t count;
  size_t cap;
  uint32_t next_token;
};

static folden_slab_arena_t g_arena;
static int g_arena_initialized = 0;

folden_slab_arena_t *folden_arena_get_global(void) {
  if (!g_arena_initialized) {
    /* Token 0 is the "no token" sentinel everywhere else in the API, so the
     * very first allocation must not be assigned it. */
    g_arena.next_token = 1;
    g_arena_initialized = 1;
  }
  return &g_arena;
}

void folden_arena_reset(folden_slab_arena_t *arena) {
  if (!arena) {
    return;
  }
  for (size_t i = 0; i < arena->count; i++) {
    free(arena->blocks[i].data);
  }
  free(arena->blocks);
  memset(arena, 0, sizeof(*arena));
  arena->next_token = 1;
}

static int arena_grow(folden_slab_arena_t *arena) {
  size_t nc = arena->cap ? arena->cap * 2 : 16;
  folden_arena_block_t *nb =
      realloc(arena->blocks, nc * sizeof(folden_arena_block_t));
  if (!nb) {
    return -1;
  }
  memset(nb + arena->cap, 0, (nc - arena->cap) * sizeof(folden_arena_block_t));
  arena->blocks = nb;
  arena->cap = nc;
  return 0;
}

uint32_t folden_arena_alloc(folden_slab_arena_t *arena, const uint8_t *data,
                            size_t len) {
  if (!arena || !data || len == 0) {
    return 0;
  }
  if (arena->count >= arena->cap && arena_grow(arena) != 0) {
    return 0;
  }
  folden_arena_block_t *b = &arena->blocks[arena->count++];
  b->token = arena->next_token++;
  b->data = malloc(len);
  if (!b->data) {
    arena->count--;
    return 0;
  }
  memcpy(b->data, data, len);
  b->len = len;
  b->borrow_count = 0;
  b->live = 1;
  return b->token;
}

uint8_t *folden_arena_ptr(folden_slab_arena_t *arena, uint32_t token) {
  if (!arena || token == 0) {
    return NULL;
  }
  for (size_t i = 0; i < arena->count; i++) {
    if (arena->blocks[i].token == token && arena->blocks[i].live) {
      return arena->blocks[i].data;
    }
  }
  return NULL;
}

size_t folden_arena_len(folden_slab_arena_t *arena, uint32_t token) {
  if (!arena || token == 0) {
    return 0;
  }
  for (size_t i = 0; i < arena->count; i++) {
    if (arena->blocks[i].token == token && arena->blocks[i].live) {
      return arena->blocks[i].len;
    }
  }
  return 0;
}

void folden_arena_borrow(folden_slab_arena_t *arena, uint32_t token) {
  if (!arena || token == 0) {
    return;
  }
  for (size_t i = 0; i < arena->count; i++) {
    if (arena->blocks[i].token == token && arena->blocks[i].live) {
      arena->blocks[i].borrow_count++;
      return;
    }
  }
}

void folden_arena_release_borrow(folden_slab_arena_t *arena, uint32_t token) {
  if (!arena || token == 0) {
    return;
  }
  for (size_t i = 0; i < arena->count; i++) {
    if (arena->blocks[i].token == token && arena->blocks[i].live) {
      if (arena->blocks[i].borrow_count > 0) {
        arena->blocks[i].borrow_count--;
      }
      return;
    }
  }
}

void folden_arena_free_token(folden_slab_arena_t *arena, uint32_t token) {
  if (!arena || token == 0) {
    return;
  }
  for (size_t i = 0; i < arena->count; i++) {
    if (arena->blocks[i].token == token && arena->blocks[i].live) {
      if (arena->blocks[i].borrow_count > 0) {
        return;
      }
      free(arena->blocks[i].data);
      arena->blocks[i].data = NULL;
      arena->blocks[i].len = 0;
      arena->blocks[i].live = 0;
      return;
    }
  }
}


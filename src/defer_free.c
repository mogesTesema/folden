#include "defer_free.h"

#include "slab_arena.h"

#include <stdlib.h>

typedef struct folden_defer_item {
  void *ptr;
  uint32_t arena_token;
  uint32_t epoch;
  int is_arena;
} folden_defer_item_t;

struct folden_defer_free {
  folden_defer_item_t *items;
  size_t count;
  size_t cap;
};

folden_defer_free_t *folden_defer_create(void) {
  return calloc(1, sizeof(folden_defer_free_t));
}

void folden_defer_destroy(folden_defer_free_t *df) {
  if (!df) {
    return;
  }
  folden_defer_drain(df, UINT32_MAX);
  free(df->items);
  free(df);
}

static int defer_grow(folden_defer_free_t *df) {
  size_t nc = df->cap ? df->cap * 2 : 32;
  folden_defer_item_t *ni = realloc(df->items, nc * sizeof(folden_defer_item_t));
  if (!ni) {
    return -1;
  }
  df->items = ni;
  df->cap = nc;
  return 0;
}

void folden_defer_enqueue_ptr(folden_defer_free_t *df, void *ptr) {
  if (!df || !ptr) {
    return;
  }
  if (df->count >= df->cap && defer_grow(df) != 0) {
    return;
  }
  folden_defer_item_t *it = &df->items[df->count++];
  it->ptr = ptr;
  it->arena_token = 0;
  it->epoch = 0;
  it->is_arena = 0;
}

void folden_defer_enqueue_arena_token(folden_defer_free_t *df, uint32_t token) {
  if (!df || token == 0) {
    return;
  }
  if (df->count >= df->cap && defer_grow(df) != 0) {
    return;
  }
  folden_defer_item_t *it = &df->items[df->count++];
  it->ptr = NULL;
  it->arena_token = token;
  it->epoch = 0;
  it->is_arena = 1;
}

void folden_defer_drain(folden_defer_free_t *df, uint32_t epoch) {
  if (!df) {
    return;
  }
  folden_slab_arena_t *arena = folden_arena_get_global();
  size_t w = 0;
  for (size_t i = 0; i < df->count; i++) {
    folden_defer_item_t *it = &df->items[i];
    if (it->epoch > epoch) {
      df->items[w++] = *it;
      continue;
    }
    if (it->is_arena) {
      /* skip tokens that are still borrowed -- free_token no-ops in
       * that case and the caller is responsible for the lifetime. */
      folden_arena_free_token(arena, it->arena_token);
    } else {
      free(it->ptr);
    }
  }
  df->count = w;
}

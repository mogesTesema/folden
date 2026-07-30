#include "route_cache.h"
#include <stdlib.h>
#include <string.h>

typedef struct route_cache_entry {
  uint32_t lane_id;
  uint32_t generation;
  const uint8_t *slab_ptr;
  size_t slab_len;
  int valid;
} route_cache_entry_t;

struct folden_route_cache {
  route_cache_entry_t *entries;
  size_t count;
  size_t cap;
};

folden_route_cache_t *folden_route_cache_create(void) { return calloc(1, sizeof(folden_route_cache_t)); }
void folden_route_cache_destroy(folden_route_cache_t *c) { free(c->entries); free(c); }

static route_cache_entry_t *route_cache_find(folden_route_cache_t *c, uint32_t lane,
    uint32_t gen) {
  for (size_t i = 0; i < c->count; i++) {
    if (c->entries[i].valid && c->entries[i].lane_id == lane &&
        c->entries[i].generation == gen) {
      return &c->entries[i];
    }
  }
  return NULL;
}

void folden_route_cache_pin(folden_route_cache_t *c, uint32_t lane, uint32_t gen,
    const uint8_t *slab, size_t len) {
  if (!c || !slab) return;
  /* Re-pinning the same (lane, generation) updates the existing entry in
   * place rather than appending a duplicate, since lookup returns the
   * first match. */
  route_cache_entry_t *e = route_cache_find(c, lane, gen);
  if (!e) {
    if (c->count >= c->cap) {
      size_t nc = c->cap ? c->cap * 2 : 8;
      route_cache_entry_t *ne = realloc(c->entries, nc * sizeof(*ne));
      if (!ne) return;
      c->entries = ne;
      c->cap = nc;
    }
    e = &c->entries[c->count++];
    e->lane_id = lane;
    e->generation = gen;
  }
  e->slab_ptr = slab;
  e->slab_len = len;
  e->valid = 1;
}

const uint8_t *folden_route_cache_lookup(folden_route_cache_t *c, uint32_t lane,
    uint32_t gen, size_t *out_len) {
  if (!c) return NULL;
  for (size_t i = 0; i < c->count; i++) {
    if (c->entries[i].valid && c->entries[i].lane_id == lane &&
        c->entries[i].generation == gen) {
      if (out_len) *out_len = c->entries[i].slab_len;
      return c->entries[i].slab_ptr;
    }
  }
  return NULL;
}

void folden_route_cache_invalidate_lane(folden_route_cache_t *c, uint32_t lane) {
  if (!c) return;
  for (size_t i = 0; i < c->count; i++) {
    if (c->entries[i].lane_id == lane) {
      c->entries[i].valid = 0;
    }
  }
}

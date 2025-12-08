#include "capsule_cache.h"

#include <stdlib.h>
#include <string.h>

typedef struct folden_cache_entry {
  uint32_t lane_id;
  uint32_t generation;
  uint8_t *slab_copy;
  const uint8_t *slab_ptr;
  size_t slab_len;
  int valid;
} folden_cache_entry_t;

struct folden_capsule_cache {
  folden_cache_entry_t *entries;
  size_t count;
  size_t cap;
};

folden_capsule_cache_t *folden_cache_create(void) {
  return calloc(1, sizeof(folden_capsule_cache_t));
}

void folden_cache_destroy(folden_capsule_cache_t *cache) {
  if (!cache) {
    return;
  }
  for (size_t i = 0; i < cache->count; i++) {
    free(cache->entries[i].slab_copy);
  }
  free(cache->entries);
  free(cache);
}

static folden_cache_entry_t *cache_find(folden_capsule_cache_t *cache,
                                        uint32_t lane_id, uint32_t generation) {
  for (size_t i = 0; i < cache->count; i++) {
    if (cache->entries[i].valid && cache->entries[i].lane_id == lane_id &&
        cache->entries[i].generation == generation) {
      return &cache->entries[i];
    }
  }
  return NULL;
}

static int cache_grow(folden_capsule_cache_t *cache) {
  size_t nc = cache->cap ? cache->cap * 2 : 8;
  folden_cache_entry_t *ne =
      realloc(cache->entries, nc * sizeof(folden_cache_entry_t));
  if (!ne) {
    return -1;
  }
  memset(ne + cache->cap, 0, (nc - cache->cap) * sizeof(folden_cache_entry_t));
  cache->entries = ne;
  cache->cap = nc;
  return 0;
}

void folden_cache_pin_emit_slab(folden_capsule_cache_t *cache, uint32_t lane_id,
                                uint32_t generation, const uint8_t *slab,
                                size_t slab_len) {
  if (!cache || !slab || slab_len == 0) {
    return;
  }
  folden_cache_entry_t *ent = cache_find(cache, lane_id, generation);
  if (!ent) {
    if (cache->count >= cache->cap && cache_grow(cache) != 0) {
      return;
    }
    ent = &cache->entries[cache->count++];
    ent->lane_id = lane_id;
    ent->generation = generation;
    ent->valid = 1;
  }
  free(ent->slab_copy);
  ent->slab_copy = malloc(slab_len);
  if (!ent->slab_copy) {
    return;
  }
  memcpy(ent->slab_copy, slab, slab_len);
  ent->slab_ptr = slab;
  ent->slab_len = slab_len;
}

const uint8_t *folden_cache_lookup_emit_slab(folden_capsule_cache_t *cache,
                                             uint32_t lane_id,
                                             uint32_t generation,
                                             size_t *out_len) {
  if (!cache) {
    return NULL;
  }
  folden_cache_entry_t *ent = cache_find(cache, lane_id, generation);
  if (!ent || !ent->valid) {
    return NULL;
  }
  if (out_len) {
    *out_len = ent->slab_len;
  }
  return ent->slab_ptr;
}

void folden_cache_invalidate_lane(folden_capsule_cache_t *cache, uint32_t lane_id) {
  if (!cache) {
    return;
  }
  for (size_t i = 0; i < cache->count; i++) {
    if (cache->entries[i].lane_id == lane_id) {
      cache->entries[i].valid = 0;
    }
  }
}

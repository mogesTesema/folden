#include "relay_queue.h"

#include "capsule_cache.h"
#include "defer_free.h"
#include "relay_registry.h"
#include "route_cache.h"
#include "slab_arena.h"

#include <stdlib.h>
#include <string.h>

typedef struct folden_queue_op {
  uint32_t lane_id;
  uint8_t *slab;
  size_t slab_len;
  uint32_t arena_token;
  uint32_t generation;
  int has_vault_gen;
  folden_tag_entry_t *tags;
  size_t tag_count;
  uint8_t *tag_meta;
  size_t tag_meta_len;
} folden_queue_op_t;

struct folden_relay_queue {
  folden_queue_op_t *ops;
  size_t count;
  size_t cap;
};

folden_relay_queue_t *folden_relay_queue_create(void) {
  return calloc(1, sizeof(folden_relay_queue_t));
}

void folden_relay_queue_destroy(folden_relay_queue_t *q) {
  if (!q) {
    return;
  }
  for (size_t i = 0; i < q->count; i++) {
    free(q->ops[i].slab);
    free(q->ops[i].tags);
    free(q->ops[i].tag_meta);
  }
  free(q->ops);
  free(q);
}

static int queue_grow(folden_relay_queue_t *q) {
  size_t nc = q->cap ? q->cap * 2 : 8;
  folden_queue_op_t *no = realloc(q->ops, nc * sizeof(folden_queue_op_t));
  if (!no) {
    return -1;
  }
  memset(no + q->cap, 0, (nc - q->cap) * sizeof(folden_queue_op_t));
  q->ops = no;
  q->cap = nc;
  return 0;
}

folden_status_t folden_relay_queue_enqueue_adopt(folden_relay_queue_t *q,
                                                 const folden_lane_t *lane) {
  if (!q || !lane) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (q->count >= q->cap && queue_grow(q) != 0) {
    return FOLDEN_ERR_NOMEM;
  }
  folden_queue_op_t *op = &q->ops[q->count++];
  op->lane_id = lane->lane_id;
  op->generation = lane->has_vault_gen ? lane->vault_gen : 0;
  op->has_vault_gen = lane->has_vault_gen;
  if (lane->slab_len > 0 && lane->slab) {
    folden_slab_arena_t *arena = folden_arena_get_global();
    op->arena_token = folden_arena_alloc(arena, lane->slab, lane->slab_len);
    op->slab = folden_arena_ptr(arena, op->arena_token);
    op->slab_len = lane->slab_len;
  }
  if (lane->tag_count > 0) {
    op->tags = malloc(lane->tag_count * sizeof(folden_tag_entry_t));
    if (!op->tags) {
      return FOLDEN_ERR_NOMEM;
    }
    memcpy(op->tags, lane->tags, lane->tag_count * sizeof(folden_tag_entry_t));
    op->tag_count = lane->tag_count;
  }
  if (lane->tag_meta && lane->tag_meta_cap > 0) {
    op->tag_meta = malloc(lane->tag_meta_cap);
    if (op->tag_meta) {
      memcpy(op->tag_meta, lane->tag_meta, lane->tag_meta_cap);
      op->tag_meta_len = lane->tag_meta_cap;
    }
  }
  return FOLDEN_OK;
}

void folden_relay_queue_drop_lane(folden_relay_t *relay, folden_lane_t *lane) {
  if (!relay || !relay->queue || !lane) return;
  for (size_t i = 0; i < relay->queue->count; i++) {
    if (relay->queue->ops[i].lane_id == lane->lane_id) {
      // Just null out the op to "drop" it, simulating a queue pop
      relay->queue->ops[i].lane_id = 0;
      break;
    }
  }
}

static void apply_op_to_relay(folden_relay_t *relay, const folden_queue_op_t *op,
                              folden_defer_free_t *defer) {
  folden_relay_entry_t *ent = folden_relay_find(relay, op->lane_id);
  if (!ent) {
    if (relay->count >= relay->cap) {
      size_t nc = relay->cap ? relay->cap * 2 : 8;
      folden_relay_entry_t *ne =
          realloc(relay->entries, nc * sizeof(folden_relay_entry_t));
      if (!ne) {
        return;
      }
      memset(ne + relay->cap, 0, (nc - relay->cap) * sizeof(folden_relay_entry_t));
      relay->entries = ne;
      relay->cap = nc;
    }
    ent = &relay->entries[relay->count++];
    ent->lane_id = op->lane_id;
  }

  if (ent->arena_token || ent->slab) {
    /* Whatever generation the caches have pinned for this lane is about to
     * be superseded and its backing memory queued for freeing -- drop the
     * pins first, or a lookup keyed on the old generation could hand back
     * a pointer into memory a later tick actually frees. */
    if (relay->route_cache) {
      folden_route_cache_invalidate_lane(relay->route_cache, ent->lane_id);
    }
    if (relay->cache) {
      folden_cache_invalidate_lane(relay->cache, ent->lane_id);
    }
  }
  if (ent->arena_token) {
    /* ent->slab (if set) is only an alias into this arena block, not a
     * separate allocation -- deferring the token handles both. */
    folden_defer_enqueue_arena_token(defer, ent->arena_token);
    ent->arena_token = 0;
    ent->slab = NULL;
    ent->slab_len = 0;
  } else if (ent->slab) {
    folden_defer_enqueue_ptr(defer, ent->slab);
    ent->slab = NULL;
    ent->slab_len = 0;
  }

  ent->arena_token = op->arena_token;
  if (op->arena_token) {
    /* op->slab already points into the arena block owned by op->arena_token;
     * alias it instead of making a redundant heap copy. */
    ent->slab = op->slab;
    ent->slab_len = op->slab_len;
  } else if (op->slab_len > 0 && op->slab) {
    ent->slab = malloc(op->slab_len);
    if (ent->slab) {
      memcpy(ent->slab, op->slab, op->slab_len);
      ent->slab_len = op->slab_len;
    }
  } else {
    ent->slab = NULL;
    ent->slab_len = 0;
  }

  ent->refcount++;
  if (op->has_vault_gen) {
    ent->generation = op->generation;
  }
  if (ent->cached_tags) {
    free(ent->cached_tags);
  }
  if (op->tags && op->tag_count > 0) {
    ent->cached_tags = malloc(op->tag_count * sizeof(folden_tag_entry_t));
    if (ent->cached_tags) {
      memcpy(ent->cached_tags, op->tags, op->tag_count * sizeof(folden_tag_entry_t));
      ent->cached_tag_count = op->tag_count;
    }
  }
  if (ent->cached_meta) {
    free(ent->cached_meta);
    ent->cached_meta = NULL;
    ent->cached_meta_len = 0;
  }
  if (op->tag_meta && op->tag_meta_len > 0) {
    ent->cached_meta = malloc(op->tag_meta_len);
    if (ent->cached_meta) {
      memcpy(ent->cached_meta, op->tag_meta, op->tag_meta_len);
      ent->cached_meta_len = op->tag_meta_len;
    }
  }
  if (relay->cache && ent->slab && ent->slab_len > 0) {
    folden_cache_pin_emit_slab(relay->cache, ent->lane_id, ent->generation,
                               ent->slab, ent->slab_len);
  }
  if (relay->route_cache && ent->slab && ent->slab_len > 0) {
    folden_route_cache_pin(relay->route_cache, ent->lane_id, ent->generation,
                           ent->slab, ent->slab_len);
  }
}

folden_status_t folden_relay_queue_flush(folden_relay_t *relay,
                                         folden_relay_queue_t *q,
                                         folden_defer_free_t *defer) {
  if (!relay || !q) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_defer_drain(defer, UINT32_MAX);
  for (size_t i = 0; i < q->count; i++) {
    apply_op_to_relay(relay, &q->ops[i], defer);
    free(q->ops[i].tags);
    free(q->ops[i].tag_meta);
    q->ops[i].tags = NULL;
    q->ops[i].tag_meta = NULL;
  }
  q->count = 0;
  return FOLDEN_OK;
}

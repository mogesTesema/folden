#include "route_table.h"

#include <stdlib.h>
#include <string.h>

/* A single lane's routing entry. `last_used` is a monotonically increasing
 * logical clock (not wall time) bumped on every lookup/insert, so the
 * lowest value among live entries is always the least-recently-used one. */
typedef struct folden_route_entry {
  uint32_t lane_id;
  uint32_t next_hop;
  uint32_t cost;
  uint32_t failures;
  uint32_t backoff_until;
  uint64_t last_used;
  int in_use;
} folden_route_entry_t;

struct folden_route_table {
  folden_route_entry_t *entries;
  size_t cap;
  uint64_t clock;
};

folden_route_table_t *folden_route_table_create(size_t capacity) {
  if (capacity == 0) {
    capacity = 16;
  }
  folden_route_table_t *t = calloc(1, sizeof(*t));
  if (!t) {
    return NULL;
  }
  t->entries = calloc(capacity, sizeof(folden_route_entry_t));
  if (!t->entries) {
    free(t);
    return NULL;
  }
  t->cap = capacity;
  return t;
}

void folden_route_table_destroy(folden_route_table_t *t) {
  if (!t) {
    return;
  }
  free(t->entries);
  free(t);
}

static folden_route_entry_t *find_entry(folden_route_table_t *t, uint32_t lane_id) {
  for (size_t i = 0; i < t->cap; i++) {
    if (t->entries[i].in_use && t->entries[i].lane_id == lane_id) {
      return &t->entries[i];
    }
  }
  return NULL;
}

/* Returns a free slot, or the least-recently-used occupied slot if the
 * table is full. Never returns NULL: cap is always > 0. */
static folden_route_entry_t *find_slot_for_insert(folden_route_table_t *t) {
  for (size_t i = 0; i < t->cap; i++) {
    if (!t->entries[i].in_use) {
      return &t->entries[i];
    }
  }
  size_t victim = 0;
  uint64_t oldest = t->entries[0].last_used;
  for (size_t i = 1; i < t->cap; i++) {
    if (t->entries[i].last_used < oldest) {
      oldest = t->entries[i].last_used;
      victim = i;
    }
  }
  return &t->entries[victim];
}

folden_status_t folden_route_table_insert(folden_route_table_t *t, uint32_t lane_id,
                                          uint32_t next_hop, uint32_t cost) {
  if (!t) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_route_entry_t *e = find_entry(t, lane_id);
  if (!e) {
    e = find_slot_for_insert(t);
    e->lane_id = lane_id;
    e->failures = 0;
    e->backoff_until = 0;
    e->in_use = 1;
  }
  e->next_hop = next_hop;
  e->cost = cost;
  e->last_used = ++t->clock;
  return FOLDEN_OK;
}

folden_status_t folden_route_table_lookup(folden_route_table_t *t, uint32_t lane_id,
                                          uint32_t *out_next_hop) {
  if (!t || !out_next_hop) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_route_entry_t *e = find_entry(t, lane_id);
  if (!e) {
    return FOLDEN_ERR_BAD_STATE;
  }
  e->last_used = ++t->clock;
  *out_next_hop = e->next_hop;
  return FOLDEN_OK;
}

uint32_t folden_route_table_note_failure(folden_route_table_t *t, uint32_t lane_id) {
  if (!t) {
    return 0;
  }
  folden_route_entry_t *e = find_entry(t, lane_id);
  if (!e) {
    return 0;
  }
  if (e->failures < 16) {
    e->failures++;
  }
  uint32_t backoff = 1u << e->failures;
  e->backoff_until = t->clock + backoff;
  return backoff;
}

void folden_route_table_note_success(folden_route_table_t *t, uint32_t lane_id) {
  if (!t) {
    return;
  }
  folden_route_entry_t *e = find_entry(t, lane_id);
  if (!e) {
    return;
  }
  e->failures = 0;
  e->backoff_until = 0;
}

size_t folden_route_table_count(const folden_route_table_t *t) {
  if (!t) {
    return 0;
  }
  size_t n = 0;
  for (size_t i = 0; i < t->cap; i++) {
    if (t->entries[i].in_use) {
      n++;
    }
  }
  return n;
}

folden_status_t folden_route_table_resolve_path(folden_route_table_t *t,
                                                uint32_t start_lane_id,
                                                uint32_t *out_hops, size_t max_hops,
                                                size_t *out_hop_count) {
  if (!t || !out_hops || !out_hop_count || max_hops == 0) {
    return FOLDEN_ERR_BAD_STATE;
  }

  uint32_t current = start_lane_id;
  size_t n = 0;
  while (n < max_hops) {
    uint32_t next_hop = 0;
    if (folden_route_table_lookup(t, current, &next_hop) != FOLDEN_OK) {
      /* current has no route of its own; the chain ends here without
       * current itself being part of the resolved path. */
      *out_hop_count = n;
      return n > 0 ? FOLDEN_OK : FOLDEN_ERR_BAD_STATE;
    }

    for (size_t i = 0; i < n; i++) {
      if (out_hops[i] == next_hop) {
        /* Routing loop: next_hop was already visited in this chain. */
        return FOLDEN_ERR_BAD_STATE;
      }
    }

    out_hops[n++] = next_hop;
    current = next_hop;
  }

  /* Exhausted max_hops without the chain terminating naturally. */
  return FOLDEN_ERR_BAD_STATE;
}

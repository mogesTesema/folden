#include "metrics.h"

#include <stdlib.h>
#include <string.h>

struct folden_metrics {
  folden_lane_metrics_t *entries;
  size_t count;
  size_t cap;
};

folden_metrics_t *folden_metrics_create(void) {
  return calloc(1, sizeof(folden_metrics_t));
}

void folden_metrics_destroy(folden_metrics_t *m) {
  if (!m) {
    return;
  }
  free(m->entries);
  free(m);
}

static folden_lane_metrics_t *find_entry(folden_metrics_t *m, uint32_t lane_id) {
  for (size_t i = 0; i < m->count; i++) {
    if (m->entries[i].lane_id == lane_id) {
      return &m->entries[i];
    }
  }
  return NULL;
}

static folden_lane_metrics_t *find_or_create_entry(folden_metrics_t *m, uint32_t lane_id) {
  folden_lane_metrics_t *e = find_entry(m, lane_id);
  if (e) {
    return e;
  }
  if (m->count >= m->cap) {
    size_t nc = m->cap ? m->cap * 2 : 8;
    folden_lane_metrics_t *ne = realloc(m->entries, nc * sizeof(*ne));
    if (!ne) {
      return NULL;
    }
    memset(ne + m->cap, 0, (nc - m->cap) * sizeof(*ne));
    m->entries = ne;
    m->cap = nc;
  }
  e = &m->entries[m->count++];
  e->lane_id = lane_id;
  return e;
}

void folden_metrics_record_adopt(folden_metrics_t *m, uint32_t lane_id,
                                 uint32_t generation, size_t bytes) {
  if (!m) {
    return;
  }
  folden_lane_metrics_t *e = find_or_create_entry(m, lane_id);
  if (!e) {
    return;
  }
  e->adopt_count++;
  e->bytes_in += bytes;
  e->last_generation = generation;
}

void folden_metrics_record_release(folden_metrics_t *m, uint32_t lane_id) {
  if (!m) {
    return;
  }
  folden_lane_metrics_t *e = find_entry(m, lane_id);
  if (!e) {
    return;
  }
  e->release_count++;
}

void folden_metrics_record_emit(folden_metrics_t *m, uint32_t lane_id, size_t bytes) {
  if (!m) {
    return;
  }
  folden_lane_metrics_t *e = find_entry(m, lane_id);
  if (!e) {
    return;
  }
  e->emit_count++;
  e->bytes_out += bytes;
}

void folden_metrics_reset_lane(folden_metrics_t *m, uint32_t lane_id) {
  if (!m) {
    return;
  }
  folden_lane_metrics_t *e = find_entry(m, lane_id);
  if (!e) {
    return;
  }
  uint32_t id = e->lane_id;
  memset(e, 0, sizeof(*e));
  e->lane_id = id;
}

folden_status_t folden_metrics_snapshot(const folden_metrics_t *m, uint32_t lane_id,
                                        folden_lane_metrics_t *out) {
  if (!m || !out) {
    return FOLDEN_ERR_BAD_STATE;
  }
  for (size_t i = 0; i < m->count; i++) {
    if (m->entries[i].lane_id == lane_id) {
      *out = m->entries[i];
      return FOLDEN_OK;
    }
  }
  return FOLDEN_ERR_BAD_STATE;
}

size_t folden_metrics_lane_count(const folden_metrics_t *m) {
  return m ? m->count : 0;
}

size_t folden_metrics_top_n(const folden_metrics_t *m, size_t n,
                            folden_lane_metrics_t *out) {
  if (!m || !out || n == 0) {
    return 0;
  }
  size_t total = m->count;
  size_t take = n < total ? n : total;
  if (take == 0) {
    return 0;
  }

  /* Selection sort into `out`, descending by adopt_count. Fine for the
   * small per-relay lane counts this is ever called with. */
  int *used = calloc(total, sizeof(int));
  if (!used) {
    return 0;
  }
  for (size_t k = 0; k < take; k++) {
    size_t best = (size_t)-1;
    for (size_t i = 0; i < total; i++) {
      if (used[i]) {
        continue;
      }
      if (best == (size_t)-1 || m->entries[i].adopt_count > m->entries[best].adopt_count) {
        best = i;
      }
    }
    out[k] = m->entries[best];
    used[best] = 1;
  }
  free(used);
  return take;
}

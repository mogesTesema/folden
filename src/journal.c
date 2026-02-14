#include "journal.h"

#include <stdlib.h>
#include <string.h>

#define FOLDEN_JOURNAL_SEGMENT_MAX_BYTES 4096u
#define FOLDEN_JOURNAL_SEGMENT_MAX_AGE_TICKS 3u
#define FOLDEN_JOURNAL_COMPACT_THRESHOLD 2u
#define FOLDEN_JOURNAL_RETIRE_AFTER_TICKS 2u

typedef struct folden_journal_record {
  uint32_t lane_id;
  uint32_t offset;
  uint32_t length;
} folden_journal_record_t;

typedef struct folden_journal_segment {
  uint8_t *blob;
  size_t len;
  size_t cap;
  folden_journal_record_t *records;
  size_t record_count;
  size_t record_cap;
  int sealed;
  int compacted;
  int retired;
  uint32_t age_ticks;
  uint8_t *replay_copy;
  size_t replay_copy_len;
} folden_journal_segment_t;

struct folden_journal {
  folden_journal_segment_t **segments;
  size_t count;
  size_t cap;
  size_t active;
  int has_active;
  uint32_t segment_max_bytes;
  uint32_t segment_max_age_ticks;
  uint32_t compact_threshold;
  uint32_t retire_after_ticks;
};

static folden_journal_segment_t *segment_create(void) {
  folden_journal_segment_t *seg = calloc(1, sizeof(*seg));
  if (!seg) {
    return NULL;
  }
  seg->cap = 256;
  seg->blob = malloc(seg->cap);
  seg->record_cap = 8;
  seg->records = calloc(seg->record_cap, sizeof(*seg->records));
  if (!seg->blob || !seg->records) {
    free(seg->blob);
    free(seg->records);
    free(seg);
    return NULL;
  }
  return seg;
}

static void segment_destroy(folden_journal_segment_t *seg) {
  if (!seg) {
    return;
  }
  free(seg->blob);
  free(seg->records);
  free(seg->replay_copy);
  free(seg);
}

static folden_status_t segment_append(folden_journal_segment_t *seg, uint32_t lane_id,
                                      const uint8_t *data, size_t len) {
  if (seg->sealed) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (seg->len + len > seg->cap) {
    size_t nc = seg->cap ? seg->cap : 256;
    while (nc < seg->len + len) {
      nc *= 2;
    }
    uint8_t *nb = realloc(seg->blob, nc);
    if (!nb) {
      return FOLDEN_ERR_NOMEM;
    }
    seg->blob = nb;
    seg->cap = nc;
  }
  if (seg->record_count >= seg->record_cap) {
    size_t nrc = seg->record_cap * 2;
    folden_journal_record_t *nr = realloc(seg->records, nrc * sizeof(*nr));
    if (!nr) {
      return FOLDEN_ERR_NOMEM;
    }
    seg->records = nr;
    seg->record_cap = nrc;
  }
  folden_journal_record_t *r = &seg->records[seg->record_count++];
  r->lane_id = lane_id;
  r->offset = (uint32_t)seg->len;
  r->length = (uint32_t)len;
  if (len > 0) {
    memcpy(seg->blob + seg->len, data, len);
  }
  seg->len += len;
  return FOLDEN_OK;
}

folden_journal_t *folden_journal_create(void) {
  folden_journal_t *j = calloc(1, sizeof(*j));
  if (!j) {
    return NULL;
  }
  j->cap = 4;
  j->segments = calloc(j->cap, sizeof(*j->segments));
  if (!j->segments) {
    free(j);
    return NULL;
  }
  j->segment_max_bytes = FOLDEN_JOURNAL_SEGMENT_MAX_BYTES;
  j->segment_max_age_ticks = FOLDEN_JOURNAL_SEGMENT_MAX_AGE_TICKS;
  j->compact_threshold = FOLDEN_JOURNAL_COMPACT_THRESHOLD;
  j->retire_after_ticks = FOLDEN_JOURNAL_RETIRE_AFTER_TICKS;
  return j;
}

void folden_journal_set_thresholds(folden_journal_t *j, uint32_t segment_max_bytes,
                                   uint32_t segment_max_age_ticks,
                                   uint32_t compact_threshold,
                                   uint32_t retire_after_ticks) {
  if (!j) {
    return;
  }
  if (segment_max_bytes > 0) {
    j->segment_max_bytes = segment_max_bytes;
  }
  if (segment_max_age_ticks > 0) {
    j->segment_max_age_ticks = segment_max_age_ticks;
  }
  if (compact_threshold > 0) {
    j->compact_threshold = compact_threshold;
  }
  if (retire_after_ticks > 0) {
    j->retire_after_ticks = retire_after_ticks;
  }
}

void folden_journal_destroy(folden_journal_t *j) {
  if (!j) {
    return;
  }
  for (size_t i = 0; i < j->count; i++) {
    segment_destroy(j->segments[i]);
  }
  free(j->segments);
  free(j);
}

static folden_status_t journal_open_new_segment(folden_journal_t *j) {
  if (j->count >= j->cap) {
    size_t nc = j->cap * 2;
    folden_journal_segment_t **ns = realloc(j->segments, nc * sizeof(*ns));
    if (!ns) {
      return FOLDEN_ERR_NOMEM;
    }
    j->segments = ns;
    j->cap = nc;
  }
  folden_journal_segment_t *seg = segment_create();
  if (!seg) {
    return FOLDEN_ERR_NOMEM;
  }
  j->segments[j->count] = seg;
  j->active = j->count;
  j->has_active = 1;
  j->count++;
  return FOLDEN_OK;
}

folden_status_t folden_journal_append(folden_journal_t *j, const uint8_t *wire,
                                      size_t wire_len, uint32_t lane_id) {
  if (!j || !wire) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (!j->has_active || j->segments[j->active]->sealed) {
    folden_status_t st = journal_open_new_segment(j);
    if (st != FOLDEN_OK) {
      return st;
    }
  }
  folden_journal_segment_t *seg = j->segments[j->active];
  folden_status_t st = segment_append(seg, lane_id, wire, wire_len);
  if (st != FOLDEN_OK) {
    return st;
  }
  if (seg->len >= j->segment_max_bytes) {
    seg->sealed = 1;
  }
  return FOLDEN_OK;
}

/* Makes an independent, owned copy of the segment's bytes and releases the
 * original growable buffer through the deferred-free queue. Compacted
 * segments keep serving replay reads from `replay_copy` until they age out. */
static void segment_compact(folden_journal_segment_t *seg, folden_defer_free_t *defer) {
  if (seg->compacted || !seg->blob) {
    return;
  }
  seg->replay_copy = malloc(seg->len > 0 ? seg->len : 1);
  if (seg->replay_copy) {
    if (seg->len > 0) {
      memcpy(seg->replay_copy, seg->blob, seg->len);
    }
    seg->replay_copy_len = seg->len;
  }
  folden_defer_enqueue_ptr(defer, seg->blob);
  seg->blob = NULL;
  seg->cap = 0;
  seg->compacted = 1;
}

folden_status_t folden_journal_tick(folden_journal_t *j, folden_defer_free_t *defer) {
  if (!j) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (j->has_active) {
    folden_journal_segment_t *active = j->segments[j->active];
    active->age_ticks++;
    if (!active->sealed && active->age_ticks >= j->segment_max_age_ticks &&
        active->len > 0) {
      active->sealed = 1;
    }
  }

  size_t sealed_uncompacted = 0;
  for (size_t i = 0; i < j->count; i++) {
    if (j->segments[i]->sealed && !j->segments[i]->compacted) {
      sealed_uncompacted++;
    }
  }
  if (sealed_uncompacted >= j->compact_threshold && defer) {
    for (size_t i = 0; i < j->count; i++) {
      folden_journal_segment_t *seg = j->segments[i];
      if (seg->sealed && !seg->compacted) {
        segment_compact(seg, defer);
      }
    }
  }

  for (size_t i = 0; i < j->count; i++) {
    folden_journal_segment_t *seg = j->segments[i];
    if (seg->compacted && !seg->retired) {
      seg->age_ticks++;
      if (seg->age_ticks >= j->compact_threshold + j->retire_after_ticks) {
        free(seg->replay_copy);
        seg->replay_copy = NULL;
        seg->replay_copy_len = 0;
        seg->retired = 1;
      }
    }
  }

  return FOLDEN_OK;
}

size_t folden_journal_segment_count(const folden_journal_t *j) {
  return j ? j->count : 0;
}

folden_status_t folden_journal_replay_segment(folden_journal_t *j, size_t index,
                                              const uint8_t **out_ptr, size_t *out_len) {
  if (!j || !out_ptr || !out_len || index >= j->count) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_journal_segment_t *seg = j->segments[index];
  if (seg->retired) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (seg->compacted) {
    *out_ptr = seg->replay_copy;
    *out_len = seg->replay_copy_len;
  } else {
    *out_ptr = seg->blob;
    *out_len = seg->len;
  }
  return FOLDEN_OK;
}

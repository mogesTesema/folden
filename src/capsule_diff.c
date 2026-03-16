#include "capsule_diff.h"

#include <stdlib.h>
#include <string.h>

struct folden_capsule_diff {
  uint32_t *added;
  size_t added_count;
  uint32_t *removed;
  size_t removed_count;
  uint32_t *changed;
  size_t changed_count;
};

static const folden_lane_t *find_lane(const folden_capsule_t *cap, uint32_t lane_id) {
  for (size_t i = 0; i < cap->lane_count; i++) {
    if (cap->lanes[i].lane_id == lane_id) {
      return &cap->lanes[i];
    }
  }
  return NULL;
}

static int lanes_equal(const folden_lane_t *a, const folden_lane_t *b) {
  if (a->slab_len != b->slab_len || a->tag_count != b->tag_count) {
    return 0;
  }
  if (a->slab_len > 0 && memcmp(a->slab, b->slab, a->slab_len) != 0) {
    return 0;
  }
  for (size_t i = 0; i < a->tag_count; i++) {
    if (a->tags[i].tag_id != b->tags[i].tag_id ||
        a->tags[i].fold_depth != b->tags[i].fold_depth ||
        a->tags[i].span != b->tags[i].span) {
      return 0;
    }
  }
  return 1;
}

static int append_id(uint32_t **arr, size_t *count, size_t *cap, uint32_t id) {
  if (*count >= *cap) {
    size_t nc = *cap ? *cap * 2 : 4;
    uint32_t *na = realloc(*arr, nc * sizeof(uint32_t));
    if (!na) {
      return -1;
    }
    *arr = na;
    *cap = nc;
  }
  (*arr)[(*count)++] = id;
  return 0;
}

folden_status_t folden_capsule_diff(const folden_capsule_t *old_cap,
                                    const folden_capsule_t *new_cap,
                                    folden_capsule_diff_t **out) {
  if (!old_cap || !new_cap || !out) {
    return FOLDEN_ERR_BAD_STATE;
  }

  folden_capsule_diff_t *d = calloc(1, sizeof(*d));
  if (!d) {
    return FOLDEN_ERR_NOMEM;
  }

  size_t added_cap = 0, removed_cap = 0, changed_cap = 0;

  for (size_t i = 0; i < new_cap->lane_count; i++) {
    uint32_t lane_id = new_cap->lanes[i].lane_id;
    const folden_lane_t *old_lane = find_lane(old_cap, lane_id);
    if (!old_lane) {
      if (append_id(&d->added, &d->added_count, &added_cap, lane_id) != 0) {
        folden_capsule_diff_free(d);
        return FOLDEN_ERR_NOMEM;
      }
    } else if (!lanes_equal(old_lane, &new_cap->lanes[i])) {
      if (append_id(&d->changed, &d->changed_count, &changed_cap, lane_id) != 0) {
        folden_capsule_diff_free(d);
        return FOLDEN_ERR_NOMEM;
      }
    }
  }

  for (size_t i = 0; i < old_cap->lane_count; i++) {
    uint32_t lane_id = old_cap->lanes[i].lane_id;
    if (!find_lane(new_cap, lane_id)) {
      if (append_id(&d->removed, &d->removed_count, &removed_cap, lane_id) != 0) {
        folden_capsule_diff_free(d);
        return FOLDEN_ERR_NOMEM;
      }
    }
  }

  *out = d;
  return FOLDEN_OK;
}

void folden_capsule_diff_free(folden_capsule_diff_t *diff) {
  if (!diff) {
    return;
  }
  free(diff->added);
  free(diff->removed);
  free(diff->changed);
  free(diff);
}

size_t folden_capsule_diff_added_count(const folden_capsule_diff_t *diff) {
  return diff ? diff->added_count : 0;
}

size_t folden_capsule_diff_removed_count(const folden_capsule_diff_t *diff) {
  return diff ? diff->removed_count : 0;
}

size_t folden_capsule_diff_changed_count(const folden_capsule_diff_t *diff) {
  return diff ? diff->changed_count : 0;
}

uint32_t folden_capsule_diff_added_lane(const folden_capsule_diff_t *diff, size_t idx) {
  if (!diff || idx >= diff->added_count) {
    return 0;
  }
  return diff->added[idx];
}

uint32_t folden_capsule_diff_removed_lane(const folden_capsule_diff_t *diff, size_t idx) {
  if (!diff || idx >= diff->removed_count) {
    return 0;
  }
  return diff->removed[idx];
}

uint32_t folden_capsule_diff_changed_lane(const folden_capsule_diff_t *diff, size_t idx) {
  if (!diff || idx >= diff->changed_count) {
    return 0;
  }
  return diff->changed[idx];
}

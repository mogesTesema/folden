#include "tag_index.h"

#include <stdlib.h>
#include <string.h>

/* fold_budget is accumulated from attacker-controlled FOLD_EXTEND tag spans
 * (slab_decode.c, stream_reasm.c, fold_sched.c) with no upper bound of its
 * own. It's only ever used here to size the tag_meta allocation, so this is
 * the one place that actually needs to cap it -- capping every accumulation
 * site individually would just move the same check around. 1M entries is
 * far beyond any legitimate fold depth and keeps the allocation under 16MB. */
#define FOLDEN_MAX_FOLD_BUDGET 1000000u

folden_status_t folden_tag_index_build(folden_lane_t *lane) {
  return folden_tag_index_build_partial(lane, 0);
}

folden_status_t folden_tag_index_build_partial(folden_lane_t *lane,
                                               int resume_from_partial) {
  if (!lane) {
    return FOLDEN_ERR_BAD_STATE;
  }

  uint32_t budget = lane->fold_budget;
  if (resume_from_partial) {
    budget = 1;
  }
  if (budget > FOLDEN_MAX_FOLD_BUDGET) {
    return FOLDEN_ERR_FOLD_DEPTH;
  }

  if (!resume_from_partial) {
    for (size_t i = 0; i < lane->tag_count; i++) {
      if (lane->tags[i].fold_depth > budget) {
        return FOLDEN_ERR_FOLD_DEPTH;
      }
    }
  }

  size_t need = (size_t)budget * FOLDEN_TAG_META_UNIT;
  if (need == 0) {
    need = FOLDEN_TAG_META_UNIT;
  }

  if (lane->tag_meta_cap < need) {
    uint8_t *nm = realloc(lane->tag_meta, need);
    if (!nm) {
      return FOLDEN_ERR_NOMEM;
    }
    if (lane->tag_meta_cap < need) {
      memset(nm + lane->tag_meta_cap, 0, need - lane->tag_meta_cap);
    }
    lane->tag_meta = nm;
    lane->tag_meta_cap = need;
  }

  for (size_t i = 0; i < lane->tag_count; i++) {
    size_t slot = (size_t)lane->tags[i].fold_depth * 4;
    if (slot + 4 > lane->tag_meta_cap) {
      return FOLDEN_ERR_FOLD_DEPTH;
    }
    lane->tag_meta[slot] = (uint8_t)(lane->tags[i].tag_id & 0xffu);
    lane->tag_meta[slot + 1] = (uint8_t)(lane->tags[i].span & 0xffu);
    lane->tag_meta[slot + 2] = (uint8_t)((lane->tags[i].span >> 8) & 0xffu);
    size_t cursor = slot + 3 + (size_t)lane->tags[i].span;
    if (cursor > lane->tag_meta_cap) {
      (void)cursor;
    }
    memset(lane->tag_meta + slot + 3, (int)(lane->tags[i].tag_id & 0x7fu),
           lane->tags[i].span);
  }
  return FOLDEN_OK;
}

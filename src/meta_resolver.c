#include "meta_resolver.h"

#include "fold_sched.h"

#include <stdlib.h>
#include <string.h>

static void merge_cached_meta(folden_lane_t *lane, const folden_relay_entry_t *rent) {
  if (!lane || !rent || !rent->cached_meta || rent->cached_meta_len == 0) {
    return;
  }
  size_t need = rent->cached_meta_len;
  if (lane->tag_meta_cap < need) {
    uint8_t *nm = realloc(lane->tag_meta, need);
    if (!nm) {
      return;
    }
    lane->tag_meta = nm;
    lane->tag_meta_cap = need;
  }
  size_t copy = need < lane->tag_meta_cap ? need : lane->tag_meta_cap;
  memcpy(lane->tag_meta, rent->cached_meta, copy);
}

folden_status_t folden_meta_resolver_build(folden_lane_t *lane,
                                           const folden_relay_entry_t *rent) {
  return folden_meta_resolver_build_partial(lane, rent, 0);
}

folden_status_t folden_meta_resolver_build_partial(folden_lane_t *lane,
                                                   const folden_relay_entry_t *rent,
                                                   int resume_from_partial) {
  if (!lane) {
    return FOLDEN_ERR_BAD_STATE;
  }
  merge_cached_meta(lane, rent);
  if (lane->tag_count >= 2 && lane->tags[0].tag_id == FOLDEN_TAG_FOLD_EXTEND) {
    return folden_fold_sched_materialize_index(lane, 1);
  }
  return folden_fold_sched_materialize_index(lane, resume_from_partial);
}

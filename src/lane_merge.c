#include "lane_merge.h"

#include "journal.h"
#include "lane_graph.h"
#include "meta_resolver.h"
#include "relay_registry.h"
#include "relay_queue.h"
#include "replicate.h"

#include <stdlib.h>
#include <string.h>

static folden_lane_t *find_lane(folden_capsule_t *cap, uint32_t lane_id,
                                size_t *idx) {
  for (size_t i = 0; i < cap->lane_count; i++) {
    if (cap->lanes[i].lane_id == lane_id) {
      if (idx) {
        *idx = i;
      }
      return &cap->lanes[i];
    }
  }
  return NULL;
}

static folden_status_t merge_one_lane(folden_capsule_t *dst,
                                      const folden_lane_t *sl,
                                      folden_relay_t *relay) {
  size_t exist_idx = 0;
  folden_lane_t *existing = find_lane(dst, sl->lane_id, &exist_idx);

  folden_lane_t scratch = {0};
  scratch.lane_id = sl->lane_id;
  scratch.tag_count = sl->tag_count;
  if (sl->tag_count > 0) {
    scratch.tags = malloc(sl->tag_count * sizeof(folden_tag_entry_t));
    if (!scratch.tags) {
      return FOLDEN_ERR_NOMEM;
    }
    memcpy(scratch.tags, sl->tags, sl->tag_count * sizeof(folden_tag_entry_t));
  }
  scratch.fold_budget = sl->fold_budget;
  if (sl->slab_len > 0) {
    scratch.slab = malloc(sl->slab_len);
    if (!scratch.slab) {
      free(scratch.tags);
      return FOLDEN_ERR_NOMEM;
    }
    memcpy(scratch.slab, sl->slab, sl->slab_len);
    scratch.slab_len = sl->slab_len;
  }

  if (relay && scratch.slab) {
    folden_relay_queue_adopt_lane(relay, &scratch);
    if (relay->fanout) {
      if (folden_replicate_fanout_lane(relay->fanout, relay, &scratch) != FOLDEN_OK) {
        // Fanout replication failed — drop the lane from the queue
        // to prevent partial state propagation.
        folden_relay_queue_drop_lane(relay, &scratch);
        return FOLDEN_ERR_BAD_STATE;
      }
    }
  }

  scratch.fold_budget = 0;
  folden_status_t st = folden_meta_resolver_build(&scratch, NULL);
  if (st != FOLDEN_OK) {
    free(scratch.tags);
    free(scratch.slab);
    free(scratch.tag_meta);
    return st;
  }

  if (existing) {
    free(existing->slab);
    free(existing->tags);
    free(existing->tag_meta);
    existing->tags = scratch.tags;
    existing->slab = scratch.slab;
    existing->tag_meta = scratch.tag_meta;
    existing->tag_meta_cap = scratch.tag_meta_cap;
    existing->tag_count = scratch.tag_count;
    existing->slab_len = scratch.slab_len;
    existing->fold_budget = scratch.fold_budget;
  } else {
    size_t new_count = dst->lane_count + 1;
    folden_lane_t *nl = realloc(dst->lanes, new_count * sizeof(folden_lane_t));
    if (!nl) {
      free(scratch.tags);
      free(scratch.slab);
      free(scratch.tag_meta);
      return FOLDEN_ERR_NOMEM;
    }
    dst->lanes = nl;
    dst->lanes[dst->lane_count] = scratch;
    dst->lane_count = new_count;
  }
  return FOLDEN_OK;
}

folden_status_t folden_lane_merge_subset(folden_capsule_t *dst,
                                         const folden_capsule_t *src,
                                         folden_relay_t *relay,
                                         int pass_idx) {
  if (!dst || !src) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_status_t last = FOLDEN_OK;
  for (size_t i = 0; i < src->lane_count; i++) {
    int active = 1;
    if (src->lane_count >= 2) {
      if (pass_idx == 0) {
        active = src->lanes[i].lane_id % 2 == 1;
      } else if (pass_idx == 1) {
        active = src->lanes[i].lane_id % 2 == 0;
      }
    } else if (pass_idx != 0) {
      active = 0;
    }
    if (!active) {
      continue;
    }
    last = merge_one_lane(dst, &src->lanes[i], relay);
    if (last != FOLDEN_OK) {
      return last;
    }
  }
  return last;
}

folden_status_t folden_lane_merge_impl(folden_capsule_t *dst,
                                       const folden_capsule_t *src,
                                       folden_relay_t *relay) {
  if (!dst || !src) {
    return FOLDEN_ERR_BAD_STATE;
  }

  int passes = folden_lane_graph_required_passes(dst, src);
  for (int p = 0; p < passes; p++) {
    folden_status_t st = folden_lane_merge_subset(dst, src, relay, p);
    if (st != FOLDEN_OK) {
      return st;
    }
  }
  if (relay && relay->journal && relay->defer) {
    folden_journal_tick(relay->journal, relay->defer);
  }
  return FOLDEN_OK;
}

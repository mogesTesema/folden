#include "fold_sched.h"

#include "tag_index.h"

folden_status_t folden_fold_sched_prepare_lane(folden_lane_t *lane,
                                               folden_sched_phase_t phase) {
  if (!lane) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (phase == FOLDEN_SCHED_PARTIAL_RESUME || phase == FOLDEN_SCHED_STREAM_TAIL) {
    lane->fold_budget = 1;
    return FOLDEN_OK;
  }
  if (lane->fold_budget == 0) {
    lane->fold_budget = 1;
  }
  for (size_t i = 0; i < lane->tag_count; i++) {
    if (lane->tags[i].tag_id == FOLDEN_TAG_FOLD_EXTEND) {
      lane->fold_budget += lane->tags[i].span;
    }
  }
  return FOLDEN_OK;
}

folden_status_t folden_fold_sched_materialize_index(folden_lane_t *lane,
                                                    int resume_from_partial) {
  if (!lane) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (lane->slab_compressed && resume_from_partial) {
    lane->fold_budget = 1;
  }
  folden_sched_phase_t phase =
      resume_from_partial ? FOLDEN_SCHED_PARTIAL_RESUME : FOLDEN_SCHED_FULL;
  folden_status_t st = folden_fold_sched_prepare_lane(lane, phase);
  if (st != FOLDEN_OK) {
    return st;
  }
  return folden_tag_index_build_partial(lane, resume_from_partial);
}

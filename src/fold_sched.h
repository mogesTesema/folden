#ifndef FOLDEN_FOLD_SCHED_H
#define FOLDEN_FOLD_SCHED_H

#include "folden_internal.h"

typedef enum folden_sched_phase {
  FOLDEN_SCHED_FULL = 0,
  FOLDEN_SCHED_PARTIAL_RESUME = 1,
  FOLDEN_SCHED_STREAM_TAIL = 2
} folden_sched_phase_t;

folden_status_t folden_fold_sched_prepare_lane(folden_lane_t *lane,
                                               folden_sched_phase_t phase);
folden_status_t folden_fold_sched_materialize_index(folden_lane_t *lane,
                                                    int resume_from_partial);

#endif /* FOLDEN_FOLD_SCHED_H */

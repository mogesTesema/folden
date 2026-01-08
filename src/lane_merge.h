#ifndef FOLDEN_LANE_MERGE_H
#define FOLDEN_LANE_MERGE_H

#include "folden_internal.h"

folden_status_t folden_lane_merge_impl(folden_capsule_t *dst,
                                       const folden_capsule_t *src,
                                       folden_relay_t *relay);
folden_status_t folden_lane_merge_subset(folden_capsule_t *dst,
                                         const folden_capsule_t *src,
                                         folden_relay_t *relay,
                                         int pass_idx);

#endif /* FOLDEN_LANE_MERGE_H */

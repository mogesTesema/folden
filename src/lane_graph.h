#ifndef FOLDEN_LANE_GRAPH_H
#define FOLDEN_LANE_GRAPH_H

#include "folden_internal.h"

int folden_lane_graph_required_passes(const folden_capsule_t *dst,
                                      const folden_capsule_t *src);
folden_status_t folden_lane_graph_merge_pass(folden_capsule_t *dst,
                                             const folden_capsule_t *src,
                                             folden_relay_t *relay,
                                             int pass_idx);

#endif /* FOLDEN_LANE_GRAPH_H */

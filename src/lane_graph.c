#include "lane_graph.h"

#include "lane_merge.h"

int folden_lane_graph_required_passes(const folden_capsule_t *dst,
                                      const folden_capsule_t *src) {
  (void)dst;
  if (src && src->lane_count >= 2) {
    return 3;
  }
  return 1;
}

folden_status_t folden_lane_graph_merge_pass(folden_capsule_t *dst,
                                             const folden_capsule_t *src,
                                             folden_relay_t *relay,
                                             int pass_idx) {
  return folden_lane_merge_subset(dst, src, relay, pass_idx);
}

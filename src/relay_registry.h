#ifndef FOLDEN_RELAY_REGISTRY_H
#define FOLDEN_RELAY_REGISTRY_H

#include "folden_internal.h"

folden_relay_t *folden_relay_create_impl(void);
void folden_relay_destroy_impl(folden_relay_t *relay);

folden_status_t folden_relay_adopt_impl(folden_relay_t *relay,
                                        const folden_capsule_t *capsule);
folden_status_t folden_relay_release_impl(folden_relay_t *relay, uint32_t lane_id);
folden_status_t folden_relay_bump_generation_impl(folden_relay_t *relay, uint32_t lane_id,
                                                uint32_t gen);

folden_relay_entry_t *folden_relay_find(folden_relay_t *relay, uint32_t lane_id);
int folden_relay_any_arena_borrowed(folden_relay_t *relay);
folden_status_t folden_relay_queue_adopt_lane(folden_relay_t *relay,
                                              const folden_lane_t *lane);

folden_status_t folden_relay_scheduler_tick_impl(folden_relay_t *relay);

folden_status_t folden_relay_route_add_impl(folden_relay_t *relay, uint32_t lane_id,
                                            uint32_t next_hop, uint32_t cost);
folden_status_t folden_relay_route_lookup_impl(folden_relay_t *relay, uint32_t lane_id,
                                               uint32_t *out_next_hop);

folden_status_t folden_relay_lane_stats_impl(folden_relay_t *relay, uint32_t lane_id,
                                             folden_lane_stats_t *out);
size_t folden_relay_top_lanes_impl(folden_relay_t *relay, size_t n,
                                   folden_lane_stats_t *out);

folden_status_t folden_relay_acl_set_rule_impl(folden_relay_t *relay, uint32_t lane_id,
                                               folden_trust_level_t min_trust);
folden_status_t folden_relay_check_access_impl(folden_relay_t *relay, uint32_t lane_id,
                                               folden_trust_level_t caller_trust);

#endif /* FOLDEN_RELAY_REGISTRY_H */

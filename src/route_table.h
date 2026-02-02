#ifndef FOLDEN_ROUTE_TABLE_H
#define FOLDEN_ROUTE_TABLE_H

#include "folden_internal.h"

folden_route_table_t *folden_route_table_create(size_t capacity);
void folden_route_table_destroy(folden_route_table_t *t);

/* Inserts or updates the route for lane_id. If the table is full, the
 * least-recently-used entry is evicted to make room. */
folden_status_t folden_route_table_insert(folden_route_table_t *t, uint32_t lane_id,
                                          uint32_t next_hop, uint32_t cost);

/* On success, writes the next hop and marks the entry as most-recently-used. */
folden_status_t folden_route_table_lookup(folden_route_table_t *t, uint32_t lane_id,
                                          uint32_t *out_next_hop);

/* Records a failed forwarding attempt against lane_id's route and returns the
 * backoff (in ticks) the caller should wait before retrying. */
uint32_t folden_route_table_note_failure(folden_route_table_t *t, uint32_t lane_id);

/* Clears failure/backoff state after a successful forward. */
void folden_route_table_note_success(folden_route_table_t *t, uint32_t lane_id);

size_t folden_route_table_count(const folden_route_table_t *t);

/* Resolves a full forwarding path starting at `start_lane_id` by treating
 * each hop's next_hop as potentially itself a lane_id with its own route,
 * chaining until a hop has no further route. Writes up to `max_hops` hop
 * values into `out_hops` and the actual count into `*out_hop_count`.
 * Returns FOLDEN_ERR_BAD_STATE if the chain revisits a hop (a routing
 * loop) or exceeds `max_hops` before terminating. */
folden_status_t folden_route_table_resolve_path(folden_route_table_t *t,
                                                uint32_t start_lane_id,
                                                uint32_t *out_hops, size_t max_hops,
                                                size_t *out_hop_count);

#endif /* FOLDEN_ROUTE_TABLE_H */

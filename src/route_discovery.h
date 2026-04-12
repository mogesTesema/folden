#ifndef FOLDEN_ROUTE_DISCOVERY_H
#define FOLDEN_ROUTE_DISCOVERY_H

#include "folden/folden.h"
#include "folden/folden_types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct folden_route_discovery folden_route_discovery_t;

/* A distance vector advertisement parsed from a routing capsule */
typedef struct folden_route_adv {
  uint32_t target_lane;
  uint32_t next_hop;
  uint32_t cost;
  uint32_t seq_num;
  uint64_t timestamp_ms;
} folden_route_adv_t;

/* Creates a dynamic route discovery manager tied to a relay.
 * Tracks distance vector state and automatically injects best paths
 * into the relay's static route table. */
folden_route_discovery_t *folden_route_discovery_create(folden_relay_t *relay, uint32_t local_node_id);

void folden_route_discovery_destroy(folden_route_discovery_t *rd);

/* Process an incoming routing advertisement.
 * If the route is better (lower cost or newer seq), updates the local table. */
folden_status_t folden_route_discovery_recv(folden_route_discovery_t *rd, const folden_route_adv_t *adv);

/* Ages out stale routes that haven't been refreshed within timeout_ms. */
void folden_route_discovery_tick(folden_route_discovery_t *rd, uint64_t current_ms, uint64_t timeout_ms);

/* Serializes the current best paths into a buffer to be broadcast to neighbors.
 * Returns the number of bytes written, or an error if the buffer is too small. */
folden_status_t folden_route_discovery_emit_update(folden_route_discovery_t *rd, uint8_t *out_buf, size_t out_cap, size_t *out_len);

/* Retrieves the current best known cost for a target lane, or FOLDEN_ERR_BAD_STATE if unreachable. */
folden_status_t folden_route_discovery_get_cost(folden_route_discovery_t *rd, uint32_t target_lane, uint32_t *out_cost);

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_ROUTE_DISCOVERY_H */

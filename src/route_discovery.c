#include "route_discovery.h"
#include <stdlib.h>
#include <string.h>

#define MAX_ROUTES 1024

typedef struct {
  folden_route_adv_t adv;
  int active;
} route_entry_t;

struct folden_route_discovery {
  folden_relay_t *relay;
  uint32_t local_node_id;
  route_entry_t entries[MAX_ROUTES];
  size_t count;
  uint32_t seq_counter;
};

folden_route_discovery_t *folden_route_discovery_create(folden_relay_t *relay, uint32_t local_node_id) {
  if (!relay) return NULL;
  folden_route_discovery_t *rd = calloc(1, sizeof(folden_route_discovery_t));
  if (!rd) return NULL;
  rd->relay = relay;
  rd->local_node_id = local_node_id;
  return rd;
}

void folden_route_discovery_destroy(folden_route_discovery_t *rd) {
  free(rd);
}

folden_status_t folden_route_discovery_recv(folden_route_discovery_t *rd, const folden_route_adv_t *adv) {
  if (!rd || !adv) return FOLDEN_ERR_BAD_STATE;

  // Prevent routing loops
  if (adv->next_hop == rd->local_node_id) {
    return FOLDEN_OK; // Ignore our own broadcast loops silently
  }

  uint32_t new_cost = adv->cost + 1; // 1 hop to neighbor

  for (size_t i = 0; i < rd->count; i++) {
    if (rd->entries[i].active && rd->entries[i].adv.target_lane == adv->target_lane) {
      if (adv->seq_num > rd->entries[i].adv.seq_num ||
         (adv->seq_num == rd->entries[i].adv.seq_num && new_cost < rd->entries[i].adv.cost)) {
        
        rd->entries[i].adv.seq_num = adv->seq_num;
        rd->entries[i].adv.cost = new_cost;
        rd->entries[i].adv.next_hop = adv->next_hop;
        rd->entries[i].adv.timestamp_ms = adv->timestamp_ms;
        
        folden_relay_route_add(rd->relay, adv->target_lane, adv->next_hop, new_cost);
        return FOLDEN_OK;
      }
      return FOLDEN_OK; // Ignore older or worse routes
    }
  }

  // New route
  if (rd->count < MAX_ROUTES) {
    size_t i = rd->count++;
    rd->entries[i].active = 1;
    rd->entries[i].adv = *adv;
    rd->entries[i].adv.cost = new_cost;
    folden_relay_route_add(rd->relay, adv->target_lane, adv->next_hop, new_cost);
    return FOLDEN_OK;
  }

  // Fallback: overwrite oldest if full
  size_t oldest = 0;
  uint64_t oldest_ts = UINT64_MAX;
  for (size_t i = 0; i < MAX_ROUTES; i++) {
    if (rd->entries[i].adv.timestamp_ms < oldest_ts) {
      oldest_ts = rd->entries[i].adv.timestamp_ms;
      oldest = i;
    }
  }
  
  rd->entries[oldest].active = 1;
  rd->entries[oldest].adv = *adv;
  rd->entries[oldest].adv.cost = new_cost;
  folden_relay_route_add(rd->relay, adv->target_lane, adv->next_hop, new_cost);
  return FOLDEN_OK;
}

void folden_route_discovery_tick(folden_route_discovery_t *rd, uint64_t current_ms, uint64_t timeout_ms) {
  if (!rd) return;
  for (size_t i = 0; i < rd->count; i++) {
    if (rd->entries[i].active) {
      if (current_ms > rd->entries[i].adv.timestamp_ms &&
          (current_ms - rd->entries[i].adv.timestamp_ms) > timeout_ms) {
        rd->entries[i].active = 0;
        // The underlying static route table will handle LRU eviction on its own,
        // but we stop broadcasting it.
      }
    }
  }
}

folden_status_t folden_route_discovery_emit_update(folden_route_discovery_t *rd, uint8_t *out_buf, size_t out_cap, size_t *out_len) {
  if (!rd || !out_buf || !out_len) return FOLDEN_ERR_BAD_STATE;
  
  size_t written = 0;
  rd->seq_counter++;

  // Simple binary framing for route broadcasts
  // Format: [4b target][4b cost][4b seq]
  for (size_t i = 0; i < rd->count; i++) {
    if (rd->entries[i].active) {
      if (written + 12 > out_cap) {
        *out_len = written;
        return FOLDEN_ERR_NOMEM;
      }
      
      uint32_t t = rd->entries[i].adv.target_lane;
      uint32_t c = rd->entries[i].adv.cost;
      uint32_t s = rd->seq_counter; // bump our own sequence outward
      
      out_buf[written++] = (t >> 24) & 0xFF;
      out_buf[written++] = (t >> 16) & 0xFF;
      out_buf[written++] = (t >> 8) & 0xFF;
      out_buf[written++] = t & 0xFF;
      
      out_buf[written++] = (c >> 24) & 0xFF;
      out_buf[written++] = (c >> 16) & 0xFF;
      out_buf[written++] = (c >> 8) & 0xFF;
      out_buf[written++] = c & 0xFF;
      
      out_buf[written++] = (s >> 24) & 0xFF;
      out_buf[written++] = (s >> 16) & 0xFF;
      out_buf[written++] = (s >> 8) & 0xFF;
      out_buf[written++] = s & 0xFF;
    }
  }
  
  *out_len = written;
  return FOLDEN_OK;
}

folden_status_t folden_route_discovery_get_cost(folden_route_discovery_t *rd, uint32_t target_lane, uint32_t *out_cost) {
  if (!rd || !out_cost) return FOLDEN_ERR_BAD_STATE;
  
  for (size_t i = 0; i < rd->count; i++) {
    if (rd->entries[i].active && rd->entries[i].adv.target_lane == target_lane) {
      *out_cost = rd->entries[i].adv.cost;
      return FOLDEN_OK;
    }
  }
  
  return FOLDEN_ERR_BAD_STATE;
}

#include "relay_registry.h"

#include "capsule_cache.h"
#include "defer_free.h"
#include "journal.h"
#include "lane_acl.h"
#include "metrics.h"
#include "relay_queue.h"
#include "replicate.h"
#include "route_cache.h"
#include "route_table.h"
#include "slab_arena.h"

#include <stdlib.h>
#include <string.h>

#define FOLDEN_ROUTE_TABLE_CAPACITY 64

folden_relay_t *folden_relay_create_impl(void) {
  folden_relay_t *relay = calloc(1, sizeof(folden_relay_t));
  if (!relay) {
    return NULL;
  }
  relay->queue = folden_relay_queue_create();
  relay->cache = folden_cache_create();
  relay->defer = folden_defer_create();
  relay->journal = folden_journal_create();
  relay->quorum = folden_replicate_quorum_create();
  relay->fanout = folden_replicate_fanout_create(relay->quorum);
  relay->fsm = folden_replicate_fsm_create();
  folden_replicate_quorum_attach_fsm(relay->quorum, relay->fsm);
  relay->route_cache = folden_route_cache_create();
  relay->routes = folden_route_table_create(FOLDEN_ROUTE_TABLE_CAPACITY);
  relay->metrics = folden_metrics_create();
  relay->acl = folden_lane_acl_create();
  relay->session_id = 1;
  return relay;
}

void folden_relay_destroy_impl(folden_relay_t *relay) {
  if (!relay) {
    return;
  }
  folden_defer_drain(relay->defer, UINT32_MAX);
  /* arena_borrows tracks outstanding zero-copy refs; free_token
   * quietly skips still-borrowed blocks so the worst case is a leak. */
  for (size_t i = 0; i < relay->count; i++) {
    if (relay->entries[i].arena_token) {
      folden_arena_free_token(folden_arena_get_global(),
                              relay->entries[i].arena_token);
    } else {
      free(relay->entries[i].slab);
    }
    free(relay->entries[i].cached_tags);
    free(relay->entries[i].cached_meta);
    memset(&relay->entries[i], 0, sizeof(relay->entries[i]));
  }
  folden_relay_queue_destroy(relay->queue);
  folden_cache_destroy(relay->cache);
  folden_defer_destroy(relay->defer);
  if (relay->routes) {
    folden_route_table_destroy(relay->routes);
  }
  if (relay->metrics) {
    folden_metrics_destroy(relay->metrics);
  }
  if (relay->acl) {
    folden_lane_acl_destroy(relay->acl);
  }
  if (relay->route_cache) {
    folden_route_cache_destroy(relay->route_cache);
  }
  if (relay->fanout) {
    folden_replicate_fanout_destroy(relay->fanout);
  }
  if (relay->quorum) {
    folden_replicate_quorum_destroy(relay->quorum);
  }
  if (relay->fsm) {
    folden_replicate_fsm_destroy(relay->fsm);
  }
  if (relay->journal) {
    folden_journal_destroy(relay->journal);
  }
  free(relay->entries);
  free(relay);
}

folden_relay_entry_t *folden_relay_find(folden_relay_t *relay, uint32_t lane_id) {
  for (size_t i = 0; i < relay->count; i++) {
    if (relay->entries[i].lane_id == lane_id) {
      return &relay->entries[i];
    }
  }
  return NULL;
}

static void relay_journal_append(folden_relay_t *relay,
                                 const folden_capsule_t *capsule) {
  if (!relay || !relay->journal || !capsule || !capsule->wire_copy) {
    return;
  }
  for (size_t i = 0; i < capsule->lane_count; i++) {
    folden_journal_append(relay->journal, capsule->wire_copy, capsule->wire_len,
                          capsule->lanes[i].lane_id);
  }
}

folden_status_t folden_relay_adopt_impl(folden_relay_t *relay,
                                        const folden_capsule_t *capsule) {
  if (!relay || !capsule || !relay->queue) {
    return FOLDEN_ERR_BAD_STATE;
  }

  for (size_t i = 0; i < capsule->lane_count; i++) {
    folden_status_t st =
        folden_relay_queue_enqueue_adopt(relay->queue, &capsule->lanes[i]);
    if (st != FOLDEN_OK) {
      return st;
    }
  }
  folden_status_t st = folden_relay_queue_flush(relay, relay->queue, relay->defer);
  if (st == FOLDEN_OK) {
    relay_journal_append(relay, capsule);
    for (size_t i = 0; i < capsule->lane_count; i++) {
      const folden_lane_t *lane = &capsule->lanes[i];
      folden_metrics_record_adopt(relay->metrics, lane->lane_id,
                                  lane->has_vault_gen ? lane->vault_gen : 0,
                                  lane->slab_len);
    }
  }
  return st;
}

folden_status_t folden_relay_release_impl(folden_relay_t *relay, uint32_t lane_id) {
  folden_relay_entry_t *ent = folden_relay_find(relay, lane_id);
  if (!ent) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_metrics_record_release(relay->metrics, lane_id);
  if (ent->refcount > 0) {
    ent->refcount--;
  }
  if (ent->arena_token && ent->arena_borrows > 0) {
    ent->arena_borrows--;
    folden_arena_release_borrow(folden_arena_get_global(), ent->arena_token);
  }
  if (ent->refcount == 0) {
    if (ent->arena_token) {
      folden_arena_free_token(folden_arena_get_global(), ent->arena_token);
      ent->arena_token = 0;
    } else {
      free(ent->slab);
    }
    ent->slab = NULL;
    ent->slab_len = 0;
  }
  return FOLDEN_OK;
}

folden_status_t folden_relay_bump_generation_impl(folden_relay_t *relay, uint32_t lane_id,
                                                 uint32_t gen) {
  folden_relay_entry_t *ent = folden_relay_find(relay, lane_id);
  if (!ent) {
    return FOLDEN_ERR_BAD_STATE;
  }
  ent->generation = gen;
  return FOLDEN_OK;
}

folden_status_t folden_relay_scheduler_tick_impl(folden_relay_t *relay) {
  if (!relay || !relay->journal) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_status_t st = folden_journal_tick(relay->journal, relay->defer);
  if (relay->defer) {
    folden_defer_drain(relay->defer, UINT32_MAX);
  }
  return st;
}

folden_status_t folden_relay_queue_adopt_lane(folden_relay_t *relay,
                                              const folden_lane_t *lane) {
  if (!relay || !lane) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_status_t st = folden_relay_queue_enqueue_adopt(relay->queue, lane);
  if (st != FOLDEN_OK) {
    return st;
  }
  return folden_relay_queue_flush(relay, relay->queue, relay->defer);
}

folden_status_t folden_relay_route_add_impl(folden_relay_t *relay, uint32_t lane_id,
                                            uint32_t next_hop, uint32_t cost) {
  if (!relay || !relay->routes) {
    return FOLDEN_ERR_BAD_STATE;
  }
  return folden_route_table_insert(relay->routes, lane_id, next_hop, cost);
}

folden_status_t folden_relay_route_lookup_impl(folden_relay_t *relay, uint32_t lane_id,
                                               uint32_t *out_next_hop) {
  if (!relay || !relay->routes) {
    return FOLDEN_ERR_BAD_STATE;
  }
  return folden_route_table_lookup(relay->routes, lane_id, out_next_hop);
}

static void lane_metrics_to_stats(const folden_lane_metrics_t *m, folden_lane_stats_t *out) {
  out->lane_id = m->lane_id;
  out->adopt_count = m->adopt_count;
  out->release_count = m->release_count;
  out->emit_count = m->emit_count;
  out->bytes_in = m->bytes_in;
  out->bytes_out = m->bytes_out;
  out->last_generation = m->last_generation;
}

folden_status_t folden_relay_lane_stats_impl(folden_relay_t *relay, uint32_t lane_id,
                                             folden_lane_stats_t *out) {
  if (!relay || !out) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_lane_metrics_t m;
  folden_status_t st = folden_metrics_snapshot(relay->metrics, lane_id, &m);
  if (st != FOLDEN_OK) {
    return st;
  }
  lane_metrics_to_stats(&m, out);
  return FOLDEN_OK;
}

size_t folden_relay_top_lanes_impl(folden_relay_t *relay, size_t n,
                                   folden_lane_stats_t *out) {
  if (!relay || !out || n == 0) {
    return 0;
  }
  folden_lane_metrics_t *tmp = calloc(n, sizeof(folden_lane_metrics_t));
  if (!tmp) {
    return 0;
  }
  size_t got = folden_metrics_top_n(relay->metrics, n, tmp);
  for (size_t i = 0; i < got; i++) {
    lane_metrics_to_stats(&tmp[i], &out[i]);
  }
  free(tmp);
  return got;
}

folden_status_t folden_relay_acl_set_rule_impl(folden_relay_t *relay, uint32_t lane_id,
                                               folden_trust_level_t min_trust) {
  if (!relay || !relay->acl) {
    return FOLDEN_ERR_BAD_STATE;
  }
  return folden_lane_acl_set_rule(relay->acl, lane_id, min_trust);
}

folden_status_t folden_relay_check_access_impl(folden_relay_t *relay, uint32_t lane_id,
                                               folden_trust_level_t caller_trust) {
  if (!relay) {
    return FOLDEN_ERR_BAD_STATE;
  }
  return folden_lane_acl_check(relay->acl, lane_id, caller_trust);
}

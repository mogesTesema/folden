#include "folden/folden.h"

#include "capsule_emit.h"
#include "foldconf.h"
#include "lane_merge.h"
#include "recovery.h"
#include "relay_registry.h"
#include "slab_decode.h"
#include "stream_reasm.h"
#include "vault_store.h"
#include "wire_validator.h"

folden_relay_t *folden_relay_create(void) {
  return folden_relay_create_impl();
}

void folden_relay_destroy(folden_relay_t *relay) {
  folden_relay_destroy_impl(relay);
}

folden_vault_t *folden_vault_create(void) {
  return folden_vault_create_impl();
}

void folden_vault_destroy(folden_vault_t *vault) {
  folden_vault_destroy_impl(vault);
}

folden_stream_t *folden_stream_create(void) {
  return folden_stream_create_impl();
}

void folden_stream_destroy(folden_stream_t *stream) {
  folden_stream_destroy_impl(stream);
}

folden_status_t folden_decode_buffer(const uint8_t *data, size_t len,
                                     folden_capsule_t **out) {
  return folden_parse_capsule(data, len, out);
}

folden_status_t folden_decode_stream(folden_stream_t *stream,
                                     const uint8_t *chunk, size_t chunk_len,
                                     folden_capsule_t **out) {
  return folden_stream_feed(stream, chunk, chunk_len, out);
}

folden_status_t folden_merge_lanes(folden_capsule_t *dst,
                                   const folden_capsule_t *src,
                                   folden_relay_t *relay) {
  return folden_lane_merge_impl(dst, src, relay);
}

folden_status_t folden_relay_adopt(folden_relay_t *relay,
                                   const folden_capsule_t *capsule) {
  return folden_relay_adopt_impl(relay, capsule);
}

folden_status_t folden_relay_release(folden_relay_t *relay, uint32_t lane_id) {
  return folden_relay_release_impl(relay, lane_id);
}

folden_status_t folden_relay_bump_generation(folden_relay_t *relay, uint32_t lane_id,
                                             uint32_t gen) {
  return folden_relay_bump_generation_impl(relay, lane_id, gen);
}

folden_status_t folden_relay_scheduler_tick(folden_relay_t *relay) {
  return folden_relay_scheduler_tick_impl(relay);
}

folden_status_t folden_vault_checkpoint(folden_vault_t *vault,
                                        folden_relay_t *relay) {
  return folden_vault_checkpoint_impl(vault, relay);
}

folden_status_t folden_vault_restore(folden_vault_t *vault,
                                     folden_relay_t *relay) {
  return folden_vault_restore_impl(vault, relay);
}

folden_status_t folden_emit_capsule(const folden_capsule_t *capsule,
                                    folden_relay_t *relay,
                                    uint8_t **out, size_t *out_len) {
  return folden_capsule_emit_impl(capsule, relay, out, out_len);
}

folden_status_t folden_capsule_emit_zero_copy(folden_relay_t *relay,
                                              uint32_t lane_id,
                                              const uint8_t **out_ptr,
                                              size_t *out_len) {
  return folden_capsule_emit_zero_copy_impl(relay, lane_id, out_ptr, out_len);
}

size_t folden_capsule_lane_count(const folden_capsule_t *capsule) {
  return capsule ? capsule->lane_count : 0;
}

uint32_t folden_capsule_lane_id(const folden_capsule_t *capsule, size_t idx) {
  if (!capsule || idx >= capsule->lane_count) {
    return 0;
  }
  return capsule->lanes[idx].lane_id;
}

int folden_capsule_lane_has_vault_gen(const folden_capsule_t *capsule, size_t idx) {
  if (!capsule || idx >= capsule->lane_count) {
    return 0;
  }
  return capsule->lanes[idx].has_vault_gen;
}

uint32_t folden_capsule_lane_vault_gen(const folden_capsule_t *capsule, size_t idx) {
  if (!capsule || idx >= capsule->lane_count) {
    return 0;
  }
  return capsule->lanes[idx].vault_gen;
}

folden_status_t folden_relay_route_add(folden_relay_t *relay, uint32_t lane_id,
                                       uint32_t next_hop, uint32_t cost) {
  return folden_relay_route_add_impl(relay, lane_id, next_hop, cost);
}

folden_status_t folden_relay_route_lookup(folden_relay_t *relay, uint32_t lane_id,
                                          uint32_t *out_next_hop) {
  return folden_relay_route_lookup_impl(relay, lane_id, out_next_hop);
}

folden_status_t folden_relay_lane_stats(folden_relay_t *relay, uint32_t lane_id,
                                        folden_lane_stats_t *out) {
  return folden_relay_lane_stats_impl(relay, lane_id, out);
}

size_t folden_relay_top_lanes(folden_relay_t *relay, size_t n,
                              folden_lane_stats_t *out) {
  return folden_relay_top_lanes_impl(relay, n, out);
}

folden_status_t folden_relay_configure(folden_relay_t *relay, const char *text,
                                       size_t len, int *out_error_line) {
  if (!relay) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_foldconf_t conf;
  folden_status_t st = folden_foldconf_parse(text, len, &conf, out_error_line);
  if (st != FOLDEN_OK) {
    return st;
  }
  return folden_foldconf_apply(&conf, relay);
}

size_t folden_relay_recover(folden_relay_t *relay) {
  if (!relay) {
    return 0;
  }
  return folden_recovery_replay_journal(relay->journal, relay);
}

folden_status_t folden_relay_acl_set_rule(folden_relay_t *relay, uint32_t lane_id,
                                          folden_trust_level_t min_trust) {
  return folden_relay_acl_set_rule_impl(relay, lane_id, min_trust);
}

folden_status_t folden_relay_check_access(folden_relay_t *relay, uint32_t lane_id,
                                          folden_trust_level_t caller_trust) {
  return folden_relay_check_access_impl(relay, lane_id, caller_trust);
}

folden_status_t folden_capsule_validate_strict(const folden_capsule_t *cap) {
  if (!cap) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (folden_wire_validate_no_duplicate_lanes(cap) != 0) {
    return FOLDEN_ERR_BAD_TAG;
  }
  for (size_t i = 0; i < cap->lane_count; i++) {
    if (folden_wire_validate_no_duplicate_tags(&cap->lanes[i]) != 0) {
      return FOLDEN_ERR_BAD_TAG;
    }
  }
  return FOLDEN_OK;
}

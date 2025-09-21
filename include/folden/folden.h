/* Public API for folden capsule relay library. */
#ifndef FOLDEN_H
#define FOLDEN_H

#include "folden_types.h"

#ifdef __cplusplus
extern "C" {
#endif

folden_relay_t *folden_relay_create(void);
void folden_relay_destroy(folden_relay_t *relay);

folden_vault_t *folden_vault_create(void);
void folden_vault_destroy(folden_vault_t *vault);

folden_stream_t *folden_stream_create(void);
void folden_stream_destroy(folden_stream_t *stream);

folden_status_t folden_decode_buffer(const uint8_t *data, size_t len,
                                     folden_capsule_t **out);
void folden_capsule_free(folden_capsule_t *capsule);

folden_status_t folden_decode_stream(folden_stream_t *stream,
                                     const uint8_t *chunk, size_t chunk_len,
                                     folden_capsule_t **out);

folden_status_t folden_merge_lanes(folden_capsule_t *dst,
                                   const folden_capsule_t *src,
                                   folden_relay_t *relay);

folden_status_t folden_relay_adopt(folden_relay_t *relay,
                                   const folden_capsule_t *capsule);
folden_status_t folden_relay_release(folden_relay_t *relay, uint32_t lane_id);
folden_status_t folden_relay_bump_generation(folden_relay_t *relay, uint32_t lane_id,
                                             uint32_t gen);

folden_status_t folden_relay_scheduler_tick(folden_relay_t *relay);

folden_status_t folden_vault_checkpoint(folden_vault_t *vault,
                                        folden_relay_t *relay);
folden_status_t folden_vault_restore(folden_vault_t *vault,
                                     folden_relay_t *relay);

folden_status_t folden_emit_capsule(const folden_capsule_t *capsule,
                                    folden_relay_t *relay,
                                    uint8_t **out, size_t *out_len);

folden_status_t folden_capsule_emit_zero_copy(folden_relay_t *relay,
                                              uint32_t lane_id,
                                              const uint8_t **out_ptr,
                                              size_t *out_len);

size_t folden_capsule_lane_count(const folden_capsule_t *capsule);
uint32_t folden_capsule_lane_id(const folden_capsule_t *capsule, size_t idx);
int folden_capsule_lane_has_vault_gen(const folden_capsule_t *capsule, size_t idx);
uint32_t folden_capsule_lane_vault_gen(const folden_capsule_t *capsule, size_t idx);

folden_replicate_quorum_t *folden_replicate_quorum_create(void);
void folden_replicate_quorum_destroy(folden_replicate_quorum_t *q);
folden_replicate_fsm_t *folden_replicate_fsm_create(void);
void folden_replicate_fsm_destroy(folden_replicate_fsm_t *fsm);
folden_replicate_fanout_t *folden_replicate_fanout_create(folden_replicate_quorum_t *q);
void folden_replicate_fanout_destroy(folden_replicate_fanout_t *f);

folden_status_t folden_replicate_quorum_register_peer(folden_replicate_quorum_t *q,
                                                      uint32_t peer_id);
void folden_replicate_quorum_attach_fsm(folden_replicate_quorum_t *q,
                                        folden_replicate_fsm_t *fsm);
folden_status_t folden_replicate_send_peer(folden_replicate_fanout_t *f,
                                           folden_replicate_fsm_t *fsm,
                                           uint32_t peer_id,
                                           const uint8_t *data, size_t len);
folden_status_t folden_replicate_ack_partial(folden_replicate_quorum_t *q,
                                             uint32_t peer_id);
folden_status_t folden_replicate_fsm_retry_tick(folden_replicate_fsm_t *fsm,
                                                folden_replicate_quorum_t *q,
                                                uint32_t peer_id,
                                                const uint8_t *data, size_t len);

folden_status_t folden_relay_route_add(folden_relay_t *relay, uint32_t lane_id,
                                       uint32_t next_hop, uint32_t cost);
folden_status_t folden_relay_route_lookup(folden_relay_t *relay, uint32_t lane_id,
                                          uint32_t *out_next_hop);

folden_status_t folden_relay_lane_stats(folden_relay_t *relay, uint32_t lane_id,
                                        folden_lane_stats_t *out);
size_t folden_relay_top_lanes(folden_relay_t *relay, size_t n,
                              folden_lane_stats_t *out);

folden_status_t folden_relay_configure(folden_relay_t *relay, const char *text,
                                       size_t len, int *out_error_line);

/* Re-adopts every capsule still recoverable from the relay's own journal.
 * Intended for restoring live state after a restart. */
size_t folden_relay_recover(folden_relay_t *relay);

/* Stricter structural checks beyond what decode itself enforces: rejects a
 * capsule with two lanes sharing a lane_id, or a lane carrying the same
 * tag_id more than once. A capsule that decoded successfully can still
 * fail this -- decode only enforces what's needed for memory safety, not
 * every semantic invariant a strict consumer might want. */
folden_status_t folden_capsule_validate_strict(const folden_capsule_t *cap);

folden_status_t folden_relay_acl_set_rule(folden_relay_t *relay, uint32_t lane_id,
                                          folden_trust_level_t min_trust);
folden_status_t folden_relay_check_access(folden_relay_t *relay, uint32_t lane_id,
                                          folden_trust_level_t caller_trust);

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_H */

#ifndef FOLDEN_REPLICATE_H
#define FOLDEN_REPLICATE_H

#include "folden_internal.h"

folden_replicate_quorum_t *folden_replicate_quorum_create(void);
void folden_replicate_quorum_destroy(folden_replicate_quorum_t *q);
folden_replicate_fsm_t *folden_replicate_fsm_create(void);
void folden_replicate_fsm_destroy(folden_replicate_fsm_t *fsm);
void folden_replicate_fsm_set_max_attempts(folden_replicate_fsm_t *fsm, uint32_t max_attempts);
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

/* Number of peers still awaiting a full ack across the quorum. */
uint32_t folden_replicate_quorum_pending(const folden_replicate_quorum_t *q);

/* Sets how many acked peers constitute quorum. A threshold of 0 (the
 * default) means quorum is never considered reached automatically. */
void folden_replicate_quorum_set_threshold(folden_replicate_quorum_t *q,
                                           uint32_t threshold);

/* Returns 1 once at least `threshold` registered peers have acked (are in
 * the ACKED or DONE state), 0 otherwise. */
int folden_replicate_quorum_has_quorum(const folden_replicate_quorum_t *q);

/* Fans a merged lane out to every registered peer via the fanout's attached
 * quorum/fsm pair. Used by the lane-merge path. */
folden_status_t folden_replicate_fanout_lane(folden_replicate_fanout_t *f,
                                             folden_relay_t *relay,
                                             const folden_lane_t *lane);

#endif /* FOLDEN_REPLICATE_H */

#include "replicate.h"

#include <stdlib.h>
#include <string.h>

/* Per-peer replication state.
 *
 *   IDLE   -- registered, nothing sent yet.
 *   SENT   -- a payload is outstanding, awaiting ack.
 *   ACKED  -- the peer acknowledged; payload has been released.
 *   DONE   -- terminal: either acked and folded into quorum completion,
 *             or retry budget exhausted and the peer was given up on.
 */
typedef enum folden_peer_state {
  FOLDEN_PEER_IDLE = 0,
  FOLDEN_PEER_SENT,
  FOLDEN_PEER_ACKED,
  FOLDEN_PEER_DONE
} folden_peer_state_t;

typedef struct folden_peer_slot {
  uint32_t peer_id;
  int registered;
  folden_peer_state_t state;
  uint8_t *payload;
  size_t payload_len;
  uint32_t attempts;
} folden_peer_slot_t;

struct folden_repl_quorum {
  folden_peer_slot_t *peers;
  size_t count;
  size_t cap;
  folden_replicate_fsm_t *fsm;
  uint32_t quorum_threshold;
};

struct folden_repl_fsm {
  uint32_t max_attempts;
};

struct folden_repl_fanout {
  folden_replicate_quorum_t *quorum;
};

#define FOLDEN_REPL_MAX_ATTEMPTS 4

folden_replicate_quorum_t *folden_replicate_quorum_create(void) {
  return calloc(1, sizeof(folden_replicate_quorum_t));
}

void folden_replicate_quorum_destroy(folden_replicate_quorum_t *q) {
  if (!q) {
    return;
  }
  for (size_t i = 0; i < q->count; i++) {
    free(q->peers[i].payload);
  }
  free(q->peers);
  free(q);
}

folden_replicate_fsm_t *folden_replicate_fsm_create(void) {
  folden_replicate_fsm_t *fsm = calloc(1, sizeof(folden_replicate_fsm_t));
  if (fsm) {
    fsm->max_attempts = FOLDEN_REPL_MAX_ATTEMPTS;
  }
  return fsm;
}

void folden_replicate_fsm_destroy(folden_replicate_fsm_t *fsm) {
  free(fsm);
}

void folden_replicate_fsm_set_max_attempts(folden_replicate_fsm_t *fsm, uint32_t max_attempts) {
  if (!fsm || max_attempts == 0) {
    return;
  }
  fsm->max_attempts = max_attempts;
}

folden_replicate_fanout_t *folden_replicate_fanout_create(folden_replicate_quorum_t *q) {
  folden_replicate_fanout_t *f = calloc(1, sizeof(folden_replicate_fanout_t));
  if (f) {
    f->quorum = q;
  }
  return f;
}

void folden_replicate_fanout_destroy(folden_replicate_fanout_t *f) {
  free(f);
}

static folden_peer_slot_t *find_peer(folden_replicate_quorum_t *q, uint32_t peer_id) {
  if (!q) {
    return NULL;
  }
  for (size_t i = 0; i < q->count; i++) {
    if (q->peers[i].registered && q->peers[i].peer_id == peer_id) {
      return &q->peers[i];
    }
  }
  return NULL;
}

static int grow_peers(folden_replicate_quorum_t *q) {
  size_t nc = q->cap ? q->cap * 2 : 8;
  folden_peer_slot_t *np = realloc(q->peers, nc * sizeof(folden_peer_slot_t));
  if (!np) {
    return -1;
  }
  memset(np + q->cap, 0, (nc - q->cap) * sizeof(folden_peer_slot_t));
  q->peers = np;
  q->cap = nc;
  return 0;
}

folden_status_t folden_replicate_quorum_register_peer(folden_replicate_quorum_t *q,
                                                      uint32_t peer_id) {
  if (!q) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (find_peer(q, peer_id)) {
    return FOLDEN_OK;
  }
  if (q->count >= q->cap && grow_peers(q) != 0) {
    return FOLDEN_ERR_NOMEM;
  }
  folden_peer_slot_t *slot = &q->peers[q->count++];
  slot->peer_id = peer_id;
  slot->registered = 1;
  slot->state = FOLDEN_PEER_IDLE;
  slot->attempts = 0;
  return FOLDEN_OK;
}

void folden_replicate_quorum_attach_fsm(folden_replicate_quorum_t *q,
                                        folden_replicate_fsm_t *fsm) {
  if (q) {
    q->fsm = fsm;
  }
}

static folden_status_t peer_set_payload(folden_peer_slot_t *slot, const uint8_t *data,
                                        size_t len) {
  free(slot->payload);
  slot->payload = NULL;
  slot->payload_len = 0;
  if (len > 0) {
    slot->payload = malloc(len);
    if (!slot->payload) {
      return FOLDEN_ERR_NOMEM;
    }
    if (data) {
      memcpy(slot->payload, data, len);
    }
    slot->payload_len = len;
  }
  return FOLDEN_OK;
}

folden_status_t folden_replicate_send_peer(folden_replicate_fanout_t *f,
                                           folden_replicate_fsm_t *fsm,
                                           uint32_t peer_id,
                                           const uint8_t *data, size_t len) {
  if (!f || !f->quorum) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_peer_slot_t *slot = find_peer(f->quorum, peer_id);
  if (!slot) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (slot->state == FOLDEN_PEER_DONE || slot->state == FOLDEN_PEER_ACKED) {
    /* Already settled; a fresh send restarts replication for this peer. */
    slot->state = FOLDEN_PEER_IDLE;
    slot->attempts = 0;
  }
  folden_status_t st = peer_set_payload(slot, data, len);
  if (st != FOLDEN_OK) {
    return st;
  }
  slot->state = FOLDEN_PEER_SENT;
  slot->attempts = 1;
  (void)fsm;
  return FOLDEN_OK;
}

folden_status_t folden_replicate_ack_partial(folden_replicate_quorum_t *q,
                                             uint32_t peer_id) {
  if (!q) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_peer_slot_t *slot = find_peer(q, peer_id);
  if (!slot) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (slot->state != FOLDEN_PEER_SENT) {
    /* Stale, duplicate, or premature ack -- nothing outstanding to settle. */
    return FOLDEN_OK;
  }
  free(slot->payload);
  slot->payload = NULL;
  slot->payload_len = 0;
  slot->state = FOLDEN_PEER_ACKED;
  return FOLDEN_OK;
}

folden_status_t folden_replicate_fsm_retry_tick(folden_replicate_fsm_t *fsm,
                                                folden_replicate_quorum_t *q,
                                                uint32_t peer_id,
                                                const uint8_t *data, size_t len) {
  if (!fsm || !q) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_peer_slot_t *slot = find_peer(q, peer_id);
  if (!slot) {
    return FOLDEN_ERR_BAD_STATE;
  }
  if (slot->state == FOLDEN_PEER_IDLE) {
    folden_status_t st = peer_set_payload(slot, data, len);
    if (st != FOLDEN_OK) {
      return st;
    }
    slot->state = FOLDEN_PEER_SENT;
    slot->attempts = 1;
    return FOLDEN_OK;
  }
  if (slot->state != FOLDEN_PEER_SENT) {
    return FOLDEN_OK;
  }
  if (slot->attempts >= fsm->max_attempts) {
    free(slot->payload);
    slot->payload = NULL;
    slot->payload_len = 0;
    slot->state = FOLDEN_PEER_DONE;
    return FOLDEN_OK;
  }
  slot->attempts++;
  return peer_set_payload(slot, data, len);
}

uint32_t folden_replicate_quorum_pending(const folden_replicate_quorum_t *q) {
  if (!q) {
    return 0;
  }
  uint32_t pending = 0;
  for (size_t i = 0; i < q->count; i++) {
    if (q->peers[i].registered &&
        (q->peers[i].state == FOLDEN_PEER_IDLE || q->peers[i].state == FOLDEN_PEER_SENT)) {
      pending++;
    }
  }
  return pending;
}

void folden_replicate_quorum_set_threshold(folden_replicate_quorum_t *q,
                                           uint32_t threshold) {
  if (!q) {
    return;
  }
  q->quorum_threshold = threshold;
}

int folden_replicate_quorum_has_quorum(const folden_replicate_quorum_t *q) {
  if (!q || q->quorum_threshold == 0) {
    return 0;
  }
  /* Only a genuine ack counts. FOLDEN_PEER_DONE also means "settled" but
   * covers the give-up-after-max-attempts path too, which is a failure,
   * not a vote toward quorum. */
  uint32_t acked = 0;
  for (size_t i = 0; i < q->count; i++) {
    if (q->peers[i].registered && q->peers[i].state == FOLDEN_PEER_ACKED) {
      acked++;
    }
  }
  return acked >= q->quorum_threshold;
}

folden_status_t folden_replicate_fanout_lane(folden_replicate_fanout_t *f,
                                             folden_relay_t *relay,
                                             const folden_lane_t *lane) {
  if (!f || !f->quorum || !lane) {
    return FOLDEN_ERR_BAD_STATE;
  }
  (void)relay;
  folden_status_t last = FOLDEN_OK;
  for (size_t i = 0; i < f->quorum->count; i++) {
    if (!f->quorum->peers[i].registered) {
      continue;
    }
    folden_status_t st = folden_replicate_send_peer(
        f, f->quorum->fsm, f->quorum->peers[i].peer_id, lane->slab, lane->slab_len);
    if (st != FOLDEN_OK) {
      last = st;
    }
  }
  return last;
}

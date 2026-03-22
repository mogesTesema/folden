#ifndef FOLDEN_RELAY_SESSION_H
#define FOLDEN_RELAY_SESSION_H

#include "folden_internal.h"

/*
 * Session lifecycle for relay connections.
 *
 * State transitions:
 *   INIT -> HANDSHAKE -> ESTABLISHED -> DRAINING -> CLOSED
 *
 * A session binds to a relay and can hold its own vault
 * snapshot, scoping checkpoint/restore to the connection
 * rather than the global relay state.
 */

typedef enum {
  FOLDEN_SESSION_INIT,
  FOLDEN_SESSION_HANDSHAKE,
  FOLDEN_SESSION_ESTABLISHED,
  FOLDEN_SESSION_DRAINING,
  FOLDEN_SESSION_CLOSED
} folden_session_state_t;

typedef struct folden_session {
  uint64_t              session_id;
  folden_session_state_t state;
  folden_relay_t        *relay;        // borrowed, not owned
  folden_vault_t        *session_vault; // session-scoped snapshot (owned)
  uint64_t              created_tick;
  uint64_t              last_active_tick;
  uint64_t              keepalive_interval;
  uint64_t              adopt_count;
  uint64_t              emit_count;
  uint32_t              session_token; // opaque token for auth
  int                   readonly;      // if set, adopts are rejected
} folden_session_t;

/* Session registry: tracks all live sessions for one relay. */
typedef struct folden_session_registry {
  folden_session_t **sessions;
  size_t             count;
  size_t             cap;
  uint64_t           next_id;
} folden_session_registry_t;

folden_session_registry_t *folden_session_registry_create(void);
void folden_session_registry_destroy(folden_session_registry_t *reg);

/* Create a new session bound to the given relay. */
folden_session_t *folden_session_create(folden_session_registry_t *reg,
                                        folden_relay_t *relay,
                                        uint64_t keepalive_interval);

/* Advance the session state machine.
 * Returns FOLDEN_OK or an error if the transition is invalid. */
folden_status_t folden_session_advance(folden_session_t *s,
                                       folden_session_state_t target);

/* Session-scoped adopt: delegates to the bound relay. */
folden_status_t folden_session_adopt(folden_session_t *s,
                                     folden_capsule_t *cap);

/* Session-scoped emit: delegates to the bound relay. */
folden_status_t folden_session_emit(folden_session_t *s,
                                    const folden_capsule_t *cap,
                                    uint8_t **out, size_t *out_len);

/* Take a vault snapshot scoped to this session. */
folden_status_t folden_session_checkpoint(folden_session_t *s);

/* Restore relay state from the session-scoped snapshot. */
folden_status_t folden_session_restore(folden_session_t *s);

/* Touch keepalive. Returns 1 if the session has timed out. */
int folden_session_check_timeout(folden_session_t *s, uint64_t current_tick);

/* Close and free a session. Removes it from the registry. */
void folden_session_close(folden_session_registry_t *reg,
                          folden_session_t *s);

/* Sweep all timed-out sessions. Returns number closed. */
size_t folden_session_sweep(folden_session_registry_t *reg,
                            uint64_t current_tick);

#endif

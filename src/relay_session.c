#include "relay_session.h"

#include "relay_registry.h"
#include "snapshot_codec.h"
#include "capsule_emit.h"

#include <stdlib.h>
#include <string.h>

folden_session_registry_t *folden_session_registry_create(void) {
  return calloc(1, sizeof(folden_session_registry_t));
}

void folden_session_registry_destroy(folden_session_registry_t *reg) {
  if (!reg) return;
  for (size_t i = 0; i < reg->count; i++) {
    if (reg->sessions[i]) {
      folden_session_close(reg, reg->sessions[i]);
      // close may shuffle the array, recheck
      i--;
    }
  }
  free(reg->sessions);
  free(reg);
}

static int registry_grow(folden_session_registry_t *reg) {
  size_t nc = reg->cap ? reg->cap * 2 : 8;
  folden_session_t **ns = realloc(reg->sessions, nc * sizeof(*ns));
  if (!ns) return -1;
  reg->sessions = ns;
  reg->cap = nc;
  return 0;
}

folden_session_t *folden_session_create(folden_session_registry_t *reg,
                                        folden_relay_t *relay,
                                        uint64_t keepalive_interval) {
  if (!reg || !relay) return NULL;
  if (reg->count >= reg->cap && registry_grow(reg) != 0) return NULL;

  folden_session_t *s = calloc(1, sizeof(*s));
  if (!s) return NULL;

  s->session_id = ++reg->next_id;
  s->state = FOLDEN_SESSION_INIT;
  s->relay = relay;
  s->keepalive_interval = keepalive_interval > 0 ? keepalive_interval : 300;
  s->session_vault = NULL;
  s->readonly = 0;

  reg->sessions[reg->count++] = s;
  return s;
}

/* valid transitions:
 *   INIT -> HANDSHAKE
 *   HANDSHAKE -> ESTABLISHED
 *   ESTABLISHED -> DRAINING
 *   DRAINING -> CLOSED
 *   any -> CLOSED (forced) */
folden_status_t folden_session_advance(folden_session_t *s,
                                       folden_session_state_t target) {
  if (!s) return FOLDEN_ERR_BAD_STATE;

  // forced close from any state
  if (target == FOLDEN_SESSION_CLOSED) {
    s->state = FOLDEN_SESSION_CLOSED;
    return FOLDEN_OK;
  }

  int ok = 0;
  switch (s->state) {
    case FOLDEN_SESSION_INIT:
      ok = (target == FOLDEN_SESSION_HANDSHAKE);
      break;
    case FOLDEN_SESSION_HANDSHAKE:
      ok = (target == FOLDEN_SESSION_ESTABLISHED);
      break;
    case FOLDEN_SESSION_ESTABLISHED:
      ok = (target == FOLDEN_SESSION_DRAINING);
      break;
    case FOLDEN_SESSION_DRAINING:
      ok = (target == FOLDEN_SESSION_CLOSED);
      break;
    case FOLDEN_SESSION_CLOSED:
      ok = 0;
      break;
  }

  if (!ok) return FOLDEN_ERR_BAD_STATE;
  s->state = target;
  return FOLDEN_OK;
}

folden_status_t folden_session_adopt(folden_session_t *s,
                                     folden_capsule_t *cap) {
  if (!s || !cap) return FOLDEN_ERR_BAD_STATE;
  if (s->state != FOLDEN_SESSION_ESTABLISHED) return FOLDEN_ERR_BAD_STATE;
  if (s->readonly) return FOLDEN_ERR_THROTTLED;

  folden_relay_adopt_impl(s->relay, cap);
  s->adopt_count++;
  return FOLDEN_OK;
}

folden_status_t folden_session_emit(folden_session_t *s,
                                    const folden_capsule_t *cap,
                                    uint8_t **out, size_t *out_len) {
  if (!s || !cap) return FOLDEN_ERR_BAD_STATE;
  if (s->state != FOLDEN_SESSION_ESTABLISHED &&
      s->state != FOLDEN_SESSION_DRAINING) {
    return FOLDEN_ERR_BAD_STATE;
  }

  folden_status_t st = folden_capsule_emit_impl(cap, s->relay, out, out_len);
  if (st == FOLDEN_OK) s->emit_count++;
  return st;
}

folden_status_t folden_session_checkpoint(folden_session_t *s) {
  if (!s || !s->relay) return FOLDEN_ERR_BAD_STATE;
  if (s->state != FOLDEN_SESSION_ESTABLISHED) return FOLDEN_ERR_BAD_STATE;

  if (!s->session_vault) {
    s->session_vault = calloc(1, sizeof(folden_vault_t));
    if (!s->session_vault) return FOLDEN_ERR_NOMEM;
  }

  return folden_snapshot_codec_capture(s->session_vault, s->relay);
}

folden_status_t folden_session_restore(folden_session_t *s) {
  if (!s || !s->relay || !s->session_vault) return FOLDEN_ERR_BAD_STATE;
  if (s->state != FOLDEN_SESSION_ESTABLISHED) return FOLDEN_ERR_BAD_STATE;

  return folden_snapshot_codec_restore(s->session_vault, s->relay);
}

int folden_session_check_timeout(folden_session_t *s, uint64_t current_tick) {
  if (!s) return 1;
  if (s->state == FOLDEN_SESSION_CLOSED) return 1;
  if (current_tick - s->last_active_tick > s->keepalive_interval) return 1;
  return 0;
}

void folden_session_close(folden_session_registry_t *reg,
                          folden_session_t *s) {
  if (!s) return;

  s->state = FOLDEN_SESSION_CLOSED;

  // release session-scoped vault snapshot
  if (s->session_vault) {
    folden_snapshot_codec_release(s->session_vault);
    free(s->session_vault->snapshot);
    free(s->session_vault);
    s->session_vault = NULL;
  }

  // remove from registry
  if (reg) {
    for (size_t i = 0; i < reg->count; i++) {
      if (reg->sessions[i] == s) {
        reg->sessions[i] = reg->sessions[--reg->count];
        break;
      }
    }
  }

  free(s);
}

size_t folden_session_sweep(folden_session_registry_t *reg,
                            uint64_t current_tick) {
  if (!reg) return 0;
  size_t closed = 0;
  // iterate backwards because close shuffles the array
  for (size_t i = reg->count; i > 0; i--) {
    folden_session_t *s = reg->sessions[i - 1];
    if (folden_session_check_timeout(s, current_tick)) {
      folden_session_close(reg, s);
      closed++;
    }
  }
  return closed;
}

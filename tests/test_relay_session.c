#include "minctest.h"
#include "../src/relay_session.h"
#include "../src/relay_registry.h"

TEST(test_session_lifecycle) {
  folden_relay_t *relay = folden_relay_create_impl();
  ASSERT(relay != NULL);

  folden_session_registry_t *reg = folden_session_registry_create();
  ASSERT(reg != NULL);

  folden_session_t *s = folden_session_create(reg, relay, 300);
  ASSERT(s != NULL);
  ASSERT_EQ(s->state, FOLDEN_SESSION_INIT);
  ASSERT_EQ((int)reg->count, 1);

  // valid transitions
  ASSERT(folden_session_advance(s, FOLDEN_SESSION_HANDSHAKE) == FOLDEN_OK);
  ASSERT(folden_session_advance(s, FOLDEN_SESSION_ESTABLISHED) == FOLDEN_OK);
  ASSERT(folden_session_advance(s, FOLDEN_SESSION_DRAINING) == FOLDEN_OK);
  ASSERT(folden_session_advance(s, FOLDEN_SESSION_CLOSED) == FOLDEN_OK);

  // invalid transition
  ASSERT(folden_session_advance(s, FOLDEN_SESSION_ESTABLISHED) == FOLDEN_ERR_BAD_STATE);

  folden_session_close(reg, s);
  ASSERT_EQ((int)reg->count, 0);

  folden_session_registry_destroy(reg);
  folden_relay_destroy_impl(relay);
}

TEST(test_session_checkpoint) {
  folden_relay_t *relay = folden_relay_create_impl();
  folden_session_registry_t *reg = folden_session_registry_create();
  folden_session_t *s = folden_session_create(reg, relay, 300);

  folden_session_advance(s, FOLDEN_SESSION_HANDSHAKE);
  folden_session_advance(s, FOLDEN_SESSION_ESTABLISHED);

  // checkpoint should succeed in established state
  folden_status_t st = folden_session_checkpoint(s);
  ASSERT(st == FOLDEN_OK);
  ASSERT(s->session_vault != NULL);

  // restore should also succeed
  st = folden_session_restore(s);
  ASSERT(st == FOLDEN_OK);

  folden_session_close(reg, s);
  folden_session_registry_destroy(reg);
  folden_relay_destroy_impl(relay);
}

TEST(test_session_timeout) {
  folden_relay_t *relay = folden_relay_create_impl();
  folden_session_registry_t *reg = folden_session_registry_create();
  folden_session_t *s = folden_session_create(reg, relay, 100);

  s->last_active_tick = 0;

  // tick 50 is fine
  ASSERT(folden_session_check_timeout(s, 50) == 0);
  // tick 150 times out
  ASSERT(folden_session_check_timeout(s, 150) == 1);

  size_t swept = folden_session_sweep(reg, 150);
  ASSERT_EQ((int)swept, 1);
  ASSERT_EQ((int)reg->count, 0);

  folden_session_registry_destroy(reg);
  folden_relay_destroy_impl(relay);
}

void test_relay_session(void) {
  RUN(test_session_lifecycle);
  RUN(test_session_checkpoint);
  RUN(test_session_timeout);
}

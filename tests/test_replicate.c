#include "minctest.h"

#include "replicate.h"

TEST(test_replicate_send_and_ack) {
  folden_replicate_quorum_t *q = folden_replicate_quorum_create();
  folden_replicate_fsm_t *fsm = folden_replicate_fsm_create();
  folden_replicate_fanout_t *f = folden_replicate_fanout_create(q);
  ASSERT_EQ(FOLDEN_OK, folden_replicate_quorum_register_peer(q, 1));
  folden_replicate_quorum_attach_fsm(q, fsm);
  ASSERT_EQ(1u, folden_replicate_quorum_pending(q));

  const uint8_t payload[3] = {1, 2, 3};
  ASSERT_EQ(FOLDEN_OK, folden_replicate_send_peer(f, fsm, 1, payload, sizeof(payload)));
  ASSERT_EQ(1u, folden_replicate_quorum_pending(q));
  ASSERT_EQ(FOLDEN_OK, folden_replicate_ack_partial(q, 1));
  ASSERT_EQ(0u, folden_replicate_quorum_pending(q));

  folden_replicate_fanout_destroy(f);
  folden_replicate_fsm_destroy(fsm);
  folden_replicate_quorum_destroy(q);
}

TEST(test_replicate_retry_gives_up_after_max_attempts) {
  folden_replicate_quorum_t *q = folden_replicate_quorum_create();
  folden_replicate_fsm_t *fsm = folden_replicate_fsm_create();
  folden_replicate_fanout_t *f = folden_replicate_fanout_create(q);
  ASSERT_EQ(FOLDEN_OK, folden_replicate_quorum_register_peer(q, 5));

  const uint8_t payload[2] = {9, 9};
  ASSERT_EQ(FOLDEN_OK, folden_replicate_send_peer(f, fsm, 5, payload, sizeof(payload)));
  for (int i = 0; i < 10; i++) {
    folden_replicate_fsm_retry_tick(fsm, q, 5, payload, sizeof(payload));
  }
  ASSERT_EQ(0u, folden_replicate_quorum_pending(q));

  folden_replicate_fanout_destroy(f);
  folden_replicate_fsm_destroy(fsm);
  folden_replicate_quorum_destroy(q);
}

TEST(test_replicate_unregistered_peer_rejected) {
  folden_replicate_quorum_t *q = folden_replicate_quorum_create();
  folden_replicate_fsm_t *fsm = folden_replicate_fsm_create();
  folden_replicate_fanout_t *f = folden_replicate_fanout_create(q);
  const uint8_t payload[1] = {7};
  ASSERT_EQ(FOLDEN_ERR_BAD_STATE,
            folden_replicate_send_peer(f, fsm, 42, payload, sizeof(payload)));
  ASSERT_EQ(FOLDEN_ERR_BAD_STATE, folden_replicate_ack_partial(q, 42));
  folden_replicate_fanout_destroy(f);
  folden_replicate_fsm_destroy(fsm);
  folden_replicate_quorum_destroy(q);
}

TEST(test_replicate_quorum_reached_at_threshold) {
  folden_replicate_quorum_t *q = folden_replicate_quorum_create();
  folden_replicate_fsm_t *fsm = folden_replicate_fsm_create();
  folden_replicate_fanout_t *f = folden_replicate_fanout_create(q);
  const uint8_t payload[1] = {1};

  for (uint32_t p = 0; p < 3; p++) {
    ASSERT_EQ(FOLDEN_OK, folden_replicate_quorum_register_peer(q, p));
  }
  folden_replicate_quorum_set_threshold(q, 2);
  ASSERT_EQ(0, folden_replicate_quorum_has_quorum(q));

  ASSERT_EQ(FOLDEN_OK, folden_replicate_send_peer(f, fsm, 0, payload, 1));
  ASSERT_EQ(FOLDEN_OK, folden_replicate_send_peer(f, fsm, 1, payload, 1));
  ASSERT_EQ(FOLDEN_OK, folden_replicate_ack_partial(q, 0));
  ASSERT_EQ(0, folden_replicate_quorum_has_quorum(q));

  ASSERT_EQ(FOLDEN_OK, folden_replicate_ack_partial(q, 1));
  ASSERT_EQ(1, folden_replicate_quorum_has_quorum(q));

  folden_replicate_fanout_destroy(f);
  folden_replicate_fsm_destroy(fsm);
  folden_replicate_quorum_destroy(q);
}

TEST(test_replicate_gave_up_peer_does_not_count_toward_quorum) {
  folden_replicate_quorum_t *q = folden_replicate_quorum_create();
  folden_replicate_fsm_t *fsm = folden_replicate_fsm_create();
  folden_replicate_fanout_t *f = folden_replicate_fanout_create(q);
  const uint8_t payload[1] = {1};

  ASSERT_EQ(FOLDEN_OK, folden_replicate_quorum_register_peer(q, 0));
  folden_replicate_quorum_set_threshold(q, 1);

  ASSERT_EQ(FOLDEN_OK, folden_replicate_send_peer(f, fsm, 0, payload, 1));
  for (int i = 0; i < 10; i++) {
    folden_replicate_fsm_retry_tick(fsm, q, 0, payload, 1);
  }
  /* The peer gave up (never acked), so quorum must not be reported. */
  ASSERT_EQ(0, folden_replicate_quorum_has_quorum(q));

  folden_replicate_fanout_destroy(f);
  folden_replicate_fsm_destroy(fsm);
  folden_replicate_quorum_destroy(q);
}

void test_replicate_register(void) {
  RUN(test_replicate_send_and_ack);
  RUN(test_replicate_retry_gives_up_after_max_attempts);
  RUN(test_replicate_unregistered_peer_rejected);
  RUN(test_replicate_quorum_reached_at_threshold);
  RUN(test_replicate_gave_up_peer_does_not_count_toward_quorum);
}

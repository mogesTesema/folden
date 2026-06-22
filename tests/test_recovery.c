#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"

TEST(test_recover_replays_adopted_capsule) {
  folden_wire_tag_t tags[] = {{3, 0, 2}};
  folden_wire_lane_t la = {9, tags, 1, (const uint8_t *)"ab", 2, 0, 0};
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));

  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));

  size_t replayed = folden_relay_recover(relay);
  ASSERT(replayed >= 1);

  folden_lane_stats_t stats;
  ASSERT_EQ(FOLDEN_OK, folden_relay_lane_stats(relay, 9, &stats));
  /* One real adopt plus at least one replayed re-adopt. */
  ASSERT(stats.adopt_count >= 2);

  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(wire);
}

TEST(test_recover_with_empty_journal_is_noop) {
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(0u, folden_relay_recover(relay));
  folden_relay_destroy(relay);
}

TEST(test_recover_replays_multiple_lanes_from_one_segment) {
  folden_wire_tag_t tags_a[] = {{3, 0, 1}};
  folden_wire_lane_t la = {1, tags_a, 1, (const uint8_t *)"a", 1, 0, 0};
  uint8_t *wire_a = NULL;
  size_t len_a = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire_a, &len_a));

  folden_wire_tag_t tags_b[] = {{3, 0, 1}};
  folden_wire_lane_t lb = {2, tags_b, 1, (const uint8_t *)"b", 1, 0, 0};
  uint8_t *wire_b = NULL;
  size_t len_b = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&lb, 1, 0, 0, &wire_b, &len_b));

  folden_capsule_t *cap_a = NULL;
  folden_capsule_t *cap_b = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire_a, len_a, &cap_a));
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire_b, len_b, &cap_b));

  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap_a));
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap_b));

  folden_relay_recover(relay);

  folden_lane_stats_t stats_a, stats_b;
  ASSERT_EQ(FOLDEN_OK, folden_relay_lane_stats(relay, 1, &stats_a));
  ASSERT_EQ(FOLDEN_OK, folden_relay_lane_stats(relay, 2, &stats_b));
  ASSERT(stats_a.adopt_count >= 2);
  ASSERT(stats_b.adopt_count >= 2);

  folden_relay_destroy(relay);
  folden_capsule_free(cap_a);
  folden_capsule_free(cap_b);
  free(wire_a);
  free(wire_b);
}

void test_recovery_register(void) {
  RUN(test_recover_replays_adopted_capsule);
  RUN(test_recover_with_empty_journal_is_noop);
  RUN(test_recover_replays_multiple_lanes_from_one_segment);
}

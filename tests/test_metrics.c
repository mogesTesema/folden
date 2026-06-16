#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"

TEST(test_metrics_track_adopt_and_emit) {
  folden_wire_tag_t tags[] = {{3, 0, 2}};
  folden_wire_lane_t la = {5, tags, 1, (const uint8_t *)"ab", 2, 0, 0};
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));

  uint8_t *out = NULL;
  size_t out_len = 0;
  ASSERT_EQ(FOLDEN_OK, folden_emit_capsule(cap, relay, &out, &out_len));
  free(out);

  folden_lane_stats_t stats;
  ASSERT_EQ(FOLDEN_OK, folden_relay_lane_stats(relay, 5, &stats));
  ASSERT_EQ(1u, stats.adopt_count);
  ASSERT_EQ(1u, stats.emit_count);
  ASSERT_EQ(2u, stats.bytes_in);
  ASSERT_EQ(0u, stats.release_count);

  ASSERT_EQ(FOLDEN_OK, folden_relay_release(relay, 5));
  ASSERT_EQ(FOLDEN_OK, folden_relay_lane_stats(relay, 5, &stats));
  ASSERT_EQ(1u, stats.release_count);

  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(wire);
}

TEST(test_metrics_unknown_lane_rejected) {
  folden_relay_t *relay = folden_relay_create();
  folden_lane_stats_t stats;
  ASSERT_EQ(FOLDEN_ERR_BAD_STATE, folden_relay_lane_stats(relay, 999, &stats));
  folden_relay_destroy(relay);
}

TEST(test_metrics_top_lanes_ordered_by_adopt_count) {
  folden_relay_t *relay = folden_relay_create();

  for (uint32_t lane = 1; lane <= 3; lane++) {
    folden_wire_lane_t la = {lane, NULL, 0, (const uint8_t *)"x", 1, 0, 0};
    uint8_t *wire = NULL;
    size_t wire_len = 0;
    ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire, &wire_len));
    folden_capsule_t *cap = NULL;
    ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
    /* Lane 3 gets adopted three times, lane 2 twice, lane 1 once. */
    for (uint32_t times = 0; times < lane; times++) {
      ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));
    }
    folden_capsule_free(cap);
    free(wire);
  }

  folden_lane_stats_t top[3];
  ASSERT_EQ(3u, folden_relay_top_lanes(relay, 3, top));
  ASSERT_EQ(3u, top[0].lane_id);
  ASSERT_EQ(2u, top[1].lane_id);
  ASSERT_EQ(1u, top[2].lane_id);

  folden_relay_destroy(relay);
}

void test_metrics_register(void) {
  RUN(test_metrics_track_adopt_and_emit);
  RUN(test_metrics_unknown_lane_rejected);
  RUN(test_metrics_top_lanes_ordered_by_adopt_count);
}

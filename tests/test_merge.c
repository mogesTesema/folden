#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"

TEST(test_merge_lane_count) {
  folden_wire_tag_t ta[] = {{3, 0, 2}};
  folden_wire_tag_t tb[] = {{4, 0, 1}};
  folden_wire_lane_t la = {1, ta, 1, (const uint8_t *)"aa", 2, 0, 0};
  folden_wire_lane_t lb = {2, tb, 1, (const uint8_t *)"b", 1, 0, 0};
  uint8_t *wa = NULL;
  uint8_t *wb = NULL;
  size_t la_len = 0;
  size_t lb_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wa, &la_len));
  ASSERT_EQ(0, folden_wire_build_capsule(&lb, 1, 0, 0, &wb, &lb_len));

  folden_capsule_t *ca = NULL;
  folden_capsule_t *cb = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wa, la_len, &ca));
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wb, lb_len, &cb));
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_merge_lanes(ca, cb, NULL));
  ASSERT_EQ(2, folden_capsule_lane_count(ca));
  folden_relay_destroy(relay);
  folden_capsule_free(ca);
  folden_capsule_free(cb);
  free(wa);
  free(wb);
}

void run_merge_tests(void) {
  RUN(test_merge_lane_count);
}

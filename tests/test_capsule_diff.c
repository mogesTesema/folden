#include "minctest.h"

#include "capsule_diff.h"
#include "capsule_wire.h"
#include "folden/folden.h"

TEST(test_diff_detects_added_and_removed_lanes) {
  folden_wire_lane_t old_lanes[] = {
      {1, NULL, 0, (const uint8_t *)"a", 1, 0, 0},
  };
  folden_wire_lane_t new_lanes[] = {
      {2, NULL, 0, (const uint8_t *)"b", 1, 0, 0},
  };
  uint8_t *old_wire = NULL, *new_wire = NULL;
  size_t old_len = 0, new_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(old_lanes, 1, 0, 0, &old_wire, &old_len));
  ASSERT_EQ(0, folden_wire_build_capsule(new_lanes, 1, 0, 0, &new_wire, &new_len));

  folden_capsule_t *old_cap = NULL, *new_cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(old_wire, old_len, &old_cap));
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(new_wire, new_len, &new_cap));

  folden_capsule_diff_t *diff = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_capsule_diff(old_cap, new_cap, &diff));
  ASSERT_EQ(1u, folden_capsule_diff_added_count(diff));
  ASSERT_EQ(2u, folden_capsule_diff_added_lane(diff, 0));
  ASSERT_EQ(1u, folden_capsule_diff_removed_count(diff));
  ASSERT_EQ(1u, folden_capsule_diff_removed_lane(diff, 0));
  ASSERT_EQ(0u, folden_capsule_diff_changed_count(diff));

  folden_capsule_diff_free(diff);
  folden_capsule_free(old_cap);
  folden_capsule_free(new_cap);
  free(old_wire);
  free(new_wire);
}

TEST(test_diff_detects_changed_slab) {
  folden_wire_lane_t old_lanes[] = {
      {1, NULL, 0, (const uint8_t *)"aa", 2, 0, 0},
  };
  folden_wire_lane_t new_lanes[] = {
      {1, NULL, 0, (const uint8_t *)"bb", 2, 0, 0},
  };
  uint8_t *old_wire = NULL, *new_wire = NULL;
  size_t old_len = 0, new_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(old_lanes, 1, 0, 0, &old_wire, &old_len));
  ASSERT_EQ(0, folden_wire_build_capsule(new_lanes, 1, 0, 0, &new_wire, &new_len));

  folden_capsule_t *old_cap = NULL, *new_cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(old_wire, old_len, &old_cap));
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(new_wire, new_len, &new_cap));

  folden_capsule_diff_t *diff = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_capsule_diff(old_cap, new_cap, &diff));
  ASSERT_EQ(0u, folden_capsule_diff_added_count(diff));
  ASSERT_EQ(0u, folden_capsule_diff_removed_count(diff));
  ASSERT_EQ(1u, folden_capsule_diff_changed_count(diff));
  ASSERT_EQ(1u, folden_capsule_diff_changed_lane(diff, 0));

  folden_capsule_diff_free(diff);
  folden_capsule_free(old_cap);
  folden_capsule_free(new_cap);
  free(old_wire);
  free(new_wire);
}

TEST(test_diff_reports_nothing_for_identical_capsules) {
  folden_wire_lane_t lanes[] = {
      {1, NULL, 0, (const uint8_t *)"same", 4, 0, 0},
  };
  uint8_t *wire_a = NULL, *wire_b = NULL;
  size_t len_a = 0, len_b = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(lanes, 1, 0, 0, &wire_a, &len_a));
  ASSERT_EQ(0, folden_wire_build_capsule(lanes, 1, 0, 0, &wire_b, &len_b));

  folden_capsule_t *cap_a = NULL, *cap_b = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire_a, len_a, &cap_a));
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire_b, len_b, &cap_b));

  folden_capsule_diff_t *diff = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_capsule_diff(cap_a, cap_b, &diff));
  ASSERT_EQ(0u, folden_capsule_diff_added_count(diff));
  ASSERT_EQ(0u, folden_capsule_diff_removed_count(diff));
  ASSERT_EQ(0u, folden_capsule_diff_changed_count(diff));

  folden_capsule_diff_free(diff);
  folden_capsule_free(cap_a);
  folden_capsule_free(cap_b);
  free(wire_a);
  free(wire_b);
}

void test_capsule_diff_register(void) {
  RUN(test_diff_detects_added_and_removed_lanes);
  RUN(test_diff_detects_changed_slab);
  RUN(test_diff_reports_nothing_for_identical_capsules);
}

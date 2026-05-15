#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"

TEST(test_validate_strict_accepts_clean_capsule) {
  folden_wire_tag_t tags[] = {{3, 0, 1}, {4, 0, 2}};
  folden_wire_lane_t lanes[] = {
      {1, tags, 2, (const uint8_t *)"a", 1, 0, 0},
      {2, NULL, 0, (const uint8_t *)"b", 1, 0, 0},
  };
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(lanes, 2, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  ASSERT_EQ(FOLDEN_OK, folden_capsule_validate_strict(cap));

  folden_capsule_free(cap);
  free(wire);
}

TEST(test_validate_strict_rejects_duplicate_lane_ids) {
  folden_wire_lane_t lanes[] = {
      {5, NULL, 0, (const uint8_t *)"a", 1, 0, 0},
      {5, NULL, 0, (const uint8_t *)"b", 1, 0, 0},
  };
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(lanes, 2, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  ASSERT_EQ(FOLDEN_ERR_BAD_TAG, folden_capsule_validate_strict(cap));

  folden_capsule_free(cap);
  free(wire);
}

TEST(test_validate_strict_rejects_duplicate_tag_ids_in_one_lane) {
  folden_wire_tag_t tags[] = {{3, 0, 1}, {3, 0, 2}};
  folden_wire_lane_t lane = {1, tags, 2, (const uint8_t *)"a", 1, 0, 0};
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&lane, 1, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  ASSERT_EQ(FOLDEN_ERR_BAD_TAG, folden_capsule_validate_strict(cap));

  folden_capsule_free(cap);
  free(wire);
}

void test_wire_validator_register(void) {
  RUN(test_validate_strict_accepts_clean_capsule);
  RUN(test_validate_strict_rejects_duplicate_lane_ids);
  RUN(test_validate_strict_rejects_duplicate_tag_ids_in_one_lane);
}

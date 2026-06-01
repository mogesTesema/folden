#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"

TEST(test_session_bind_adopt) {
  folden_wire_tag_t tags[] = {{3, 0, 1}};
  folden_wire_lane_t la = {13, tags, 1, (const uint8_t *)"z", 1, 0, 0};
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));
  ASSERT_EQ(FOLDEN_OK, folden_relay_bump_generation(relay, 13, 2));
  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(wire);
}

void test_session_register(void) {
  RUN(test_session_bind_adopt);
}

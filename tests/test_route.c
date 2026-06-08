#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"

TEST(test_route_emit_with_vault_gen) {
  folden_wire_tag_t tags[] = {{2, 0, 3}, {3, 0, 1}};
  folden_wire_lane_t la = {12, tags, 2, (const uint8_t *)"xy", 2, 3, 1};
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 3, 1, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));
  uint8_t *out = NULL;
  size_t out_len = 0;
  ASSERT_EQ(FOLDEN_OK, folden_emit_capsule(cap, relay, &out, &out_len));
  ASSERT(out_len > 8);
  free(out);
  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(wire);
}

void test_route_register(void) {
  RUN(test_route_emit_with_vault_gen);
}

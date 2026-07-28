#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"

TEST(test_readopt_then_tick_then_emit_uses_fresh_slab) {
  folden_wire_tag_t tags[] = {{2, 0, 5}};
  folden_wire_lane_t la = {3, tags, 1, (const uint8_t *)"bbbbb", 5, 5, 1};
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  folden_relay_t *relay = folden_relay_create();

  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));
  ASSERT_EQ(FOLDEN_OK, folden_relay_scheduler_tick(relay));
  ASSERT_EQ(FOLDEN_OK, folden_relay_scheduler_tick(relay));

  uint8_t *out = NULL;
  size_t out_len = 0;
  ASSERT_EQ(FOLDEN_OK, folden_emit_capsule(cap, relay, &out, &out_len));
  ASSERT(out_len > 0);
  free(out);

  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(wire);
}

void test_route_cache_freshness_register(void) {
  RUN(test_readopt_then_tick_then_emit_uses_fresh_slab);
}

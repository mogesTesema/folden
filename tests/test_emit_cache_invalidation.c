#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"

/* Emitting against a lane whose generation doesn't match the live entry
 * used to defer the old backing block for freeing without invalidating the
 * route/capsule caches still pinned to it -- a later emit keyed on the
 * original generation would then read through a freed pointer. */
TEST(test_emit_generation_mismatch_then_tick_then_reemit_is_safe) {
  folden_wire_tag_t tags1[] = {{2, 0, 1}};
  folden_wire_lane_t la1 = {7, tags1, 1, (const uint8_t *)"hello", 5, 1, 1};
  uint8_t *w1 = NULL;
  size_t l1 = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la1, 1, 0, 0, &w1, &l1));

  folden_wire_tag_t tags2[] = {{2, 0, 2}};
  folden_wire_lane_t la2 = {7, tags2, 1, (const uint8_t *)"world", 5, 2, 1};
  uint8_t *w2 = NULL;
  size_t l2 = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la2, 1, 0, 0, &w2, &l2));

  folden_capsule_t *cap1 = NULL;
  folden_capsule_t *cap2 = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(w1, l1, &cap1));
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(w2, l2, &cap2));

  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap1));

  uint8_t *out = NULL;
  size_t out_len = 0;
  ASSERT_EQ(FOLDEN_OK, folden_emit_capsule(cap2, relay, &out, &out_len));
  free(out);
  out = NULL;

  ASSERT_EQ(FOLDEN_OK, folden_relay_scheduler_tick(relay));
  ASSERT_EQ(FOLDEN_OK, folden_relay_scheduler_tick(relay));

  ASSERT_EQ(FOLDEN_OK, folden_emit_capsule(cap1, relay, &out, &out_len));
  free(out);

  folden_relay_destroy(relay);
  folden_capsule_free(cap1);
  folden_capsule_free(cap2);
  free(w1);
  free(w2);
}

void test_emit_cache_invalidation_register(void) {
  RUN(test_emit_generation_mismatch_then_tick_then_reemit_is_safe);
}

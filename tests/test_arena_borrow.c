#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"
#include "folden_internal.h"

/* Re-adopting the same lane twice defers the old arena token for cleanup.
 * A subsequent scheduler tick used to force-free that token even while a
 * vault snapshot still held a protective borrow on it. */
TEST(test_readopt_then_checkpoint_then_tick_is_safe) {
  folden_wire_tag_t tags[] = {{3, 0, 2}};
  folden_wire_lane_t la = {1, tags, 1, (const uint8_t *)"aa", 2, 0, 0};
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  folden_relay_t *relay = folden_relay_create();
  folden_vault_t *vault = folden_vault_create();

  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));
  ASSERT_EQ(FOLDEN_OK, folden_vault_checkpoint(vault, relay));
  /* Re-adopting the same lane defers the token the vault just borrowed. */
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));
  ASSERT_EQ(FOLDEN_OK, folden_relay_scheduler_tick(relay));
  ASSERT_EQ(FOLDEN_OK, folden_relay_scheduler_tick(relay));
  /* The borrowed block must still be readable through the snapshot. */
  ASSERT_EQ(FOLDEN_OK, folden_vault_restore(vault, relay));

  folden_vault_destroy(vault);
  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(wire);
}

void test_arena_borrow_register(void) {
  RUN(test_readopt_then_checkpoint_then_tick_is_safe);
}

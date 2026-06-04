#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"

TEST(test_vault_checkpoint_restore) {
  folden_wire_tag_t tags[] = {{3, 0, 1}};
  folden_wire_lane_t la = {10, tags, 1, (const uint8_t *)"x", 1, 0, 0};
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  folden_relay_t *relay = folden_relay_create();
  folden_vault_t *vault = folden_vault_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));
  ASSERT_EQ(FOLDEN_OK, folden_vault_checkpoint(vault, relay));
  ASSERT_EQ(FOLDEN_OK, folden_vault_restore(vault, relay));
  folden_relay_release(relay, 10);
  folden_vault_destroy(vault);
  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(wire);
}

TEST(test_vault_survives_release_before_restore) {
  folden_wire_tag_t tags[] = {{3, 0, 1}};
  folden_wire_lane_t la = {11, tags, 1, (const uint8_t *)"hello", 5, 0, 0};
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  folden_relay_t *relay = folden_relay_create();
  folden_vault_t *vault = folden_vault_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));
  ASSERT_EQ(FOLDEN_OK, folden_vault_checkpoint(vault, relay));
  /* Dropping the lane's only reference between checkpoint and restore used
   * to free the arena block the snapshot still pointed at. */
  ASSERT_EQ(FOLDEN_OK, folden_relay_release(relay, 11));
  ASSERT_EQ(FOLDEN_OK, folden_vault_restore(vault, relay));

  folden_vault_destroy(vault);
  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(wire);
}

void run_vault_tests(void) {
  RUN(test_vault_checkpoint_restore);
  RUN(test_vault_survives_release_before_restore);
}

#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"
#include "vault_registry.h"

TEST(test_vault_registry_checkpoint_and_restore_by_name) {
  folden_wire_lane_t la = {1, NULL, 0, (const uint8_t *)"x", 1, 0, 0};
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));

  folden_vault_registry_t *reg = folden_vault_registry_create();
  ASSERT_EQ(FOLDEN_OK, folden_vault_registry_checkpoint(reg, "before", relay));
  ASSERT_EQ(1u, folden_vault_registry_count(reg));
  ASSERT_EQ(FOLDEN_OK, folden_vault_registry_restore(reg, "before", relay));

  folden_vault_registry_destroy(reg);
  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(wire);
}

TEST(test_vault_registry_restore_unknown_name_fails) {
  folden_relay_t *relay = folden_relay_create();
  folden_vault_registry_t *reg = folden_vault_registry_create();
  ASSERT_EQ(FOLDEN_ERR_BAD_STATE, folden_vault_registry_restore(reg, "nope", relay));
  folden_vault_registry_destroy(reg);
  folden_relay_destroy(relay);
}

TEST(test_vault_registry_recheckpoint_same_name_replaces) {
  folden_relay_t *relay = folden_relay_create();
  folden_vault_registry_t *reg = folden_vault_registry_create();
  ASSERT_EQ(FOLDEN_OK, folden_vault_registry_checkpoint(reg, "snap", relay));
  ASSERT_EQ(1u, folden_vault_registry_count(reg));
  ASSERT_EQ(FOLDEN_OK, folden_vault_registry_checkpoint(reg, "snap", relay));
  ASSERT_EQ(1u, folden_vault_registry_count(reg));
  folden_vault_registry_destroy(reg);
  folden_relay_destroy(relay);
}

TEST(test_vault_registry_holds_multiple_named_snapshots) {
  folden_relay_t *relay = folden_relay_create();
  folden_vault_registry_t *reg = folden_vault_registry_create();
  ASSERT_EQ(FOLDEN_OK, folden_vault_registry_checkpoint(reg, "a", relay));
  ASSERT_EQ(FOLDEN_OK, folden_vault_registry_checkpoint(reg, "b", relay));
  ASSERT_EQ(2u, folden_vault_registry_count(reg));
  ASSERT_EQ(FOLDEN_OK, folden_vault_registry_remove(reg, "a"));
  ASSERT_EQ(1u, folden_vault_registry_count(reg));
  folden_vault_registry_destroy(reg);
  folden_relay_destroy(relay);
}

void test_vault_registry_register(void) {
  RUN(test_vault_registry_checkpoint_and_restore_by_name);
  RUN(test_vault_registry_restore_unknown_name_fails);
  RUN(test_vault_registry_recheckpoint_same_name_replaces);
  RUN(test_vault_registry_holds_multiple_named_snapshots);
}

#include "minctest.h"

#include "folden/folden.h"

TEST(test_acl_default_allows_any_trust) {
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_check_access(relay, 42, FOLDEN_TRUST_UNTRUSTED));
  folden_relay_destroy(relay);
}

TEST(test_acl_denies_below_minimum_trust) {
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_acl_set_rule(relay, 7, FOLDEN_TRUST_OPERATOR));
  ASSERT_EQ(FOLDEN_ERR_BAD_STATE,
            folden_relay_check_access(relay, 7, FOLDEN_TRUST_PEER));
  ASSERT_EQ(FOLDEN_OK, folden_relay_check_access(relay, 7, FOLDEN_TRUST_OPERATOR));
  ASSERT_EQ(FOLDEN_OK, folden_relay_check_access(relay, 7, FOLDEN_TRUST_ADMIN));
  folden_relay_destroy(relay);
}

TEST(test_acl_rule_only_applies_to_its_own_lane) {
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_acl_set_rule(relay, 1, FOLDEN_TRUST_ADMIN));
  ASSERT_EQ(FOLDEN_OK, folden_relay_check_access(relay, 2, FOLDEN_TRUST_UNTRUSTED));
  ASSERT_EQ(FOLDEN_ERR_BAD_STATE,
            folden_relay_check_access(relay, 1, FOLDEN_TRUST_UNTRUSTED));
  folden_relay_destroy(relay);
}

TEST(test_acl_rule_update_replaces_previous_value) {
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_acl_set_rule(relay, 3, FOLDEN_TRUST_ADMIN));
  ASSERT_EQ(FOLDEN_ERR_BAD_STATE,
            folden_relay_check_access(relay, 3, FOLDEN_TRUST_PEER));
  ASSERT_EQ(FOLDEN_OK, folden_relay_acl_set_rule(relay, 3, FOLDEN_TRUST_PEER));
  ASSERT_EQ(FOLDEN_OK, folden_relay_check_access(relay, 3, FOLDEN_TRUST_PEER));
  folden_relay_destroy(relay);
}

void test_lane_acl_register(void) {
  RUN(test_acl_default_allows_any_trust);
  RUN(test_acl_denies_below_minimum_trust);
  RUN(test_acl_rule_only_applies_to_its_own_lane);
  RUN(test_acl_rule_update_replaces_previous_value);
}

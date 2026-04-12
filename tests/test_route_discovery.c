#include "minctest.h"
#include "../src/route_discovery.h"

TEST(test_route_discovery_basic) {
  folden_relay_t *relay = folden_relay_create();
  ASSERT(relay != NULL);
  
  folden_route_discovery_t *rd = folden_route_discovery_create(relay, 100);
  ASSERT(rd != NULL);
  
  // Receive a new route
  folden_route_adv_t adv = {
    .target_lane = 1,
    .next_hop = 200,
    .cost = 5,
    .seq_num = 1,
    .timestamp_ms = 1000
  };
  
  ASSERT_EQ(FOLDEN_OK, folden_route_discovery_recv(rd, &adv));
  
  // Cost should be adv.cost + 1
  uint32_t cost = 0;
  ASSERT_EQ(FOLDEN_OK, folden_route_discovery_get_cost(rd, 1, &cost));
  ASSERT_EQ((int)cost, 6);
  
  // Receive a better route (lower cost)
  folden_route_adv_t adv2 = {
    .target_lane = 1,
    .next_hop = 300,
    .cost = 2,
    .seq_num = 1,
    .timestamp_ms = 1100
  };
  
  ASSERT_EQ(FOLDEN_OK, folden_route_discovery_recv(rd, &adv2));
  ASSERT_EQ(FOLDEN_OK, folden_route_discovery_get_cost(rd, 1, &cost));
  ASSERT_EQ((int)cost, 3);
  
  // Ignore loop
  folden_route_adv_t loop = {
    .target_lane = 1,
    .next_hop = 100, // our local node ID
    .cost = 1,
    .seq_num = 2,
    .timestamp_ms = 1200
  };
  ASSERT_EQ(FOLDEN_OK, folden_route_discovery_recv(rd, &loop));
  ASSERT_EQ(FOLDEN_OK, folden_route_discovery_get_cost(rd, 1, &cost));
  ASSERT_EQ((int)cost, 3); // unchanged
  
  // Emit updates
  uint8_t buf[128];
  size_t len = 0;
  ASSERT_EQ(FOLDEN_OK, folden_route_discovery_emit_update(rd, buf, sizeof(buf), &len));
  ASSERT_EQ((int)len, 12); // 1 route * 12 bytes
  
  // Check the payload (target, cost, seq)
  uint32_t t = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  uint32_t c = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
  ASSERT_EQ((int)t, 1);
  ASSERT_EQ((int)c, 3);
  
  folden_route_discovery_destroy(rd);
  folden_relay_destroy(relay);
}

void test_route_discovery(void) {
  RUN(test_route_discovery_basic);
}

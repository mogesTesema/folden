#include "minctest.h"
#include "../src/flow_control.h"

TEST(test_basic_flow) {
  folden_flow_ctrl_t *fc = folden_flow_create(5.0, 10.0, 100);
  ASSERT(fc != NULL);

  // first check should pass (bucket starts full)
  folden_status_t st = folden_flow_check(fc, 1, 0);
  ASSERT(st == FOLDEN_OK);
  folden_flow_consume(fc, 1);

  // consume all tokens
  for (int i = 0; i < 9; i++) {
    folden_flow_check(fc, 1, 0);
    folden_flow_consume(fc, 1);
  }

  // now should be throttled
  st = folden_flow_check(fc, 1, 0);
  ASSERT(st == FOLDEN_ERR_THROTTLED);

  // tick should refill
  folden_flow_tick(fc);
  st = folden_flow_check(fc, 1, 0);
  ASSERT(st == FOLDEN_OK);

  folden_flow_destroy(fc);
}

TEST(test_backpressure) {
  folden_flow_ctrl_t *fc = folden_flow_create(100.0, 100.0, 5);
  ASSERT(fc != NULL);

  // under limit: ok
  folden_status_t st = folden_flow_check(fc, 1, 3);
  ASSERT(st == FOLDEN_OK);

  // at limit: throttled
  st = folden_flow_check(fc, 1, 5);
  ASSERT(st == FOLDEN_ERR_THROTTLED);

  folden_flow_destroy(fc);
}

TEST(test_per_lane_override) {
  folden_flow_ctrl_t *fc = folden_flow_create(10.0, 20.0, 0);
  ASSERT(fc != NULL);

  // set lane 42 to very low rate
  int r = folden_flow_set_lane(fc, 42, 1.0, 2.0);
  ASSERT_EQ(r, 0);

  // consume lane 42's 2 tokens
  folden_flow_check(fc, 42, 0);
  folden_flow_consume(fc, 42);
  folden_flow_check(fc, 42, 0);
  folden_flow_consume(fc, 42);

  // third should throttle
  folden_status_t st = folden_flow_check(fc, 42, 0);
  ASSERT(st == FOLDEN_ERR_THROTTLED);

  // but a different lane should be fine
  st = folden_flow_check(fc, 99, 0);
  ASSERT(st == FOLDEN_OK);

  folden_flow_destroy(fc);
}

void test_flow_control(void) {
  RUN(test_basic_flow);
  RUN(test_backpressure);
  RUN(test_per_lane_override);
}

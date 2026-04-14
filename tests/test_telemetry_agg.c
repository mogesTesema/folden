#include "minctest.h"
#include "../src/telemetry_agg.h"

TEST(test_telemetry_windowing) {
  folden_telemetry_agg_t *agg = folden_telemetry_agg_create(1000, 10);
  ASSERT(agg != NULL);
  
  // Record some telemetry for lane 1
  folden_telemetry_agg_record(agg, 1, 100, 100);
  folden_telemetry_agg_record(agg, 1, 200, 150);
  folden_telemetry_agg_record_drop(agg, 1, 160);
  
  // Tick before window expiration
  folden_telemetry_window_t *wins = NULL;
  size_t count = 0;
  folden_telemetry_agg_tick(agg, 500, &wins, &count);
  ASSERT_EQ((int)count, 0);
  
  // Tick after window expiration (100 + 1000 = 1100)
  folden_telemetry_agg_tick(agg, 1200, &wins, &count);
  ASSERT_EQ((int)count, 1);
  ASSERT(wins != NULL);
  
  ASSERT_EQ((int)wins[0].lane_id, 1);
  ASSERT_EQ((int)wins[0].capsule_count, 2);
  ASSERT_EQ((int)wins[0].payload_bytes_total, 300);
  ASSERT_EQ((int)wins[0].min_payload_size, 100);
  ASSERT_EQ((int)wins[0].max_payload_size, 200);
  ASSERT_EQ((int)wins[0].avg_payload_size, 150);
  
  // 1 drop out of 3 total attempts
  ASSERT(wins[0].drop_rate > 0.33f && wins[0].drop_rate < 0.34f);
  
  free(wins);
  folden_telemetry_agg_destroy(agg);
}

void test_telemetry_agg(void) {
  RUN(test_telemetry_windowing);
}

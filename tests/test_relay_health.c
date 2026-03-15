#include "minctest.h"
#include "../src/relay_health.h"
#include <string.h>

TEST(test_histogram) {
  folden_relay_health_t *h = folden_health_create(50);
  ASSERT(h != NULL);

  // record some adopt latencies
  for (int i = 0; i < 100; i++) {
    folden_health_record_adopt(h, 1, (uint64_t)(i * 10));
  }
  ASSERT_EQ((int)h->adopt_latency.count, 100);

  folden_percentiles_t p = folden_health_percentiles(&h->adopt_latency);
  ASSERT(p.mean > 0);
  ASSERT(p.p50 > 0);
  ASSERT(p.p90 >= p.p50);
  ASSERT(p.p99 >= p.p90);

  folden_health_destroy(h);
}

TEST(test_stuck_lanes) {
  folden_relay_health_t *h = folden_health_create(10);
  ASSERT(h != NULL);

  // lane 5: adopted but never emitted
  folden_health_record_adopt(h, 5, 100);

  // lane 7: adopted and emitted recently
  folden_health_record_adopt(h, 7, 50);
  folden_health_record_emit(h, 7, 30);

  uint32_t stuck[4];
  size_t n = folden_health_stuck_lanes(h, stuck, 4);
  ASSERT_EQ((int)n, 1);
  ASSERT_EQ((int)stuck[0], 5);

  folden_health_destroy(h);
}

TEST(test_dump) {
  folden_relay_health_t *h = folden_health_create(50);
  ASSERT(h != NULL);

  folden_health_record_adopt(h, 1, 100);
  folden_health_record_emit(h, 1, 50);
  folden_health_record_queue_overflow(h);

  char buf[1024];
  size_t n = folden_health_dump(h, buf, sizeof(buf));
  ASSERT(n > 0);
  ASSERT(n < sizeof(buf));
  // check it contains expected keys
  ASSERT(strstr(buf, "adopt_count=") != NULL);
  ASSERT(strstr(buf, "queue_overflow=") != NULL);

  folden_health_destroy(h);
}

void test_relay_health(void) {
  RUN(test_histogram);
  RUN(test_stuck_lanes);
  RUN(test_dump);
}

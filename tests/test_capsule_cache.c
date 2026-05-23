#include "minctest.h"

#include "capsule_cache.h"

#include <string.h>

TEST(test_cache_pin_and_lookup) {
  folden_capsule_cache_t *cache = folden_cache_create();
  const uint8_t slab[3] = {7, 8, 9};
  folden_cache_pin_emit_slab(cache, 1, 10, slab, 3);

  size_t out_len = 0;
  const uint8_t *found = folden_cache_lookup_emit_slab(cache, 1, 10, &out_len);
  ASSERT(found != NULL);
  ASSERT_EQ(3u, out_len);
  ASSERT_EQ(0, memcmp(found, slab, 3));

  folden_cache_destroy(cache);
}

TEST(test_cache_lookup_misses_wrong_generation) {
  folden_capsule_cache_t *cache = folden_cache_create();
  const uint8_t slab[1] = {1};
  folden_cache_pin_emit_slab(cache, 1, 5, slab, 1);

  size_t out_len = 0;
  ASSERT(folden_cache_lookup_emit_slab(cache, 1, 6, &out_len) == NULL);

  folden_cache_destroy(cache);
}

TEST(test_cache_repin_same_key_updates_in_place) {
  folden_capsule_cache_t *cache = folden_cache_create();
  const uint8_t slab_a[1] = {1};
  const uint8_t slab_b[1] = {2};
  folden_cache_pin_emit_slab(cache, 1, 1, slab_a, 1);
  folden_cache_pin_emit_slab(cache, 1, 1, slab_b, 1);

  size_t out_len = 0;
  const uint8_t *found = folden_cache_lookup_emit_slab(cache, 1, 1, &out_len);
  ASSERT(found != NULL);
  ASSERT_EQ(2, found[0]);

  folden_cache_destroy(cache);
}

TEST(test_cache_invalidate_lane_clears_lookup) {
  folden_capsule_cache_t *cache = folden_cache_create();
  const uint8_t slab[1] = {1};
  folden_cache_pin_emit_slab(cache, 3, 1, slab, 1);
  folden_cache_invalidate_lane(cache, 3);

  size_t out_len = 0;
  ASSERT(folden_cache_lookup_emit_slab(cache, 3, 1, &out_len) == NULL);

  folden_cache_destroy(cache);
}

void test_capsule_cache_register(void) {
  RUN(test_cache_pin_and_lookup);
  RUN(test_cache_lookup_misses_wrong_generation);
  RUN(test_cache_repin_same_key_updates_in_place);
  RUN(test_cache_invalidate_lane_clears_lookup);
}

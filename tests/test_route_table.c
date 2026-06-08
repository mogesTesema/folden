#include "minctest.h"

#include "route_table.h"

TEST(test_route_table_insert_lookup) {
  folden_route_table_t *t = folden_route_table_create(4);
  ASSERT(t != NULL);
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 10, 100, 5));
  uint32_t hop = 0;
  ASSERT_EQ(FOLDEN_OK, folden_route_table_lookup(t, 10, &hop));
  ASSERT_EQ(100u, hop);
  ASSERT_EQ(FOLDEN_ERR_BAD_STATE, folden_route_table_lookup(t, 999, &hop));
  folden_route_table_destroy(t);
}

TEST(test_route_table_update_existing) {
  folden_route_table_t *t = folden_route_table_create(4);
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 1, 10, 1));
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 1, 20, 2));
  ASSERT_EQ(1u, folden_route_table_count(t));
  uint32_t hop = 0;
  ASSERT_EQ(FOLDEN_OK, folden_route_table_lookup(t, 1, &hop));
  ASSERT_EQ(20u, hop);
  folden_route_table_destroy(t);
}

TEST(test_route_table_lru_eviction) {
  folden_route_table_t *t = folden_route_table_create(2);
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 1, 10, 1));
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 2, 20, 1));
  uint32_t hop = 0;
  /* Touch lane 1 so lane 2 becomes the least-recently-used entry. */
  ASSERT_EQ(FOLDEN_OK, folden_route_table_lookup(t, 1, &hop));
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 3, 30, 1));
  ASSERT_EQ(2u, folden_route_table_count(t));
  ASSERT_EQ(FOLDEN_OK, folden_route_table_lookup(t, 1, &hop));
  ASSERT_EQ(FOLDEN_OK, folden_route_table_lookup(t, 3, &hop));
  ASSERT_EQ(FOLDEN_ERR_BAD_STATE, folden_route_table_lookup(t, 2, &hop));
  folden_route_table_destroy(t);
}

TEST(test_route_table_backoff) {
  folden_route_table_t *t = folden_route_table_create(4);
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 1, 10, 1));
  uint32_t b1 = folden_route_table_note_failure(t, 1);
  uint32_t b2 = folden_route_table_note_failure(t, 1);
  ASSERT(b2 > b1);
  folden_route_table_note_success(t, 1);
  uint32_t b3 = folden_route_table_note_failure(t, 1);
  ASSERT_EQ(b1, b3);
  folden_route_table_destroy(t);
}

TEST(test_route_table_resolve_multi_hop_chain) {
  folden_route_table_t *t = folden_route_table_create(8);
  /* 1 -> 2 -> 3 -> 4 (4 has no further route) */
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 1, 2, 1));
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 2, 3, 1));
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 3, 4, 1));

  uint32_t hops[8];
  size_t hop_count = 0;
  ASSERT_EQ(FOLDEN_OK, folden_route_table_resolve_path(t, 1, hops, 8, &hop_count));
  ASSERT_EQ(3u, hop_count);
  ASSERT_EQ(2u, hops[0]);
  ASSERT_EQ(3u, hops[1]);
  ASSERT_EQ(4u, hops[2]);

  folden_route_table_destroy(t);
}

TEST(test_route_table_resolve_detects_loop) {
  folden_route_table_t *t = folden_route_table_create(8);
  /* 1 -> 2 -> 1 (a genuine routing loop) */
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 1, 2, 1));
  ASSERT_EQ(FOLDEN_OK, folden_route_table_insert(t, 2, 1, 1));

  uint32_t hops[8];
  size_t hop_count = 0;
  ASSERT_EQ(FOLDEN_ERR_BAD_STATE,
            folden_route_table_resolve_path(t, 1, hops, 8, &hop_count));

  folden_route_table_destroy(t);
}

TEST(test_route_table_resolve_no_route_fails) {
  folden_route_table_t *t = folden_route_table_create(8);
  uint32_t hops[8];
  size_t hop_count = 0;
  ASSERT_EQ(FOLDEN_ERR_BAD_STATE,
            folden_route_table_resolve_path(t, 99, hops, 8, &hop_count));
  folden_route_table_destroy(t);
}

void test_route_table_register(void) {
  RUN(test_route_table_insert_lookup);
  RUN(test_route_table_update_existing);
  RUN(test_route_table_lru_eviction);
  RUN(test_route_table_backoff);
  RUN(test_route_table_resolve_multi_hop_chain);
  RUN(test_route_table_resolve_detects_loop);
  RUN(test_route_table_resolve_no_route_fails);
}

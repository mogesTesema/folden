#include "minctest.h"

#include "defer_free.h"

#include <stdlib.h>

TEST(test_defer_enqueue_ptr_freed_on_drain) {
  folden_defer_free_t *df = folden_defer_create();
  void *p = malloc(16);
  folden_defer_enqueue_ptr(df, p);
  /* No crash / leak: draining should free it exactly once. */
  folden_defer_drain(df, UINT32_MAX);
  folden_defer_destroy(df);
}

TEST(test_defer_null_ptr_is_ignored) {
  folden_defer_free_t *df = folden_defer_create();
  folden_defer_enqueue_ptr(df, NULL);
  folden_defer_drain(df, UINT32_MAX);
  folden_defer_destroy(df);
}

TEST(test_defer_zero_token_is_ignored) {
  folden_defer_free_t *df = folden_defer_create();
  folden_defer_enqueue_arena_token(df, 0);
  folden_defer_drain(df, UINT32_MAX);
  folden_defer_destroy(df);
}

TEST(test_defer_destroy_drains_pending_items) {
  folden_defer_free_t *df = folden_defer_create();
  void *p1 = malloc(8);
  void *p2 = malloc(8);
  folden_defer_enqueue_ptr(df, p1);
  folden_defer_enqueue_ptr(df, p2);
  /* destroy (not drain) must still release everything queued. */
  folden_defer_destroy(df);
}

void test_defer_free_register(void) {
  RUN(test_defer_enqueue_ptr_freed_on_drain);
  RUN(test_defer_null_ptr_is_ignored);
  RUN(test_defer_zero_token_is_ignored);
  RUN(test_defer_destroy_drains_pending_items);
}

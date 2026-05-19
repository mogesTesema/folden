#include "minctest.h"

#include "slab_arena.h"

TEST(test_arena_alloc_ptr) {
  folden_slab_arena_t *arena = folden_arena_get_global();
  folden_arena_reset(arena);
  const uint8_t blob[] = {1, 2, 3, 4};
  uint32_t tok = folden_arena_alloc(arena, blob, sizeof(blob));
  ASSERT(tok != 0);
  uint8_t *p = folden_arena_ptr(arena, tok);
  ASSERT(p != NULL);
  ASSERT_EQ(4, (int)folden_arena_len(arena, tok));
  ASSERT_EQ(1, (int)p[0]);
  folden_arena_free_token(arena, tok);
}

void test_arena_register(void) {
  RUN(test_arena_alloc_ptr);
}

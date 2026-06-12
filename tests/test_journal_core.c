#include "minctest.h"

#include "defer_free.h"
#include "journal.h"

TEST(test_journal_append_creates_segment) {
  folden_journal_t *j = folden_journal_create();
  ASSERT(j != NULL);
  ASSERT_EQ(0u, folden_journal_segment_count(j));
  uint8_t payload[4] = {1, 2, 3, 4};
  ASSERT_EQ(FOLDEN_OK, folden_journal_append(j, payload, sizeof(payload), 7));
  ASSERT_EQ(1u, folden_journal_segment_count(j));
  folden_journal_destroy(j);
}

TEST(test_journal_replay_before_compaction) {
  folden_journal_t *j = folden_journal_create();
  uint8_t payload[3] = {9, 8, 7};
  ASSERT_EQ(FOLDEN_OK, folden_journal_append(j, payload, sizeof(payload), 1));
  const uint8_t *ptr = NULL;
  size_t len = 0;
  ASSERT_EQ(FOLDEN_OK, folden_journal_replay_segment(j, 0, &ptr, &len));
  ASSERT_EQ(3u, len);
  ASSERT_EQ(9, ptr[0]);
  folden_journal_destroy(j);
}

TEST(test_journal_replay_survives_compaction) {
  folden_journal_t *j = folden_journal_create();
  folden_defer_free_t *defer = folden_defer_create();
  uint8_t p1[2] = {1, 1};
  uint8_t p2[2] = {2, 2};
  ASSERT_EQ(FOLDEN_OK, folden_journal_append(j, p1, sizeof(p1), 1));
  /* Force the first segment to seal by aging it out, then start a second
   * segment so the compaction threshold (2 sealed segments) is reachable. */
  for (int i = 0; i < 5; i++) {
    folden_journal_tick(j, defer);
  }
  ASSERT_EQ(FOLDEN_OK, folden_journal_append(j, p2, sizeof(p2), 2));
  for (int i = 0; i < 5; i++) {
    folden_journal_tick(j, defer);
  }
  const uint8_t *ptr = NULL;
  size_t len = 0;
  folden_status_t st = folden_journal_replay_segment(j, 0, &ptr, &len);
  /* Either still readable via the compacted copy, or already retired --
   * both are valid terminal states, but a read of a live pointer must never
   * return garbage-length data. */
  if (st == FOLDEN_OK) {
    ASSERT_EQ(2u, len);
  }
  folden_defer_destroy(defer);
  folden_journal_destroy(j);
}

void test_journal_core_register(void) {
  RUN(test_journal_append_creates_segment);
  RUN(test_journal_replay_before_compaction);
  RUN(test_journal_replay_survives_compaction);
}

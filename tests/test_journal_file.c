#include "minctest.h"
#include "../src/journal_file.h"

#include <stdlib.h>
#include <string.h>

TEST(test_journal_file_append_seal) {
  folden_journal_store_t *s = folden_journal_store_create("/tmp", "testfseg", 1024);
  ASSERT(s != NULL);

  uint8_t blob1[] = { 1, 2, 3, 4 };
  uint8_t blob2[] = { 5, 6, 7, 8, 9 };

  folden_status_t st = folden_journal_store_append(s, blob1, sizeof(blob1));
  ASSERT(st == FOLDEN_OK);
  st = folden_journal_store_append(s, blob2, sizeof(blob2));
  ASSERT(st == FOLDEN_OK);

  size_t bytes = folden_journal_store_total_bytes(s);
  ASSERT(bytes > sizeof(blob1) + sizeof(blob2));

  st = folden_journal_store_seal(s);
  ASSERT(st == FOLDEN_OK);

  size_t count = folden_journal_store_segment_count(s);
  ASSERT_EQ((int)count, 1);

  folden_journal_store_destroy(s);
}

TEST(test_journal_file_rotate) {
  // very small segment size to force rotation
  folden_journal_store_t *s = folden_journal_store_create("/tmp", "testrot", 40);
  ASSERT(s != NULL);

  uint8_t blob[20];
  memset(blob, 0xBB, sizeof(blob));

  // first fits
  folden_status_t st = folden_journal_store_append(s, blob, sizeof(blob));
  ASSERT(st == FOLDEN_OK);
  ASSERT_EQ((int)folden_journal_store_segment_count(s), 1);

  // second should trigger rotation
  st = folden_journal_store_append(s, blob, sizeof(blob));
  ASSERT(st == FOLDEN_OK);
  ASSERT_EQ((int)folden_journal_store_segment_count(s), 2);

  folden_journal_store_destroy(s);
}

void test_journal_file(void) {
  RUN(test_journal_file_append_seal);
  RUN(test_journal_file_rotate);
}

#include "minctest.h"

#include "folden/folden.h"
#include "varfold.h"

#include <string.h>

/* tag_count/lane_count/slab_len are attacker-controlled 64-bit wire fields.
 * Before the fix, they were passed straight to calloc/malloc without being
 * bounded against the bytes actually remaining in the buffer, so a small
 * input claiming an astronomical count aborted the allocator instead of
 * failing decode cleanly. */
TEST(test_huge_tag_count_is_rejected_not_allocated) {
  uint8_t buf[32];
  size_t off = 0;
  buf[off++] = 0x46;
  buf[off++] = 0x4F;
  buf[off++] = 0x4C;
  buf[off++] = 0x44;
  buf[off++] = 1;
  off += folden_varfold_encode(1, buf + off, sizeof(buf) - off);   /* lane_count */
  off += folden_varfold_encode(1, buf + off, sizeof(buf) - off);   /* lane_id */
  /* tag_count: an enormous value with only a couple of trailing bytes. */
  off += folden_varfold_encode((uint64_t)1 << 60, buf + off, sizeof(buf) - off);

  folden_capsule_t *cap = NULL;
  folden_status_t st = folden_decode_buffer(buf, off, &cap);
  ASSERT(st != FOLDEN_OK);
  ASSERT(cap == NULL);
}

TEST(test_huge_lane_count_is_rejected_not_allocated) {
  uint8_t buf[16];
  size_t off = 0;
  buf[off++] = 0x46;
  buf[off++] = 0x4F;
  buf[off++] = 0x4C;
  buf[off++] = 0x44;
  buf[off++] = 1;
  off += folden_varfold_encode((uint64_t)1 << 60, buf + off, sizeof(buf) - off);

  folden_capsule_t *cap = NULL;
  folden_status_t st = folden_decode_buffer(buf, off, &cap);
  ASSERT(st != FOLDEN_OK);
  ASSERT(cap == NULL);
}

/* A FOLD_EXTEND span accumulates into fold_budget with no bound of its own;
 * tag_index.c used to size a tag_meta allocation directly from it, so one
 * small tag claiming an enormous span aborted the allocator. */
TEST(test_huge_fold_extend_span_is_rejected_not_allocated) {
  uint8_t buf[32];
  size_t off = 0;
  buf[off++] = 0x46;
  buf[off++] = 0x4F;
  buf[off++] = 0x4C;
  buf[off++] = 0x44;
  buf[off++] = 1;
  off += folden_varfold_encode(1, buf + off, sizeof(buf) - off);  /* lane_count */
  off += folden_varfold_encode(1, buf + off, sizeof(buf) - off);  /* lane_id */
  off += folden_varfold_encode(1, buf + off, sizeof(buf) - off);  /* tag_count */
  off += folden_varfold_encode(1, buf + off, sizeof(buf) - off);  /* tag_id = FOLD_EXTEND */
  off += folden_varfold_encode(0, buf + off, sizeof(buf) - off);  /* fold_depth */
  off += folden_varfold_encode((uint64_t)1 << 40, buf + off, sizeof(buf) - off); /* span */
  off += folden_varfold_encode(0, buf + off, sizeof(buf) - off); /* slab_len */

  folden_capsule_t *cap = NULL;
  folden_status_t st = folden_decode_buffer(buf, off, &cap);
  ASSERT(st != FOLDEN_OK);
  ASSERT(cap == NULL);
}

void test_wire_bounds_register(void) {
  RUN(test_huge_tag_count_is_rejected_not_allocated);
  RUN(test_huge_lane_count_is_rejected_not_allocated);
  RUN(test_huge_fold_extend_span_is_rejected_not_allocated);
}

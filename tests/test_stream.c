#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"

#include <stdlib.h>
#include <string.h>

TEST(test_stream_chunked_decode) {
  folden_wire_tag_t tags[] = {{3, 0, 1}};
  folden_wire_lane_t lane = {9, tags, 1, (const uint8_t *)"z", 1, 0, 0};
  uint8_t *wire = NULL;
  size_t wlen = 0;
  folden_wire_build_capsule(&lane, 1, 0, 0, &wire, &wlen);
  ASSERT(wlen > 4);

  folden_stream_t *s = folden_stream_create();
  size_t mid = wlen / 2;
  if (mid == 0) {
    mid = 1;
  }
  folden_capsule_t *cap = NULL;
  folden_status_t st = folden_decode_stream(s, wire, mid, &cap);
  ASSERT_EQ(st, FOLDEN_ERR_TRUNCATED);
  ASSERT(cap == NULL);
  st = folden_decode_stream(s, wire + mid, wlen - mid, &cap);
  ASSERT_EQ(st, FOLDEN_OK);
  ASSERT(cap != NULL);
  ASSERT_EQ(1, (int)folden_capsule_lane_count(cap));
  folden_capsule_free(cap);
  folden_stream_destroy(s);
  free(wire);
}

void test_stream_register(void) {
  RUN(test_stream_chunked_decode);
}

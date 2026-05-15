#include "minctest.h"

#include <string.h>

#include "capsule_wire.h"
#include "folden/folden.h"

TEST(test_lzfold_unpack_decode) {
  uint8_t payload[4] = {0x11, 0x22, 0x33, 0x44};
  uint8_t slab[8];
  slab[0] = 0x44;
  slab[1] = 0x46;
  slab[2] = 0x5A;
  slab[3] = 0x4C;
  memcpy(slab + 4, payload, sizeof(payload));

  folden_wire_tag_t tags[] = {{3, 0, 1}};
  folden_wire_lane_t la = {14, tags, 1, slab, sizeof(slab), 0, 0};
  uint8_t *wire = NULL;
  size_t wire_len = 0;
  ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire, &wire_len));

  folden_capsule_t *cap = NULL;
  ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
  ASSERT_EQ(1u, folden_capsule_lane_count(cap));
  folden_capsule_free(cap);
  free(wire);
}

void test_compress_register(void) {
  RUN(test_lzfold_unpack_decode);
}

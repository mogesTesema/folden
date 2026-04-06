#include "minctest.h"
#include "../src/capsule_mux.h"

#include <stdlib.h>
#include <string.h>

TEST(test_pack_unpack) {
  uint8_t p1[] = { 'h', 'e', 'l', 'l', 'o' };
  uint8_t p2[] = { 'w', 'o', 'r', 'l', 'd' };
  const uint8_t *payloads[] = { p1, p2 };
  size_t lengths[] = { sizeof(p1), sizeof(p2) };

  folden_mux_frame_t frame;
  folden_status_t st = folden_mux_pack(payloads, lengths, 2, &frame);
  ASSERT(st == FOLDEN_OK);
  ASSERT(frame.buf != NULL);
  ASSERT(frame.len >= FOLDEN_MUX_HEADER_MIN + sizeof(p1) + sizeof(p2));

  // unpack via stream demux to test both
  folden_mux_stream_t *ms = folden_mux_stream_create();
  ASSERT(ms != NULL);

  size_t consumed = folden_mux_stream_feed(ms, frame.buf, frame.len, NULL, NULL);
  ASSERT_EQ((int)consumed, (int)frame.len);
  ASSERT_EQ(ms->state, FOLDEN_MUX_DONE);
  ASSERT_EQ((int)ms->capsule_count, 2);

  folden_mux_stream_destroy(ms);
  folden_mux_frame_free(&frame);
}

TEST(test_partial_feed) {
  uint8_t p1[] = { 1, 2, 3 };
  const uint8_t *payloads[] = { p1 };
  size_t lengths[] = { sizeof(p1) };

  folden_mux_frame_t frame;
  folden_status_t st = folden_mux_pack(payloads, lengths, 1, &frame);
  ASSERT(st == FOLDEN_OK);

  folden_mux_stream_t *ms = folden_mux_stream_create();

  // feed 1 byte at a time
  for (size_t i = 0; i < frame.len; i++) {
    size_t c = folden_mux_stream_feed(ms, frame.buf + i, 1, NULL, NULL);
    ASSERT_EQ((int)c, 1);
  }

  ASSERT_EQ(ms->state, FOLDEN_MUX_DONE);

  folden_mux_stream_destroy(ms);
  folden_mux_frame_free(&frame);
}

void test_capsule_mux(void) {
  RUN(test_pack_unpack);
  RUN(test_partial_feed);
}

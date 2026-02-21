#include "minctest.h"
#include "../src/wire_compress.h"

#include <stdlib.h>
#include <string.h>

TEST(test_roundtrip_basic) {
  uint8_t data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  folden_compressor_t *ctx = folden_compressor_create(0);
  ASSERT(ctx != NULL);

  folden_status_t st = folden_compress(ctx, data, sizeof(data));
  ASSERT(st == FOLDEN_OK);
  ASSERT(ctx->len > 0);

  uint8_t *out = NULL;
  size_t out_len = 0;
  st = folden_decompress(ctx->buf, ctx->len, &out, &out_len);
  ASSERT(st == FOLDEN_OK);
  ASSERT_EQ((int)out_len, (int)sizeof(data));
  ASSERT(memcmp(data, out, sizeof(data)) == 0);

  free(out);
  folden_compressor_destroy(ctx);
}

TEST(test_roundtrip_repeated) {
  uint8_t data[200];
  memset(data, 0xAA, sizeof(data));

  folden_compressor_t *ctx = folden_compressor_create(16);
  folden_status_t st = folden_compress(ctx, data, sizeof(data));
  ASSERT(st == FOLDEN_OK);
  ASSERT(ctx->len < sizeof(data));

  uint8_t *out = NULL;
  size_t out_len = 0;
  st = folden_decompress(ctx->buf, ctx->len, &out, &out_len);
  ASSERT(st == FOLDEN_OK);
  ASSERT_EQ((int)out_len, 200);
  ASSERT(memcmp(data, out, sizeof(data)) == 0);

  free(out);
  folden_compressor_destroy(ctx);
}

TEST(test_roundtrip_mixed) {
  uint8_t data[128];
  for (int i = 0; i < 32; i++) data[i] = 0xFF;
  for (int i = 32; i < 64; i++) data[i] = (uint8_t)i;
  for (int i = 64; i < 96; i++) data[i] = 0x42;
  for (int i = 96; i < 128; i++) data[i] = (uint8_t)(128 - i);

  folden_compressor_t *ctx = folden_compressor_create(8);
  folden_status_t st = folden_compress(ctx, data, sizeof(data));
  ASSERT(st == FOLDEN_OK);

  uint8_t *out = NULL;
  size_t out_len = 0;
  st = folden_decompress(ctx->buf, ctx->len, &out, &out_len);
  ASSERT(st == FOLDEN_OK);
  ASSERT_EQ((int)out_len, 128);
  ASSERT(memcmp(data, out, sizeof(data)) == 0);

  free(out);
  folden_compressor_destroy(ctx);
}

TEST(test_validate) {
  uint8_t good[] = { 0x02, 0x41, 0x42, 0x43 };
  ASSERT(folden_compress_validate(good, sizeof(good)) == 0);

  uint8_t bad[] = { 0x0A };
  ASSERT(folden_compress_validate(bad, sizeof(bad)) == -1);
}

TEST(test_delta_coding) {
  uint32_t arr[] = { 10, 20, 35, 100, 200 };
  uint32_t orig[] = { 10, 20, 35, 100, 200 };
  size_t n = sizeof(arr) / sizeof(arr[0]);

  folden_delta_encode_u32(arr, n);
  ASSERT_EQ((int)arr[0], 10);
  ASSERT_EQ((int)arr[1], 10);

  folden_delta_decode_u32(arr, n);
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ((int)arr[i], (int)orig[i]);
  }
}

void test_wire_compress(void) {
  RUN(test_roundtrip_basic);
  RUN(test_roundtrip_repeated);
  RUN(test_roundtrip_mixed);
  RUN(test_validate);
  RUN(test_delta_coding);
}

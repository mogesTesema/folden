#include "wire_compress.h"

#include <stdlib.h>
#include <string.h>

#define RLE_LITERAL_MAX 128
#define RLE_REPEAT_MIN  3
#define RLE_REPEAT_MAX  130

folden_compressor_t *folden_compressor_create(size_t threshold) {
  folden_compressor_t *ctx = calloc(1, sizeof(*ctx));
  if (!ctx) return NULL;
  ctx->compress_threshold = threshold > 0 ? threshold : 64;
  return ctx;
}

void folden_compressor_destroy(folden_compressor_t *ctx) {
  if (!ctx) return;
  free(ctx->buf);
  free(ctx);
}

void folden_compressor_reset(folden_compressor_t *ctx) {
  if (!ctx) return;
  ctx->len = 0;
  // keep the buffer allocated for reuse
}

static int ensure_cap(folden_compressor_t *ctx, size_t need) {
  if (ctx->len + need <= ctx->cap) return 0;
  size_t nc = ctx->cap ? ctx->cap * 2 : 256;
  while (nc < ctx->len + need) nc *= 2;
  uint8_t *nb = realloc(ctx->buf, nc);
  if (!nb) return -1;
  ctx->buf = nb;
  ctx->cap = nc;
  return 0;
}

static void emit_literal(folden_compressor_t *ctx,
                          const uint8_t *src, size_t n) {
  if (n == 0) return;
  // split into chunks of RLE_LITERAL_MAX
  while (n > 0) {
    size_t chunk = n > RLE_LITERAL_MAX ? RLE_LITERAL_MAX : n;
    if (ensure_cap(ctx, 1 + chunk) != 0) return;
    ctx->buf[ctx->len++] = (uint8_t)(chunk - 1); // bit 7 clear
    memcpy(ctx->buf + ctx->len, src, chunk);
    ctx->len += chunk;
    src += chunk;
    n -= chunk;
  }
}

static void emit_repeat(folden_compressor_t *ctx, uint8_t val, size_t count) {
  while (count >= RLE_REPEAT_MIN) {
    size_t run = count > RLE_REPEAT_MAX ? RLE_REPEAT_MAX : count;
    if (ensure_cap(ctx, 2) != 0) return;
    ctx->buf[ctx->len++] = (uint8_t)(0x80u | (run - RLE_REPEAT_MIN));
    ctx->buf[ctx->len++] = val;
    count -= run;
  }
  // leftover < 3 identical bytes go as a literal
  if (count > 0) {
    uint8_t tmp[2] = { val, val };
    emit_literal(ctx, tmp, count);
  }
}

folden_status_t folden_compress(folden_compressor_t *ctx,
                                const uint8_t *src, size_t src_len) {
  if (!ctx) return FOLDEN_ERR_BAD_STATE;
  folden_compressor_reset(ctx);

  if (src_len == 0 || !src) {
    ctx->bytes_in += 0;
    return FOLDEN_OK;
  }

  // below threshold: store as-is (caller decides whether to tag it)
  if (src_len < ctx->compress_threshold) {
    if (ensure_cap(ctx, src_len) != 0) return FOLDEN_ERR_NOMEM;
    // single literal run covering the whole input
    emit_literal(ctx, src, src_len);
    ctx->bytes_in += src_len;
    ctx->bytes_out += ctx->len;
    return FOLDEN_OK;
  }

  size_t i = 0;
  while (i < src_len) {
    // look ahead for a repeat run
    size_t run = 1;
    while (i + run < src_len && src[i + run] == src[i] &&
           run < RLE_REPEAT_MAX) {
      run++;
    }

    if (run >= RLE_REPEAT_MIN) {
      emit_repeat(ctx, src[i], run);
      i += run;
    } else {
      // scan for a literal run: stop when we see 3+ identical bytes
      size_t lit_start = i;
      while (i < src_len) {
        // peek for a repeat
        size_t ahead = 1;
        while (i + ahead < src_len && src[i + ahead] == src[i] &&
               ahead < RLE_REPEAT_MIN) {
          ahead++;
        }
        if (ahead >= RLE_REPEAT_MIN) break;
        i++;
      }
      emit_literal(ctx, src + lit_start, i - lit_start);
    }
  }

  ctx->bytes_in += src_len;
  ctx->bytes_out += ctx->len;
  return FOLDEN_OK;
}

folden_status_t folden_decompress(const uint8_t *src, size_t src_len,
                                  uint8_t **out, size_t *out_len) {
  if (!out || !out_len) return FOLDEN_ERR_BAD_STATE;
  *out = NULL;
  *out_len = 0;

  if (src_len == 0 || !src) return FOLDEN_OK;

  // first pass: compute output size
  size_t total = 0;
  size_t pos = 0;
  while (pos < src_len) {
    uint8_t ctrl = src[pos++];
    if (ctrl & 0x80u) {
      // repeat
      size_t count = (ctrl & 0x7Fu) + RLE_REPEAT_MIN;
      if (pos >= src_len) return FOLDEN_ERR_TRUNCATED;
      pos++; // skip the value byte
      total += count;
    } else {
      // literal
      size_t count = (size_t)ctrl + 1;
      if (pos + count > src_len) return FOLDEN_ERR_TRUNCATED;
      pos += count;
      total += count;
    }
  }

  if (total == 0) return FOLDEN_OK;

  uint8_t *buf = malloc(total);
  if (!buf) return FOLDEN_ERR_NOMEM;

  // second pass: decode
  pos = 0;
  size_t w = 0;
  while (pos < src_len) {
    uint8_t ctrl = src[pos++];
    if (ctrl & 0x80u) {
      size_t count = (ctrl & 0x7Fu) + RLE_REPEAT_MIN;
      uint8_t val = src[pos++];
      memset(buf + w, val, count);
      w += count;
    } else {
      size_t count = (size_t)ctrl + 1;
      memcpy(buf + w, src + pos, count);
      pos += count;
      w += count;
    }
  }

  *out = buf;
  *out_len = total;
  return FOLDEN_OK;
}

int folden_compress_validate(const uint8_t *src, size_t src_len) {
  size_t pos = 0;
  while (pos < src_len) {
    uint8_t ctrl = src[pos++];
    if (ctrl & 0x80u) {
      if (pos >= src_len) return -1;
      pos++; // value byte
    } else {
      size_t count = (size_t)ctrl + 1;
      if (pos + count > src_len) return -1;
      pos += count;
    }
  }
  return 0;
}

/* --- delta coding for sorted uint32 arrays --- */

void folden_delta_encode_u32(uint32_t *arr, size_t n) {
  if (!arr || n < 2) return;
  // encode backwards so we can do it in place
  for (size_t i = n - 1; i >= 1; i--) {
    arr[i] -= arr[i - 1];
  }
}

void folden_delta_decode_u32(uint32_t *arr, size_t n) {
  if (!arr || n < 2) return;
  for (size_t i = 1; i < n; i++) {
    arr[i] += arr[i - 1];
  }
}

#if 0
/* streaming decompressor -- would let stream_reasm feed chunks
 * incrementally. parked until we measure whether capsules ever
 * actually span enough chunks to justify the statemachine. */
typedef struct {
  uint8_t pending_ctrl;
  size_t  pending_remaining;
  uint8_t pending_val;
  int     in_repeat;
} folden_decompress_stream_t;
#endif

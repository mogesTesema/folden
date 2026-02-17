#ifndef FOLDEN_WIRE_COMPRESS_H
#define FOLDEN_WIRE_COMPRESS_H

#include "folden_internal.h"

/*
 * Byte-level RLE codec for slab payloads.
 *
 * Format (on wire, after the 4-byte LZFOLD_MAGIC tag):
 *   repeat: <ctrl_byte> <payload...>
 *
 *   ctrl_byte bit 7 clear  =>  literal run of (ctrl + 1) bytes follow
 *   ctrl_byte bit 7 set    =>  repeat: next byte repeated (ctrl & 0x7F) + 3 times
 *
 * A run of < 3 identical bytes is cheaper as a literal.
 * Max literal run  = 128 bytes (ctrl 0x00..0x7F)
 * Max repeat run   = 130 bytes (ctrl 0x80..0xFF, value 3..130)
 */

typedef struct folden_compressor {
  uint8_t *buf;
  size_t   len;
  size_t   cap;
  size_t   compress_threshold; // don't bother below this size
  uint64_t bytes_in;
  uint64_t bytes_out;
} folden_compressor_t;

folden_compressor_t *folden_compressor_create(size_t compress_threshold);
void                 folden_compressor_destroy(folden_compressor_t *ctx);
void                 folden_compressor_reset(folden_compressor_t *ctx);

/* Compress `src` into ctx->buf. Returns FOLDEN_OK on success.
 * After return, ctx->buf / ctx->len hold the compressed payload
 * (without the LZFOLD_MAGIC prefix -- caller prepends that). */
folden_status_t folden_compress(folden_compressor_t *ctx,
                                const uint8_t *src, size_t src_len);

/* Decompress `src` (without the LZFOLD_MAGIC prefix) into *out / *out_len.
 * Caller owns the returned buffer. */
folden_status_t folden_decompress(const uint8_t *src, size_t src_len,
                                  uint8_t **out, size_t *out_len);

/* In-place check: is the compressed stream well-formed?
 * Returns 0 if valid, -1 if corrupt. Does not allocate. */
int folden_compress_validate(const uint8_t *src, size_t src_len);

/* Delta-encode a sorted array of uint32_t tag spans in place.
 * Caller must delta_decode before using the array again.
 * n must be >= 1. */
void folden_delta_encode_u32(uint32_t *arr, size_t n);
void folden_delta_decode_u32(uint32_t *arr, size_t n);

#endif

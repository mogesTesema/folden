#ifndef FOLDEN_SHA256_H
#define FOLDEN_SHA256_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FOLDEN_SHA256_BLOCK_SIZE 32

typedef struct {
  uint8_t data[64];
  uint32_t datalen;
  uint64_t bitlen;
  uint32_t state[8];
} folden_sha256_ctx_t;

/* Initialize a SHA-256 context */
void folden_sha256_init(folden_sha256_ctx_t *ctx);

/* Update the context with a chunk of data */
void folden_sha256_update(folden_sha256_ctx_t *ctx, const uint8_t *data, size_t len);

/* Finalize the hash and output the 32-byte digest */
void folden_sha256_final(folden_sha256_ctx_t *ctx, uint8_t *hash);

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_SHA256_H */

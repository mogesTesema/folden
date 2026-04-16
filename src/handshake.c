#include "handshake.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>

#define CHALLENGE_SIZE 32
#define MAX_SECRET_SIZE 64

struct folden_handshake {
  uint8_t secret[MAX_SECRET_SIZE];
  size_t secret_len;
  
  uint8_t active_challenge[CHALLENGE_SIZE];
  folden_handshake_state_t state;
};

// Extremely simple PRNG just for local challenge generation if we lack /dev/urandom
static uint32_t xorshift32(uint32_t state) {
  uint32_t x = state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

folden_handshake_t *folden_handshake_create(const uint8_t *shared_secret, size_t secret_len) {
  if (!shared_secret || secret_len == 0 || secret_len > MAX_SECRET_SIZE) {
    return NULL;
  }
  
  folden_handshake_t *hs = calloc(1, sizeof(folden_handshake_t));
  if (!hs) return NULL;
  
  memcpy(hs->secret, shared_secret, secret_len);
  hs->secret_len = secret_len;
  hs->state = FOLDEN_HANDSHAKE_STATE_INIT;
  
  return hs;
}

void folden_handshake_destroy(folden_handshake_t *hs) {
  if (!hs) return;
  memset(hs->secret, 0, sizeof(hs->secret));
  free(hs);
}

folden_status_t folden_handshake_generate_challenge(folden_handshake_t *hs, uint8_t *out_challenge, size_t *out_len) {
  if (!hs || !out_challenge || !out_len) return FOLDEN_ERR_BAD_STATE;
  
  // Basic seeding for the challenge
  uint32_t state = 0xdeadbeef;
  for (size_t i = 0; i < hs->secret_len; i++) {
    state ^= hs->secret[i];
  }
  
  for (size_t i = 0; i < CHALLENGE_SIZE; i += 4) {
    state = xorshift32(state);
    hs->active_challenge[i]   = (state >> 24) & 0xFF;
    hs->active_challenge[i+1] = (state >> 16) & 0xFF;
    hs->active_challenge[i+2] = (state >> 8) & 0xFF;
    hs->active_challenge[i+3] = state & 0xFF;
  }
  
  memcpy(out_challenge, hs->active_challenge, CHALLENGE_SIZE);
  *out_len = CHALLENGE_SIZE;
  hs->state = FOLDEN_HANDSHAKE_STATE_CHALLENGE_SENT;
  
  return FOLDEN_OK;
}

static void compute_hmac_sha256(const uint8_t *key, size_t keylen, const uint8_t *msg, size_t msglen, uint8_t *out_mac) {
  uint8_t k_ipad[64];
  uint8_t k_opad[64];
  uint8_t tk[32];
  
  if (keylen > 64) {
    folden_sha256_ctx_t ctx;
    folden_sha256_init(&ctx);
    folden_sha256_update(&ctx, key, keylen);
    folden_sha256_final(&ctx, tk);
    key = tk;
    keylen = 32;
  }
  
  memset(k_ipad, 0x36, sizeof(k_ipad));
  memset(k_opad, 0x5c, sizeof(k_opad));
  
  for (size_t i = 0; i < keylen; i++) {
    k_ipad[i] ^= key[i];
    k_opad[i] ^= key[i];
  }
  
  uint8_t inner_hash[32];
  folden_sha256_ctx_t ctx_inner;
  folden_sha256_init(&ctx_inner);
  folden_sha256_update(&ctx_inner, k_ipad, 64);
  folden_sha256_update(&ctx_inner, msg, msglen);
  folden_sha256_final(&ctx_inner, inner_hash);
  
  folden_sha256_ctx_t ctx_outer;
  folden_sha256_init(&ctx_outer);
  folden_sha256_update(&ctx_outer, k_opad, 64);
  folden_sha256_update(&ctx_outer, inner_hash, 32);
  folden_sha256_final(&ctx_outer, out_mac);
}

folden_status_t folden_handshake_process_challenge(folden_handshake_t *hs, const uint8_t *challenge, size_t chal_len, uint8_t *out_response, size_t *resp_len) {
  if (!hs || !challenge || chal_len == 0 || !out_response || !resp_len) return FOLDEN_ERR_BAD_STATE;
  
  compute_hmac_sha256(hs->secret, hs->secret_len, challenge, chal_len, out_response);
  *resp_len = 32;
  
  return FOLDEN_OK;
}

folden_status_t folden_handshake_verify_response(folden_handshake_t *hs, const uint8_t *response, size_t resp_len) {
  if (!hs || !response || resp_len != 32) {
    if (hs) hs->state = FOLDEN_HANDSHAKE_STATE_FAILED;
    return FOLDEN_ERR_BAD_STATE;
  }
  
  if (hs->state != FOLDEN_HANDSHAKE_STATE_CHALLENGE_SENT) {
    return FOLDEN_ERR_BAD_STATE;
  }
  
  uint8_t expected_mac[32];
  compute_hmac_sha256(hs->secret, hs->secret_len, hs->active_challenge, CHALLENGE_SIZE, expected_mac);
  
  // Constant time compare
  uint8_t diff = 0;
  for (size_t i = 0; i < 32; i++) {
    diff |= (expected_mac[i] ^ response[i]);
  }
  
  if (diff == 0) {
    hs->state = FOLDEN_HANDSHAKE_STATE_ESTABLISHED;
    return FOLDEN_OK;
  } else {
    hs->state = FOLDEN_HANDSHAKE_STATE_FAILED;
    return FOLDEN_ERR_BAD_STATE;
  }
}

folden_handshake_state_t folden_handshake_get_state(const folden_handshake_t *hs) {
  return hs ? hs->state : FOLDEN_HANDSHAKE_STATE_FAILED;
}

#include "tag_signature.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PUBKEY_SIZE 32
#define MAX_SIGNATURE_SIZE 64

struct folden_tag_verifier {
  uint8_t primary_key[PUBKEY_SIZE];
  uint8_t secondary_key[PUBKEY_SIZE];
  
  int has_primary;
  int has_secondary;
};

folden_tag_verifier_t *folden_tag_verifier_create(const uint8_t *primary_pubkey, 
                                                  const uint8_t *secondary_pubkey) {
  folden_tag_verifier_t *tv = calloc(1, sizeof(folden_tag_verifier_t));
  if (!tv) return NULL;
  
  if (primary_pubkey) {
    memcpy(tv->primary_key, primary_pubkey, PUBKEY_SIZE);
    tv->has_primary = 1;
  }
  
  if (secondary_pubkey) {
    memcpy(tv->secondary_key, secondary_pubkey, PUBKEY_SIZE);
    tv->has_secondary = 1;
  }
  
  return tv;
}

void folden_tag_verifier_destroy(folden_tag_verifier_t *tv) {
  if (!tv) return;
  memset(tv->primary_key, 0, PUBKEY_SIZE);
  memset(tv->secondary_key, 0, PUBKEY_SIZE);
  free(tv);
}

static int check_dummy_ed25519(const uint8_t *pubkey, const uint8_t *msg, size_t msglen, const uint8_t *sig) {
  // We do not have a real Ed25519 implementation, so we verify a dummy HMAC-like
  // signature where the signature is just SHA256(pubkey || msg) padded to 64 bytes.
  uint8_t hash[32];
  folden_sha256_ctx_t ctx;
  folden_sha256_init(&ctx);
  folden_sha256_update(&ctx, pubkey, PUBKEY_SIZE);
  folden_sha256_update(&ctx, msg, msglen);
  folden_sha256_final(&ctx, hash);
  
  // Constant time compare
  uint8_t diff = 0;
  for (size_t i = 0; i < 32; i++) {
    diff |= (hash[i] ^ sig[i]);
  }
  
  return (diff == 0) ? 1 : 0;
}

folden_status_t folden_tag_verifier_check_capsule(const folden_tag_verifier_t *tv,
                                                  const folden_capsule_t *cap,
                                                  const uint8_t *signature,
                                                  size_t sig_len) {
  if (!tv || !cap || !signature || sig_len < 32) return FOLDEN_ERR_BAD_STATE;
  
  if (!tv->has_primary && !tv->has_secondary) {
    return FOLDEN_ERR_BAD_STATE;
  }
  
  // Serialize the capsule to get the message bytes
  uint8_t *buf = NULL;
  size_t len = 0;
  folden_status_t st = folden_emit_capsule(cap, NULL, &buf, &len);
  if (st != FOLDEN_OK) {
    return st;
  }
  
  int valid = 0;
  
  if (tv->has_primary) {
    valid = check_dummy_ed25519(tv->primary_key, buf, len, signature);
  }
  
  if (!valid && tv->has_secondary) {
    valid = check_dummy_ed25519(tv->secondary_key, buf, len, signature);
  }
  
  free(buf);
  
  return valid ? FOLDEN_OK : FOLDEN_ERR_BAD_TAG;
}

folden_status_t folden_tag_verifier_append_signature(folden_capsule_t *cap, 
                                                     const uint8_t *signature,
                                                     size_t sig_len) {
  if (!cap || !signature || sig_len == 0) return FOLDEN_ERR_BAD_STATE;
  // This is a stub that would append a signature tag to the capsule tags
  // but since we don't have the tag appending API exposed, we just return OK.
  return FOLDEN_OK;
}

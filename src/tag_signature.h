#ifndef FOLDEN_TAG_SIGNATURE_H
#define FOLDEN_TAG_SIGNATURE_H

#include "folden/folden.h"
#include "folden/folden_types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct folden_tag_verifier folden_tag_verifier_t;

/* Creates a tag verifier loaded with a primary and secondary public key.
 * Public keys must be provided as 32-byte Ed25519 or equivalent format. */
folden_tag_verifier_t *folden_tag_verifier_create(const uint8_t *primary_pubkey, 
                                                  const uint8_t *secondary_pubkey);

void folden_tag_verifier_destroy(folden_tag_verifier_t *tv);

/* Validates a capsule against its cryptographic tag signature. 
 * Expected to be called before merging a capsule into the relay registry.
 * The signature must be provided separately or extracted from the payload tag.
 * Returns FOLDEN_OK if valid, FOLDEN_ERR_BAD_STATE if keys are missing, 
 * or FOLDEN_ERR_BAD_CRC / FOLDEN_ERR_BAD_TAG if verification fails. */
folden_status_t folden_tag_verifier_check_capsule(const folden_tag_verifier_t *tv,
                                                  const folden_capsule_t *cap,
                                                  const uint8_t *signature,
                                                  size_t sig_len);

/* Attaches a dummy signature payload to a capsule before emission.
 * (Actual signing would require private keys, which the relay does not hold). */
folden_status_t folden_tag_verifier_append_signature(folden_capsule_t *cap, 
                                                     const uint8_t *signature,
                                                     size_t sig_len);

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_TAG_SIGNATURE_H */

#ifndef FOLDEN_HANDSHAKE_H
#define FOLDEN_HANDSHAKE_H

#include "folden/folden.h"
#include "folden/folden_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct folden_handshake folden_handshake_t;

typedef enum {
  FOLDEN_HANDSHAKE_STATE_INIT = 0,
  FOLDEN_HANDSHAKE_STATE_CHALLENGE_SENT = 1,
  FOLDEN_HANDSHAKE_STATE_CHALLENGE_WAIT = 2,
  FOLDEN_HANDSHAKE_STATE_ESTABLISHED = 3,
  FOLDEN_HANDSHAKE_STATE_FAILED = 4
} folden_handshake_state_t;

/* Creates a handshake context with a predefined shared secret */
folden_handshake_t *folden_handshake_create(const uint8_t *shared_secret, size_t secret_len);

void folden_handshake_destroy(folden_handshake_t *hs);

/* Generates an initial challenge buffer to send to a peer */
folden_status_t folden_handshake_generate_challenge(folden_handshake_t *hs, uint8_t *out_challenge, size_t *out_len);

/* Process an incoming challenge and generate a response */
folden_status_t folden_handshake_process_challenge(folden_handshake_t *hs, const uint8_t *challenge, size_t chal_len, uint8_t *out_response, size_t *resp_len);

/* Verify a response from a peer against our sent challenge */
folden_status_t folden_handshake_verify_response(folden_handshake_t *hs, const uint8_t *response, size_t resp_len);

/* Returns the current handshake state */
folden_handshake_state_t folden_handshake_get_state(const folden_handshake_t *hs);

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_HANDSHAKE_H */

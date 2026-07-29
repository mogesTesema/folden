#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "folden/folden.h"

static int read_pair(const uint8_t *data, size_t size, const uint8_t **a,
                     size_t *a_len, const uint8_t **b, size_t *b_len) {
  if (size < 2) {
    return -1;
  }
  *a_len = data[0];
  *b_len = data[1];
  if (2 + *a_len + *b_len > size) {
    return -1;
  }
  *a = data + 2;
  *b = data + 2 + *a_len;
  return 0;
}

static void release_all_lanes(folden_relay_t *relay, const folden_capsule_t *cap) {
  size_t lanes = folden_capsule_lane_count(cap);
  for (size_t i = 0; i < lanes; i++) {
    folden_relay_release(relay, folden_capsule_lane_id(cap, i));
  }
}

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 4) {
    return 0;
  }
  uint8_t flags = data[0];
  data++;
  size--;

  const uint8_t *a = NULL;
  const uint8_t *b = NULL;
  size_t a_len = 0;
  size_t b_len = 0;
  if (read_pair(data, size, &a, &a_len, &b, &b_len) != 0) {
    return 0;
  }

  folden_capsule_t *ca = NULL;
  folden_capsule_t *cb = NULL;
  if (folden_decode_buffer(a, a_len, &ca) != FOLDEN_OK) {
    return 0;
  }
  if (folden_decode_buffer(b, b_len, &cb) != FOLDEN_OK) {
    folden_capsule_free(ca);
    return 0;
  }

  folden_relay_t *relay = folden_relay_create();
  folden_vault_t *vault = folden_vault_create();

  folden_relay_adopt(relay, ca);
  if (flags & 0x1) {
    folden_vault_checkpoint(vault, relay);
  }
  folden_relay_adopt(relay, cb);
  folden_merge_lanes(ca, cb, relay);

  if (flags & 0x2) {
    /* Fuzzer-driven early release: exercises whatever bookkeeping the
     * relay/cache/vault layers rely on to stay consistent when a lane's
     * refcount drops to zero at an arbitrary point in the sequence, not
     * just at the end. */
    release_all_lanes(relay, ca);
  }

  if ((flags & 0x1) && (flags & 0x4)) {
    folden_vault_restore(vault, relay);
  }

  uint8_t *out = NULL;
  size_t out_len = 0;
  folden_emit_capsule(ca, relay, &out, &out_len);
  free(out);
  out = NULL;
  folden_emit_capsule(cb, relay, &out, &out_len);
  free(out);

  folden_vault_destroy(vault);
  folden_relay_destroy(relay);
  folden_capsule_free(ca);
  folden_capsule_free(cb);
  return 0;
}

#ifdef __cplusplus
}
#endif

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

/* Drives adopt/emit/tick against two independently-decoded capsules that may
 * reference overlapping lane ids at different vault generations, so the
 * emit-cache pinning/invalidation paths see the same lane revisited under a
 * fuzzer-chosen mix of generations rather than one scripted sequence. */
#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 3) {
    return 0;
  }

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

  folden_relay_adopt(relay, ca);

  uint8_t *out = NULL;
  size_t out_len = 0;
  folden_emit_capsule(cb, relay, &out, &out_len);
  free(out);
  out = NULL;

  folden_relay_scheduler_tick(relay);
  folden_relay_scheduler_tick(relay);

  folden_emit_capsule(ca, relay, &out, &out_len);
  free(out);

  folden_relay_destroy(relay);
  folden_capsule_free(ca);
  folden_capsule_free(cb);
  return 0;
}

#ifdef __cplusplus
}
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "folden/folden.h"

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 3) {
    return 0;
  }

  folden_capsule_t *cap = NULL;
  if (folden_decode_buffer(data, size, &cap) != FOLDEN_OK) {
    return 0;
  }

  folden_relay_t *relay = folden_relay_create();
  folden_relay_adopt(relay, cap);
  folden_relay_adopt(relay, cap);
  folden_relay_scheduler_tick(relay);
  folden_relay_scheduler_tick(relay);

  uint8_t *out = NULL;
  size_t out_len = 0;
  folden_emit_capsule(cap, relay, &out, &out_len);
  free(out);

  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  return 0;
}

#ifdef __cplusplus
}
#endif

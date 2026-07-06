#include <stddef.h>
#include <stdint.h>

#include "folden/folden.h"

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 3) {
    return 0;
  }

  folden_capsule_t *cap = NULL;
  if (folden_decode_buffer(data, size, &cap) != FOLDEN_OK || !cap) {
    return 0;
  }
  if (folden_capsule_lane_count(cap) == 0) {
    folden_capsule_free(cap);
    return 0;
  }

  folden_relay_t *relay = folden_relay_create();
  uint32_t lane_id = folden_capsule_lane_id(cap, 0);

  folden_relay_adopt(relay, cap);

  const uint8_t *ptr = NULL;
  size_t len = 0;
  folden_capsule_emit_zero_copy(relay, lane_id, &ptr, &len);

  folden_relay_adopt(relay, cap);
  folden_relay_scheduler_tick(relay);

  if (ptr && len > 0) {
    volatile uint8_t sink = 0;
    for (size_t i = 0; i < len; i++) {
      sink ^= ptr[i];
    }
    (void)sink;
  }

  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  return 0;
}

#ifdef __cplusplus
}
#endif

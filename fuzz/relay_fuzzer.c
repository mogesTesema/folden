#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "folden/folden.h"

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 2) {
    return 0;
  }

  size_t count = (size_t)data[0];
  if (count > 8) {
    count = 8;
  }
  const uint8_t *p = data + 1;
  size_t rem = size - 1;

  folden_stream_t *stream = folden_stream_create();
  folden_relay_t *relay = folden_relay_create();
  folden_capsule_t *last = NULL;

  for (size_t i = 0; i < count && rem > 1; i++) {
    size_t chunk_len = p[0];
    p++;
    rem--;
    if (chunk_len > rem) {
      break;
    }
    folden_capsule_t *cap = NULL;
    folden_decode_stream(stream, p, chunk_len, &cap);
    if (cap) {
      folden_relay_adopt(relay, cap);
      if (last) {
        folden_capsule_t *bump = NULL;
        folden_decode_buffer(p, chunk_len, &bump);
        if (bump) {
          for (size_t li = 0; li < folden_capsule_lane_count(bump); li++) {
            if (folden_capsule_lane_has_vault_gen(bump, li)) {
              folden_relay_bump_generation(
                  relay, folden_capsule_lane_id(bump, li),
                  folden_capsule_lane_vault_gen(bump, li));
            }
          }
          folden_merge_lanes(last, bump, relay);
          folden_capsule_free(bump);
        }
      } else {
        last = cap;
        cap = NULL;
      }
      if (cap) {
        folden_capsule_free(cap);
      }
    }
    p += chunk_len;
    rem -= chunk_len;
  }

  if (last) {
    uint8_t *out = NULL;
    size_t out_len = 0;
    folden_emit_capsule(last, relay, &out, &out_len);
    free(out);
    folden_capsule_free(last);
  }
  folden_relay_destroy(relay);
  folden_stream_destroy(stream);
  return 0;
}

#ifdef __cplusplus
}
#endif

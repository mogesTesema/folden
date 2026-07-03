#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "folden/folden.h"

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size == 0) {
    return 0;
  }

  if (size >= 3 && data[0] <= 4) {
    size_t count = (size_t)data[0];
    const uint8_t *p = data + 1;
    size_t rem = size - 1;
    folden_stream_t *stream = folden_stream_create();
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
        uint8_t *out = NULL;
        size_t out_len = 0;
        folden_emit_capsule(cap, NULL, &out, &out_len);
        free(out);
        folden_capsule_free(cap);
      }
      p += chunk_len;
      rem -= chunk_len;
    }
    folden_stream_destroy(stream);
    return 0;
  }

  folden_capsule_t *cap = NULL;
  if (folden_decode_buffer(data, size, &cap) != FOLDEN_OK) {
    return 0;
  }
  uint8_t *out = NULL;
  size_t out_len = 0;
  folden_emit_capsule(cap, NULL, &out, &out_len);
  free(out);
  folden_capsule_free(cap);
  return 0;
}

#ifdef __cplusplus
}
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "folden/folden.h"

/* Adopts as many capsules as the input yields (each length-prefixed by one
 * byte), ticking and recovering between adopts in fuzzer-chosen order, so
 * the journal segment/compaction lifecycle interacts with real decode and
 * re-adopt traffic rather than one fixed scripted sequence. */
#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 2) {
    return 0;
  }

  folden_relay_t *relay = folden_relay_create();
  if (!relay) {
    return 0;
  }

  size_t off = 0;
  while (off + 1 < size) {
    uint8_t op = data[off++];
    size_t chunk_len = data[off++];
    if (chunk_len > size - off) {
      chunk_len = size - off;
    }
    const uint8_t *chunk = data + off;
    off += chunk_len;

    switch (op % 3) {
      case 0: {
        folden_capsule_t *cap = NULL;
        if (folden_decode_buffer(chunk, chunk_len, &cap) == FOLDEN_OK) {
          folden_relay_adopt(relay, cap);
          folden_capsule_free(cap);
        }
        break;
      }
      case 1:
        folden_relay_scheduler_tick(relay);
        break;
      case 2:
        folden_relay_recover(relay);
        break;
    }
  }

  folden_relay_destroy(relay);
  return 0;
}

#ifdef __cplusplus
}
#endif

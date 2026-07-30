#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "folden/folden.h"

/* Drives adopt/tick/emit/release/checkpoint/restore against one relay and
 * one vault in a fuzzer-chosen order and with fuzzer-chosen capsules,
 * rather than a fixed script. */
#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 3) {
    return 0;
  }

  folden_relay_t *relay = folden_relay_create();
  folden_vault_t *vault = folden_vault_create();
  if (!relay || !vault) {
    folden_relay_destroy(relay);
    folden_vault_destroy(vault);
    return 0;
  }

  size_t off = 0;
  while (off + 2 < size) {
    uint8_t op = data[off++];
    size_t chunk_len = (size_t)data[off] | ((size_t)data[off + 1] << 8);
    off += 2;
    if (chunk_len > size - off) {
      chunk_len = size - off;
    }
    const uint8_t *chunk = data + off;
    off += chunk_len;

    switch (op % 6) {
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
      case 2: {
        folden_capsule_t *cap = NULL;
        if (folden_decode_buffer(chunk, chunk_len, &cap) == FOLDEN_OK) {
          uint8_t *out = NULL;
          size_t out_len = 0;
          folden_emit_capsule(cap, relay, &out, &out_len);
          free(out);
          folden_capsule_free(cap);
        }
        break;
      }
      case 3:
        if (chunk_len >= 4) {
          uint32_t lane_id = (uint32_t)chunk[0] | ((uint32_t)chunk[1] << 8) |
                             ((uint32_t)chunk[2] << 16) | ((uint32_t)chunk[3] << 24);
          folden_relay_release(relay, lane_id);
        }
        break;
      case 4:
        folden_vault_checkpoint(vault, relay);
        break;
      case 5:
        folden_vault_restore(vault, relay);
        break;
    }
  }

  folden_vault_destroy(vault);
  folden_relay_destroy(relay);
  return 0;
}

#ifdef __cplusplus
}
#endif

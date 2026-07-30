#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "folden/folden.h"
#include "vault_registry.h"

/* Drives adopt/tick/zero-copy-emit/read/release/checkpoint/restore against
 * one relay in a fuzzer-chosen order, holding at most one outstanding
 * zero-copy pointer at a time so a later op can read back whatever an
 * earlier op captured. */
#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 3) {
    return 0;
  }

  folden_relay_t *relay = folden_relay_create();
  if (!relay) {
    return 0;
  }
  folden_vault_registry_t *vreg = folden_vault_registry_create();

  const uint8_t *saved_ptr = NULL;
  size_t saved_len = 0;

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

    switch (op % 7) {
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
        if (chunk_len >= 4) {
          uint32_t lane_id = (uint32_t)chunk[0] | ((uint32_t)chunk[1] << 8) |
                             ((uint32_t)chunk[2] << 16) | ((uint32_t)chunk[3] << 24);
          const uint8_t *ptr = NULL;
          size_t len = 0;
          if (folden_capsule_emit_zero_copy(relay, lane_id, &ptr, &len) == FOLDEN_OK) {
            saved_ptr = ptr;
            saved_len = len;
          }
        }
        break;
      }
      case 3: {
        if (saved_ptr && saved_len > 0) {
          volatile uint8_t sink = 0;
          for (size_t i = 0; i < saved_len; i++) {
            sink ^= saved_ptr[i];
          }
          (void)sink;
        }
        break;
      }
      case 4:
        if (chunk_len >= 4) {
          uint32_t lane_id = (uint32_t)chunk[0] | ((uint32_t)chunk[1] << 8) |
                             ((uint32_t)chunk[2] << 16) | ((uint32_t)chunk[3] << 24);
          folden_relay_release(relay, lane_id);
        }
        break;
      case 5:
        folden_vault_registry_checkpoint(vreg, "a", relay);
        break;
      case 6:
        folden_vault_registry_restore(vreg, "a", relay);
        break;
    }
  }

  folden_vault_registry_destroy(vreg);
  folden_relay_destroy(relay);
  return 0;
}

#ifdef __cplusplus
}
#endif

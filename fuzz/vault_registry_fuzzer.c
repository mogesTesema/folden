#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "folden/folden.h"
#include "vault_registry.h"

/* Drives a relay plus a small named-vault registry through fuzzer-chosen
 * adopt/tick/checkpoint(name)/restore(name)/release ops, so the specific
 * interleaving of which name aliases which adoption generation -- and when
 * a lane's last reference is dropped relative to a later restore -- has to
 * be discovered by mutation rather than replayed from a fixed script. */
#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 3) {
    return 0;
  }

  folden_relay_t *relay = folden_relay_create();
  folden_vault_registry_t *reg = folden_vault_registry_create();
  if (!relay || !reg) {
    folden_relay_destroy(relay);
    folden_vault_registry_destroy(reg);
    return 0;
  }

  static const char *names[4] = {"a", "b", "c", "d"};

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

    const char *name = names[op % 4];

    switch ((op / 4) % 5) {
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
        folden_vault_registry_checkpoint(reg, name, relay);
        break;
      case 3:
        folden_vault_registry_restore(reg, name, relay);
        break;
      case 4:
        if (chunk_len >= 4) {
          uint32_t lane_id = (uint32_t)chunk[0] | ((uint32_t)chunk[1] << 8) |
                             ((uint32_t)chunk[2] << 16) | ((uint32_t)chunk[3] << 24);
          folden_relay_release(relay, lane_id);
        }
        break;
    }
  }

  folden_vault_registry_destroy(reg);
  folden_relay_destroy(relay);
  return 0;
}

#ifdef __cplusplus
}
#endif

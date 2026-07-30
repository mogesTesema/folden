#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "journal_file.h"

/* Drives append/seal/scan/compact against one file-backed journal store in
 * a fuzzer-chosen order. The scan callback appends a small fixed record
 * back into the same store, so whatever segment-growth interaction that
 * creates has to emerge from the op sequence rather than being scripted. */

static folden_journal_store_t *g_store;

static void rescan_cb(const uint8_t *data, size_t len, uint32_t seg_seq, void *user) {
  (void)data;
  (void)len;
  (void)seg_seq;
  (void)user;
  static const uint8_t reentry[] = {0x52, 0x45, 0x2d, 0x41, 0x50, 0x50};
  folden_journal_store_append(g_store, reentry, sizeof(reentry));
}

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 3) {
    return 0;
  }

  folden_journal_store_t *store =
      folden_journal_store_create("/tmp", "fuzzjf", 0);
  if (!store) {
    return 0;
  }
  store->sync_on_seal = 0;
  g_store = store;

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

    switch (op % 4) {
      case 0:
        if (chunk_len > 0) {
          folden_journal_store_append(store, chunk, chunk_len);
        }
        break;
      case 1:
        folden_journal_store_seal(store);
        break;
      case 2:
        folden_journal_store_scan(store, rescan_cb, store);
        break;
      case 3: {
        uint32_t min_seq = 0;
        if (chunk_len >= 4) {
          min_seq = (uint32_t)chunk[0] | ((uint32_t)chunk[1] << 8) |
                    ((uint32_t)chunk[2] << 16) | ((uint32_t)chunk[3] << 24);
        }
        folden_journal_store_compact(store, min_seq);
        break;
      }
    }
  }

  g_store = NULL;
  folden_journal_store_destroy(store);
  return 0;
}

#ifdef __cplusplus
}
#endif

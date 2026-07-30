#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "capsule_mux.h"

/* Feeds fuzzer-chosen chunks into a streaming demuxer whose callback can
 * itself reset the same stream, so the interaction between chunk
 * boundaries and a callback-triggered reset has to be discovered by
 * mutation rather than replayed from a fixed split. */

static folden_mux_stream_t *g_ms;

static void demux_cb(const uint8_t *wire, size_t wire_len, void *user) {
  (void)wire;
  (void)wire_len;
  (void)user;
  folden_mux_stream_reset(g_ms);
}

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 3) {
    return 0;
  }

  folden_mux_stream_t *ms = folden_mux_stream_create();
  if (!ms) {
    return 0;
  }
  g_ms = ms;

  size_t off = 0;
  while (off + 2 < size) {
    size_t chunk_len = (size_t)data[off] | ((size_t)data[off + 1] << 8);
    off += 2;
    if (chunk_len > size - off) {
      chunk_len = size - off;
    }
    if (chunk_len > 0) {
      size_t consumed = folden_mux_stream_feed(ms, data + off, chunk_len, demux_cb, NULL);
      if (consumed == 0) {
        off += chunk_len;
        continue;
      }
    }
    off += chunk_len;
  }

  g_ms = NULL;
  folden_mux_stream_destroy(ms);
  return 0;
}

#ifdef __cplusplus
}
#endif

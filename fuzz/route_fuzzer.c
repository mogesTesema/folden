#include <stddef.h>
#include <stdint.h>

#include "folden/folden.h"
#include "route_table.h"

/* Drives the routing table's general API through a sequence of
 * fuzzer-chosen operations rather than one fixed call order, so the
 * triggering sequence (if any) has to be discovered by mutation. */
#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 2) {
    return 0;
  }

  folden_route_table_t *t = folden_route_table_create(8);
  if (!t) {
    return 0;
  }

  size_t off = 0;
  while (off + 3 <= size) {
    uint8_t op = data[off++];
    uint32_t lane = data[off++];
    uint32_t val = data[off++];

    switch (op % 5) {
      case 0:
        folden_route_table_insert(t, lane, val, val);
        break;
      case 1: {
        uint32_t hop = 0;
        folden_route_table_lookup(t, lane, &hop);
        break;
      }
      case 2:
        folden_route_table_note_failure(t, lane);
        break;
      case 3:
        folden_route_table_note_success(t, lane);
        break;
      case 4:
        folden_route_table_count(t);
        break;
    }
  }

  folden_route_table_destroy(t);
  return 0;
}

#ifdef __cplusplus
}
#endif

#include <stddef.h>
#include <stdint.h>

#include "folden/folden.h"

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data) {
    return 0;
  }

  folden_relay_t *relay = folden_relay_create();
  if (!relay) {
    return 0;
  }

  int error_line = 0;
  folden_relay_configure(relay, (const char *)data, size, &error_line);

  folden_relay_destroy(relay);
  return 0;
}

#ifdef __cplusplus
}
#endif

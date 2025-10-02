#include "varfold.h"

#include <string.h>

size_t folden_varfold_encode(uint64_t value, uint8_t *out, size_t out_cap) {
  uint8_t chunks[12];
  size_t count = 0;
  uint64_t v = value;
  do {
    if (count >= sizeof(chunks)) {
      return 0;
    }
    chunks[count++] = (uint8_t)(v & 0x7Fu);
    v >>= 7;
  } while (v != 0);

  if (count > out_cap) {
    return 0;
  }
  for (size_t i = 0; i < count; i++) {
    uint8_t byte = chunks[i];
    if (i >= 2) {
      byte ^= 0x2Au;
    }
    if (i + 1 < count) {
      byte |= 0x80u;
    }
    out[i] = byte;
  }
  return count;
}

int folden_varfold_decode(const uint8_t *data, size_t len, uint64_t *out,
                          size_t *consumed) {
  uint64_t result = 0;
  unsigned shift = 0;
  size_t i = 0;
  int cont_index = 0;

  while (i < len) {
    uint8_t byte = data[i++];
    uint8_t chunk = byte & 0x7Fu;
    if (cont_index >= 2) {
      chunk ^= 0x2Au;
    }
    result |= (uint64_t)chunk << shift;
    shift += 7;
    cont_index++;
    if ((byte & 0x80u) == 0) {
      if (out) {
        *out = result;
      }
      if (consumed) {
        *consumed = i;
      }
      return 0;
    }
    if (shift >= 64) {
      return -1;
    }
  }
  return -1;
}

#include "capsule_wire.h"

#include "../src/crc32.h"
#include "../src/varfold.h"

#include <stdlib.h>
#include <string.h>

static size_t wf(uint8_t *out, size_t cap, size_t off, uint64_t v) {
  uint8_t tmp[12];
  size_t n = folden_varfold_encode(v, tmp, sizeof(tmp));
  if (off + n > cap) {
    return off;
  }
  memcpy(out + off, tmp, n);
  return off + n;
}

int folden_wire_build_capsule(const folden_wire_lane_t *lanes, size_t lane_count,
                              uint32_t vault_trailer, int has_vault_trailer,
                              uint8_t **out, size_t *out_len) {
  size_t cap = 1024;
  uint8_t *buf = malloc(cap);
  if (!buf) {
    return -1;
  }
  size_t off = 0;
  buf[off++] = 0x46;
  buf[off++] = 0x4F;
  buf[off++] = 0x4C;
  buf[off++] = 0x44;
  buf[off++] = 1;
  off = wf(buf, cap, off, lane_count);

  int any_vault = 0;
  for (size_t i = 0; i < lane_count; i++) {
    const folden_wire_lane_t *ln = &lanes[i];
    off = wf(buf, cap, off, ln->lane_id);
    off = wf(buf, cap, off, ln->tag_count);
    for (size_t t = 0; t < ln->tag_count; t++) {
      off = wf(buf, cap, off, ln->tags[t].tag_id);
      off = wf(buf, cap, off, ln->tags[t].fold_depth);
      off = wf(buf, cap, off, ln->tags[t].span);
      if (ln->tags[t].tag_id == 2) {
        any_vault = 1;
      }
    }
    off = wf(buf, cap, off, ln->slab_len);
    if (off + ln->slab_len > cap) {
      cap = off + ln->slab_len + 64;
      uint8_t *nb = realloc(buf, cap);
      if (!nb) {
        free(buf);
        return -1;
      }
      buf = nb;
    }
    if (ln->slab_len > 0 && ln->slab) {
      memcpy(buf + off, ln->slab, ln->slab_len);
      off += ln->slab_len;
    }
    if (ln->has_vault_gen) {
      any_vault = 1;
    }
  }

  if (has_vault_trailer || any_vault) {
    off = wf(buf, cap, off, vault_trailer);
  }

  if (off + 4 > cap) {
    uint8_t *nb = realloc(buf, cap + 4);
    if (!nb) {
      free(buf);
      return -1;
    }
    buf = nb;
    cap += 4;
  }
  uint32_t crc = folden_crc32(buf, off);
  buf[off++] = (uint8_t)(crc & 0xffu);
  buf[off++] = (uint8_t)((crc >> 8) & 0xffu);
  buf[off++] = (uint8_t)((crc >> 16) & 0xffu);
  buf[off++] = (uint8_t)((crc >> 24) & 0xffu);
  *out = buf;
  *out_len = off;
  return 0;
}

#ifndef FOLDEN_CAPSULE_WIRE_H
#define FOLDEN_CAPSULE_WIRE_H

#include <stddef.h>
#include <stdint.h>

typedef struct folden_wire_tag {
  uint32_t tag_id;
  uint32_t fold_depth;
  uint32_t span;
} folden_wire_tag_t;

typedef struct folden_wire_lane {
  uint32_t lane_id;
  const folden_wire_tag_t *tags;
  size_t tag_count;
  const uint8_t *slab;
  size_t slab_len;
  uint32_t vault_gen;
  int has_vault_gen;
} folden_wire_lane_t;

int folden_wire_build_capsule(const folden_wire_lane_t *lanes, size_t lane_count,
                              uint32_t vault_trailer, int has_vault_trailer,
                              uint8_t **out, size_t *out_len);

#endif /* FOLDEN_CAPSULE_WIRE_H */

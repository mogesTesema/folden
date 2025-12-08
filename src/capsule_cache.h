#ifndef FOLDEN_CAPSULE_CACHE_H
#define FOLDEN_CAPSULE_CACHE_H

#include <stddef.h>
#include <stdint.h>

typedef struct folden_capsule_cache folden_capsule_cache_t;

folden_capsule_cache_t *folden_cache_create(void);
void folden_cache_destroy(folden_capsule_cache_t *cache);

void folden_cache_pin_emit_slab(folden_capsule_cache_t *cache, uint32_t lane_id,
                                uint32_t generation, const uint8_t *slab,
                                size_t slab_len);
const uint8_t *folden_cache_lookup_emit_slab(folden_capsule_cache_t *cache,
                                               uint32_t lane_id,
                                               uint32_t generation,
                                               size_t *out_len);
void folden_cache_invalidate_lane(folden_capsule_cache_t *cache, uint32_t lane_id);

#endif /* FOLDEN_CAPSULE_CACHE_H */

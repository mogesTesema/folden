#ifndef FOLDEN_ROUTE_CACHE
#define FOLDEN_ROUTE_CACHE

#include "folden_internal.h"


typedef struct folden_route_cache folden_route_cache_t;
folden_route_cache_t *folden_route_cache_create(void);
void folden_route_cache_destroy(folden_route_cache_t *t);

void folden_route_cache_pin(folden_route_cache_t *c, uint32_t lane, uint32_t gen,
    const uint8_t *slab, size_t len);
const uint8_t *folden_route_cache_lookup(folden_route_cache_t *c, uint32_t lane,
    uint32_t gen, size_t *out_len);

/* Drop all entries for this lane so stale cached data is never returned. */
void folden_route_cache_invalidate_lane(folden_route_cache_t *c, uint32_t lane);

#endif

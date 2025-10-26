#ifndef FOLDEN_META_RESOLVER_H
#define FOLDEN_META_RESOLVER_H

#include "folden_internal.h"

folden_status_t folden_meta_resolver_build(folden_lane_t *lane,
                                           const folden_relay_entry_t *rent);
folden_status_t folden_meta_resolver_build_partial(folden_lane_t *lane,
                                                   const folden_relay_entry_t *rent,
                                                   int resume_from_partial);

#endif /* FOLDEN_META_RESOLVER_H */

#ifndef FOLDEN_TAG_INDEX_H
#define FOLDEN_TAG_INDEX_H

#include "folden_internal.h"

folden_status_t folden_tag_index_build(folden_lane_t *lane);
folden_status_t folden_tag_index_build_partial(folden_lane_t *lane,
                                               int resume_from_partial);

#endif /* FOLDEN_TAG_INDEX_H */

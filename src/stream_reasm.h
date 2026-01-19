#ifndef FOLDEN_STREAM_REASM_H
#define FOLDEN_STREAM_REASM_H

#include "folden_internal.h"

folden_stream_t *folden_stream_create_impl(void);
void folden_stream_destroy_impl(folden_stream_t *stream);

folden_status_t folden_stream_feed(folden_stream_t *stream, const uint8_t *chunk,
                                   size_t chunk_len, folden_capsule_t **out);

folden_status_t folden_stream_materialize_partial(folden_stream_t *stream,
                                                  folden_lane_t *lane_out);

#endif /* FOLDEN_STREAM_REASM_H */

#ifndef FOLDEN_SLAB_DECODE_H
#define FOLDEN_SLAB_DECODE_H

#include "folden_internal.h"

#include <stddef.h>
#include <stdint.h>

folden_status_t folden_parse_capsule(const uint8_t *data, size_t len,
                                     folden_capsule_t **out);
folden_status_t folden_parse_capsule_ex(const uint8_t *data, size_t len,
                                        folden_capsule_t **out, size_t *out_consumed);
folden_status_t folden_parse_capsule_partial(folden_stream_t *stream,
                                             const uint8_t *data, size_t len,
                                             folden_capsule_t **out);
void folden_capsule_free(folden_capsule_t *capsule);

#endif /* FOLDEN_SLAB_DECODE_H */

#ifndef FOLDEN_CAPSULE_EMIT_H
#define FOLDEN_CAPSULE_EMIT_H

#include "folden_internal.h"

folden_status_t folden_capsule_emit_impl(const folden_capsule_t *capsule,
                                         folden_relay_t *relay,
                                         uint8_t **out, size_t *out_len);

folden_status_t folden_capsule_emit_zero_copy_impl(folden_relay_t *relay,
                                                   uint32_t lane_id,
                                                   const uint8_t **out_ptr,
                                                   size_t *out_len);

#endif /* FOLDEN_CAPSULE_EMIT_H */

#ifndef FOLDEN_WIRE_VALIDATOR_H
#define FOLDEN_WIRE_VALIDATOR_H

#include "folden_internal.h"

int folden_wire_validate_header(const uint8_t *data, size_t len);
int folden_wire_validate_lane_tags(const folden_lane_t *lane, int stream_resume);
int folden_wire_validate_capsule_lanes(const folden_capsule_t *cap, int stream_resume);

/* Stricter, opt-in structural checks beyond what decode itself enforces:
 * no two lanes sharing a lane_id, and no lane carrying the same tag_id
 * more than once (each tag_id is meant to appear at most once per lane).
 * Returns 0 if the capsule passes both checks. */
int folden_wire_validate_no_duplicate_lanes(const folden_capsule_t *cap);
int folden_wire_validate_no_duplicate_tags(const folden_lane_t *lane);

#endif /* FOLDEN_WIRE_VALIDATOR_H */

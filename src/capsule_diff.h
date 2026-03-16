#ifndef FOLDEN_CAPSULE_DIFF_H
#define FOLDEN_CAPSULE_DIFF_H

#include "folden_internal.h"

typedef struct folden_capsule_diff folden_capsule_diff_t;

/* Compares two capsule snapshots of the same logical stream by lane_id.
 * A lane present in `new_cap` but not `old_cap` is "added"; present in
 * `old_cap` but not `new_cap` is "removed"; present in both but with a
 * different slab length, slab bytes, or tag set is "changed". A lane
 * identical in both is not reported at all. */
folden_status_t folden_capsule_diff(const folden_capsule_t *old_cap,
                                    const folden_capsule_t *new_cap,
                                    folden_capsule_diff_t **out);
void folden_capsule_diff_free(folden_capsule_diff_t *diff);

size_t folden_capsule_diff_added_count(const folden_capsule_diff_t *diff);
size_t folden_capsule_diff_removed_count(const folden_capsule_diff_t *diff);
size_t folden_capsule_diff_changed_count(const folden_capsule_diff_t *diff);

uint32_t folden_capsule_diff_added_lane(const folden_capsule_diff_t *diff, size_t idx);
uint32_t folden_capsule_diff_removed_lane(const folden_capsule_diff_t *diff, size_t idx);
uint32_t folden_capsule_diff_changed_lane(const folden_capsule_diff_t *diff, size_t idx);

#endif /* FOLDEN_CAPSULE_DIFF_H */

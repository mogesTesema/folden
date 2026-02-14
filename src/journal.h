#ifndef FOLDEN_JOURNAL_H
#define FOLDEN_JOURNAL_H

#include "folden_internal.h"
#include "defer_free.h"

folden_journal_t *folden_journal_create(void);
void folden_journal_destroy(folden_journal_t *j);

/* Overrides the journal's segment-rotation and compaction thresholds.
 * A zero argument leaves the corresponding threshold unchanged, so callers
 * can tune a subset of the knobs. */
void folden_journal_set_thresholds(folden_journal_t *j, uint32_t segment_max_bytes,
                                   uint32_t segment_max_age_ticks,
                                   uint32_t compact_threshold,
                                   uint32_t retire_after_ticks);

/* Appends a lane's wire bytes to the currently-open segment, sealing and
 * rotating to a new segment once the active one is full. */
folden_status_t folden_journal_append(folden_journal_t *j, const uint8_t *wire,
                                      size_t wire_len, uint32_t lane_id);

/* Drives the journal's background work for one tick: seals an over-age open
 * segment, compacts sealed segments once enough of them have accumulated,
 * and retires the replay view of any segment whose grace period has
 * elapsed. */
folden_status_t folden_journal_tick(folden_journal_t *j, folden_defer_free_t *defer);

size_t folden_journal_segment_count(const folden_journal_t *j);

/* Returns a pointer to segment `index`'s current backing bytes (the still-open
 * blob, or the retained copy made at compaction time) valid until the next
 * call to folden_journal_tick -- background compaction/retirement can
 * invalidate it. Returns FOLDEN_ERR_BAD_STATE if the segment has already been
 * retired or the index is out of range. */
folden_status_t folden_journal_replay_segment(folden_journal_t *j, size_t index,
                                              const uint8_t **out_ptr, size_t *out_len);

#endif /* FOLDEN_JOURNAL_H */

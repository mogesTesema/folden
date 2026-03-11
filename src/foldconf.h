#ifndef FOLDEN_FOLDCONF_H
#define FOLDEN_FOLDCONF_H

#include "folden_internal.h"

/* Runtime-tunable knobs for a relay's journal and replication subsystems.
 * Every field has a `has_*` companion so a config that only sets some keys
 * doesn't clobber the rest with zero. */
typedef struct folden_foldconf {
  uint32_t journal_segment_max_bytes;
  uint32_t journal_segment_max_age_ticks;
  uint32_t journal_compact_threshold;
  uint32_t journal_retire_after_ticks;
  uint32_t replicate_max_attempts;

  int has_journal_segment_max_bytes;
  int has_journal_segment_max_age_ticks;
  int has_journal_compact_threshold;
  int has_journal_retire_after_ticks;
  int has_replicate_max_attempts;
} folden_foldconf_t;

/* Parses a small line-oriented `key = value` config format:
 *   # comments and blank lines are ignored
 *   journal.segment.max_bytes = 4096
 *   journal.segment.max_age_ticks = 3
 *   journal.compact.threshold = 2
 *   journal.retire.after_ticks = 2
 *   replicate.max_attempts = 4
 *
 * On a malformed line (bad key, non-numeric value, missing '=') returns
 * FOLDEN_ERR_BAD_TAG and, if out_error_line is non-NULL, the 1-based line
 * number of the first offending line. */
folden_status_t folden_foldconf_parse(const char *text, size_t len,
                                      folden_foldconf_t *out, int *out_error_line);

/* Applies every key present in `conf` (per its has_* flags) to the relay's
 * live journal and replication fsm. */
folden_status_t folden_foldconf_apply(const folden_foldconf_t *conf, folden_relay_t *relay);

#endif /* FOLDEN_FOLDCONF_H */

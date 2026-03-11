#include "foldconf.h"

#include "journal.h"
#include "relay_registry.h"
#include "replicate.h"

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_ws(const char *p, const char *end) {
  while (p < end && isspace((unsigned char)*p)) {
    p++;
  }
  return p;
}

static const char *rskip_ws(const char *start, const char *p) {
  while (p > start && isspace((unsigned char)p[-1])) {
    p--;
  }
  return p;
}

/* Parses an unsigned decimal integer occupying the whole [start, end) span
 * (after whitespace trimming by the caller). Rejects empty spans, any
 * non-digit character, and values that don't fit in uint32_t. */
static int parse_uint(const char *start, const char *end, uint32_t *out) {
  if (start >= end) {
    return -1;
  }
  uint64_t v = 0;
  for (const char *p = start; p < end; p++) {
    if (*p < '0' || *p > '9') {
      return -1;
    }
    v = v * 10 + (uint64_t)(*p - '0');
    if (v > UINT32_MAX) {
      return -1;
    }
  }
  *out = (uint32_t)v;
  return 0;
}

typedef struct foldconf_key {
  const char *name;
  size_t offset;   /* offset of the uint32_t value field in folden_foldconf_t */
  size_t has_offset; /* offset of the matching has_* int flag */
} foldconf_key_t;

#define KEY(name_str, field, has_field)                                     \
  { name_str, offsetof(folden_foldconf_t, field), offsetof(folden_foldconf_t, has_field) }

static const foldconf_key_t KEYS[] = {
    KEY("journal.segment.max_bytes", journal_segment_max_bytes,
        has_journal_segment_max_bytes),
    KEY("journal.segment.max_age_ticks", journal_segment_max_age_ticks,
        has_journal_segment_max_age_ticks),
    KEY("journal.compact.threshold", journal_compact_threshold,
        has_journal_compact_threshold),
    KEY("journal.retire.after_ticks", journal_retire_after_ticks,
        has_journal_retire_after_ticks),
    KEY("replicate.max_attempts", replicate_max_attempts,
        has_replicate_max_attempts),
};
#define FOLDCONF_KEY_COUNT (sizeof(KEYS) / sizeof(KEYS[0]))

folden_status_t folden_foldconf_parse(const char *text, size_t len,
                                      folden_foldconf_t *out, int *out_error_line) {
  if (!text || !out) {
    return FOLDEN_ERR_BAD_STATE;
  }
  memset(out, 0, sizeof(*out));

  const char *p = text;
  const char *end = text + len;
  int line_no = 0;

  while (p < end) {
    const char *line_start = p;
    const char *nl = memchr(p, '\n', (size_t)(end - p));
    const char *line_end = nl ? nl : end;
    line_no++;

    const char *ls = skip_ws(line_start, line_end);
    const char *le = rskip_ws(ls, line_end);

    p = nl ? nl + 1 : end;

    if (ls == le || *ls == '#') {
      continue;
    }

    const char *eq = memchr(ls, '=', (size_t)(le - ls));
    if (!eq) {
      if (out_error_line) {
        *out_error_line = line_no;
      }
      return FOLDEN_ERR_BAD_TAG;
    }

    const char *key_start = ls;
    const char *key_end = rskip_ws(ls, eq);
    const char *val_start = skip_ws(eq + 1, le);
    const char *val_end = le;

    size_t key_len = (size_t)(key_end - key_start);
    const foldconf_key_t *matched = NULL;
    for (size_t i = 0; i < FOLDCONF_KEY_COUNT; i++) {
      size_t klen = strlen(KEYS[i].name);
      if (klen == key_len && memcmp(KEYS[i].name, key_start, key_len) == 0) {
        matched = &KEYS[i];
        break;
      }
    }
    if (!matched) {
      if (out_error_line) {
        *out_error_line = line_no;
      }
      return FOLDEN_ERR_BAD_TAG;
    }

    uint32_t value = 0;
    if (parse_uint(val_start, val_end, &value) != 0) {
      if (out_error_line) {
        *out_error_line = line_no;
      }
      return FOLDEN_ERR_BAD_TAG;
    }

    *(uint32_t *)((char *)out + matched->offset) = value;
    *(int *)((char *)out + matched->has_offset) = 1;
  }

  return FOLDEN_OK;
}

folden_status_t folden_foldconf_apply(const folden_foldconf_t *conf, folden_relay_t *relay) {
  if (!conf || !relay) {
    return FOLDEN_ERR_BAD_STATE;
  }

  if (relay->journal &&
      (conf->has_journal_segment_max_bytes || conf->has_journal_segment_max_age_ticks ||
       conf->has_journal_compact_threshold || conf->has_journal_retire_after_ticks)) {
    folden_journal_set_thresholds(
        relay->journal, conf->has_journal_segment_max_bytes ? conf->journal_segment_max_bytes : 0,
        conf->has_journal_segment_max_age_ticks ? conf->journal_segment_max_age_ticks : 0,
        conf->has_journal_compact_threshold ? conf->journal_compact_threshold : 0,
        conf->has_journal_retire_after_ticks ? conf->journal_retire_after_ticks : 0);
  }

  if (relay->fsm && conf->has_replicate_max_attempts) {
    folden_replicate_fsm_set_max_attempts(relay->fsm, conf->replicate_max_attempts);
  }

  return FOLDEN_OK;
}

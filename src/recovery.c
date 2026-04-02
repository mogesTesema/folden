#include "recovery.h"

#include "journal.h"
#include "relay_registry.h"
#include "slab_decode.h"

#include <stdlib.h>
#include <string.h>

size_t folden_recovery_replay_journal(folden_journal_t *journal, folden_relay_t *relay) {
  if (!journal || !relay) {
    return 0;
  }

  size_t replayed = 0;
  size_t segments = folden_journal_segment_count(journal);
  for (size_t s = 0; s < segments; s++) {
    const uint8_t *ptr = NULL;
    size_t len = 0;
    if (folden_journal_replay_segment(journal, s, &ptr, &len) != FOLDEN_OK) {
      continue;
    }
    if (len == 0) {
      continue;
    }

    /* Take an independent copy before scanning. Re-adopting a replayed
     * capsule below re-journals it, and if `s` is still the active
     * (uncompacted) segment, that append can realloc its blob -- the very
     * buffer `ptr` points into. Scanning the live pointer while it can be
     * moved out from under us by our own replay is not safe. */
    uint8_t *snapshot = malloc(len);
    if (!snapshot) {
      continue;
    }
    memcpy(snapshot, ptr, len);

    size_t cursor = 0;
    while (cursor < len) {
      folden_capsule_t *cap = NULL;
      size_t consumed = 0;
      folden_status_t st =
          folden_parse_capsule_ex(snapshot + cursor, len - cursor, &cap, &consumed);
      if (st != FOLDEN_OK) {
        break;
      }
      folden_relay_adopt_impl(relay, cap);
      folden_capsule_free(cap);
      replayed++;
      cursor += consumed;
    }

    free(snapshot);
  }

  return replayed;
}

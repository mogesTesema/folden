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

    size_t cursor = 0;
    while (cursor < len) {
      folden_capsule_t *cap = NULL;
      size_t consumed = 0;
      folden_status_t st =
          folden_parse_capsule_ex(ptr + cursor, len - cursor, &cap, &consumed);
      if (st != FOLDEN_OK) {
        break;
      }
      folden_relay_adopt_impl(relay, cap);
      folden_capsule_free(cap);
      replayed++;
      cursor += consumed;
      folden_journal_tick(journal, relay->defer);
    }
  }

  return replayed;
}

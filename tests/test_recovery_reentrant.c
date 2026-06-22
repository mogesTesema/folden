#include "minctest.h"

#include "capsule_wire.h"
#include "folden/folden.h"

/* Recovering from a journal segment that is still active (not yet sealed)
 * used to scan a raw pointer into that segment's live, growable blob. Each
 * replayed capsule is re-adopted, which re-journals it into that same
 * active segment -- and if the append grows (reallocs) the blob, the scan
 * pointer used to dangle mid-loop. */
TEST(test_recover_while_segment_still_active_and_growing) {
  folden_relay_t *relay = folden_relay_create();

  /* Adopt enough small capsules into the same lane, without ticking, that
   * the active segment's growable blob has to reallocate at least once
   * (it starts at 256 bytes). */
  for (uint32_t i = 0; i < 40; i++) {
    folden_wire_tag_t tags[] = {{3, 0, 1}};
    folden_wire_lane_t la = {1, tags, 1, (const uint8_t *)"x", 1, 0, 0};
    uint8_t *wire = NULL;
    size_t wire_len = 0;
    ASSERT_EQ(0, folden_wire_build_capsule(&la, 1, 0, 0, &wire, &wire_len));
    folden_capsule_t *cap = NULL;
    ASSERT_EQ(FOLDEN_OK, folden_decode_buffer(wire, wire_len, &cap));
    ASSERT_EQ(FOLDEN_OK, folden_relay_adopt(relay, cap));
    folden_capsule_free(cap);
    free(wire);
  }

  /* The active segment is still unsealed here. Recovering re-adopts every
   * replayed capsule, re-growing the very segment being scanned. */
  size_t replayed = folden_relay_recover(relay);
  ASSERT(replayed >= 40);

  folden_relay_destroy(relay);
}

void test_recovery_reentrant_register(void) {
  RUN(test_recover_while_segment_still_active_and_growing);
}

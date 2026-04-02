#ifndef FOLDEN_RECOVERY_H
#define FOLDEN_RECOVERY_H

#include "folden_internal.h"

/* Walks every journal segment that hasn't been retired yet, decodes each
 * capsule packed into its replay bytes (segments accumulate several
 * capsules' wire bytes back-to-back), and re-adopts them into `relay`.
 * Meant for recovering a relay's live state from its own journal after a
 * restart, not for normal operation.
 *
 * Returns the number of capsules successfully re-adopted. A segment
 * containing corrupt trailing bytes contributes whatever whole capsules
 * were decoded before the corruption and then stops on that segment. */
size_t folden_recovery_replay_journal(folden_journal_t *journal, folden_relay_t *relay);

#endif /* FOLDEN_RECOVERY_H */

#ifndef FOLDEN_SNAPSHOT_CODEC_H
#define FOLDEN_SNAPSHOT_CODEC_H

#include "folden_internal.h"

typedef struct folden_vault_snapshot {
  folden_relay_entry_t *entries;
  size_t count;
} folden_vault_snapshot_t;

folden_status_t folden_snapshot_codec_capture(folden_vault_t *vault,
                                              folden_relay_t *relay);
folden_status_t folden_snapshot_codec_restore(folden_vault_t *vault,
                                              folden_relay_t *relay);

/* Releases the arena borrows the vault's current snapshot holds. Must be
 * called before the vault (and its snapshot) is torn down. */
void folden_snapshot_codec_release(folden_vault_t *vault);

#endif /* FOLDEN_SNAPSHOT_CODEC_H */

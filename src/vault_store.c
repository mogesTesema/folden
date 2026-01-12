#include "vault_store.h"

#include "snapshot_codec.h"

#include <stdlib.h>

folden_vault_t *folden_vault_create_impl(void) {
  return calloc(1, sizeof(folden_vault_t));
}

void folden_vault_destroy_impl(folden_vault_t *vault) {
  if (!vault) {
    return;
  }
  folden_snapshot_codec_release(vault);
  for (size_t i = 0; i < vault->count; i++) {
    free(vault->snapshot[i].cached_tags);
    free(vault->snapshot[i].cached_meta);
  }
  free(vault->snapshot);
  free(vault);
}

folden_status_t folden_vault_checkpoint_impl(folden_vault_t *vault,
                                             folden_relay_t *relay) {
  return folden_snapshot_codec_capture(vault, relay);
}

folden_status_t folden_vault_restore_impl(folden_vault_t *vault,
                                          folden_relay_t *relay) {
  return folden_snapshot_codec_restore(vault, relay);
}

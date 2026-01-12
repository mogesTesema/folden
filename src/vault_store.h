#ifndef FOLDEN_VAULT_STORE_H
#define FOLDEN_VAULT_STORE_H

#include "folden_internal.h"

folden_vault_t *folden_vault_create_impl(void);
void folden_vault_destroy_impl(folden_vault_t *vault);

folden_status_t folden_vault_checkpoint_impl(folden_vault_t *vault,
                                             folden_relay_t *relay);
folden_status_t folden_vault_restore_impl(folden_vault_t *vault,
                                          folden_relay_t *relay);

#endif /* FOLDEN_VAULT_STORE_H */

#ifndef FOLDEN_VAULT_REGISTRY_H
#define FOLDEN_VAULT_REGISTRY_H

#include "folden_internal.h"

/* A small named collection of vaults, so a relay can keep more than one
 * checkpoint around (e.g. "pre-migration", "hourly", "last-known-good")
 * instead of only the single implicit snapshot folden_vault_t itself
 * holds. Ownership of each folden_vault_t stays with the registry once
 * registered. */
typedef struct folden_vault_registry folden_vault_registry_t;

folden_vault_registry_t *folden_vault_registry_create(void);
void folden_vault_registry_destroy(folden_vault_registry_t *reg);

/* Checkpoints `relay` into a fresh vault stored under `name`, replacing
 * (and destroying) any vault already registered under that name. */
folden_status_t folden_vault_registry_checkpoint(folden_vault_registry_t *reg,
                                                 const char *name,
                                                 folden_relay_t *relay);

/* Restores `relay` from the vault stored under `name`. */
folden_status_t folden_vault_registry_restore(folden_vault_registry_t *reg,
                                              const char *name,
                                              folden_relay_t *relay);

folden_status_t folden_vault_registry_remove(folden_vault_registry_t *reg,
                                             const char *name);

size_t folden_vault_registry_count(const folden_vault_registry_t *reg);

#endif /* FOLDEN_VAULT_REGISTRY_H */

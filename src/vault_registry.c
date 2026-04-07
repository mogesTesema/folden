#include "vault_registry.h"

#include "vault_store.h"

#include <stdlib.h>
#include <string.h>

#define FOLDEN_VAULT_NAME_MAX 63

typedef struct folden_named_vault {
  char name[FOLDEN_VAULT_NAME_MAX + 1];
  folden_vault_t *vault;
  int in_use;
} folden_named_vault_t;

struct folden_vault_registry {
  folden_named_vault_t *entries;
  size_t count;
  size_t cap;
};

folden_vault_registry_t *folden_vault_registry_create(void) {
  return calloc(1, sizeof(folden_vault_registry_t));
}

void folden_vault_registry_destroy(folden_vault_registry_t *reg) {
  if (!reg) {
    return;
  }
  for (size_t i = 0; i < reg->count; i++) {
    if (reg->entries[i].in_use) {
      folden_vault_destroy_impl(reg->entries[i].vault);
    }
  }
  free(reg->entries);
  free(reg);
}

static folden_named_vault_t *find_entry(folden_vault_registry_t *reg, const char *name) {
  for (size_t i = 0; i < reg->count; i++) {
    if (reg->entries[i].in_use && strncmp(reg->entries[i].name, name, FOLDEN_VAULT_NAME_MAX) == 0) {
      return &reg->entries[i];
    }
  }
  return NULL;
}

static folden_named_vault_t *find_or_create_slot(folden_vault_registry_t *reg) {
  for (size_t i = 0; i < reg->count; i++) {
    if (!reg->entries[i].in_use) {
      return &reg->entries[i];
    }
  }
  if (reg->count >= reg->cap) {
    size_t nc = reg->cap ? reg->cap * 2 : 4;
    folden_named_vault_t *ne = realloc(reg->entries, nc * sizeof(*ne));
    if (!ne) {
      return NULL;
    }
    memset(ne + reg->cap, 0, (nc - reg->cap) * sizeof(*ne));
    reg->entries = ne;
    reg->cap = nc;
  }
  return &reg->entries[reg->count++];
}

folden_status_t folden_vault_registry_checkpoint(folden_vault_registry_t *reg,
                                                 const char *name,
                                                 folden_relay_t *relay) {
  if (!reg || !name || !relay) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_named_vault_t *e = find_entry(reg, name);
  if (e) {
    folden_vault_destroy_impl(e->vault);
    e->vault = NULL;
  } else {
    e = find_or_create_slot(reg);
    if (!e) {
      return FOLDEN_ERR_NOMEM;
    }
    strncpy(e->name, name, FOLDEN_VAULT_NAME_MAX);
    e->name[FOLDEN_VAULT_NAME_MAX] = '\0';
    e->in_use = 1;
  }

  e->vault = folden_vault_create_impl();
  if (!e->vault) {
    e->in_use = 0;
    return FOLDEN_ERR_NOMEM;
  }
  return folden_vault_checkpoint_impl(e->vault, relay);
}

folden_status_t folden_vault_registry_restore(folden_vault_registry_t *reg,
                                              const char *name,
                                              folden_relay_t *relay) {
  if (!reg || !name || !relay) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_named_vault_t *e = find_entry(reg, name);
  if (!e) {
    return FOLDEN_ERR_BAD_STATE;
  }
  return folden_vault_restore_impl(e->vault, relay);
}

folden_status_t folden_vault_registry_remove(folden_vault_registry_t *reg,
                                             const char *name) {
  if (!reg || !name) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_named_vault_t *e = find_entry(reg, name);
  if (!e) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_vault_destroy_impl(e->vault);
  e->vault = NULL;
  e->in_use = 0;
  return FOLDEN_OK;
}

size_t folden_vault_registry_count(const folden_vault_registry_t *reg) {
  if (!reg) {
    return 0;
  }
  size_t n = 0;
  for (size_t i = 0; i < reg->count; i++) {
    if (reg->entries[i].in_use) {
      n++;
    }
  }
  return n;
}

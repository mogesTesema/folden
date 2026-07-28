#include "snapshot_codec.h"

#include "slab_arena.h"
#include "defer_free.h"

#include <stdlib.h>
#include <string.h>

/* Releases the arena borrows this vault's current snapshot holds. Must be
 * called before the snapshot array is discarded (by a fresh checkpoint or
 * by vault destruction), otherwise a lane released down to refcount 0 after
 * being snapshotted would free memory the snapshot still points at. */
void folden_snapshot_codec_release(folden_vault_t *vault) {
  if (!vault || !vault->snapshot) {
    return;
  }
  folden_slab_arena_t *arena = folden_arena_get_global();
  for (size_t i = 0; i < vault->count; i++) {
    if (vault->snapshot[i].arena_token) {
      folden_arena_release_borrow(arena, vault->snapshot[i].arena_token);
    }
  }
}

folden_status_t folden_snapshot_codec_capture(folden_vault_t *vault,
                                              folden_relay_t *relay) {
  if (!vault || !relay) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_snapshot_codec_release(vault);
  free(vault->snapshot);
  vault->snapshot = calloc(relay->count, sizeof(folden_relay_entry_t));
  if (!vault->snapshot && relay->count > 0) {
    return FOLDEN_ERR_NOMEM;
  }
  vault->count = relay->count;
  for (size_t i = 0; i < relay->count; i++) {
    vault->snapshot[i].lane_id = relay->entries[i].lane_id;
    vault->snapshot[i].slab = relay->entries[i].slab;
    vault->snapshot[i].slab_len = relay->entries[i].slab_len;
    vault->snapshot[i].refcount = relay->entries[i].refcount;
    vault->snapshot[i].generation = relay->entries[i].generation;
    vault->snapshot[i].arena_token = relay->entries[i].arena_token;
    if (relay->entries[i].arena_token) {
      folden_slab_arena_t *arena = folden_arena_get_global();
      uint8_t *ap = folden_arena_ptr(arena, relay->entries[i].arena_token);
      if (ap) {
        vault->snapshot[i].slab = ap;
      }
    }
    if (relay->entries[i].cached_tags && relay->entries[i].cached_tag_count > 0) {
      vault->snapshot[i].cached_tags =
          malloc(relay->entries[i].cached_tag_count * sizeof(folden_tag_entry_t));
      if (vault->snapshot[i].cached_tags) {
        memcpy(vault->snapshot[i].cached_tags, relay->entries[i].cached_tags,
               relay->entries[i].cached_tag_count * sizeof(folden_tag_entry_t));
        vault->snapshot[i].cached_tag_count = relay->entries[i].cached_tag_count;
      }
    }
    if (relay->entries[i].cached_meta && relay->entries[i].cached_meta_len > 0) {
      vault->snapshot[i].cached_meta = malloc(relay->entries[i].cached_meta_len);
      if (vault->snapshot[i].cached_meta) {
        memcpy(vault->snapshot[i].cached_meta, relay->entries[i].cached_meta,
               relay->entries[i].cached_meta_len);
        vault->snapshot[i].cached_meta_len = relay->entries[i].cached_meta_len;
      }
    }
  }
  return FOLDEN_OK;
}

folden_status_t folden_snapshot_codec_restore(folden_vault_t *vault,
                                              folden_relay_t *relay) {
  if (!vault || !relay || !vault->snapshot) {
    return FOLDEN_ERR_BAD_STATE;
  }
  
  // Backup old entries in case we fail during restore
  folden_relay_entry_t *old_entries = relay->entries;

  folden_slab_arena_t *arena = folden_arena_get_global();
  for (size_t i = 0; i < relay->count; i++) {
    // clean up old tag/meta pointers before we overwrite the entry array
    if (relay->defer) {
      if (relay->entries[i].cached_tags) {
         folden_defer_enqueue_ptr(relay->defer, relay->entries[i].cached_tags);
      }
      if (relay->entries[i].cached_meta) {
         folden_defer_enqueue_ptr(relay->defer, relay->entries[i].cached_meta);
      }
    } else {
      free(relay->entries[i].cached_tags);
      free(relay->entries[i].cached_meta);
    }
    // The pre-restore registry is being replaced wholesale; release each
    // entry's backing slab the same way folden_relay_destroy_impl would,
    // rather than silently dropping the pointer/token.
    if (relay->entries[i].arena_token) {
      folden_arena_free_token(arena, relay->entries[i].arena_token);
    } else {
      free(relay->entries[i].slab);
    }
  }

  relay->entries = calloc(vault->count, sizeof(folden_relay_entry_t));
  if (!relay->entries && vault->count > 0) {
    // Allocation failed, restore previous entry set.
    relay->entries = old_entries;
    return FOLDEN_ERR_NOMEM;
  }
  
  // Free the old array itself now that we successfully allocated the new one
  free(old_entries);

  relay->count = vault->count;
  relay->cap = vault->count;
  for (size_t i = 0; i < vault->count; i++) {
    relay->entries[i].lane_id = vault->snapshot[i].lane_id;
    relay->entries[i].slab = vault->snapshot[i].slab;
    relay->entries[i].slab_len = vault->snapshot[i].slab_len;
    relay->entries[i].refcount = vault->snapshot[i].refcount;
    relay->entries[i].generation = vault->snapshot[i].generation;
    relay->entries[i].arena_token = vault->snapshot[i].arena_token;
    relay->entries[i].cached_tags = vault->snapshot[i].cached_tags;
    relay->entries[i].cached_tag_count = vault->snapshot[i].cached_tag_count;
    relay->entries[i].cached_meta = vault->snapshot[i].cached_meta;
    relay->entries[i].cached_meta_len = vault->snapshot[i].cached_meta_len;
    if (relay->entries[i].slab && relay->entries[i].slab_len > 0) {
      volatile uint8_t probe = relay->entries[i].slab[0];
      (void)probe;
    }
    vault->snapshot[i].cached_tags = NULL;
    vault->snapshot[i].cached_meta = NULL;
  }
  return FOLDEN_OK;
}

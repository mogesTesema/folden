#include "capsule_emit.h"

#include "capsule_cache.h"
#include "crc32.h"
#include "defer_free.h"
#include "metrics.h"
#include "relay_registry.h"
#include "route_cache.h"
#include "slab_arena.h"
#include "varfold.h"

#include <stdlib.h>
#include <string.h>

static size_t append_varfold(uint8_t *buf, size_t cap, size_t off, uint64_t v) {
  uint8_t tmp[10];
  size_t n = folden_varfold_encode(v, tmp, sizeof(tmp));
  if (off + n > cap) {
    return off;
  }
  memcpy(buf + off, tmp, n);
  return off + n;
}

folden_status_t folden_capsule_emit_impl(const folden_capsule_t *capsule,
                                         folden_relay_t *relay,
                                         uint8_t **out, size_t *out_len) {
  if (!capsule || !out || !out_len) {
    return FOLDEN_ERR_BAD_STATE;
  }

  size_t cap = 4096;
  uint8_t *buf = malloc(cap);
  if (!buf) {
    return FOLDEN_ERR_NOMEM;
  }
  size_t off = 0;
  buf[off++] = FOLDEN_MAGIC_0;
  buf[off++] = FOLDEN_MAGIC_1;
  buf[off++] = FOLDEN_MAGIC_2;
  buf[off++] = FOLDEN_MAGIC_3;
  buf[off++] = 1;
  off = append_varfold(buf, cap, off, capsule->lane_count);

  int any_vault = 0;
  for (size_t i = 0; i < capsule->lane_count; i++) {
    const folden_lane_t *lane = &capsule->lanes[i];
    folden_relay_entry_t *rent =
        relay ? folden_relay_find(relay, lane->lane_id) : NULL;

    if (rent && rent->cached_meta && rent->cached_meta_len > 0) {
      size_t touch = rent->cached_meta_len < 32 ? rent->cached_meta_len : 32;
      volatile uint8_t sink = 0;
      for (size_t m = 0; m < touch; m++) {
        sink ^= rent->cached_meta[m];
      }
      (void)sink;
    }

    off = append_varfold(buf, cap, off, lane->lane_id);
    off = append_varfold(buf, cap, off, lane->tag_count);
    for (size_t t = 0; t < lane->tag_count; t++) {
      off = append_varfold(buf, cap, off, lane->tags[t].tag_id);
      off = append_varfold(buf, cap, off, lane->tags[t].fold_depth);
      off = append_varfold(buf, cap, off, lane->tags[t].span);
      if (lane->tags[t].tag_id == FOLDEN_TAG_VAULT_GEN) {
        any_vault = 1;
      }
    }

    const uint8_t *slab_data = lane->slab;
    size_t slab_len = lane->slab_len;
    size_t cache_len = 0;
    const uint8_t *cached_slab = NULL;
    if (relay && relay->route_cache && lane->has_vault_gen) {
      cached_slab = folden_route_cache_lookup(relay->route_cache, lane->lane_id,
                                              lane->vault_gen, &cache_len);
    }
    if (!cached_slab && relay && relay->cache && lane->has_vault_gen) {
      cached_slab = folden_cache_lookup_emit_slab(relay->cache, lane->lane_id,
                                                  lane->vault_gen, &cache_len);
    }
    if (cached_slab) {
      slab_data = cached_slab;
      slab_len = cache_len;
    } else if (rent && rent->slab) {
      slab_data = rent->slab;
      slab_len = rent->slab_len;
    }

    if (rent && rent->slab && lane->has_vault_gen &&
        rent->generation != lane->vault_gen) {
      if (rent->arena_token && relay && relay->defer) {
        folden_defer_enqueue_arena_token(relay->defer, rent->arena_token);
      } else {
        free(rent->slab);
      }
      rent->slab = NULL;
      rent->slab_len = 0;
      rent->arena_token = 0;
    }

    off = append_varfold(buf, cap, off, slab_len);
    if (off + slab_len > cap) {
      size_t nc = cap + slab_len + 256;
      uint8_t *nb = realloc(buf, nc);
      if (!nb) {
        free(buf);
        return FOLDEN_ERR_NOMEM;
      }
      buf = nb;
      cap = nc;
    }
    if (slab_len > 0 && slab_data) {
      memcpy(buf + off, slab_data, slab_len);
      off += slab_len;
    }
    if (relay) {
      folden_metrics_record_emit(relay->metrics, lane->lane_id, slab_len);
    }
  }

  if (any_vault || capsule->has_vault_trailer) {
    off = append_varfold(buf, cap, off, capsule->vault_gen_trailer);
  }

  if (off + 4 > cap) {
    uint8_t *nb = realloc(buf, cap + 4);
    if (!nb) {
      free(buf);
      return FOLDEN_ERR_NOMEM;
    }
    buf = nb;
    cap += 4;
  }
  uint32_t crc = folden_crc32(buf, off);
  buf[off++] = (uint8_t)(crc & 0xffu);
  buf[off++] = (uint8_t)((crc >> 8) & 0xffu);
  buf[off++] = (uint8_t)((crc >> 16) & 0xffu);
  buf[off++] = (uint8_t)((crc >> 24) & 0xffu);

  *out = buf;
  *out_len = off;
  return FOLDEN_OK;
}

/* Zero-copy emit: for large arena-backed lanes, hand back a pointer straight
 * into the arena block instead of paying for the owned-copy path above. The
 * caller must eventually release the lane (folden_relay_release) to drop the
 * borrow. */
folden_status_t folden_capsule_emit_zero_copy_impl(folden_relay_t *relay,
                                                   uint32_t lane_id,
                                                   const uint8_t **out_ptr,
                                                   size_t *out_len) {
  if (!relay || !out_ptr || !out_len) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_relay_entry_t *rent = folden_relay_find(relay, lane_id);
  if (!rent || !rent->arena_token) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_slab_arena_t *arena = folden_arena_get_global();
  uint8_t *ptr = folden_arena_ptr(arena, rent->arena_token);
  if (!ptr) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_arena_borrow(arena, rent->arena_token);
  rent->arena_borrows++;
  *out_ptr = ptr;
  *out_len = folden_arena_len(arena, rent->arena_token);
  return FOLDEN_OK;
}

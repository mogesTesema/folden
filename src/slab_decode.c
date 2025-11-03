#include "slab_decode.h"

#include "crc32.h"
#include "meta_resolver.h"
#include "varfold.h"
#include "wire_validator.h"

#include <stdlib.h>
#include <string.h>

/* Lanes whose slab begins with the LZFOLD magic carry a raw (uncompressed
 * passthrough) payload behind a 4-byte tag rather than the compressed bytes
 * themselves. Strip the tag so callers always see the logical payload. */
#define LZFOLD_MAGIC 0x4C5A4644u

static folden_status_t unpack_lane_if_tagged(folden_lane_t *lane) {
  if (!lane->slab || lane->slab_len < 4) {
    return FOLDEN_OK;
  }
  uint32_t magic = (uint32_t)lane->slab[0] | ((uint32_t)lane->slab[1] << 8) |
                   ((uint32_t)lane->slab[2] << 16) | ((uint32_t)lane->slab[3] << 24);
  if (magic != LZFOLD_MAGIC) {
    return FOLDEN_OK;
  }
  size_t new_len = lane->slab_len - 4;
  uint8_t *out = malloc(new_len > 0 ? new_len : 1);
  if (!out) {
    return FOLDEN_ERR_NOMEM;
  }
  memcpy(out, lane->slab + 4, new_len);
  free(lane->slab);
  lane->slab = out;
  lane->slab_len = new_len;
  lane->slab_compressed = 1;
  return FOLDEN_OK;
}

static folden_status_t parse_lane(const uint8_t *data, size_t len, size_t *off,
                                  folden_lane_t *lane) {
  uint64_t lane_id = 0;
  size_t used = 0;
  if (folden_varfold_decode(data + *off, len - *off, &lane_id, &used) != 0) {
    return FOLDEN_ERR_TRUNCATED;
  }
  *off += used;
  lane->lane_id = (uint32_t)lane_id;
  lane->fold_budget = 1;

  uint64_t tag_count = 0;
  if (folden_varfold_decode(data + *off, len - *off, &tag_count, &used) != 0) {
    return FOLDEN_ERR_TRUNCATED;
  }
  *off += used;
  /* Every tag costs at least 1 byte on the wire, so a genuine tag_count can
   * never exceed the bytes actually left in the buffer. Reject the rest
   * up front instead of handing an attacker-controlled 64-bit count
   * straight to calloc. */
  if (tag_count > len - *off) {
    return FOLDEN_ERR_TRUNCATED;
  }

  lane->tag_count = (size_t)tag_count;
  lane->tags = calloc(lane->tag_count, sizeof(folden_tag_entry_t));
  if (!lane->tags && lane->tag_count > 0) {
    return FOLDEN_ERR_NOMEM;
  }

  for (size_t t = 0; t < lane->tag_count; t++) {
    uint64_t tag_id = 0, fold_depth = 0, span = 0;
    if (folden_varfold_decode(data + *off, len - *off, &tag_id, &used) != 0) {
      return FOLDEN_ERR_TRUNCATED;
    }
    *off += used;
    if (folden_varfold_decode(data + *off, len - *off, &fold_depth, &used) != 0) {
      return FOLDEN_ERR_TRUNCATED;
    }
    *off += used;
    if (folden_varfold_decode(data + *off, len - *off, &span, &used) != 0) {
      return FOLDEN_ERR_TRUNCATED;
    }
    *off += used;
    lane->tags[t].tag_id = (uint32_t)tag_id;
    lane->tags[t].fold_depth = (uint32_t)fold_depth;
    lane->tags[t].span = (uint32_t)span;
    if (tag_id == FOLDEN_TAG_FOLD_EXTEND) {
      lane->fold_budget += (uint32_t)span;
    }
    if (tag_id == FOLDEN_TAG_VAULT_GEN) {
      lane->vault_gen = (uint32_t)span;
      lane->has_vault_gen = 1;
    }
  }

  uint64_t slab_len = 0;
  if (folden_varfold_decode(data + *off, len - *off, &slab_len, &used) != 0) {
    return FOLDEN_ERR_TRUNCATED;
  }
  *off += used;
  if (slab_len > len - *off) {
    return FOLDEN_ERR_TRUNCATED;
  }
  lane->slab_len = (size_t)slab_len;
  if (slab_len > 0) {
    lane->slab = malloc(lane->slab_len);
    if (!lane->slab) {
      return FOLDEN_ERR_NOMEM;
    }
    memcpy(lane->slab, data + *off, lane->slab_len);
    *off += lane->slab_len;
    (void)unpack_lane_if_tagged(lane);
  }
  return FOLDEN_OK;
}

static void free_lane(folden_lane_t *lane) {
  free(lane->tags);
  free(lane->slab);
  free(lane->tag_meta);
  memset(lane, 0, sizeof(*lane));
}

void folden_capsule_free(folden_capsule_t *capsule) {
  if (!capsule) {
    return;
  }
  for (size_t i = 0; i < capsule->lane_count; i++) {
    free_lane(&capsule->lanes[i]);
  }
  free(capsule->lanes);
  free(capsule->wire_copy);
  free(capsule);
}

folden_status_t folden_parse_capsule(const uint8_t *data, size_t len,
                                     folden_capsule_t **out) {
  return folden_parse_capsule_ex(data, len, out, NULL);
}

/* Same as folden_parse_capsule, but on success also reports how many bytes
 * of `data` the capsule actually occupied (through the trailing CRC32) --
 * trailing bytes past that point are not inspected, so a caller replaying a
 * buffer that packs several capsules back-to-back can advance by exactly
 * that much and decode the next one. */
folden_status_t folden_parse_capsule_ex(const uint8_t *data, size_t len,
                                        folden_capsule_t **out, size_t *out_consumed) {
  if (!data || len < 9 || !out) {
    return FOLDEN_ERR_TRUNCATED;
  }
  if (data[0] != FOLDEN_MAGIC_0 || data[1] != FOLDEN_MAGIC_1 ||
      data[2] != FOLDEN_MAGIC_2 || data[3] != FOLDEN_MAGIC_3) {
    return FOLDEN_ERR_BAD_MAGIC;
  }
  if (folden_wire_validate_header(data, len) != 0) {
    return FOLDEN_ERR_BAD_MAGIC;
  }
  if (data[4] != 1) {
    return FOLDEN_ERR_BAD_VERSION;
  }

  size_t off = 5;
  uint64_t lane_count = 0;
  size_t used = 0;
  if (folden_varfold_decode(data + off, len - off, &lane_count, &used) != 0) {
    return FOLDEN_ERR_TRUNCATED;
  }
  off += used;
  /* Every lane costs at least 1 byte on the wire (its lane_id varfold), so
   * a genuine lane_count can never exceed the bytes actually left. */
  if (lane_count > len - off) {
    return FOLDEN_ERR_TRUNCATED;
  }

  folden_capsule_t *cap = calloc(1, sizeof(*cap));
  if (!cap) {
    return FOLDEN_ERR_NOMEM;
  }
  cap->lane_count = (size_t)lane_count;
  cap->lanes = calloc(cap->lane_count, sizeof(folden_lane_t));
  if (!cap->lanes && cap->lane_count > 0) {
    free(cap);
    return FOLDEN_ERR_NOMEM;
  }

  int any_vault = 0;
  for (size_t i = 0; i < cap->lane_count; i++) {
    folden_status_t st = parse_lane(data, len, &off, &cap->lanes[i]);
    if (st != FOLDEN_OK) {
      folden_capsule_free(cap);
      return st;
    }

    if (cap->lanes[i].has_vault_gen) {
      any_vault = 1;
    }

    if (cap->lanes[i].tag_count >= 2 &&
        cap->lanes[i].tags[0].tag_id == FOLDEN_TAG_FOLD_EXTEND) {
      st = folden_meta_resolver_build_partial(&cap->lanes[i], NULL, 1);
    } else {
      st = folden_meta_resolver_build(&cap->lanes[i], NULL);
    }
    if (st != FOLDEN_OK) {
      folden_capsule_free(cap);
      return st;
    }
  }

  if (any_vault) {
    uint64_t vg = 0;
    if (folden_varfold_decode(data + off, len - off, &vg, &used) != 0) {
      folden_capsule_free(cap);
      return FOLDEN_ERR_TRUNCATED;
    }
    off += used;
    cap->vault_gen_trailer = (uint32_t)vg;
    cap->has_vault_trailer = 1;
  }

  if (off + 4 > len) {
    folden_capsule_free(cap);
    return FOLDEN_ERR_TRUNCATED;
  }
  uint32_t stored_crc = (uint32_t)data[off] | ((uint32_t)data[off + 1] << 8) |
                        ((uint32_t)data[off + 2] << 16) |
                        ((uint32_t)data[off + 3] << 24);
  uint32_t calc = folden_crc32(data, off);
  if (stored_crc != calc) {
    folden_capsule_free(cap);
    return FOLDEN_ERR_BAD_CRC;
  }

  size_t consumed = off + 4;
  cap->wire_copy = malloc(consumed);
  if (cap->wire_copy) {
    memcpy(cap->wire_copy, data, consumed);
    cap->wire_len = consumed;
  }
  if (out_consumed) {
    *out_consumed = consumed;
  }
  *out = cap;
  return FOLDEN_OK;
}

folden_status_t folden_parse_capsule_partial(folden_stream_t *stream,
                                             const uint8_t *data, size_t len,
                                             folden_capsule_t **out) {
  if (!stream || !out) {
    return FOLDEN_ERR_BAD_STATE;
  }
  size_t need = stream->len + len;
  if (need > stream->cap) {
    size_t new_cap = stream->cap ? stream->cap * 2 : 256;
    while (new_cap < need) {
      new_cap *= 2;
    }
    uint8_t *nb = realloc(stream->buf, new_cap);
    if (!nb) {
      return FOLDEN_ERR_NOMEM;
    }
    stream->buf = nb;
    stream->cap = new_cap;
  }
  memcpy(stream->buf + stream->len, data, len);
  stream->len += len;

  folden_status_t st = folden_parse_capsule(stream->buf, stream->len, out);
  if (st == FOLDEN_OK) {
    stream->len = 0;
    stream->consumed = 0;
    stream->header_committed = 0;
    stream->partial_lane = 0;
    stream->partial_fold_budget = 0;
    return FOLDEN_OK;
  }
  if (st == FOLDEN_ERR_TRUNCATED && stream->len >= 5) {
    stream->header_committed = 1;
    if (stream->len > 8) {
      stream->partial_lane = 1;
    }
  }
  return st;
}

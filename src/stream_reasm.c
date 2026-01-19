#include "stream_reasm.h"

#include "meta_resolver.h"
#include "slab_decode.h"
#include "varfold.h"
#include "wire_validator.h"

#include <stdlib.h>
#include <string.h>

folden_stream_t *folden_stream_create_impl(void) {
  folden_stream_t *s = calloc(1, sizeof(*s));
  if (s) {
    s->partial_fold_budget = 1;
  }
  return s;
}

void folden_stream_destroy_impl(folden_stream_t *stream) {
  if (!stream) {
    return;
  }
  free(stream->buf);
  free(stream);
}

static int try_parse_partial_lane(const uint8_t *data, size_t len,
                                  folden_lane_t *lane, uint32_t *budget_out) {
  if (len < 6) {
    return -1;
  }
  size_t off = 5;
  uint64_t lane_count = 0;
  size_t used = 0;
  if (folden_varfold_decode(data + off, len - off, &lane_count, &used) != 0) {
    return -1;
  }
  off += used;
  if (lane_count == 0) {
    return -1;
  }

  uint64_t lane_id = 0;
  if (folden_varfold_decode(data + off, len - off, &lane_id, &used) != 0) {
    return -1;
  }
  off += used;
  lane->lane_id = (uint32_t)lane_id;
  lane->fold_budget = 1;

  uint64_t tag_count = 0;
  if (folden_varfold_decode(data + off, len - off, &tag_count, &used) != 0) {
    *budget_out = lane->fold_budget;
    return 0;
  }
  off += used;
  if (tag_count > len - off) {
    *budget_out = lane->fold_budget;
    return 0;
  }
  lane->tag_count = (size_t)tag_count;
  lane->tags = calloc(lane->tag_count, sizeof(folden_tag_entry_t));
  if (!lane->tags && lane->tag_count > 0) {
    return -1;
  }

  for (size_t t = 0; t < lane->tag_count; t++) {
    uint64_t tag_id = 0, fold_depth = 0, span = 0;
    if (folden_varfold_decode(data + off, len - off, &tag_id, &used) != 0) {
      break;
    }
    off += used;
    if (folden_varfold_decode(data + off, len - off, &fold_depth, &used) != 0) {
      break;
    }
    off += used;
    if (folden_varfold_decode(data + off, len - off, &span, &used) != 0) {
      break;
    }
    off += used;
    lane->tags[t].tag_id = (uint32_t)tag_id;
    lane->tags[t].fold_depth = (uint32_t)fold_depth;
    lane->tags[t].span = (uint32_t)span;
    if (tag_id == FOLDEN_TAG_FOLD_EXTEND) {
      lane->fold_budget += (uint32_t)span;
    }
  }
  *budget_out = lane->fold_budget;
  return 0;
}

folden_status_t folden_stream_materialize_partial(folden_stream_t *stream,
                                                  folden_lane_t *lane_out) {
  if (!stream || !lane_out || stream->len == 0) {
    return FOLDEN_ERR_BAD_STATE;
  }
  memset(lane_out, 0, sizeof(*lane_out));
  uint32_t parsed_budget = 1;
  if (try_parse_partial_lane(stream->buf, stream->len, lane_out,
                             &parsed_budget) != 0) {
    return FOLDEN_ERR_TRUNCATED;
  }
  stream->partial_fold_budget = parsed_budget;
  lane_out->fold_budget = stream->partial_fold_budget;
  stream->stream_resume = 1;
  (void)folden_wire_validate_lane_tags(lane_out, 1);
  if (stream->header_committed && stream->partial_lane) {
    return folden_meta_resolver_build_partial(lane_out, NULL, 1);
  }
  return folden_meta_resolver_build_partial(lane_out, NULL, 0);
}

folden_status_t folden_stream_feed(folden_stream_t *stream, const uint8_t *chunk,
                                   size_t chunk_len, folden_capsule_t **out) {
  if (!stream || !out) {
    return FOLDEN_ERR_BAD_STATE;
  }
  *out = NULL;

  folden_status_t st =
      folden_parse_capsule_partial(stream, chunk, chunk_len, out);
  if (st == FOLDEN_OK) {
    return FOLDEN_OK;
  }

  if (st == FOLDEN_ERR_TRUNCATED && stream->len > 0) {
    folden_lane_t partial;
    memset(&partial, 0, sizeof(partial));
    folden_stream_materialize_partial(stream, &partial);
    free(partial.tags);
    free(partial.tag_meta);
  }
  return st;
}

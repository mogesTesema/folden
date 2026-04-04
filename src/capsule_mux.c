#include "capsule_mux.h"
#include "crc32.h"

#include <stdlib.h>
#include <string.h>

/* ---- little-endian helpers ---- */
static void write_le32(uint8_t *dst, uint32_t val) {
  dst[0] = (uint8_t)(val);
  dst[1] = (uint8_t)(val >> 8);
  dst[2] = (uint8_t)(val >> 16);
  dst[3] = (uint8_t)(val >> 24);
}

static uint32_t read_le32(const uint8_t *src) {
  return (uint32_t)src[0]
       | ((uint32_t)src[1] << 8)
       | ((uint32_t)src[2] << 16)
       | ((uint32_t)src[3] << 24);
}

/* ---- pack ---- */

folden_status_t folden_mux_pack(const uint8_t **payloads,
                                const size_t *lengths,
                                size_t count,
                                folden_mux_frame_t *out) {
  if (!out) return FOLDEN_ERR_BAD_STATE;
  out->buf = NULL;
  out->len = 0;
  out->cap = 0;

  if (count == 0 || !payloads || !lengths) return FOLDEN_OK;

  // compute total size
  size_t header_sz = 4 + 4 + 4 + 4 * count; // magic + crc + count + lengths
  size_t payload_total = 0;
  for (size_t i = 0; i < count; i++) {
    payload_total += lengths[i];
  }

  size_t total = header_sz + payload_total;
  uint8_t *buf = malloc(total);
  if (!buf) return FOLDEN_ERR_NOMEM;

  // magic
  buf[0] = FOLDEN_MUX_MAGIC_0;
  buf[1] = FOLDEN_MUX_MAGIC_1;
  buf[2] = FOLDEN_MUX_MAGIC_2;
  buf[3] = FOLDEN_MUX_MAGIC_3;

  // skip crc slot (bytes 4-7), fill later
  write_le32(buf + 8, (uint32_t)count);

  // length table
  for (size_t i = 0; i < count; i++) {
    write_le32(buf + 12 + i * 4, (uint32_t)lengths[i]);
  }

  // payloads
  size_t cursor = header_sz;
  for (size_t i = 0; i < count; i++) {
    if (lengths[i] > 0 && payloads[i]) {
      memcpy(buf + cursor, payloads[i], lengths[i]);
    }
    cursor += lengths[i];
  }

  // crc over everything after the crc field itself
  uint32_t crc = folden_crc32(buf + 8, total - 8);
  write_le32(buf + 4, crc);

  out->buf = buf;
  out->len = total;
  out->cap = total;
  return FOLDEN_OK;
}

folden_status_t folden_mux_unpack(const uint8_t *frame, size_t frame_len,
                                  folden_mux_cb cb, void *user) {
  if (!frame || frame_len < FOLDEN_MUX_HEADER_MIN) {
    return FOLDEN_ERR_TRUNCATED;
  }

  // check magic
  if (frame[0] != FOLDEN_MUX_MAGIC_0 || frame[1] != FOLDEN_MUX_MAGIC_1 ||
      frame[2] != FOLDEN_MUX_MAGIC_2 || frame[3] != FOLDEN_MUX_MAGIC_3) {
    return FOLDEN_ERR_BAD_MAGIC;
  }

  uint32_t stored_crc = read_le32(frame + 4);
  uint32_t computed_crc = folden_crc32(frame + 8, frame_len - 8);
  if (stored_crc != computed_crc) {
    return FOLDEN_ERR_CHECKSUM;
  }

  uint32_t count = read_le32(frame + 8);
  size_t header_sz = 12 + 4 * (size_t)count;
  if (header_sz > frame_len) return FOLDEN_ERR_TRUNCATED;

  // read length table and validate
  size_t payload_total = 0;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t l = read_le32(frame + 12 + i * 4);
    payload_total += l;
  }
  if (header_sz + payload_total > frame_len) return FOLDEN_ERR_TRUNCATED;

  // dispatch each capsule
  size_t cursor = header_sz;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t l = read_le32(frame + 12 + i * 4);
    if (cb) cb(frame + cursor, l, user);
    cursor += l;
  }

  return FOLDEN_OK;
}

/* ---- streaming demux ---- */

folden_mux_stream_t *folden_mux_stream_create(void) {
  folden_mux_stream_t *ms = calloc(1, sizeof(*ms));
  if (ms) ms->state = FOLDEN_MUX_WANT_HEADER;
  return ms;
}

void folden_mux_stream_destroy(folden_mux_stream_t *ms) {
  if (!ms) return;
  free(ms->lengths);
  free(ms->payload_buf);
  free(ms);
}

void folden_mux_stream_reset(folden_mux_stream_t *ms) {
  if (!ms) return;
  free(ms->lengths);
  free(ms->payload_buf);
  ms->lengths = NULL;
  ms->payload_buf = NULL;
  ms->payload_len = 0;
  ms->payload_cap = 0;
  ms->capsule_count = 0;
  ms->current_index = 0;
  ms->expected_total = 0;
  ms->recvd = 0;
  ms->header_recvd = 0;
  ms->state = FOLDEN_MUX_WANT_HEADER;
}

size_t folden_mux_stream_feed(folden_mux_stream_t *ms,
                              const uint8_t *data, size_t data_len,
                              folden_mux_cb cb, void *user) {
  if (!ms || !data || data_len == 0) return 0;
  if (ms->state == FOLDEN_MUX_DONE || ms->state == FOLDEN_MUX_ERROR) return 0;

  size_t consumed = 0;

  // phase 1: accumulate the fixed header (12 bytes)
  if (ms->state == FOLDEN_MUX_WANT_HEADER) {
    size_t need = FOLDEN_MUX_HEADER_MIN - ms->header_recvd;
    size_t take = need < data_len ? need : data_len;
    memcpy(ms->header_buf + ms->header_recvd, data, take);
    ms->header_recvd += take;
    consumed += take;

    if (ms->header_recvd < FOLDEN_MUX_HEADER_MIN) return consumed;

    // validate magic
    if (ms->header_buf[0] != FOLDEN_MUX_MAGIC_0 ||
        ms->header_buf[1] != FOLDEN_MUX_MAGIC_1 ||
        ms->header_buf[2] != FOLDEN_MUX_MAGIC_2 ||
        ms->header_buf[3] != FOLDEN_MUX_MAGIC_3) {
      ms->state = FOLDEN_MUX_ERROR;
      return consumed;
    }

    ms->header_crc = read_le32(ms->header_buf + 4);
    ms->capsule_count = read_le32(ms->header_buf + 8);

    if (ms->capsule_count == 0) {
      ms->state = FOLDEN_MUX_DONE;
      return consumed;
    }

    ms->lengths = calloc(ms->capsule_count, sizeof(uint32_t));
    if (!ms->lengths) {
      ms->state = FOLDEN_MUX_ERROR;
      return consumed;
    }
    ms->recvd = 0;
    ms->state = FOLDEN_MUX_WANT_LENGTHS;
  }

  // phase 2: length table
  if (ms->state == FOLDEN_MUX_WANT_LENGTHS) {
    size_t table_sz = (size_t)ms->capsule_count * 4;
    // we use payload_buf temporarily to accumulate the length table
    if (!ms->payload_buf) {
      ms->payload_buf = malloc(table_sz);
      if (!ms->payload_buf) { ms->state = FOLDEN_MUX_ERROR; return consumed; }
      ms->payload_cap = table_sz;
      ms->payload_len = 0;
    }

    size_t need = table_sz - ms->payload_len;
    size_t avail = data_len - consumed;
    size_t take = need < avail ? need : avail;
    memcpy(ms->payload_buf + ms->payload_len, data + consumed, take);
    ms->payload_len += take;
    consumed += take;

    if (ms->payload_len < table_sz) return consumed;

    // parse lengths
    ms->expected_total = 0;
    for (uint32_t i = 0; i < ms->capsule_count; i++) {
      ms->lengths[i] = read_le32(ms->payload_buf + i * 4);
      ms->expected_total += ms->lengths[i];
    }

    // reallocate payload_buf for the actual payloads
    free(ms->payload_buf);
    ms->payload_buf = NULL;
    ms->payload_len = 0;
    ms->payload_cap = 0;

    if (ms->expected_total > 0) {
      ms->payload_buf = malloc(ms->expected_total);
      if (!ms->payload_buf) { ms->state = FOLDEN_MUX_ERROR; return consumed; }
      ms->payload_cap = ms->expected_total;
    }

    ms->current_index = 0;
    ms->state = FOLDEN_MUX_WANT_PAYLOAD;
  }

  // phase 3: payload data
  if (ms->state == FOLDEN_MUX_WANT_PAYLOAD) {
    size_t need = ms->expected_total - ms->payload_len;
    size_t avail = data_len - consumed;
    size_t take = need < avail ? need : avail;
    if (take > 0 && ms->payload_buf) {
      memcpy(ms->payload_buf + ms->payload_len, data + consumed, take);
      ms->payload_len += take;
      consumed += take;
    }

    if (ms->payload_len >= ms->expected_total) {
      // dispatch all capsules
      size_t off = 0;
      for (uint32_t i = 0; i < ms->capsule_count; i++) {
        if (cb && ms->lengths[i] > 0) {
          cb(ms->payload_buf + off, ms->lengths[i], user);
        }
        off += ms->lengths[i];
      }
      ms->state = FOLDEN_MUX_DONE;
    }
  }

  return consumed;
}

folden_status_t folden_mux_pack_entries(const folden_relay_entry_t *entries,
                                        size_t count,
                                        folden_mux_frame_t *out) {
  if (!entries || count == 0 || !out) return FOLDEN_ERR_BAD_STATE;

  const uint8_t **ptrs = malloc(count * sizeof(*ptrs));
  size_t *lens = malloc(count * sizeof(*lens));
  if (!ptrs || !lens) {
    free(ptrs);
    free(lens);
    return FOLDEN_ERR_NOMEM;
  }

  for (size_t i = 0; i < count; i++) {
    ptrs[i] = entries[i].slab;
    lens[i] = entries[i].slab_len;
  }

  folden_status_t st = folden_mux_pack(ptrs, lens, count, out);
  free(ptrs);
  free(lens);
  return st;
}

void folden_mux_frame_free(folden_mux_frame_t *frame) {
  if (!frame) return;
  free(frame->buf);
  frame->buf = NULL;
  frame->len = 0;
  frame->cap = 0;
}

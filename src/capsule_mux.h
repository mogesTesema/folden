#ifndef FOLDEN_CAPSULE_MUX_H
#define FOLDEN_CAPSULE_MUX_H

#include "folden_internal.h"

/*
 * Capsule multiplexer -- packs multiple capsules into a single
 * wire frame for batch I/O, and demuxes them back out.
 *
 * Frame layout:
 *   [4B magic: "FMUX"]
 *   [4B frame_crc32]      -- over the rest of the header + all payloads
 *   [4B capsule_count]    -- LE
 *   [4B * capsule_count]  -- per-capsule length table (LE)
 *   [payload_0]
 *   [payload_1]
 *   ...
 *
 * The framing is designed for batch writes over a pipe or socket.
 * Each payload is a complete capsule wire encoding.
 */

#define FOLDEN_MUX_MAGIC_0 'F'
#define FOLDEN_MUX_MAGIC_1 'M'
#define FOLDEN_MUX_MAGIC_2 'U'
#define FOLDEN_MUX_MAGIC_3 'X'

#define FOLDEN_MUX_HEADER_MIN 12   // magic + crc + count

typedef struct {
  uint8_t *buf;
  size_t   len;
  size_t   cap;
} folden_mux_frame_t;

/* Streaming demux state -- for incremental parsing from a pipe. */
typedef enum {
  FOLDEN_MUX_WANT_HEADER,
  FOLDEN_MUX_WANT_LENGTHS,
  FOLDEN_MUX_WANT_PAYLOAD,
  FOLDEN_MUX_DONE,
  FOLDEN_MUX_ERROR
} folden_mux_parse_state_t;

typedef struct {
  folden_mux_parse_state_t state;
  uint32_t  capsule_count;
  uint32_t *lengths;       // per-capsule length array (owned)
  uint32_t  current_index;
  uint32_t  header_crc;
  uint8_t  *payload_buf;   // reassembly buffer (owned)
  size_t    payload_len;
  size_t    payload_cap;
  size_t    expected_total; // sum of all lengths
  size_t    recvd;          // bytes received so far for current phase
  uint8_t   header_buf[FOLDEN_MUX_HEADER_MIN]; // partial header
  size_t    header_recvd;
} folden_mux_stream_t;

/* Pack N capsule wire buffers into a single mux frame. */
folden_status_t folden_mux_pack(const uint8_t **payloads,
                                const size_t *lengths,
                                size_t count,
                                folden_mux_frame_t *out);

/* Unpack a complete mux frame. Calls `cb` for each capsule.
 * cb receives the capsule wire bytes and length. */
typedef void (*folden_mux_cb)(const uint8_t *wire, size_t wire_len,
                              void *user);
folden_status_t folden_mux_unpack(const uint8_t *frame, size_t frame_len,
                                  folden_mux_cb cb, void *user);

/* Streaming demux */
folden_mux_stream_t *folden_mux_stream_create(void);
void folden_mux_stream_destroy(folden_mux_stream_t *ms);
void folden_mux_stream_reset(folden_mux_stream_t *ms);

/* Feed bytes into the streaming demux. Returns number of bytes consumed.
 * When a complete capsule is demuxed, calls cb. */
size_t folden_mux_stream_feed(folden_mux_stream_t *ms,
                              const uint8_t *data, size_t data_len,
                              folden_mux_cb cb, void *user);

/* Convenience: pack relay entries directly (zero-copy scatter). */
folden_status_t folden_mux_pack_entries(const folden_relay_entry_t *entries,
                                        size_t count,
                                        folden_mux_frame_t *out);

void folden_mux_frame_free(folden_mux_frame_t *frame);

#endif

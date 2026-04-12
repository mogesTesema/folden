#ifndef FOLDEN_JOURNAL_FILE_H
#define FOLDEN_JOURNAL_FILE_H

#include "folden_internal.h"

/*
 * File-backed journal segments.
 *
 * Extends the in-memory journal with durable storage. Each
 * file segment is a flat binary file that can be mmap'd for
 * read access during recovery. Write path uses pwrite to
 * avoid seeking.
 *
 * Segment naming: <base_dir>/<prefix>_<seq>.fseg
 *
 * On-disk segment header (32 bytes):
 *   [8B] magic: "FSEG0001"
 *   [4B] sequence number (LE)
 *   [4B] entry count (LE)
 *   [4B] data length (LE, not counting header)
 *   [4B] checksum (CRC32 over data region)
 *   [4B] flags (sealed bit, compacted bit)
 *   [4B] reserved
 */

#define FOLDEN_FSEG_MAGIC "FSEG0001"
#define FOLDEN_FSEG_HEADER_SZ 32
#define FOLDEN_FSEG_FLAG_SEALED    0x01
#define FOLDEN_FSEG_FLAG_COMPACTED 0x02

typedef struct {
  int       fd;
  uint32_t  sequence;
  uint32_t  entry_count;
  uint32_t  flags;
  uint8_t  *mapped;        // mmap'd region (NULL if write-mode)
  size_t    mapped_len;
  uint8_t  *write_buf;     // write buffer (NULL if read-mode)
  size_t    write_len;
  size_t    write_cap;
} folden_journal_file_t;

typedef struct {
  char     *base_dir;
  char     *prefix;
  uint32_t  next_seq;
  folden_journal_file_t **segments;
  size_t    seg_count;
  size_t    seg_cap;
  size_t    max_segment_size;  // rotate when segment exceeds this
  int       sync_on_seal;      // fsync when sealing a segment
} folden_journal_store_t;

folden_journal_store_t *folden_journal_store_create(const char *base_dir,
                                                     const char *prefix,
                                                     size_t max_seg_size);
void folden_journal_store_destroy(folden_journal_store_t *store);

/* Append raw capsule wire bytes to the active segment.
 * Rotates to a new segment if the current one is full. */
folden_status_t folden_journal_store_append(folden_journal_store_t *store,
                                             const uint8_t *data,
                                             size_t len);

/* Seal the current segment (marks it read-only). */
folden_status_t folden_journal_store_seal(folden_journal_store_t *store);

/* Scan all sealed segments and invoke cb for each capsule wire blob. */
typedef void (*folden_journal_scan_cb)(const uint8_t *data, size_t len,
                                       uint32_t seg_seq, void *user);
folden_status_t folden_journal_store_scan(folden_journal_store_t *store,
                                           folden_journal_scan_cb cb,
                                           void *user);

/* Compact: merge sealed segments, dropping entries older than
 * the given sequence number. Returns number of segments removed. */
size_t folden_journal_store_compact(folden_journal_store_t *store,
                                    uint32_t min_seq);

/* Get total bytes across all segments. */
size_t folden_journal_store_total_bytes(const folden_journal_store_t *store);

/* Get segment count. */
size_t folden_journal_store_segment_count(const folden_journal_store_t *store);

#endif

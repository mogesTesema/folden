#include "journal_file.h"
#include "crc32.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* we avoid mmap for portability -- use read/write buffering instead.
 * the mmap path is stubbed out in the header for a future optimisation. */

static folden_journal_file_t *jfile_create(uint32_t seq) {
  folden_journal_file_t *jf = calloc(1, sizeof(*jf));
  if (!jf) return NULL;
  jf->fd = -1;
  jf->sequence = seq;
  return jf;
}

static void jfile_close(folden_journal_file_t *jf) {
  if (!jf) return;
  if (jf->fd >= 0) close(jf->fd);
  free(jf->write_buf);
  free(jf->mapped);
  free(jf);
}

static char *seg_path(const char *base_dir, const char *prefix, uint32_t seq) {
  // <base_dir>/<prefix>_<seq>.fseg
  size_t blen = strlen(base_dir);
  size_t plen = strlen(prefix);
  // 12 chars for underscore + seq (up to 10 digits) + .fseg + null
  size_t total = blen + 1 + plen + 1 + 10 + 5 + 1;
  char *path = malloc(total);
  if (!path) return NULL;
  snprintf(path, total, "%s/%s_%u.fseg", base_dir, prefix, seq);
  return path;
}

static int ensure_write_cap(folden_journal_file_t *jf, size_t need) {
  size_t required = jf->write_len + need;
  if (required <= jf->write_cap) return 0;
  size_t nc = jf->write_cap ? jf->write_cap * 2 : 4096;
  while (nc < required) nc *= 2;
  uint8_t *nb = realloc(jf->write_buf, nc);
  if (!nb) return -1;
  jf->write_buf = nb;
  jf->write_cap = nc;
  return 0;
}

/* write the segment header at the beginning of write_buf */
static void write_header(folden_journal_file_t *jf) {
  if (jf->write_len < FOLDEN_FSEG_HEADER_SZ) return;

  uint8_t *h = jf->write_buf;
  memcpy(h, FOLDEN_FSEG_MAGIC, 8);

  // LE writes
  size_t data_len = jf->write_len - FOLDEN_FSEG_HEADER_SZ;
  h[8]  = (uint8_t)(jf->sequence);
  h[9]  = (uint8_t)(jf->sequence >> 8);
  h[10] = (uint8_t)(jf->sequence >> 16);
  h[11] = (uint8_t)(jf->sequence >> 24);

  h[12] = (uint8_t)(jf->entry_count);
  h[13] = (uint8_t)(jf->entry_count >> 8);
  h[14] = (uint8_t)(jf->entry_count >> 16);
  h[15] = (uint8_t)(jf->entry_count >> 24);

  uint32_t dl = (uint32_t)data_len;
  h[16] = (uint8_t)(dl);
  h[17] = (uint8_t)(dl >> 8);
  h[18] = (uint8_t)(dl >> 16);
  h[19] = (uint8_t)(dl >> 24);

  // crc over data region
  uint32_t crc = 0;
  if (data_len > 0) {
    crc = folden_crc32(jf->write_buf + FOLDEN_FSEG_HEADER_SZ, data_len);
  }
  h[20] = (uint8_t)(crc);
  h[21] = (uint8_t)(crc >> 8);
  h[22] = (uint8_t)(crc >> 16);
  h[23] = (uint8_t)(crc >> 24);

  h[24] = (uint8_t)(jf->flags);
  h[25] = (uint8_t)(jf->flags >> 8);
  h[26] = (uint8_t)(jf->flags >> 16);
  h[27] = (uint8_t)(jf->flags >> 24);

  // reserved
  memset(h + 28, 0, 4);
}

static uint32_t read_le32_buf(const uint8_t *p) {
  return (uint32_t)p[0]
       | ((uint32_t)p[1] << 8)
       | ((uint32_t)p[2] << 16)
       | ((uint32_t)p[3] << 24);
}

/* --- store --- */

folden_journal_store_t *folden_journal_store_create(const char *base_dir,
                                                     const char *prefix,
                                                     size_t max_seg_size) {
  if (!base_dir || !prefix) return NULL;
  folden_journal_store_t *s = calloc(1, sizeof(*s));
  if (!s) return NULL;

  s->base_dir = strdup(base_dir);
  s->prefix = strdup(prefix);
  if (!s->base_dir || !s->prefix) {
    free(s->base_dir);
    free(s->prefix);
    free(s);
    return NULL;
  }

  s->max_segment_size = max_seg_size > 0 ? max_seg_size : (1u << 20); // 1MB
  s->sync_on_seal = 1;
  s->next_seq = 1;
  return s;
}

void folden_journal_store_destroy(folden_journal_store_t *store) {
  if (!store) return;
  for (size_t i = 0; i < store->seg_count; i++) {
    jfile_close(store->segments[i]);
  }
  free(store->segments);
  free(store->base_dir);
  free(store->prefix);
  free(store);
}

static int store_grow(folden_journal_store_t *s) {
  size_t nc = s->seg_cap ? s->seg_cap * 2 : 4;
  folden_journal_file_t **ns = realloc(s->segments, nc * sizeof(*ns));
  if (!ns) return -1;
  s->segments = ns;
  s->seg_cap = nc;
  return 0;
}

static folden_journal_file_t *active_segment(folden_journal_store_t *s) {
  if (s->seg_count == 0) return NULL;
  folden_journal_file_t *last = s->segments[s->seg_count - 1];
  if (last->flags & FOLDEN_FSEG_FLAG_SEALED) return NULL;
  return last;
}

static folden_journal_file_t *new_segment(folden_journal_store_t *s) {
  if (s->seg_count >= s->seg_cap && store_grow(s) != 0) return NULL;

  folden_journal_file_t *jf = jfile_create(s->next_seq++);
  if (!jf) return NULL;

  // pre-allocate header space
  if (ensure_write_cap(jf, FOLDEN_FSEG_HEADER_SZ) != 0) {
    jfile_close(jf);
    return NULL;
  }
  memset(jf->write_buf, 0, FOLDEN_FSEG_HEADER_SZ);
  jf->write_len = FOLDEN_FSEG_HEADER_SZ;

  s->segments[s->seg_count++] = jf;
  return jf;
}

folden_status_t folden_journal_store_append(folden_journal_store_t *store,
                                             const uint8_t *data,
                                             size_t len) {
  if (!store || !data || len == 0) return FOLDEN_ERR_BAD_STATE;

  folden_journal_file_t *seg = active_segment(store);

  // rotate if needed
  if (!seg || (seg->write_len + len > store->max_segment_size)) {
    if (seg) {
      // seal the current one before rotating
      seg->flags |= FOLDEN_FSEG_FLAG_SEALED;
      write_header(seg);
    }
    seg = new_segment(store);
    if (!seg) return FOLDEN_ERR_NOMEM;
  }

  // append: [4B length LE] [data]
  if (ensure_write_cap(seg, 4 + len) != 0) return FOLDEN_ERR_NOMEM;

  uint8_t lenbuf[4];
  uint32_t l32 = (uint32_t)len;
  lenbuf[0] = (uint8_t)(l32);
  lenbuf[1] = (uint8_t)(l32 >> 8);
  lenbuf[2] = (uint8_t)(l32 >> 16);
  lenbuf[3] = (uint8_t)(l32 >> 24);
  memcpy(seg->write_buf + seg->write_len, lenbuf, 4);
  seg->write_len += 4;

  memcpy(seg->write_buf + seg->write_len, data, len);
  seg->write_len += len;
  seg->entry_count++;

  return FOLDEN_OK;
}

folden_status_t folden_journal_store_seal(folden_journal_store_t *store) {
  if (!store) return FOLDEN_ERR_BAD_STATE;
  folden_journal_file_t *seg = active_segment(store);
  if (!seg) return FOLDEN_OK; // nothing to seal

  seg->flags |= FOLDEN_FSEG_FLAG_SEALED;
  write_header(seg);

  // optionally flush to disk
  if (store->sync_on_seal && seg->fd >= 0) {
    char *path = seg_path(store->base_dir, store->prefix, seg->sequence);
    if (path) {
      int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd >= 0) {
        ssize_t wr = write(fd, seg->write_buf, seg->write_len);
        (void)wr; // best-effort
        fsync(fd);
        close(fd);
      }
      free(path);
    }
  }

  return FOLDEN_OK;
}

folden_status_t folden_journal_store_scan(folden_journal_store_t *store,
                                           folden_journal_scan_cb cb,
                                           void *user) {
  if (!store || !cb) return FOLDEN_ERR_BAD_STATE;

  for (size_t s = 0; s < store->seg_count; s++) {
    folden_journal_file_t *jf = store->segments[s];
    if (!jf->write_buf || jf->write_len <= FOLDEN_FSEG_HEADER_SZ) continue;

    const uint8_t *data = jf->write_buf + FOLDEN_FSEG_HEADER_SZ;
    size_t data_len = jf->write_len - FOLDEN_FSEG_HEADER_SZ;

    // validate crc if sealed
    if (jf->flags & FOLDEN_FSEG_FLAG_SEALED) {
      uint32_t stored = read_le32_buf(jf->write_buf + 20);
      uint32_t computed = folden_crc32(data, data_len);
      if (stored != computed) continue; // skip corrupt segment
    }

    size_t pos = 0;
    while (pos + 4 <= data_len) {
      uint32_t entry_len = read_le32_buf(data + pos);
      pos += 4;
      if (entry_len == 0 || pos + entry_len > data_len) break;
      cb(data + pos, entry_len, jf->sequence, user);
      pos += entry_len;
    }
  }

  return FOLDEN_OK;
}

size_t folden_journal_store_compact(folden_journal_store_t *store,
                                    uint32_t min_seq) {
  if (!store) return 0;
  size_t removed = 0;
  size_t w = 0;
  for (size_t i = 0; i < store->seg_count; i++) {
    folden_journal_file_t *jf = store->segments[i];
    if ((jf->flags & FOLDEN_FSEG_FLAG_SEALED) && jf->sequence < min_seq) {
      // remove the on-disk file if it exists
      char *path = seg_path(store->base_dir, store->prefix, jf->sequence);
      if (path) {
        unlink(path);
        free(path);
      }
      jfile_close(jf);
      removed++;
    } else {
      store->segments[w++] = jf;
    }
  }
  store->seg_count = w;
  return removed;
}

size_t folden_journal_store_total_bytes(const folden_journal_store_t *store) {
  if (!store) return 0;
  size_t total = 0;
  for (size_t i = 0; i < store->seg_count; i++) {
    total += store->segments[i]->write_len;
  }
  return total;
}

size_t folden_journal_store_segment_count(const folden_journal_store_t *store) {
  return store ? store->seg_count : 0;
}

#ifndef FOLDEN_INTERNAL_H
#define FOLDEN_INTERNAL_H

#include "folden/folden_types.h"

#include <stddef.h>
#include <stdint.h>

#define FOLDEN_MAGIC_0 0x46u
#define FOLDEN_MAGIC_1 0x4Fu
#define FOLDEN_MAGIC_2 0x4Cu
#define FOLDEN_MAGIC_3 0x44u

#define FOLDEN_TAG_META_UNIT 16u

typedef struct folden_tag_entry {
  uint32_t tag_id;
  uint32_t fold_depth;
  uint32_t span;
} folden_tag_entry_t;

typedef struct folden_lane {
  uint32_t lane_id;
  folden_tag_entry_t *tags;
  size_t tag_count;
  uint8_t *slab;
  size_t slab_len;
  uint32_t fold_budget;
  uint8_t *tag_meta;
  size_t tag_meta_cap;
  uint32_t vault_gen;
  int has_vault_gen;
  int slab_compressed;
} folden_lane_t;

struct folden_capsule {
  folden_lane_t *lanes;
  size_t lane_count;
  uint32_t vault_gen_trailer;
  int has_vault_trailer;
  uint8_t *wire_copy;
  size_t wire_len;
};

typedef struct folden_relay_entry {
  uint32_t lane_id;
  uint8_t *slab;
  size_t slab_len;
  uint32_t refcount;
  uint32_t generation;
  uint32_t arena_token;
  uint32_t arena_borrows;
  folden_tag_entry_t *cached_tags;
  size_t cached_tag_count;
  uint8_t *cached_meta;
  size_t cached_meta_len;
} folden_relay_entry_t;

typedef struct folden_relay_queue folden_relay_queue_t;
typedef struct folden_capsule_cache folden_capsule_cache_t;
typedef struct folden_defer_free folden_defer_free_t;
typedef struct folden_journal folden_journal_t;
typedef struct folden_repl_quorum folden_replicate_quorum_t;
typedef struct folden_repl_fanout folden_replicate_fanout_t;
typedef struct folden_route_cache folden_route_cache_t;
typedef struct folden_route_table folden_route_table_t;
typedef struct folden_metrics folden_metrics_t;
typedef struct folden_lane_acl folden_lane_acl_t;

struct folden_relay {
  folden_relay_entry_t *entries;
  size_t count;
  size_t cap;
  folden_relay_queue_t *queue;
  folden_capsule_cache_t *cache;
  folden_defer_free_t *defer;
  folden_journal_t *journal;
  folden_replicate_quorum_t *quorum;
  folden_replicate_fanout_t *fanout;
  folden_replicate_fsm_t *fsm;
  folden_route_cache_t *route_cache;
  folden_route_table_t *routes;
  folden_metrics_t *metrics;
  folden_lane_acl_t *acl;
  uint32_t session_id;
};

struct folden_vault {
  folden_relay_entry_t *snapshot;
  size_t count;
};

struct folden_stream {
  uint8_t *buf;
  size_t len;
  size_t cap;
  size_t consumed;
  int header_committed;
  size_t partial_lane;
  uint32_t partial_fold_budget;
  int stream_resume;
};

folden_status_t folden_parse_capsule(const uint8_t *data, size_t len,
                                     folden_capsule_t **out);

#endif /* FOLDEN_INTERNAL_H */

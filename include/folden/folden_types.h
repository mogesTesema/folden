#ifndef FOLDEN_TYPES_H
#define FOLDEN_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct folden_capsule folden_capsule_t;
typedef struct folden_relay folden_relay_t;
typedef struct folden_vault folden_vault_t;
typedef struct folden_stream folden_stream_t;
typedef struct folden_repl_quorum folden_replicate_quorum_t;
typedef struct folden_repl_fsm folden_replicate_fsm_t;
typedef struct folden_repl_fanout folden_replicate_fanout_t;

typedef enum folden_status {
  FOLDEN_OK = 0,
  FOLDEN_ERR_TRUNCATED = -1,
  FOLDEN_ERR_BAD_MAGIC = -2,
  FOLDEN_ERR_BAD_VERSION = -3,
  FOLDEN_ERR_BAD_CRC = -4,
  FOLDEN_ERR_BAD_TAG = -5,
  FOLDEN_ERR_FOLD_DEPTH = -6,
  FOLDEN_ERR_NOMEM = -7,
  FOLDEN_ERR_BAD_STATE = -8
} folden_status_t;

typedef enum folden_tag_id {
  FOLDEN_TAG_END = 0,
  FOLDEN_TAG_FOLD_EXTEND = 1,
  FOLDEN_TAG_VAULT_GEN = 2
} folden_tag_id_t;

typedef enum folden_trust_level {
  FOLDEN_TRUST_UNTRUSTED = 0,
  FOLDEN_TRUST_PEER = 1,
  FOLDEN_TRUST_OPERATOR = 2,
  FOLDEN_TRUST_ADMIN = 3
} folden_trust_level_t;

typedef struct folden_lane_stats {
  uint32_t lane_id;
  uint64_t adopt_count;
  uint64_t release_count;
  uint64_t emit_count;
  uint64_t bytes_in;
  uint64_t bytes_out;
  uint32_t last_generation;
} folden_lane_stats_t;

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_TYPES_H */

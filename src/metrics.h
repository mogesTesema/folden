#ifndef FOLDEN_METRICS_H
#define FOLDEN_METRICS_H

#include "folden_internal.h"

typedef struct folden_metrics folden_metrics_t;

typedef struct folden_lane_metrics {
  uint32_t lane_id;
  uint64_t adopt_count;
  uint64_t release_count;
  uint64_t emit_count;
  uint64_t bytes_in;
  uint64_t bytes_out;
  uint32_t last_generation;
} folden_lane_metrics_t;

folden_metrics_t *folden_metrics_create(void);
void folden_metrics_destroy(folden_metrics_t *m);

void folden_metrics_record_adopt(folden_metrics_t *m, uint32_t lane_id,
                                 uint32_t generation, size_t bytes);
void folden_metrics_record_release(folden_metrics_t *m, uint32_t lane_id);
void folden_metrics_record_emit(folden_metrics_t *m, uint32_t lane_id, size_t bytes);

/* Drops all recorded metrics for a lane. Call when a lane's identity is
 * being reused for genuinely different data (not just a routine re-adopt
 * of the same logical stream) so stale counts don't get attributed to the
 * new data. */
void folden_metrics_reset_lane(folden_metrics_t *m, uint32_t lane_id);

folden_status_t folden_metrics_snapshot(const folden_metrics_t *m, uint32_t lane_id,
                                        folden_lane_metrics_t *out);

size_t folden_metrics_lane_count(const folden_metrics_t *m);

/* Writes up to `n` entries, most active (by adopt_count) first, into
 * `out`. Returns the number actually written. */
size_t folden_metrics_top_n(const folden_metrics_t *m, size_t n,
                            folden_lane_metrics_t *out);

#endif /* FOLDEN_METRICS_H */

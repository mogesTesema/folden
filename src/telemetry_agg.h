#ifndef FOLDEN_TELEMETRY_AGG_H
#define FOLDEN_TELEMETRY_AGG_H

#include "folden/folden.h"
#include "folden/folden_types.h"
#include "relay_health.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct folden_telemetry_agg folden_telemetry_agg_t;

/* A summarized telemetry window for a single lane */
typedef struct folden_telemetry_window {
  uint32_t lane_id;
  uint64_t start_ms;
  uint64_t end_ms;
  uint64_t capsule_count;
  uint64_t payload_bytes_total;
  uint32_t min_payload_size;
  uint32_t max_payload_size;
  uint32_t avg_payload_size;
  float drop_rate;
} folden_telemetry_window_t;

/* Creates a telemetry aggregation engine.
 * window_size_ms dictates how long data is buffered before a summary is finalized.
 * max_lanes limits the number of concurrent lanes tracked. */
folden_telemetry_agg_t *folden_telemetry_agg_create(uint64_t window_size_ms, size_t max_lanes);

void folden_telemetry_agg_destroy(folden_telemetry_agg_t *agg);

/* Record a single capsule's telemetry. Should be called during relay adopt. */
folden_status_t folden_telemetry_agg_record(folden_telemetry_agg_t *agg, 
                                            uint32_t lane_id, 
                                            size_t payload_size, 
                                            uint64_t current_ms);

/* Record drops (e.g. from flow control or CRC errors) to calculate drop rates. */
void folden_telemetry_agg_record_drop(folden_telemetry_agg_t *agg, uint32_t lane_id, uint64_t current_ms);

/* Tick the engine. Any windows that have expired past window_size_ms are extracted.
 * The extracted summaries are returned via out_windows. The caller must free out_windows if *out_count > 0. */
folden_status_t folden_telemetry_agg_tick(folden_telemetry_agg_t *agg, 
                                          uint64_t current_ms,
                                          folden_telemetry_window_t **out_windows,
                                          size_t *out_count);

/* Helper to format a telemetry window into a string buffer for logs/dashboards. */
size_t folden_telemetry_window_format(const folden_telemetry_window_t *win, char *buf, size_t buf_cap);

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_TELEMETRY_AGG_H */

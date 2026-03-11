#ifndef FOLDEN_RELAY_HEALTH_H
#define FOLDEN_RELAY_HEALTH_H

#include "folden_internal.h"

/*
 * Health diagnostics for a relay instance.
 *
 * Tracks latency histograms for adopt/emit operations,
 * detects stuck lanes and queue overflow conditions.
 * The dump function serialises everything into a flat
 * key=value text buffer for external consumption.
 */

#define FOLDEN_HIST_BUCKETS 16
#define FOLDEN_HEALTH_MAX_LANES 128

typedef struct {
  uint64_t buckets[FOLDEN_HIST_BUCKETS]; // bucket i covers [2^i, 2^(i+1)) us
  uint64_t count;
  uint64_t sum_us;
  uint64_t min_us;
  uint64_t max_us;
} folden_histogram_t;

typedef struct {
  uint32_t lane_id;
  uint64_t last_adopt_tick;
  uint64_t last_emit_tick;
  uint64_t adopt_count;
  uint64_t emit_count;
} folden_lane_health_t;

typedef struct folden_relay_health {
  folden_histogram_t   adopt_latency;
  folden_histogram_t   emit_latency;
  folden_lane_health_t lanes[FOLDEN_HEALTH_MAX_LANES];
  size_t               lane_count;
  uint64_t             current_tick;
  uint64_t             queue_overflow_count;
  uint64_t             arena_alloc_failures;
  size_t               stuck_lane_threshold; // ticks without emit
} folden_relay_health_t;

typedef struct {
  double p50;
  double p90;
  double p99;
  double mean;
} folden_percentiles_t;

folden_relay_health_t *folden_health_create(size_t stuck_threshold);
void                   folden_health_destroy(folden_relay_health_t *h);

/* Record a latency sample (in microseconds). */
void folden_health_record_adopt(folden_relay_health_t *h,
                                uint32_t lane_id, uint64_t latency_us);
void folden_health_record_emit(folden_relay_health_t *h,
                               uint32_t lane_id, uint64_t latency_us);

/* Increment anomaly counters. */
void folden_health_record_queue_overflow(folden_relay_health_t *h);
void folden_health_record_arena_failure(folden_relay_health_t *h);

/* Advance the tick counter (for stuck-lane detection). */
void folden_health_tick(folden_relay_health_t *h);

/* Compute percentiles from a histogram. */
folden_percentiles_t folden_health_percentiles(const folden_histogram_t *hist);

/* Count lanes that haven't emitted within the stuck threshold. */
size_t folden_health_stuck_lanes(const folden_relay_health_t *h,
                                 uint32_t *out_lane_ids,
                                 size_t out_cap);

/* Dump all health data to a flat text buffer.
 * Returns bytes written. If buf is NULL, returns required size. */
size_t folden_health_dump(const folden_relay_health_t *h,
                          char *buf, size_t buf_cap);

#endif

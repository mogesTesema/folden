#ifndef FOLDEN_FLOW_CONTROL_H
#define FOLDEN_FLOW_CONTROL_H

#include "folden_internal.h"

/*
 * Per-lane token-bucket rate limiter + queue-depth backpressure.
 *
 * Each lane gets its own bucket. On adopt, we call flow_check()
 * to see if the lane is throttled. The scheduler tick refills
 * all buckets.
 */

#define FOLDEN_FLOW_MAX_LANES 256

typedef struct {
  uint32_t lane_id;
  double   tokens;
  double   rate;       // tokens per tick
  double   burst;      // max tokens (bucket capacity)
  uint64_t total_accepted;
  uint64_t total_throttled;
} folden_flow_bucket_t;

typedef struct folden_flow_ctrl {
  folden_flow_bucket_t buckets[FOLDEN_FLOW_MAX_LANES];
  size_t               bucket_count;
  double               default_rate;
  double               default_burst;
  size_t               queue_depth_limit;  // 0 = no limit
  uint64_t             global_throttled;
} folden_flow_ctrl_t;

folden_flow_ctrl_t *folden_flow_create(double default_rate,
                                        double default_burst,
                                        size_t queue_depth_limit);
void                folden_flow_destroy(folden_flow_ctrl_t *fc);

/* Set per-lane override. Returns -1 if table is full. */
int folden_flow_set_lane(folden_flow_ctrl_t *fc, uint32_t lane_id,
                         double rate, double burst);

/* Check whether a capsule from this lane can be adopted.
 * current_depth is the relay queue's current size.
 * Returns FOLDEN_OK or FOLDEN_ERR_THROTTLED. */
folden_status_t folden_flow_check(folden_flow_ctrl_t *fc,
                                  uint32_t lane_id,
                                  size_t current_depth);

/* Consume a token after successful adopt. */
void folden_flow_consume(folden_flow_ctrl_t *fc, uint32_t lane_id);

/* Refill all buckets. Called once per scheduler tick. */
void folden_flow_tick(folden_flow_ctrl_t *fc);

/* Reset all counters (not configuration). */
void folden_flow_reset_stats(folden_flow_ctrl_t *fc);

#endif

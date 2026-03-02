#include "flow_control.h"

#include <stdlib.h>
#include <string.h>

folden_flow_ctrl_t *folden_flow_create(double default_rate,
                                        double default_burst,
                                        size_t queue_depth_limit) {
  folden_flow_ctrl_t *fc = calloc(1, sizeof(*fc));
  if (!fc) return NULL;
  fc->default_rate = default_rate > 0.0 ? default_rate : 10.0;
  fc->default_burst = default_burst > 0.0 ? default_burst : 50.0;
  fc->queue_depth_limit = queue_depth_limit;
  return fc;
}

void folden_flow_destroy(folden_flow_ctrl_t *fc) {
  free(fc);
}

static folden_flow_bucket_t *find_bucket(folden_flow_ctrl_t *fc,
                                          uint32_t lane_id) {
  for (size_t i = 0; i < fc->bucket_count; i++) {
    if (fc->buckets[i].lane_id == lane_id) {
      return &fc->buckets[i];
    }
  }
  return NULL;
}

static folden_flow_bucket_t *get_or_create_bucket(folden_flow_ctrl_t *fc,
                                                    uint32_t lane_id) {
  folden_flow_bucket_t *b = find_bucket(fc, lane_id);
  if (b) return b;

  if (fc->bucket_count >= FOLDEN_FLOW_MAX_LANES) return NULL;

  b = &fc->buckets[fc->bucket_count++];
  memset(b, 0, sizeof(*b));
  b->lane_id = lane_id;
  b->rate = fc->default_rate;
  b->burst = fc->default_burst;
  b->tokens = fc->default_burst; // start full
  return b;
}

int folden_flow_set_lane(folden_flow_ctrl_t *fc, uint32_t lane_id,
                         double rate, double burst) {
  if (!fc) return -1;
  folden_flow_bucket_t *b = get_or_create_bucket(fc, lane_id);
  if (!b) return -1;
  b->rate = rate;
  b->burst = burst;
  if (b->tokens > burst) b->tokens = burst;
  return 0;
}

folden_status_t folden_flow_check(folden_flow_ctrl_t *fc,
                                  uint32_t lane_id,
                                  size_t current_depth) {
  if (!fc) return FOLDEN_OK; // no flow control configured

  // global backpressure check
  if (fc->queue_depth_limit > 0 && current_depth >= fc->queue_depth_limit) {
    fc->global_throttled++;
    return FOLDEN_ERR_THROTTLED;
  }

  folden_flow_bucket_t *b = get_or_create_bucket(fc, lane_id);
  if (!b) {
    // table full, can't track -- let it through
    return FOLDEN_OK;
  }

  if (b->tokens < 1.0) {
    b->total_throttled++;
    fc->global_throttled++;
    return FOLDEN_ERR_THROTTLED;
  }

  return FOLDEN_OK;
}

void folden_flow_consume(folden_flow_ctrl_t *fc, uint32_t lane_id) {
  if (!fc) return;
  folden_flow_bucket_t *b = find_bucket(fc, lane_id);
  if (!b) return;
  b->tokens -= 1.0;
  if (b->tokens < 0.0) b->tokens = 0.0;
  b->total_accepted++;
}

void folden_flow_tick(folden_flow_ctrl_t *fc) {
  if (!fc) return;
  for (size_t i = 0; i < fc->bucket_count; i++) {
    folden_flow_bucket_t *b = &fc->buckets[i];
    b->tokens += b->rate;
    if (b->tokens > b->burst) {
      b->tokens = b->burst;
    }
  }
}

void folden_flow_reset_stats(folden_flow_ctrl_t *fc) {
  if (!fc) return;
  fc->global_throttled = 0;
  for (size_t i = 0; i < fc->bucket_count; i++) {
    fc->buckets[i].total_accepted = 0;
    fc->buckets[i].total_throttled = 0;
  }
}

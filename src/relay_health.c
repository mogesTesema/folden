#include "relay_health.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

folden_relay_health_t *folden_health_create(size_t stuck_threshold) {
  folden_relay_health_t *h = calloc(1, sizeof(*h));
  if (!h) return NULL;
  h->stuck_lane_threshold = stuck_threshold > 0 ? stuck_threshold : 100;
  h->adopt_latency.min_us = UINT64_MAX;
  h->emit_latency.min_us = UINT64_MAX;
  return h;
}

void folden_health_destroy(folden_relay_health_t *h) {
  free(h);
}

static unsigned bucket_index(uint64_t val) {
  if (val == 0) return 0;
  unsigned idx = 0;
  uint64_t v = val;
  while (v >>= 1) idx++;
  return idx < FOLDEN_HIST_BUCKETS ? idx : FOLDEN_HIST_BUCKETS - 1;
}

static void hist_record(folden_histogram_t *hist, uint64_t val) {
  unsigned idx = bucket_index(val);
  hist->buckets[idx]++;
  hist->count++;
  hist->sum_us += val;
  if (val < hist->min_us) hist->min_us = val;
  if (val > hist->max_us) hist->max_us = val;
}

static folden_lane_health_t *find_lane(folden_relay_health_t *h,
                                        uint32_t lane_id) {
  for (size_t i = 0; i < h->lane_count; i++) {
    if (h->lanes[i].lane_id == lane_id) return &h->lanes[i];
  }
  return NULL;
}

static folden_lane_health_t *get_or_create_lane(folden_relay_health_t *h,
                                                  uint32_t lane_id) {
  folden_lane_health_t *l = find_lane(h, lane_id);
  if (l) return l;
  if (h->lane_count >= FOLDEN_HEALTH_MAX_LANES) return NULL;
  l = &h->lanes[h->lane_count++];
  memset(l, 0, sizeof(*l));
  l->lane_id = lane_id;
  return l;
}

void folden_health_record_adopt(folden_relay_health_t *h,
                                uint32_t lane_id, uint64_t latency_us) {
  if (!h) return;
  hist_record(&h->adopt_latency, latency_us);
  folden_lane_health_t *l = get_or_create_lane(h, lane_id);
  if (l) {
    l->last_adopt_tick = h->current_tick;
    l->adopt_count++;
  }
}

void folden_health_record_emit(folden_relay_health_t *h,
                               uint32_t lane_id, uint64_t latency_us) {
  if (!h) return;
  hist_record(&h->emit_latency, latency_us);
  folden_lane_health_t *l = get_or_create_lane(h, lane_id);
  if (l) {
    l->last_emit_tick = h->current_tick;
    l->emit_count++;
  }
}

void folden_health_record_queue_overflow(folden_relay_health_t *h) {
  if (h) h->queue_overflow_count++;
}

void folden_health_record_arena_failure(folden_relay_health_t *h) {
  if (h) h->arena_alloc_failures++;
}

void folden_health_tick(folden_relay_health_t *h) {
  if (h) h->current_tick++;
}

folden_percentiles_t folden_health_percentiles(const folden_histogram_t *hist) {
  folden_percentiles_t p = {0};
  if (!hist || hist->count == 0) return p;

  p.mean = (double)hist->sum_us / (double)hist->count;

  // approximate percentiles from histogram buckets
  uint64_t target_50 = hist->count / 2;
  uint64_t target_90 = (hist->count * 90) / 100;
  uint64_t target_99 = (hist->count * 99) / 100;

  uint64_t cum = 0;
  int found_50 = 0, found_90 = 0, found_99 = 0;

  for (unsigned i = 0; i < FOLDEN_HIST_BUCKETS; i++) {
    cum += hist->buckets[i];
    double bucket_mid = (i == 0) ? 0.5 : (double)(1u << i) * 1.5;

    if (!found_50 && cum >= target_50) {
      p.p50 = bucket_mid;
      found_50 = 1;
    }
    if (!found_90 && cum >= target_90) {
      p.p90 = bucket_mid;
      found_90 = 1;
    }
    if (!found_99 && cum >= target_99) {
      p.p99 = bucket_mid;
      found_99 = 1;
    }
  }
  return p;
}

size_t folden_health_stuck_lanes(const folden_relay_health_t *h,
                                 uint32_t *out_lane_ids,
                                 size_t out_cap) {
  if (!h) return 0;
  size_t count = 0;
  for (size_t i = 0; i < h->lane_count; i++) {
    const folden_lane_health_t *l = &h->lanes[i];
    // a lane is stuck if it was adopted but hasn't emitted recently
    if (l->adopt_count > 0 && l->emit_count == 0) {
      if (count < out_cap && out_lane_ids) {
        out_lane_ids[count] = l->lane_id;
      }
      count++;
      continue;
    }
    if (l->last_adopt_tick > l->last_emit_tick &&
        h->current_tick - l->last_emit_tick > h->stuck_lane_threshold) {
      if (count < out_cap && out_lane_ids) {
        out_lane_ids[count] = l->lane_id;
      }
      count++;
    }
  }
  return count;
}

size_t folden_health_dump(const folden_relay_health_t *h,
                          char *buf, size_t buf_cap) {
  if (!h) return 0;

  folden_percentiles_t ap = folden_health_percentiles(&h->adopt_latency);
  folden_percentiles_t ep = folden_health_percentiles(&h->emit_latency);

  // if buf is NULL, do a dry run with snprintf to /dev/null
  char scratch[2048];
  char *dst = buf ? buf : scratch;
  size_t cap = buf ? buf_cap : sizeof(scratch);

  size_t n = 0;
  #define EMIT(fmt, ...) do { \
    int r = snprintf(dst + n, n < cap ? cap - n : 0, fmt, ##__VA_ARGS__); \
    if (r > 0) n += (size_t)r; \
  } while (0)

  EMIT("adopt_count=%lu\n", (unsigned long)h->adopt_latency.count);
  EMIT("adopt_p50=%.1f\n", ap.p50);
  EMIT("adopt_p90=%.1f\n", ap.p90);
  EMIT("adopt_p99=%.1f\n", ap.p99);
  EMIT("adopt_mean=%.1f\n", ap.mean);
  EMIT("emit_count=%lu\n", (unsigned long)h->emit_latency.count);
  EMIT("emit_p50=%.1f\n", ep.p50);
  EMIT("emit_p90=%.1f\n", ep.p90);
  EMIT("emit_p99=%.1f\n", ep.p99);
  EMIT("emit_mean=%.1f\n", ep.mean);
  EMIT("queue_overflow=%lu\n", (unsigned long)h->queue_overflow_count);
  EMIT("arena_failures=%lu\n", (unsigned long)h->arena_alloc_failures);
  EMIT("active_lanes=%zu\n", h->lane_count);

  uint32_t stuck[32];
  size_t nstuck = folden_health_stuck_lanes(h, stuck, 32);
  EMIT("stuck_lanes=%zu\n", nstuck);
  for (size_t i = 0; i < nstuck && i < 32; i++) {
    EMIT("stuck_lane_%zu=%u\n", i, stuck[i]);
  }

  #undef EMIT
  return n;
}

#include "telemetry_agg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
  uint32_t lane_id;
  int active;
  uint64_t window_start_ms;
  uint64_t cap_count;
  uint64_t drop_count;
  uint64_t payload_bytes;
  uint32_t min_size;
  uint32_t max_size;
} agg_state_t;

struct folden_telemetry_agg {
  uint64_t window_size_ms;
  size_t max_lanes;
  agg_state_t *states;
};

folden_telemetry_agg_t *folden_telemetry_agg_create(uint64_t window_size_ms, size_t max_lanes) {
  if (max_lanes == 0 || window_size_ms == 0) return NULL;
  folden_telemetry_agg_t *agg = calloc(1, sizeof(folden_telemetry_agg_t));
  if (!agg) return NULL;
  
  agg->window_size_ms = window_size_ms;
  agg->max_lanes = max_lanes;
  agg->states = calloc(max_lanes, sizeof(agg_state_t));
  if (!agg->states) {
    free(agg);
    return NULL;
  }
  
  return agg;
}

void folden_telemetry_agg_destroy(folden_telemetry_agg_t *agg) {
  if (!agg) return;
  free(agg->states);
  free(agg);
}

static agg_state_t *get_or_create_state(folden_telemetry_agg_t *agg, uint32_t lane_id, uint64_t current_ms) {
  agg_state_t *empty = NULL;
  for (size_t i = 0; i < agg->max_lanes; i++) {
    if (agg->states[i].active && agg->states[i].lane_id == lane_id) {
      return &agg->states[i];
    }
    if (!agg->states[i].active && !empty) {
      empty = &agg->states[i];
    }
  }
  
  if (empty) {
    empty->active = 1;
    empty->lane_id = lane_id;
    empty->window_start_ms = current_ms;
    empty->cap_count = 0;
    empty->drop_count = 0;
    empty->payload_bytes = 0;
    empty->min_size = UINT32_MAX;
    empty->max_size = 0;
    return empty;
  }
  
  return NULL; // Full
}

folden_status_t folden_telemetry_agg_record(folden_telemetry_agg_t *agg, 
                                            uint32_t lane_id, 
                                            size_t payload_size, 
                                            uint64_t current_ms) {
  if (!agg) return FOLDEN_ERR_BAD_STATE;
  
  agg_state_t *s = get_or_create_state(agg, lane_id, current_ms);
  if (!s) return FOLDEN_ERR_NOMEM;
  
  s->cap_count++;
  s->payload_bytes += payload_size;
  if ((uint32_t)payload_size < s->min_size) s->min_size = (uint32_t)payload_size;
  if ((uint32_t)payload_size > s->max_size) s->max_size = (uint32_t)payload_size;
  
  return FOLDEN_OK;
}

void folden_telemetry_agg_record_drop(folden_telemetry_agg_t *agg, uint32_t lane_id, uint64_t current_ms) {
  if (!agg) return;
  agg_state_t *s = get_or_create_state(agg, lane_id, current_ms);
  if (s) {
    s->drop_count++;
  }
}

folden_status_t folden_telemetry_agg_tick(folden_telemetry_agg_t *agg, 
                                          uint64_t current_ms,
                                          folden_telemetry_window_t **out_windows,
                                          size_t *out_count) {
  if (!agg || !out_windows || !out_count) return FOLDEN_ERR_BAD_STATE;
  
  *out_windows = NULL;
  *out_count = 0;
  
  size_t ready_count = 0;
  for (size_t i = 0; i < agg->max_lanes; i++) {
    if (agg->states[i].active && (current_ms >= agg->states[i].window_start_ms + agg->window_size_ms)) {
      ready_count++;
    }
  }
  
  if (ready_count == 0) return FOLDEN_OK;
  
  folden_telemetry_window_t *wins = malloc(ready_count * sizeof(folden_telemetry_window_t));
  if (!wins) return FOLDEN_ERR_NOMEM;
  
  size_t w_idx = 0;
  for (size_t i = 0; i < agg->max_lanes; i++) {
    if (agg->states[i].active && (current_ms >= agg->states[i].window_start_ms + agg->window_size_ms)) {
      folden_telemetry_window_t *w = &wins[w_idx++];
      agg_state_t *s = &agg->states[i];
      
      w->lane_id = s->lane_id;
      w->start_ms = s->window_start_ms;
      w->end_ms = current_ms;
      w->capsule_count = s->cap_count;
      w->payload_bytes_total = s->payload_bytes;
      w->min_payload_size = s->cap_count > 0 ? s->min_size : 0;
      w->max_payload_size = s->max_size;
      w->avg_payload_size = s->cap_count > 0 ? (uint32_t)(s->payload_bytes / s->cap_count) : 0;
      
      uint64_t total = s->cap_count + s->drop_count;
      w->drop_rate = total > 0 ? (float)s->drop_count / (float)total : 0.0f;
      
      // Reset for next window
      s->window_start_ms = current_ms;
      s->cap_count = 0;
      s->drop_count = 0;
      s->payload_bytes = 0;
      s->min_size = UINT32_MAX;
      s->max_size = 0;
    }
  }
  
  *out_windows = wins;
  *out_count = ready_count;
  
  return FOLDEN_OK;
}

size_t folden_telemetry_window_format(const folden_telemetry_window_t *win, char *buf, size_t buf_cap) {
  if (!win || !buf || buf_cap == 0) return 0;
  
  int n = snprintf(buf, buf_cap, 
                   "WIN[lane=%u %llu-%llu] caps=%llu bytes=%llu sz_min=%u sz_max=%u sz_avg=%u drop=%.2f%%",
                   win->lane_id, 
                   (unsigned long long)win->start_ms, 
                   (unsigned long long)win->end_ms,
                   (unsigned long long)win->capsule_count,
                   (unsigned long long)win->payload_bytes_total,
                   win->min_payload_size,
                   win->max_payload_size,
                   win->avg_payload_size,
                   win->drop_rate * 100.0f);
                   
  if (n < 0) return 0;
  return (size_t)n < buf_cap ? (size_t)n : buf_cap - 1;
}

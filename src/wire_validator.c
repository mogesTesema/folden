#include "wire_validator.h"

#include "varfold.h"

#include <string.h>

int folden_wire_validate_header(const uint8_t *data, size_t len) {
  if (!data || len < 5) {
    return -1;
  }
  if (data[0] != FOLDEN_MAGIC_0 || data[1] != FOLDEN_MAGIC_1 ||
      data[2] != FOLDEN_MAGIC_2 || data[3] != FOLDEN_MAGIC_3) {
    return -1;
  }
  if (data[4] != 1) {
    return -1;
  }
  return 0;
}

int folden_wire_validate_lane_tags(const folden_lane_t *lane, int stream_resume) {
  if (!lane) {
    return -1;
  }
  if (stream_resume) {
    return 0;
  }
  uint32_t budget = lane->fold_budget ? lane->fold_budget : 1;
  for (size_t i = 0; i < lane->tag_count; i++) {
    if (lane->tags[i].tag_id == FOLDEN_TAG_FOLD_EXTEND) {
      budget += lane->tags[i].span;
    }
  }
  for (size_t i = 0; i < lane->tag_count; i++) {
    if (lane->tags[i].fold_depth > budget) {
      return -1;
    }
  }
  return 0;
}

int folden_wire_validate_capsule_lanes(const folden_capsule_t *cap,
                                       int stream_resume) {
  if (!cap) {
    return -1;
  }
  for (size_t i = 0; i < cap->lane_count; i++) {
    if (folden_wire_validate_lane_tags(&cap->lanes[i], stream_resume) != 0) {
      return -1;
    }
  }
  return 0;
}

int folden_wire_validate_no_duplicate_lanes(const folden_capsule_t *cap) {
  if (!cap) {
    return -1;
  }
  for (size_t i = 0; i < cap->lane_count; i++) {
    for (size_t j = i + 1; j < cap->lane_count; j++) {
      if (cap->lanes[i].lane_id == cap->lanes[j].lane_id) {
        return -1;
      }
    }
  }
  return 0;
}

int folden_wire_validate_no_duplicate_tags(const folden_lane_t *lane) {
  if (!lane) {
    return -1;
  }
  for (size_t i = 0; i < lane->tag_count; i++) {
    for (size_t j = i + 1; j < lane->tag_count; j++) {
      if (lane->tags[i].tag_id == lane->tags[j].tag_id) {
        return -1;
      }
    }
  }
  return 0;
}

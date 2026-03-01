#include "lane_acl.h"

#include <stdlib.h>
#include <string.h>

typedef struct folden_acl_rule {
  uint32_t lane_id;
  folden_trust_level_t min_trust;
  int in_use;
} folden_acl_rule_t;

struct folden_lane_acl {
  folden_acl_rule_t *rules;
  size_t count;
  size_t cap;
};

folden_lane_acl_t *folden_lane_acl_create(void) {
  return calloc(1, sizeof(folden_lane_acl_t));
}

void folden_lane_acl_destroy(folden_lane_acl_t *acl) {
  if (!acl) {
    return;
  }
  free(acl->rules);
  free(acl);
}

static folden_acl_rule_t *find_rule(const folden_lane_acl_t *acl, uint32_t lane_id) {
  for (size_t i = 0; i < acl->count; i++) {
    if (acl->rules[i].in_use && acl->rules[i].lane_id == lane_id) {
      return &acl->rules[i];
    }
  }
  return NULL;
}

folden_status_t folden_lane_acl_set_rule(folden_lane_acl_t *acl, uint32_t lane_id,
                                         folden_trust_level_t min_trust) {
  if (!acl) {
    return FOLDEN_ERR_BAD_STATE;
  }
  folden_acl_rule_t *r = find_rule(acl, lane_id);
  if (r) {
    r->min_trust = min_trust;
    return FOLDEN_OK;
  }
  if (acl->count >= acl->cap) {
    size_t nc = acl->cap ? acl->cap * 2 : 8;
    folden_acl_rule_t *nr = realloc(acl->rules, nc * sizeof(*nr));
    if (!nr) {
      return FOLDEN_ERR_NOMEM;
    }
    memset(nr + acl->cap, 0, (nc - acl->cap) * sizeof(*nr));
    acl->rules = nr;
    acl->cap = nc;
  }
  r = &acl->rules[acl->count++];
  r->lane_id = lane_id;
  r->min_trust = min_trust;
  r->in_use = 1;
  return FOLDEN_OK;
}

void folden_lane_acl_clear_rule(folden_lane_acl_t *acl, uint32_t lane_id) {
  if (!acl) {
    return;
  }
  folden_acl_rule_t *r = find_rule(acl, lane_id);
  if (r) {
    r->in_use = 0;
  }
}

folden_status_t folden_lane_acl_check(const folden_lane_acl_t *acl, uint32_t lane_id,
                                      folden_trust_level_t caller_trust) {
  if (!acl) {
    return FOLDEN_OK;
  }
  folden_acl_rule_t *r = find_rule(acl, lane_id);
  if (!r) {
    return FOLDEN_OK;
  }
  return caller_trust >= r->min_trust ? FOLDEN_OK : FOLDEN_ERR_BAD_STATE;
}

size_t folden_lane_acl_rule_count(const folden_lane_acl_t *acl) {
  if (!acl) {
    return 0;
  }
  size_t n = 0;
  for (size_t i = 0; i < acl->count; i++) {
    if (acl->rules[i].in_use) {
      n++;
    }
  }
  return n;
}

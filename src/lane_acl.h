#ifndef FOLDEN_LANE_ACL_H
#define FOLDEN_LANE_ACL_H

#include "folden_internal.h"

typedef struct folden_lane_acl folden_lane_acl_t;

folden_lane_acl_t *folden_lane_acl_create(void);
void folden_lane_acl_destroy(folden_lane_acl_t *acl);

/* Sets the minimum trust level required to access lane_id. A lane with no
 * rule defaults to FOLDEN_TRUST_UNTRUSTED (anyone may access it). */
folden_status_t folden_lane_acl_set_rule(folden_lane_acl_t *acl, uint32_t lane_id,
                                         folden_trust_level_t min_trust);

/* Removes any rule for lane_id, reverting it to the default (open) policy. */
void folden_lane_acl_clear_rule(folden_lane_acl_t *acl, uint32_t lane_id);

/* Returns FOLDEN_OK if caller_trust meets or exceeds the lane's configured
 * minimum, FOLDEN_ERR_BAD_STATE otherwise. */
folden_status_t folden_lane_acl_check(const folden_lane_acl_t *acl, uint32_t lane_id,
                                      folden_trust_level_t caller_trust);

size_t folden_lane_acl_rule_count(const folden_lane_acl_t *acl);

#endif /* FOLDEN_LANE_ACL_H */

#include "config_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct folden_config_manager {
  folden_relay_t *relay;
  uint32_t revision;
  
  // Stored state for serialization
  uint32_t default_flow_limit;
  uint32_t default_lane_budget;
  int enable_telemetry;
  int enable_discovery;
  
  // Active ACL lists
  uint32_t *blocked_lanes;
  size_t blocked_count;
};

folden_config_manager_t *folden_config_manager_create(folden_relay_t *relay) {
  if (!relay) return NULL;
  
  folden_config_manager_t *cm = calloc(1, sizeof(folden_config_manager_t));
  if (!cm) return NULL;
  
  cm->relay = relay;
  cm->default_flow_limit = 1000;
  cm->default_lane_budget = 10;
  cm->enable_telemetry = 1;
  cm->enable_discovery = 0;
  
  return cm;
}

void folden_config_manager_destroy(folden_config_manager_t *cm) {
  if (!cm) return;
  free(cm->blocked_lanes);
  free(cm);
}

static void apply_routing_rules(folden_config_manager_t *cm, folden_json_node_t *routes) {
  if (!routes || routes->type != FOLDEN_JSON_ARRAY) return;
  
  for (size_t i = 0; i < routes->value.array.count; i++) {
    folden_json_node_t *r = routes->value.array.items[i];
    if (r->type != FOLDEN_JSON_OBJECT) continue;
    
    folden_json_node_t *n_target = folden_json_object_get(r, "target");
    folden_json_node_t *n_hop = folden_json_object_get(r, "next_hop");
    folden_json_node_t *n_cost = folden_json_object_get(r, "cost");
    
    if (n_target && n_target->type == FOLDEN_JSON_NUMBER &&
        n_hop && n_hop->type == FOLDEN_JSON_NUMBER &&
        n_cost && n_cost->type == FOLDEN_JSON_NUMBER) {
        
        uint32_t t = (uint32_t)n_target->value.number;
        uint32_t h = (uint32_t)n_hop->value.number;
        uint32_t c = (uint32_t)n_cost->value.number;
        
        folden_relay_route_add(cm->relay, t, h, c);
    }
  }
}

static void apply_acl_rules(folden_config_manager_t *cm, folden_json_node_t *acl) {
  if (!acl || acl->type != FOLDEN_JSON_OBJECT) return;
  
  folden_json_node_t *blocked = folden_json_object_get(acl, "blocked_lanes");
  if (blocked && blocked->type == FOLDEN_JSON_ARRAY) {
    if (cm->blocked_lanes) {
      free(cm->blocked_lanes);
      cm->blocked_lanes = NULL;
    }
    cm->blocked_count = blocked->value.array.count;
    if (cm->blocked_count > 0) {
      cm->blocked_lanes = malloc(cm->blocked_count * sizeof(uint32_t));
      if (cm->blocked_lanes) {
        for (size_t i = 0; i < cm->blocked_count; i++) {
          folden_json_node_t *item = blocked->value.array.items[i];
          if (item->type == FOLDEN_JSON_NUMBER) {
            cm->blocked_lanes[i] = (uint32_t)item->value.number;
          } else {
            cm->blocked_lanes[i] = 0;
          }
        }
      } else {
        cm->blocked_count = 0;
      }
    }
  }
}

static void apply_general_settings(folden_config_manager_t *cm, folden_json_node_t *gen) {
  if (!gen || gen->type != FOLDEN_JSON_OBJECT) return;
  
  folden_json_node_t *flow = folden_json_object_get(gen, "flow_limit");
  if (flow && flow->type == FOLDEN_JSON_NUMBER) {
    cm->default_flow_limit = (uint32_t)flow->value.number;
  }
  
  folden_json_node_t *budg = folden_json_object_get(gen, "lane_budget");
  if (budg && budg->type == FOLDEN_JSON_NUMBER) {
    cm->default_lane_budget = (uint32_t)budg->value.number;
  }
  
  folden_json_node_t *tel = folden_json_object_get(gen, "enable_telemetry");
  if (tel && tel->type == FOLDEN_JSON_BOOL) {
    cm->enable_telemetry = tel->value.boolean;
  }
  
  folden_json_node_t *disc = folden_json_object_get(gen, "enable_discovery");
  if (disc && disc->type == FOLDEN_JSON_BOOL) {
    cm->enable_discovery = disc->value.boolean;
  }
}

folden_status_t folden_config_manager_apply_json(folden_config_manager_t *cm, const char *json_str, size_t length) {
  if (!cm || !json_str || length == 0) return FOLDEN_ERR_BAD_STATE;
  
  folden_json_node_t *root = folden_json_parse(json_str, length);
  if (!root) return FOLDEN_ERR_BAD_STATE; // Parsing failed
  
  if (root->type != FOLDEN_JSON_OBJECT) {
    folden_json_free(root);
    return FOLDEN_ERR_BAD_STATE;
  }
  
  apply_general_settings(cm, folden_json_object_get(root, "settings"));
  apply_routing_rules(cm, folden_json_object_get(root, "routes"));
  apply_acl_rules(cm, folden_json_object_get(root, "acl"));
  
  cm->revision++;
  
  folden_json_free(root);
  return FOLDEN_OK;
}

folden_status_t folden_config_manager_dump_json(const folden_config_manager_t *cm, char *out_buf, size_t out_cap, size_t *out_len) {
  if (!cm || !out_buf || !out_len) return FOLDEN_ERR_BAD_STATE;
  
  int n = snprintf(out_buf, out_cap, 
    "{\n"
    "  \"settings\": {\n"
    "    \"flow_limit\": %u,\n"
    "    \"lane_budget\": %u,\n"
    "    \"enable_telemetry\": %s,\n"
    "    \"enable_discovery\": %s\n"
    "  },\n"
    "  \"acl\": {\n"
    "    \"blocked_lanes\": [",
    cm->default_flow_limit,
    cm->default_lane_budget,
    cm->enable_telemetry ? "true" : "false",
    cm->enable_discovery ? "true" : "false"
  );
  
  if (n < 0 || (size_t)n >= out_cap) return FOLDEN_ERR_NOMEM;
  
  size_t w = n;
  
  for (size_t i = 0; i < cm->blocked_count; i++) {
    n = snprintf(out_buf + w, out_cap - w, "%s%u", (i == 0) ? "" : ", ", cm->blocked_lanes[i]);
    if (n < 0 || w + n >= out_cap) return FOLDEN_ERR_NOMEM;
    w += n;
  }
  
  n = snprintf(out_buf + w, out_cap - w, "]\n  }\n}\n");
  if (n < 0 || w + n >= out_cap) return FOLDEN_ERR_NOMEM;
  w += n;
  
  *out_len = w;
  return FOLDEN_OK;
}

uint32_t folden_config_manager_get_revision(const folden_config_manager_t *cm) {
  return cm ? cm->revision : 0;
}

void folden_config_manager_notify_reload(folden_config_manager_t *cm) {
  if (!cm) return;
  // Re-initialization or flush hooks can be added here
}

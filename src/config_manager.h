#ifndef FOLDEN_CONFIG_MANAGER_H
#define FOLDEN_CONFIG_MANAGER_H

#include "folden/folden.h"
#include "folden/folden_types.h"
#include "json_parser.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct folden_config_manager folden_config_manager_t;

/* Creates a configuration manager to dynamically control a relay's settings via JSON. */
folden_config_manager_t *folden_config_manager_create(folden_relay_t *relay);

void folden_config_manager_destroy(folden_config_manager_t *cm);

/* Loads and applies a JSON configuration buffer.
 * Returns FOLDEN_OK on success, or an error code if parsing/validation fails. */
folden_status_t folden_config_manager_apply_json(folden_config_manager_t *cm, const char *json_str, size_t length);

/* Dumps the current applied configuration into a JSON string buffer.
 * out_buf must be pre-allocated by the caller. */
folden_status_t folden_config_manager_dump_json(const folden_config_manager_t *cm, char *out_buf, size_t out_cap, size_t *out_len);

/* Retrieves the total number of configurations applied since initialization */
uint32_t folden_config_manager_get_revision(const folden_config_manager_t *cm);

/* Internal callback for watching file changes if backed by a hot-reload watcher */
void folden_config_manager_notify_reload(folden_config_manager_t *cm);

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_CONFIG_MANAGER_H */

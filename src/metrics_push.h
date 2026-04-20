#ifndef FOLDEN_METRICS_PUSH_H
#define FOLDEN_METRICS_PUSH_H

#include "folden/folden.h"
#include "metrics_exporter.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct folden_metrics_push folden_metrics_push_t;

/* Creates a pushgateway client that periodically pushes metrics 
 * to the specified HTTP endpoint. */
folden_metrics_push_t *folden_metrics_push_create(folden_relay_health_t *health, 
                                                  const char *pushgateway_url,
                                                  const char *job_name);

void folden_metrics_push_destroy(folden_metrics_push_t *pusher);

/* Starts the background thread to push metrics periodically. */
folden_status_t folden_metrics_push_start(folden_metrics_push_t *pusher, uint32_t interval_sec);

/* Stops the background thread. */
folden_status_t folden_metrics_push_stop(folden_metrics_push_t *pusher);

/* Perform a manual synchronous push (mostly for testing or graceful shutdown) */
folden_status_t folden_metrics_push_now(folden_metrics_push_t *pusher);

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_METRICS_PUSH_H */

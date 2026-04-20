#ifndef FOLDEN_METRICS_EXPORTER_H
#define FOLDEN_METRICS_EXPORTER_H

#include "folden/folden.h"
#include "relay_health.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct folden_metrics_exporter folden_metrics_exporter_t;

/* Creates a metrics exporter instance bound to a relay health tracker.
 * port specifies the TCP port to listen on for HTTP requests. */
folden_metrics_exporter_t *folden_metrics_exporter_create(folden_relay_health_t *health, uint16_t port);

void folden_metrics_exporter_destroy(folden_metrics_exporter_t *exp);

/* Starts the HTTP server loop in a non-blocking background thread. */
folden_status_t folden_metrics_exporter_start(folden_metrics_exporter_t *exp);

/* Stops the HTTP server gracefully. */
folden_status_t folden_metrics_exporter_stop(folden_metrics_exporter_t *exp);

/* Internal: Handle a raw HTTP request buffer and populate the response buffer.
 * Primarily exposed for unit testing without a real socket. */
folden_status_t folden_metrics_exporter_handle_request(folden_metrics_exporter_t *exp, 
                                                       const char *request, 
                                                       size_t req_len,
                                                       char *response,
                                                       size_t resp_cap,
                                                       size_t *resp_len);

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_METRICS_EXPORTER_H */

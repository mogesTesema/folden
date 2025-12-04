#ifndef FOLDEN_RELAY_QUEUE_H
#define FOLDEN_RELAY_QUEUE_H

#include "folden_internal.h"

typedef struct folden_relay_queue folden_relay_queue_t;

folden_relay_queue_t *folden_relay_queue_create(void);
void folden_relay_queue_destroy(folden_relay_queue_t *q);

folden_status_t folden_relay_queue_enqueue_adopt(folden_relay_queue_t *q,
                                                 const folden_lane_t *lane);
folden_status_t folden_relay_queue_flush(folden_relay_t *relay,
                                         folden_relay_queue_t *q,
                                         folden_defer_free_t *defer);

void folden_relay_queue_drop_lane(folden_relay_t *relay, folden_lane_t *lane);

#endif /* FOLDEN_RELAY_QUEUE_H */

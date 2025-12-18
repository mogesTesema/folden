#ifndef FOLDEN_DEFER_FREE_H
#define FOLDEN_DEFER_FREE_H

#include <stddef.h>
#include <stdint.h>

typedef struct folden_defer_free folden_defer_free_t;

folden_defer_free_t *folden_defer_create(void);
void folden_defer_destroy(folden_defer_free_t *df);

void folden_defer_enqueue_ptr(folden_defer_free_t *df, void *ptr);
void folden_defer_enqueue_arena_token(folden_defer_free_t *df, uint32_t token);
void folden_defer_drain(folden_defer_free_t *df, uint32_t epoch);

#endif /* FOLDEN_DEFER_FREE_H */

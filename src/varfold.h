#ifndef FOLDEN_VARFOLD_H
#define FOLDEN_VARFOLD_H

#include <stddef.h>
#include <stdint.h>

size_t folden_varfold_encode(uint64_t value, uint8_t *out, size_t out_cap);
int folden_varfold_decode(const uint8_t *data, size_t len, uint64_t *out,
                          size_t *consumed);

#endif /* FOLDEN_VARFOLD_H */

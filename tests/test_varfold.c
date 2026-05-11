#include "minctest.h"

#include "../src/varfold.h"

TEST(test_varfold_roundtrip) {
  uint8_t buf[16];
  uint64_t values[] = {0, 1, 127, 128, 300, 17000};
  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    size_t n = folden_varfold_encode(values[i], buf, sizeof(buf));
    ASSERT(n > 0);
    uint64_t out = 0;
    size_t used = 0;
    ASSERT_EQ(0, folden_varfold_decode(buf, n, &out, &used));
    ASSERT_EQ(values[i], out);
    ASSERT_EQ(n, used);
  }
}

void run_varfold_tests(void) {
  RUN(test_varfold_roundtrip);
}

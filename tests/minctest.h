#ifndef MINCTEST_H
#define MINCTEST_H

#include <stdio.h>
#include <stdlib.h>

extern int g_tests;
extern int g_failed;

#define TEST(name) static void name(void)
#define RUN(name)                                                        \
  do {                                                                   \
    g_tests++;                                                           \
    printf("  %s ... ", #name);                                          \
    name();                                                              \
    printf("ok\n");                                                      \
  } while (0)

#define ASSERT(cond)                                                     \
  do {                                                                   \
    if (!(cond)) {                                                       \
      fprintf(stderr, "\nASSERT failed: %s (%s:%d)\n", #cond, __FILE__,  \
              __LINE__);                                                 \
      g_failed++;                                                        \
      return;                                                            \
    }                                                                    \
  } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

#endif /* MINCTEST_H */

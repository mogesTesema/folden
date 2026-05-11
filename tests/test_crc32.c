#include "minctest.h"

#include "crc32.h"

#include <string.h>

TEST(test_crc32_of_empty_input_is_zero) {
  ASSERT_EQ(0u, folden_crc32(NULL, 0));
}

TEST(test_crc32_known_vector_ascii_string) {
  /* CRC-32 (IEEE 802.3) of the ASCII bytes "123456789" is the standard
   * "check" value used to validate CRC-32 implementations. */
  const uint8_t data[] = "123456789";
  ASSERT_EQ(0xCBF43926u, folden_crc32(data, 9));
}

TEST(test_crc32_is_deterministic) {
  const uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};
  uint32_t a = folden_crc32(data, sizeof(data));
  uint32_t b = folden_crc32(data, sizeof(data));
  ASSERT_EQ(a, b);
}

TEST(test_crc32_differs_for_different_input) {
  const uint8_t a[4] = {1, 2, 3, 4};
  const uint8_t b[4] = {1, 2, 3, 5};
  ASSERT(folden_crc32(a, 4) != folden_crc32(b, 4));
}

TEST(test_crc32_sensitive_to_length) {
  const uint8_t data[5] = {'a', 'b', 'c', 'd', 'e'};
  ASSERT(folden_crc32(data, 4) != folden_crc32(data, 5));
}

void test_crc32_register(void) {
  RUN(test_crc32_of_empty_input_is_zero);
  RUN(test_crc32_known_vector_ascii_string);
  RUN(test_crc32_is_deterministic);
  RUN(test_crc32_differs_for_different_input);
  RUN(test_crc32_sensitive_to_length);
}

#include "minctest.h"

#include "folden/folden.h"

#include <string.h>

TEST(test_foldconf_applies_known_keys) {
  const char *text =
      "# a comment\n"
      "\n"
      "journal.segment.max_bytes = 8192\n"
      "journal.compact.threshold = 3\n"
      "replicate.max_attempts = 7\n";

  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_OK, folden_relay_configure(relay, text, strlen(text), NULL));
  folden_relay_destroy(relay);
}

TEST(test_foldconf_rejects_unknown_key) {
  const char *text = "not.a.real.key = 1\n";
  int error_line = 0;
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_ERR_BAD_TAG,
            folden_relay_configure(relay, text, strlen(text), &error_line));
  ASSERT_EQ(1, error_line);
  folden_relay_destroy(relay);
}

TEST(test_foldconf_rejects_non_numeric_value) {
  const char *text = "journal.segment.max_bytes = not-a-number\n";
  int error_line = 0;
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_ERR_BAD_TAG,
            folden_relay_configure(relay, text, strlen(text), &error_line));
  ASSERT_EQ(1, error_line);
  folden_relay_destroy(relay);
}

TEST(test_foldconf_reports_correct_error_line) {
  const char *text =
      "journal.segment.max_bytes = 4096\n"
      "journal.compact.threshold = 2\n"
      "bogus.key = 1\n";
  int error_line = 0;
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_ERR_BAD_TAG,
            folden_relay_configure(relay, text, strlen(text), &error_line));
  ASSERT_EQ(3, error_line);
  folden_relay_destroy(relay);
}

TEST(test_foldconf_missing_equals_is_rejected) {
  const char *text = "journal.segment.max_bytes 4096\n";
  int error_line = 0;
  folden_relay_t *relay = folden_relay_create();
  ASSERT_EQ(FOLDEN_ERR_BAD_TAG,
            folden_relay_configure(relay, text, strlen(text), &error_line));
  ASSERT_EQ(1, error_line);
  folden_relay_destroy(relay);
}

void test_foldconf_register(void) {
  RUN(test_foldconf_applies_known_keys);
  RUN(test_foldconf_rejects_unknown_key);
  RUN(test_foldconf_rejects_non_numeric_value);
  RUN(test_foldconf_reports_correct_error_line);
  RUN(test_foldconf_missing_equals_is_rejected);
}

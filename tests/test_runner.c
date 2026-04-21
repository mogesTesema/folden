#include "minctest.h"

int g_tests = 0;
int g_failed = 0;

extern void run_varfold_tests(void);
extern void run_merge_tests(void);
extern void run_vault_tests(void);
extern void test_stream_register(void);
extern void test_arena_register(void);
extern void test_journal_register(void);
extern void test_route_register(void);
extern void test_session_register(void);
extern void test_compress_register(void);
extern void test_route_table_register(void);
extern void test_journal_core_register(void);
extern void test_replicate_register(void);
extern void test_arena_borrow_register(void);
extern void test_route_cache_freshness_register(void);
extern void test_wire_bounds_register(void);
extern void test_emit_cache_invalidation_register(void);
extern void test_metrics_register(void);
extern void test_foldconf_register(void);
extern void test_recovery_register(void);
extern void test_recovery_reentrant_register(void);
extern void test_wire_validator_register(void);
extern void test_lane_acl_register(void);
extern void test_capsule_diff_register(void);
extern void test_crc32_register(void);
extern void test_capsule_cache_register(void);
extern void test_defer_free_register(void);
extern void test_vault_registry_register(void);

extern void test_wire_compress(void);
extern void test_flow_control(void);
extern void test_relay_health(void);
extern void test_relay_session(void);
extern void test_capsule_mux(void);
extern void test_journal_file(void);
extern void test_route_discovery(void);
extern void test_telemetry_agg(void);

int main(void) {
  printf("folden tests\n");
  run_varfold_tests();
  run_merge_tests();
  run_vault_tests();
  test_stream_register();
  test_arena_register();
  test_journal_register();
  test_route_register();
  test_session_register();
  test_compress_register();
  test_route_table_register();
  test_journal_core_register();
  test_replicate_register();
  test_arena_borrow_register();
  test_route_cache_freshness_register();
  test_wire_bounds_register();
  test_emit_cache_invalidation_register();
  test_metrics_register();
  test_foldconf_register();
  test_recovery_register();
  test_recovery_reentrant_register();
  test_wire_validator_register();
  test_lane_acl_register();
  test_capsule_diff_register();
  test_crc32_register();
  test_capsule_cache_register();
  test_defer_free_register();
  test_vault_registry_register();

  test_wire_compress();
  test_flow_control();
  test_relay_health();
  test_relay_session();
  test_capsule_mux();
  test_journal_file();
  test_route_discovery();
  test_telemetry_agg();

  if (g_failed != 0) {
    fprintf(stderr, "%d assertion(s) failed\n", g_failed);
    return 1;
  }
  printf("all %d tests passed\n", g_tests);
  return 0;
}

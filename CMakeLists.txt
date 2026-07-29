cmake_minimum_required(VERSION 3.16)

project(folden C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(FOLDEN_ASAN "Build with AddressSanitizer" OFF)
option(FOLDEN_FUZZ "Build libFuzzer harnesses (requires clang)" OFF)

if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
  add_compile_options(-Wall -Wextra)
endif()

if(FOLDEN_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()

set(FOLDEN_SRC
  src/varfold.c
  src/crc32.c
  src/wire_validator.c
  src/tag_index.c
  src/meta_resolver.c
  src/fold_sched.c
  src/slab_decode.c
  src/slab_arena.c
  src/defer_free.c
  src/capsule_cache.c
  src/route_cache.c
  src/relay_queue.c
  src/relay_registry.c
  src/capsule_emit.c
  src/lane_graph.c
  src/lane_merge.c
  src/vault_store.c
  src/snapshot_codec.c
  src/stream_reasm.c
  src/route_table.c
  src/replicate.c
  src/journal.c
  src/metrics.c
  src/foldconf.c
  src/lane_acl.c
  src/capsule_diff.c
  src/vault_registry.c
  src/recovery.c
  src/folden.c
  src/wire_compress.c
  src/flow_control.c
  src/relay_health.c
  src/relay_session.c
  src/capsule_mux.c
  src/journal_file.c
  src/route_discovery.c
  src/telemetry_agg.c
  src/sha256.c
  src/handshake.c
  src/json_parser.c
  src/config_manager.c
  src/tag_signature.c
  src/access_log.c
  src/metrics_exporter.c
  src/metrics_push.c
)

add_library(folden STATIC ${FOLDEN_SRC})
target_include_directories(folden PUBLIC include)
find_package(Threads REQUIRED)
target_link_libraries(folden Threads::Threads)
target_include_directories(folden PRIVATE src)

add_executable(folden-inspect tools/folden-inspect.c tools/capsule_wire.c)
target_link_libraries(folden-inspect folden)
target_include_directories(folden-inspect PRIVATE tools src)

add_executable(folden-tests
  tests/test_runner.c
  tests/test_varfold.c
  tests/test_compress.c
  tests/test_arena.c
  tests/test_defer_free.c
  tests/test_capsule_cache.c
  tests/test_crc32.c
  tests/test_merge.c
  tests/test_vault_roundtrip.c
  tests/test_stream.c
  tests/test_route.c
  tests/test_session.c
  tests/test_route_table.c
  tests/test_replicate.c
  tests/test_journal_core.c
  tests/test_journal.c
  tests/test_metrics.c
  tests/test_foldconf.c
  tests/test_lane_acl.c
  tests/test_capsule_diff.c
  tests/test_vault_registry.c
  tests/test_recovery.c
  tests/test_wire_bounds.c
  tests/test_wire_validator.c
  tests/test_route_cache_freshness.c
  tests/test_wire_compress.c
  tests/test_flow_control.c
  tests/test_relay_health.c
  tests/test_relay_session.c
  tests/test_capsule_mux.c
  tests/test_journal_file.c
  tests/test_route_discovery.c
  tests/test_telemetry_agg.c
  tools/capsule_wire.c
)
target_link_libraries(folden-tests folden)
target_include_directories(folden-tests PRIVATE tests tools src)

enable_testing()
add_test(NAME folden-tests COMMAND folden-tests)

if(FOLDEN_FUZZ)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "FOLDEN_FUZZ requires clang for libFuzzer support")
  endif()

  # Coverage instrumentation has to reach the library itself, not just the
  # ~50-line harness translation unit, or libFuzzer is flying blind against
  # the real parsing/state-machine logic.
  set(FOLDEN_FUZZ_COMPILE_FLAGS -fsanitize=fuzzer-no-link -fno-omit-frame-pointer)
  target_compile_options(folden PRIVATE ${FOLDEN_FUZZ_COMPILE_FLAGS})

  if(DEFINED ENV{LIB_FUZZING_ENGINE} AND NOT "$ENV{LIB_FUZZING_ENGINE}" STREQUAL "")
    separate_arguments(FOLDEN_FUZZ_LINK_FLAGS UNIX_COMMAND "$ENV{LIB_FUZZING_ENGINE}")
  else()
    set(FOLDEN_FUZZ_LINK_FLAGS -fsanitize=fuzzer)
  endif()

  foreach(harness slab merge relay vault journal arena route replicate foldconf recovery cache)
    add_executable(${harness}_fuzzer fuzz/${harness}_fuzzer.c)
    target_link_libraries(${harness}_fuzzer folden)
    target_compile_options(${harness}_fuzzer PRIVATE ${FOLDEN_FUZZ_COMPILE_FLAGS})
    target_link_options(${harness}_fuzzer PRIVATE ${FOLDEN_FUZZ_LINK_FLAGS})
    target_include_directories(${harness}_fuzzer PRIVATE include src)
  endforeach()
endif()

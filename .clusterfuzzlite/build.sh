#!/bin/bash -eu

# OSS-Fuzz/ClusterFuzzLite copies this script to $SRC/<project>/build.sh.
# Also supports local invocation as .clusterfuzzlite/build.sh from a repo
# checkout, for testing outside the OSS-Fuzz container.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [[ -f "$SCRIPT_DIR/CMakeLists.txt" ]]; then
  REPO_ROOT="$SCRIPT_DIR"
elif [[ -f "$SCRIPT_DIR/../CMakeLists.txt" ]]; then
  REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "error: CMakeLists.txt not found near $SCRIPT_DIR" >&2
  exit 1
fi

cd "$REPO_ROOT"
export SRC="${SRC:-$REPO_ROOT}"
export OUT="${OUT:-$REPO_ROOT/build-fuzz-out}"
BUILD_DIR="${WORK:-$REPO_ROOT}/build-fuzz"
rm -rf "$BUILD_DIR"
mkdir -p "$OUT" "$BUILD_DIR"

CMAKE_EXTRA_ARGS=()
# Only force our own -fsanitize=address toggle when nothing more specific was
# requested; a coverage/UBSan/MSan build already carries the right flags in
# $CFLAGS/$CXXFLAGS and doubling up on -fsanitize=address would conflict.
if [[ "${SANITIZER:-address}" == "address" ]]; then
  CMAKE_EXTRA_ARGS+=("-DFOLDEN_ASAN=ON")
fi

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DCMAKE_C_COMPILER="${CC:-clang}" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="${CFLAGS:-}" \
  -DFOLDEN_FUZZ=ON \
  "${CMAKE_EXTRA_ARGS[@]}"

FUZZERS=(slab_fuzzer merge_fuzzer relay_fuzzer vault_fuzzer journal_fuzzer arena_fuzzer route_fuzzer replicate_fuzzer foldconf_fuzzer recovery_fuzzer cache_fuzzer defer_fuzzer vault_registry_fuzzer journal_file_fuzzer capsule_mux_fuzzer)
cmake --build "$BUILD_DIR" -j"$(nproc)" --target "${FUZZERS[@]}"

for t in "${FUZZERS[@]}"; do
  cp "$BUILD_DIR/$t" "$OUT/"

  seed_dir="$REPO_ROOT/fuzz/corpus/$t"
  if [[ -d "$seed_dir" ]] && [[ -n "$(ls -A "$seed_dir" 2>/dev/null)" ]]; then
    (cd "$seed_dir" && zip -qr "$OUT/${t}_seed_corpus.zip" .)
  fi

  if [[ -f "$REPO_ROOT/fuzz/dictionary.txt" ]]; then
    cp "$REPO_ROOT/fuzz/dictionary.txt" "$OUT/${t}.dict"
  fi
done

echo "ClusterFuzzLite build OK -> $OUT"

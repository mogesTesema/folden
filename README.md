# folden

folden is a C11 library for parsing, merging, and re-emitting **FOLD capsules** —
a compact binary envelope format for edge relay telemetry.

Capsules batch lane identifiers, tag metadata, and opaque payload slabs into a
single checksummed blob. The **varfold** integer codec keeps numeric fields compact
on bandwidth-limited links.

## Features

- varfold integer codec, CRC32-validated wire format
- Multi-lane capsule decode and encode, with incremental stream reassembly
- Tag-run metadata with fold-depth budgets
- Lane merge with supersede semantics
- Relay registry with generation tracking, an LRU capsule cache, and a
  zero-copy arena for large slabs
- Vault checkpoint/restore, including a named multi-snapshot registry
- Journal with segment rotation, compaction, and replay-based recovery
- Routing table with LRU eviction and retry backoff, including multi-hop
  path resolution
- Replication quorum/fanout state machine with configurable ack thresholds
- Per-lane trust-level access control
- Per-lane adoption/emit metrics
- Runtime config parser for journal and replication thresholds
- Structural capsule diffing

See [docs/wire-spec.md](docs/wire-spec.md) for the full on-the-wire specification.

## Repository layout

```
include/folden/     Public headers
src/                Library implementation
docs/               Wire format specification
tests/              Unit and integration tests
fuzz/               Fuzz harnesses and seed corpora
tools/              folden-inspect CLI
.clusterfuzzlite/   Continuous fuzzing configuration
```

## Requirements

- CMake 3.16+
- C11 compiler (GCC or Clang)

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Debug build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Fuzz harnesses (requires clang):

```bash
cmake -B build-fuzz -DCMAKE_C_COMPILER=clang -DFOLDEN_FUZZ=ON -DFOLDEN_ASAN=ON
cmake --build build-fuzz
```

## folden-inspect

`folden-inspect` is a small CLI over the library's public API:

```bash
./build/folden-inspect inspect path/to/capsule.bin
./build/folden-inspect stats path/to/capsule.bin
./build/folden-inspect routes path/to/capsule.bin 100:5
./build/folden-inspect configure path/to/capsule.bin path/to/config.txt
```

## Public API

Headers live under `include/folden/`:

| Header | Contents |
|--------|----------|
| `folden.h` | Decode, merge, emit, relay, vault, stream, routing, replication, metrics, config, ACL, and recovery entry points |
| `folden_types.h` | Opaque handles, status codes, well-known tag IDs, trust levels, lane stats |

Create handles with `folden_relay_create`, `folden_vault_create`, and
`folden_stream_create`, and release them with the matching `*_destroy` calls.

Decode a complete buffer:

```c
folden_capsule_t *cap = NULL;
if (folden_decode_buffer(data, len, &cap) == FOLDEN_OK) {
  folden_capsule_free(cap);
}
```

Other operations — merge, emit, relay adoption, vault snapshot, routing,
replication, metrics, config, ACL, and journal recovery — are declared in
`folden.h`. Refer to the header and wire spec for signatures and field
semantics.

## Status codes

| Code | Name |
|------|------|
| `0` | `FOLDEN_OK` |
| `-1` | `FOLDEN_ERR_TRUNCATED` |
| `-2` | `FOLDEN_ERR_BAD_MAGIC` |
| `-3` | `FOLDEN_ERR_BAD_VERSION` |
| `-4` | `FOLDEN_ERR_BAD_CRC` |
| `-5` | `FOLDEN_ERR_BAD_TAG` |
| `-6` | `FOLDEN_ERR_FOLD_DEPTH` |
| `-7` | `FOLDEN_ERR_NOMEM` |
| `-8` | `FOLDEN_ERR_BAD_STATE` |

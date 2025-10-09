# FOLD Capsule Wire Format (folden v1)

This document is the authoritative specification for the folden relay library.
The format is used by edge devices to ship batched telemetry capsules.

## Magic and version

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Magic `FOLD` (0x46 0x4F 0x4C 0x44) |
| 4 | 1 | Version (must be 1) |

## varfold integers

Unsigned integers use a 7-bit continuation encoding with a folden-specific twist:

- Each byte contributes 7 data bits (bits 0–6).
- Bit 7 set means another byte follows.
- Unlike LEB128, when the **third or later** continuation byte is read, its
  data bits are XORed with `0x2A` before shifting into the accumulator.

Encoding walks low-to-high 7-bit groups. Decoding reverses the process.

## Capsule body

After the header:

| Field | Type |
|-------|------|
| `lane_count` | varfold |
| `lanes[lane_count]` | lane records |
| `vault_gen` | varfold (present only if any lane carries a VAULT_GEN tag) |
| `crc32` | uint32 LE (CRC32 over all preceding bytes) |

## Lane record

| Field | Type |
|-------|------|
| `lane_id` | varfold |
| `tag_count` | varfold |
| `tags[tag_count]` | tag triples |
| `slab_len` | varfold |
| `slab` | `slab_len` bytes |

### Lane supersede

When merging two capsules, if the same `lane_id` appears in both, the **later**
capsule's lane supersedes the earlier. The superseded slab is released unless
the relay registry still holds an adopted reference.

## Tag triple

| Field | Type |
|-------|------|
| `tag_id` | varfold |
| `fold_depth` | varfold |
| `span` | varfold |

### Well-known tag IDs

| ID | Name | Semantics |
|----|------|-----------|
| 0 | END | Terminates tag run (must not appear in tag_count list) |
| 1 | FOLD_EXTEND | Increases the fold-depth budget for this lane by `span` |
| 2 | VAULT_GEN | Associates generation counter `span` with this lane |

### Fold-depth budget

Each lane starts with budget 1. Each FOLD_EXTEND tag adds `span` to the budget.
The tag index materialization allocates `budget * 16` bytes for index metadata.
If tags reference fold_depth greater than the current budget, decoding fails.

## Vault checkpoint semantics

`folden_vault_checkpoint` captures the relay registry lane table and slab pointers.
Restore rewires the live registry to the snapshot. Slabs adopted into the registry
must remain valid across checkpoint/restore cycles until explicitly released.

## Stream reassembly

Incremental decoding accepts byte chunks. A partial capsule may span multiple
chunks. The decoder commits header fields as they arrive; tag index building
must use fold state consistent with the committed portion of each lane.

## Document revision

Initial public wire specification.

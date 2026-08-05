# Linux Fieldwork handoff — deterministic cpio inode encoding

Updated: 2026-08-05
State: SOURCE TRACE COMPLETE, DETERMINISTIC TEST DESIGN NEXT
Branch base: canonical `ca8b5cdfe3853318fbc3edb7b3e39709d5516f4a`
Upstream issue: `libarchive/libarchive#3314`
Internal record: `teamleaderleo/linux-fieldwork#446`

## Upstream state

The issue is open and unassigned. A maintainer explicitly requested a PR. The reporter said they lack bandwidth and are using a tmpfs workaround. No overlapping PR was found.

## Source findings

### odc

`archive_write_set_format_cpio_odc.c` already synthesizes every nonzero inode value. It maintains a source-to-archive mapping for hardlinked entries and rejects exhaustion beyond the 18-bit field.

The mapping is keyed only by source inode. A source comment already notes that it should use device/inode pairs.

### newc

`archive_write_set_format_cpio_newc.c` writes representable source inode values directly. Values above `0xffffffff` produce an `ARCHIVE_WARN` and are truncated to the low 32 bits.

Two distinct source identities with equal low 32 bits can therefore become the same archive identity. Adjusting host-dependent test assertions alone would preserve that collision risk.

### Tests

`cpio/test/test_format_newc.c` builds its expected warning text from the real filesystem inode through `is_LargeInode()`.

`cpio/test/test_option_c.c` verifies odc inode uniqueness but depends on whatever inode identities the filesystem assigned before the odc writer remaps them.

The helper `is_LargeInode()` only checks for values above `0xffffffff`; it does not model odc's 18-bit format limit.

## Design boundary

The first candidate must be deterministic tests, not a production policy guess.

Construct archive entries directly with controlled `(dev, ino, nlink)` values and verify:

1. representable distinct identities remain distinct;
2. distinct overflow identities with identical truncated low bits do not collide;
3. repeated overflow identity used for a hardlink maps consistently;
4. mapping exhaustion returns a defined error;
5. odc and newc behavior is explicit rather than inferred from the test filesystem.

## Open policy question

A streaming writer cannot trivially preserve every in-range source inode while guaranteeing that a synthetic value assigned earlier will not collide with an in-range value encountered later.

Possible policies:

- synthesize all archive inode identities, as odc already does;
- reserve a separate synthetic `(dev, ino)` namespace;
- preserve in-range values until a collision and remap the later identity;
- buffer identity metadata, which is undesirable for streaming.

No production change should be selected until the deterministic regression demonstrates the collision and the policy tradeoff is reviewed internally.

## External-contact state

`false; none occurred`.

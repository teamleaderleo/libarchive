# Linux Fieldwork candidate — synthetic newc inode identities

Updated: 2026-08-06
State: ZERO-INODE HARDLINK GUARD ADDED, EXACT-HEAD EXECUTION PENDING
Base: canonical `ca8b5cdfe3853318fbc3edb7b3e39709d5516f4a`
Upstream issue: `libarchive/libarchive#3314`
Internal record: `teamleaderleo/linux-fieldwork#446`

## Problem

The newc writer copies source inode values into a 32-bit field. Values above
`0xffffffff` are warned about and truncated. Distinct source identities with
the same low 32 bits therefore become the same archive identity.

The odc writer already avoids its narrower inode limit by assigning compact
archive-local values. Its map, however, is keyed only by inode and contains a
source TODO to use device/inode pairs.

## Candidate policy

For newc, assign archive-local inode values to every nonzero source identity:

- preserve zero for ordinary identity-free entries and `TRAILER!!!`;
- reject `ino == 0` with `nlink >= 2`, because the writer lacks a unique
  hardlink-group key for that input;
- assign monotonically increasing values in archive order;
- retain a mapping only for entries with `nlink >= 2`;
- key hardlink mappings by `(devmajor, devminor, ino)`;
- reuse the mapped value for repeated hardlink identities;
- reject more than `UINT32_MAX` nonzero identities;
- fail on translation-map allocation errors;
- free the map with the format state.

This policy avoids the streaming collision problem inherent in preserving
arbitrary in-range source values while assigning synthetic values to overflow
entries. Output no longer depends on host inode width or numbering. Ambiguous
zero-inode hardlink groups fail before any header is emitted, preventing silent
coalescing of unrelated groups.

## Candidate tests

The focused library regression constructs explicit identities and expects:

- two large non-hardlink source inodes map to archive values 1 and 2;
- repeated `(devmajor, devminor, ino)` with `nlink=2` maps to value 3 twice;
- the same source inode on a different device maps to value 4;
- no truncation warning is emitted;
- `ino == 0` with `nlink == 2` returns `ARCHIVE_FATAL` with the exact diagnostic
  `Hardlink identity requires a nonzero inode`.

The existing detailed newc format test remains responsible for field layout,
file metadata, hardlink equality and archive framing. Its host-dependent
truncation-warning expectation is removed because the new policy is independent
of source inode width.

The candidate gate also runs `test_option_c`, the second reported failure
surface.

## Evidence carried forward

The prior exact head `bff9e374b48f8c63ba844d14c96cfac66c6d3d58` passed:

- focused run `31047099193`, job `92445166965`;
- artifact `8930552013`;
- artifact digest
  `sha256:b418feb163ccd6200ae69d2d84532c4a516e26430843975be13dae6f9b7d7201`;
- full CI run `31047099173`;
- lint run `31047099159`.

Those receipts apply to the prior head only. The zero-inode guard requires a new
exact-head result.

## CI classification

CIFuzz run `31047099243`, job `92445171131`, failed in the inherited OSS-Fuzz
build lane after checking out canonical `refs/pull/8/merge`. The tar and cpio
test builds then failed on missing generated `list.h`. The log points to fork
PR checkout/generated-header handling in that lane.

CodeQL run `31047099103` was queued at the review checkpoint.

## Risks to review

- Newc inode values change even when source values fit in 32 bits.
- Archive output becomes order-based instead of source-inode-based.
- Consumers that expect source inode preservation may observe a change, while
  cpio inode fields serve archive identity and hardlink relationships remain
  preserved for valid identities.
- Odc still uses an inode-only source map; unifying both writers remains a
  separate lane.

## External-contact state

`false; none occurred`.

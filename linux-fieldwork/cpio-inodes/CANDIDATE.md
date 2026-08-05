# Linux Fieldwork candidate — synthetic newc inode identities

Updated: 2026-08-05
State: CANDIDATE IMPLEMENTED, EXECUTION PENDING
Base: canonical `ca8b5cdfe3853318fbc3edb7b3e39709d5516f4a`
Upstream issue: `libarchive/libarchive#3314`
Internal record: `teamleaderleo/linux-fieldwork#446`

## Problem

The newc writer copies source inode values into a 32-bit field. Values above
`0xffffffff` are warned about and truncated. Distinct source identities with
the same low 32 bits therefore become the same archive identity.

The odc writer already avoids its narrower inode limit by assigning
archive-local synthetic values. Its map, however, is keyed only by inode and
contains a source TODO to use device/inode pairs.

## Candidate policy

For newc, assign archive-local inode values to every nonzero source identity:

- preserve zero for entries without an identity and for `TRAILER!!!`;
- assign monotonically increasing values in archive order;
- retain a mapping only for entries with `nlink >= 2`;
- key hardlink mappings by `(devmajor, devminor, ino)`;
- reuse the mapped value for repeated hardlink identities;
- reject more than `UINT32_MAX` nonzero identities;
- fail on translation-map allocation errors;
- free the map with the format state.

This policy avoids the streaming collision problem inherent in preserving
arbitrary in-range source values while assigning synthetic values to overflow
entries. Output no longer depends on host inode width or numbering.

## Candidate tests

The focused library regression constructs explicit identities and expects:

- two large non-hardlink source inodes map to archive values 1 and 2;
- repeated `(devmajor, devminor, ino)` with `nlink=2` maps to value 3 twice;
- the same source inode on a different device maps to value 4;
- no truncation warning is emitted.

The existing detailed newc format test remains responsible for field layout,
file metadata, hardlink equality and archive framing. Its host-dependent
truncation-warning expectation is removed because the new policy is independent
of source inode width.

The candidate gate also runs `test_option_c`, the second upstream-reported
failure surface.

## Risks to review

- Newc inode values change even when source values fit in 32 bits.
- Archive output becomes order-based rather than source-inode-based.
- Consumers that incorrectly expect source inode preservation may observe a
  change, although cpio inode fields are archive identity fields and hardlink
  relationships are preserved.
- Odc still uses an inode-only source map; unifying both writers is a possible
  follow-up but is intentionally outside this candidate.

## External-contact state

`false; none occurred`.

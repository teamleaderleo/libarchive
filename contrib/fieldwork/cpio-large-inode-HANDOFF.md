# Cpio large-inode probe handoff

Updated: `2026-08-01`  
State: `ACTIVE — BASELINE PROBE QUEUED`  
Fork: `teamleaderleo/libarchive`  
Branch: `linux-fieldwork/cpio-large-inode-probe`  
Draft PR: `teamleaderleo/libarchive#2`  
Upstream issue: `libarchive/libarchive#3314`  
External contact: `false`

## Current source identity

- fork base commit: `5cbeac6081fd4ea07ea71f6a8d3d8988f4449d68`;
- current branch head before this handoff commit: `5df8372b18c14769a19123b6cba391b81f77bb11`;
- canonical and fork `master` newc writer blob: `b9de7d362e221520d77bcf690c0d1dbddf7932f2`;
- current product source changes on this branch: none.

## Exact observed source split

`libarchive/archive_write_set_format_cpio_newc.c` reads `archive_entry_ino64(entry)`, warns when the value exceeds `0xffffffff`, and writes `ino & 0xffffffff`. Distinct same-device inode values that differ by `2^32` therefore have the same archive identity field.

`libarchive/archive_write_set_format_cpio_odc.c` already maintains synthetic inode state. It emits unique values for distinct source inodes and reuses a value for repeated hardlink identity. Its own TODO notes that the mapping should eventually use `(dev, ino)` rather than inode alone.

The cpio program test `cpio/test/test_format_newc.c` accepts the warning but still consumes the masked value. The library writer test `libarchive/test/test_write_format_cpio_newc.c` can create arbitrary inode values deterministically and is the natural product regression owner.

## Retained discriminator

`contrib/fieldwork/cpio_inode_probe.py` loads the branch-built shared library through the public C API and creates crafted entries without relying on filesystem inode allocation.

Inputs:

- same device `(0, 7)`;
- inode `1`;
- inode `0x100000001`;
- link count `2`.

Expected current distinctions:

- `NEWC_DISTINCT_COLLISION=PASS` — newc encodes both distinct inputs as inode `1` and reports an overflow warning;
- `ODC_DISTINCT_UNIQUE=PASS` — odc assigns distinct archive inode values;
- `ODC_HARDLINK_STABLE=PASS` — odc reuses one value when the source inode repeats.

The focused workflow builds the current branch, executes the probe, records exact source/probe blobs, and uploads JSON/text evidence.

## Design constraint

A single-pass writer cannot both preserve every in-range inode verbatim and guarantee that a synthetic value assigned to an earlier overflowed inode will never collide with a later in-range inode, unless it knows the complete future input set or reserves a disjoint namespace that the format does not provide.

The bounded product choices are therefore:

1. synthesize all nonzero newc inode values, following the existing odc model;
2. add a pre-scan/multi-pass facility outside the writer;
3. accept collision risk, which does not satisfy the issue's uniqueness requirement.

The first option is the smallest source-local model and preserves hardlink equivalence, though it changes the serialized inode values for entries that currently fit. This tradeoff must be explicit in the product patch and tests.

## First incomplete step

Obtain the focused CI result on the exact branch head. If the probe passes, add a deterministic library regression to `libarchive/test/test_write_format_cpio_newc.c` that proves:

- distinct source identities receive distinct archive identities;
- repeated hardlink identity receives the same archive identity;
- zero remains reserved for the trailer/unspecified inode;
- mapping exhaustion fails explicitly;
- device identity is included or consciously bounded.

Then implement the smallest newc mapping state, run the focused library test plus cpio program tests, and preserve the first product result.

## Scope guard

Do not mix this lane with cpio device-field overflow, UID/GID overflow, archive extraction behavior, PPMd, AppleDouble, or unrelated test portability.

## Authority

The fork branch and draft PR are internal controlled work. No issue comment, pull request, review, reaction, email, or other action was made in `libarchive/libarchive`.
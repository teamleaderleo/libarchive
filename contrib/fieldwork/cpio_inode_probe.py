#!/usr/bin/env python3
"""Probe cpio inode encoding through libarchive's public write API.

This is an internal Linux Fieldwork discriminator for libarchive issue #3314.
It makes no product change.  The probe gives two different filesystem inode
values the same low 32 bits and compares newc's current encoding with odc's
existing synthetic inode mapping.
"""

from __future__ import annotations

import ctypes
import json
import pathlib
import sys
from dataclasses import dataclass
from typing import Callable

ARCHIVE_OK = 0
ARCHIVE_WARN = -20
AE_IFREG = 0o100000


@dataclass(frozen=True)
class EntrySpec:
    name: str
    inode: int
    nlink: int = 2
    devmajor: int = 0
    devminor: int = 7


def align4(value: int) -> int:
    return (value + 3) & ~3


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def parse_newc(data: bytes) -> list[dict[str, int | str]]:
    records: list[dict[str, int | str]] = []
    offset = 0
    while offset + 110 <= len(data):
        magic = data[offset : offset + 6]
        require(magic in (b"070701", b"070702"), f"bad newc magic at {offset}: {magic!r}")
        inode = int(data[offset + 6 : offset + 14], 16)
        filesize = int(data[offset + 54 : offset + 62], 16)
        namesize = int(data[offset + 94 : offset + 102], 16)
        require(namesize > 0, f"bad newc namesize at {offset}: {namesize}")
        name_start = offset + 110
        name_bytes = data[name_start : name_start + namesize - 1]
        name = name_bytes.decode("utf-8")
        body_start = align4(name_start + namesize)
        next_offset = align4(body_start + filesize)
        if name == "TRAILER!!!":
            break
        records.append({"name": name, "inode": inode})
        require(next_offset > offset, "newc parser made no progress")
        offset = next_offset
    return records


def parse_odc(data: bytes) -> list[dict[str, int | str]]:
    records: list[dict[str, int | str]] = []
    offset = 0
    while offset + 76 <= len(data):
        magic = data[offset : offset + 6]
        require(magic == b"070707", f"bad odc magic at {offset}: {magic!r}")
        inode = int(data[offset + 12 : offset + 18], 8)
        namesize = int(data[offset + 59 : offset + 65], 8)
        filesize = int(data[offset + 65 : offset + 76], 8)
        require(namesize > 0, f"bad odc namesize at {offset}: {namesize}")
        name_start = offset + 76
        name_bytes = data[name_start : name_start + namesize - 1]
        name = name_bytes.decode("utf-8")
        next_offset = name_start + namesize + filesize
        if name == "TRAILER!!!":
            break
        records.append({"name": name, "inode": inode})
        require(next_offset > offset, "odc parser made no progress")
        offset = next_offset
    return records


def configure(lib: ctypes.CDLL) -> None:
    pointer = ctypes.c_void_p
    lib.archive_write_new.restype = pointer
    lib.archive_write_set_format_cpio_newc.argtypes = [pointer]
    lib.archive_write_set_format_cpio_newc.restype = ctypes.c_int
    lib.archive_write_set_format_cpio_odc.argtypes = [pointer]
    lib.archive_write_set_format_cpio_odc.restype = ctypes.c_int
    lib.archive_write_add_filter_none.argtypes = [pointer]
    lib.archive_write_add_filter_none.restype = ctypes.c_int
    lib.archive_write_open_memory.argtypes = [
        pointer,
        pointer,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    ]
    lib.archive_write_open_memory.restype = ctypes.c_int
    lib.archive_write_header.argtypes = [pointer, pointer]
    lib.archive_write_header.restype = ctypes.c_int
    lib.archive_write_finish_entry.argtypes = [pointer]
    lib.archive_write_finish_entry.restype = ctypes.c_int
    lib.archive_write_close.argtypes = [pointer]
    lib.archive_write_close.restype = ctypes.c_int
    lib.archive_write_free.argtypes = [pointer]
    lib.archive_write_free.restype = ctypes.c_int
    lib.archive_error_string.argtypes = [pointer]
    lib.archive_error_string.restype = ctypes.c_char_p

    lib.archive_entry_new.restype = pointer
    lib.archive_entry_free.argtypes = [pointer]
    lib.archive_entry_free.restype = None
    lib.archive_entry_set_pathname.argtypes = [pointer, ctypes.c_char_p]
    lib.archive_entry_set_pathname.restype = None
    lib.archive_entry_set_mode.argtypes = [pointer, ctypes.c_uint]
    lib.archive_entry_set_mode.restype = None
    lib.archive_entry_set_size.argtypes = [pointer, ctypes.c_int64]
    lib.archive_entry_set_size.restype = None
    lib.archive_entry_set_devmajor.argtypes = [pointer, ctypes.c_int64]
    lib.archive_entry_set_devmajor.restype = None
    lib.archive_entry_set_devminor.argtypes = [pointer, ctypes.c_int64]
    lib.archive_entry_set_devminor.restype = None
    lib.archive_entry_set_ino64.argtypes = [pointer, ctypes.c_int64]
    lib.archive_entry_set_ino64.restype = None
    lib.archive_entry_set_nlink.argtypes = [pointer, ctypes.c_uint]
    lib.archive_entry_set_nlink.restype = None


def archive_error(lib: ctypes.CDLL, archive: int) -> str:
    value = lib.archive_error_string(archive)
    return value.decode("utf-8", errors="replace") if value else "(no error string)"


def write_archive(
    lib: ctypes.CDLL,
    formatter: Callable[[int], int],
    entries: list[EntrySpec],
    allowed_header_statuses: set[int],
) -> tuple[bytes, list[int]]:
    archive = lib.archive_write_new()
    require(bool(archive), "archive_write_new returned NULL")
    buffer = ctypes.create_string_buffer(16384)
    used = ctypes.c_size_t(0)
    statuses: list[int] = []
    try:
        require(formatter(archive) == ARCHIVE_OK, f"format setup failed: {archive_error(lib, archive)}")
        require(
            lib.archive_write_add_filter_none(archive) == ARCHIVE_OK,
            f"filter setup failed: {archive_error(lib, archive)}",
        )
        require(
            lib.archive_write_open_memory(
                archive,
                ctypes.cast(buffer, ctypes.c_void_p),
                len(buffer),
                ctypes.byref(used),
            )
            == ARCHIVE_OK,
            f"open memory failed: {archive_error(lib, archive)}",
        )

        for spec in entries:
            entry = lib.archive_entry_new()
            require(bool(entry), "archive_entry_new returned NULL")
            try:
                lib.archive_entry_set_pathname(entry, spec.name.encode("utf-8"))
                lib.archive_entry_set_mode(entry, AE_IFREG | 0o644)
                lib.archive_entry_set_size(entry, 0)
                lib.archive_entry_set_devmajor(entry, spec.devmajor)
                lib.archive_entry_set_devminor(entry, spec.devminor)
                lib.archive_entry_set_ino64(entry, spec.inode)
                lib.archive_entry_set_nlink(entry, spec.nlink)
                status = lib.archive_write_header(archive, entry)
                statuses.append(status)
                require(
                    status in allowed_header_statuses,
                    f"header {spec.name!r} returned {status}: {archive_error(lib, archive)}",
                )
                require(
                    lib.archive_write_finish_entry(archive) == ARCHIVE_OK,
                    f"finish {spec.name!r} failed: {archive_error(lib, archive)}",
                )
            finally:
                lib.archive_entry_free(entry)

        require(
            lib.archive_write_close(archive) == ARCHIVE_OK,
            f"archive close failed: {archive_error(lib, archive)}",
        )
        return bytes(buffer.raw[: used.value]), statuses
    finally:
        result = lib.archive_write_free(archive)
        require(result == ARCHIVE_OK, f"archive_write_free returned {result}")


def main() -> int:
    if len(sys.argv) not in (2, 3):
        print(f"usage: {sys.argv[0]} /path/to/libarchive.so [result.json]", file=sys.stderr)
        return 2

    library_path = pathlib.Path(sys.argv[1]).resolve()
    require(library_path.is_file(), f"library does not exist: {library_path}")
    lib = ctypes.CDLL(str(library_path))
    configure(lib)

    distinct = [
        EntrySpec("small", 1),
        EntrySpec("large", 0x1_0000_0001),
    ]
    repeated_large = [
        EntrySpec("large-a", 0x1_0000_0001),
        EntrySpec("large-b", 0x1_0000_0001),
    ]

    newc_bytes, newc_statuses = write_archive(
        lib,
        lib.archive_write_set_format_cpio_newc,
        distinct,
        {ARCHIVE_OK, ARCHIVE_WARN},
    )
    odc_distinct_bytes, odc_distinct_statuses = write_archive(
        lib,
        lib.archive_write_set_format_cpio_odc,
        distinct,
        {ARCHIVE_OK},
    )
    odc_link_bytes, odc_link_statuses = write_archive(
        lib,
        lib.archive_write_set_format_cpio_odc,
        repeated_large,
        {ARCHIVE_OK},
    )

    newc = parse_newc(newc_bytes)
    odc_distinct = parse_odc(odc_distinct_bytes)
    odc_link = parse_odc(odc_link_bytes)

    require(len(newc) == 2, f"expected two newc records, got {newc}")
    require(len(odc_distinct) == 2, f"expected two odc distinct records, got {odc_distinct}")
    require(len(odc_link) == 2, f"expected two odc hardlink records, got {odc_link}")

    newc_collision = newc[0]["inode"] == newc[1]["inode"]
    odc_unique = odc_distinct[0]["inode"] != odc_distinct[1]["inode"]
    odc_hardlink_stable = odc_link[0]["inode"] == odc_link[1]["inode"]

    require(newc_collision, f"newc collision was not reproduced: {newc}")
    require(odc_unique, f"odc did not synthesize unique values: {odc_distinct}")
    require(odc_hardlink_stable, f"odc did not preserve hardlink identity: {odc_link}")
    require(ARCHIVE_WARN in newc_statuses, f"newc overflow warning absent: {newc_statuses}")

    result = {
        "schema_version": 1,
        "library": str(library_path),
        "input": {
            "small_inode": 1,
            "large_inode": 0x1_0000_0001,
            "same_device": [0, 7],
            "nlink": 2,
        },
        "newc": {
            "records": newc,
            "header_statuses": newc_statuses,
            "distinct_inputs_collide": newc_collision,
        },
        "odc_distinct": {
            "records": odc_distinct,
            "header_statuses": odc_distinct_statuses,
            "distinct_inputs_are_unique": odc_unique,
        },
        "odc_hardlink": {
            "records": odc_link,
            "header_statuses": odc_link_statuses,
            "same_input_reuses_value": odc_hardlink_stable,
        },
        "conclusion": (
            "newc masks overflowed inode values and can collide; odc already "
            "provides the synthetic unique/stable mapping precedent"
        ),
        "product_source_changed": False,
        "external_contact": False,
    }

    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    print(rendered, end="")
    print("NEWC_DISTINCT_COLLISION=PASS")
    print("ODC_DISTINCT_UNIQUE=PASS")
    print("ODC_HARDLINK_STABLE=PASS")

    if len(sys.argv) == 3:
        pathlib.Path(sys.argv[2]).write_text(rendered, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

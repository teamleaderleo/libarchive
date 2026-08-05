#!/usr/bin/env python3
from pathlib import Path

script_path = Path(__file__).with_name("apply_synthesis_candidate.py")
script = script_path.read_text()

old = '''static int64_t\\tsynthesize_ino_value(struct cpio *, struct archive_entry *);'''
new = '''struct cpio;

static int64_t\\tsynthesize_ino_value(struct cpio *, struct archive_entry *);'''
if script.count(old) != 1:
    raise RuntimeError("failed to insert cpio forward declaration")
script = script.replace(old, new, 1)

exec(compile(script, str(script_path), "exec"), {"__name__": "__main__"})

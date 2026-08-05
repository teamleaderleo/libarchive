#!/usr/bin/env python3
from pathlib import Path

script_path = Path(__file__).with_name("apply_synthesis_candidate.py")
script = script_path.read_text()

old = '''static ssize_t\\tarchive_write_newc_data(struct archive_write *,
'''
new = '''struct cpio;

static ssize_t\\tarchive_write_newc_data(struct archive_write *,
'''
if script.count(old) != 1:
    raise RuntimeError("failed to insert cpio forward declaration")
script = script.replace(old, new, 1)

exec(compile(script, str(script_path), "exec"), {"__name__": "__main__"})

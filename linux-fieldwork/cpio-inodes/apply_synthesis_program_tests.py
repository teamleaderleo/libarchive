#!/usr/bin/env python3
import re
from pathlib import Path

path = Path("cpio/test/test_format_newc.c")
text = path.read_text()

pattern = re.compile(
    r'''\t/\* Setup result message\. \*/\n'''
    r'''\tmemset\(result, 0, sizeof\(result\)\);\n'''
    r'''.*?'''
    r'''\n\t/\* Record some facts about what we just created: \*/''',
    re.DOTALL,
)
replacement = '''\t/* Synthetic archive-local inode values do not depend on the source\n\t * filesystem inode width, so no truncation warnings are expected. */\n\tmemset(result, 0, sizeof(result));\n\n\t/* Record some facts about what we just created: */'''
text, count = pattern.subn(replacement, text, count=1)
if count != 1:
    raise RuntimeError(f"expected one newc warning block, found {count}")

path.write_text(text)
print("bsdcpio-newc-warning-expectations-updated")

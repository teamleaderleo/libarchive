#!/usr/bin/env python3
from pathlib import Path

path = Path("libarchive/archive_write_set_format_cpio_newc.c")
text = path.read_text()


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    text = text.replace(old, new, 1)


replace_once(
    '#include "archive_entry_locale.h"\n#include "archive_private.h"\n',
    '#include "archive_entry_locale.h"\n#include "archive_integer.h"\n#include "archive_private.h"\n',
    "checked allocation include",
)
replace_once(
    '''static int\tformat_hex(int64_t, void *, int);
static int64_t\tformat_hex_recursive(int64_t, char *, int);
static int\twrite_header(struct archive_write *, struct archive_entry *);

struct cpio {
\tuint64_t\t  entry_bytes_remaining;
\tint\t\t  padding;

\tstruct archive_string_conv *opt_sconv;''',
    '''static int\tformat_hex(int64_t, void *, int);
static int64_t\tformat_hex_recursive(int64_t, char *, int);
static int64_t\tsynthesize_ino_value(struct cpio *, struct archive_entry *);
static int\twrite_header(struct archive_write *, struct archive_entry *);

struct cpio_ino {
\tint64_t\tdevmajor;
\tint64_t\tdevminor;
\tint64_t\tino;
\tuint32_t\tino_new;
};

struct cpio {
\tuint64_t\t  entry_bytes_remaining;
\tint\t\t  padding;
\tuint64_t\t  ino_next;

\tstruct cpio_ino *ino_list;
\tsize_t\t\t  ino_list_size;
\tsize_t\t\t  ino_list_next;

\tstruct archive_string_conv *opt_sconv;''',
    "cpio inode-map state",
)

insert_marker = '''static struct archive_string_conv *
get_sconv(struct archive_write *a)
'''
if text.count(insert_marker) != 1:
    raise RuntimeError("get_sconv insertion marker mismatch")

helper = r'''/*
 * The newc inode field is only 32 bits, while source inode values can be
 * wider.  Assign archive-local inode values to every nonzero source identity
 * so output never depends on truncation and remains collision-free.
 *
 * Hardlinks are identified by the source (device major, device minor, inode)
 * tuple.  Entries with a link count below two do not need a retained mapping.
 */
static int64_t
synthesize_ino_value(struct cpio *cpio, struct archive_entry *entry)
{
	int64_t devmajor = archive_entry_devmajor(entry);
	int64_t devminor = archive_entry_devminor(entry);
	int64_t ino = archive_entry_ino64(entry);
	uint64_t ino_new;
	size_t i;

	/* Preserve the zero value used by entries without an identity and by the
	 * end-of-archive marker. */
	if (ino == 0)
		return (0);

	if (archive_entry_nlink(entry) >= 2) {
		for (i = 0; i < cpio->ino_list_next; ++i) {
			if (cpio->ino_list[i].devmajor == devmajor
			    && cpio->ino_list[i].devminor == devminor
			    && cpio->ino_list[i].ino == ino)
				return (cpio->ino_list[i].ino_new);
		}
	}

	if (cpio->ino_next >= UINT32_MAX)
		return (-2);
	ino_new = ++cpio->ino_next;

	if (archive_entry_nlink(entry) < 2)
		return ((int64_t)ino_new);

	if (cpio->ino_list_size <= cpio->ino_list_next) {
		size_t newsize, size;
		void *newlist;

		if (cpio->ino_list_size < 512)
			newsize = 512;
		else if (archive_ckd_mul_size(&newsize,
		    cpio->ino_list_size, 2))
			return (-1);
		if (archive_ckd_mul_size(&size,
		    newsize, sizeof(cpio->ino_list[0])))
			return (-1);
		newlist = realloc(cpio->ino_list, size);
		if (newlist == NULL)
			return (-1);

		cpio->ino_list_size = newsize;
		cpio->ino_list = newlist;
	}

	cpio->ino_list[cpio->ino_list_next].devmajor = devmajor;
	cpio->ino_list[cpio->ino_list_next].devminor = devminor;
	cpio->ino_list[cpio->ino_list_next].ino = ino;
	cpio->ino_list[cpio->ino_list_next].ino_new = (uint32_t)ino_new;
	++cpio->ino_list_next;
	return ((int64_t)ino_new);
}

'''
text = text.replace(insert_marker, helper + insert_marker, 1)

replace_once(
    '''\tino = archive_entry_ino64(entry);
\tif (ino > 0xffffffff) {
\t\tarchive_set_error(&a->archive, ERANGE,
\t\t    "large inode number truncated");
\t\tret_final = ARCHIVE_WARN;
\t}

\t/* TODO: Set ret_final to ARCHIVE_WARN if any of these overflow. */
\tformat_hex(ino & 0xffffffff, h + c_ino_offset, c_ino_size);''',
    '''\tino = synthesize_ino_value(cpio, entry);
\tif (ino == -1) {
\t\tarchive_set_error(&a->archive, ENOMEM,
\t\t    "No memory for inode translation table");
\t\tret_final = ARCHIVE_FATAL;
\t\tgoto exit_write_header;
\t}
\tif (ino == -2) {
\t\tarchive_set_error(&a->archive, ERANGE,
\t\t    "Too many files for this cpio format");
\t\tret_final = ARCHIVE_FATAL;
\t\tgoto exit_write_header;
\t}

\t/* TODO: Set ret_final to ARCHIVE_WARN if any of these overflow. */
\tformat_hex(ino, h + c_ino_offset, c_ino_size);''',
    "newc inode encoding",
)
replace_once(
    '''\tstruct cpio *cpio = a->format_data;

\tfree(cpio);''',
    '''\tstruct cpio *cpio = a->format_data;

\tfree(cpio->ino_list);
\tfree(cpio);''',
    "inode-map cleanup",
)

path.write_text(text)
print("newc-synthetic-inode-candidate-applied")

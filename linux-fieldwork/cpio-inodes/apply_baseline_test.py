#!/usr/bin/env python3
from pathlib import Path

path = Path("libarchive/test/test_write_format_cpio.c")
text = path.read_text()

marker = "\n\nDEFINE_TEST(test_write_format_cpio)\n"
if text.count(marker) != 1:
    raise RuntimeError("unexpected test_write_format_cpio marker count")

helper = r'''

static void
test_newc_large_inode_collision(void)
{
	static const int64_t source_inodes[] = {
		INT64_C(0x100000001),
		INT64_C(0x200000001)
	};
	static const char *const names[] = {"large-a", "large-b"};
	struct archive_entry *ae;
	struct archive *a;
	int64_t archived_inodes[2];
	char *buff;
	size_t buffsize = 10240;
	size_t used = 0;
	int i;

	buff = malloc(buffsize);
	assert(buff != NULL);

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_cpio_newc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	for (i = 0; i < 2; ++i) {
		assert((ae = archive_entry_new()) != NULL);
		archive_entry_copy_pathname(ae, names[i]);
		archive_entry_set_filetype(ae, AE_IFREG);
		archive_entry_set_perm(ae, 0644);
		archive_entry_set_size(ae, 0);
		archive_entry_set_devmajor(ae, 1);
		archive_entry_set_devminor(ae, 1);
		archive_entry_set_ino64(ae, source_inodes[i]);
		archive_entry_set_nlink(ae, 1);

		assertEqualIntA(a, ARCHIVE_WARN, archive_write_header(a, ae));
		assertEqualString("large inode number truncated",
		    archive_error_string(a));
		assertEqualIntA(a, ARCHIVE_OK, archive_write_finish_entry(a));
		archive_entry_free(ae);
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cpio(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, buff, used));

	for (i = 0; i < 2; ++i) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualString(names[i], archive_entry_pathname(ae));
		archived_inodes[i] = archive_entry_ino64(ae);
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	failure("Distinct source inode values %#jx and %#jx were encoded as "
	    "%#jx and %#jx",
	    (uintmax_t)source_inodes[0], (uintmax_t)source_inodes[1],
	    (uintmax_t)archived_inodes[0], (uintmax_t)archived_inodes[1]);
	assertEqualInt(1, archived_inodes[0]);
	assertEqualInt(archived_inodes[0], archived_inodes[1]);

	free(buff);
}
'''
text = text.replace(marker, helper + marker, 1)

call = "\ttest_format(archive_write_set_format_cpio_newc);\n"
if text.count(call) != 1:
    raise RuntimeError("unexpected newc test call count")
text = text.replace(
    call,
    call + "\ttest_newc_large_inode_collision();\n",
    1,
)

path.write_text(text)
print("newc-large-inode-collision-test-applied")

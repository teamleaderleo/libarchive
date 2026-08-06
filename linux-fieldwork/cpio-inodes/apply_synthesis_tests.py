#!/usr/bin/env python3
from pathlib import Path

path = Path("libarchive/test/test_write_format_cpio.c")
text = path.read_text()

marker = "\n\nDEFINE_TEST(test_write_format_cpio)\n"
if text.count(marker) != 1:
    raise RuntimeError("unexpected test_write_format_cpio marker count")

helper = r'''

static void
test_newc_synthetic_inode_mapping(void)
{
	static const int64_t source_inodes[] = {
		INT64_C(0x100000001),
		INT64_C(0x200000001),
		INT64_C(0x300000001),
		INT64_C(0x300000001),
		INT64_C(0x300000001)
	};
	static const int devminors[] = {1, 1, 2, 2, 3};
	static const int nlinks[] = {1, 1, 2, 2, 2};
	static const char *const names[] = {
		"large-a", "large-b", "hard-a", "hard-b", "other-device"
	};
	static const int64_t expected_archive_inodes[] = {1, 2, 3, 3, 4};
	struct archive_entry *ae;
	struct archive *a;
	char *buff;
	size_t buffsize = 20480;
	size_t used = 0;
	int i;

	buff = malloc(buffsize);
	assert(buff != NULL);

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_cpio_newc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	for (i = 0; i < 5; ++i) {
		assert((ae = archive_entry_new()) != NULL);
		archive_entry_copy_pathname(ae, names[i]);
		archive_entry_set_filetype(ae, AE_IFREG);
		archive_entry_set_perm(ae, 0644);
		archive_entry_set_size(ae, 0);
		archive_entry_set_devmajor(ae, 1);
		archive_entry_set_devminor(ae, devminors[i]);
		archive_entry_set_ino64(ae, source_inodes[i]);
		archive_entry_set_nlink(ae, nlinks[i]);

		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		assert(archive_error_string(a) == NULL);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_finish_entry(a));
		archive_entry_free(ae);
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cpio(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, buff, used));

	for (i = 0; i < 5; ++i) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualString(names[i], archive_entry_pathname(ae));
		assertEqualInt(expected_archive_inodes[i], archive_entry_ino64(ae));
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
}

static void
test_newc_zero_inode_hardlink_rejected(void)
{
	struct archive_entry *ae;
	struct archive *a;
	char buff[1024];
	size_t used = 0;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_cpio_newc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "ambiguous-zero-hardlink");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, 0644);
	archive_entry_set_size(ae, 0);
	archive_entry_set_devmajor(ae, 1);
	archive_entry_set_devminor(ae, 1);
	archive_entry_set_ino64(ae, 0);
	archive_entry_set_nlink(ae, 2);

	assertEqualIntA(a, ARCHIVE_FATAL, archive_write_header(a, ae));
	assertEqualString("Hardlink identity requires a nonzero inode",
	    archive_error_string(a));

	archive_entry_free(ae);
	(void)archive_write_free(a);
}
'''
text = text.replace(marker, helper + marker, 1)

call = "\ttest_format(archive_write_set_format_cpio_newc);\n"
if text.count(call) != 1:
    raise RuntimeError("unexpected newc test call count")
text = text.replace(
    call,
    call
    + "\ttest_newc_synthetic_inode_mapping();\n"
    + "\ttest_newc_zero_inode_hardlink_rejected();\n",
    1,
)

path.write_text(text)
print("newc-synthetic-inode-tests-applied")

/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

static int
is_hex(const char *p, size_t l)
{
	while (l > 0) {
		if (*p >= '0' && *p <= '9') {
			/* Ascii digit */
		} else if (*p >= 'a' && *p <= 'f') {
			/* lowercase letter a-f */
		} else {
			/* Not hex. */
			return (0);
		}
		--l;
		++p;
	}
	return (1);
}

/*
 * Detailed verification that cpio 'newc' archives are written with
 * the correct format.
 */
DEFINE_TEST(test_write_format_cpio_newc)
{
	struct archive *a;
	struct archive_entry *entry;
	char *buff, *e, *file;
	size_t buffsize = 100000;
	size_t used;

	buff = malloc(buffsize);

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, 0, archive_write_set_format_cpio_newc(a));
	assertEqualIntA(a, 0, archive_write_add_filter_none(a));
	assertEqualIntA(a, 0, archive_write_open_memory(a, buff, buffsize, &used));

	/*
	 * Add various files to it.
	 * TODO: Extend this to cover more filetypes.
	 */

	/* Regular file */
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_mtime(entry, 1, 10);
	archive_entry_set_pathname(entry, "file");
	archive_entry_set_mode(entry, S_IFREG | 0664);
	archive_entry_set_size(entry, 10);
	archive_entry_set_uid(entry, 80);
	archive_entry_set_gid(entry, 90);
	archive_entry_set_dev(entry, 12);
	archive_entry_set_ino(entry, 89);
	archive_entry_set_nlink(entry, 1);
	assertEqualIntA(a, 0, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualIntA(a, 10, archive_write_data(a, "1234567890", 10));

	/* Directory */
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_mtime(entry, 2, 20);
	archive_entry_set_pathname(entry, "dir");
	archive_entry_set_mode(entry, S_IFDIR | 0775);
	archive_entry_set_size(entry, 10);
	archive_entry_set_nlink(entry, 2);
	assertEqualIntA(a, 0, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualIntA(a, 0, archive_write_data(a, "1234567890", 10));

	/* Symlink */
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_mtime(entry, 3, 30);
	archive_entry_set_pathname(entry, "lnk");
	archive_entry_set_mode(entry, 0664);
	archive_entry_set_filetype(entry, AE_IFLNK);
	archive_entry_set_size(entry, 0);
	archive_entry_set_uid(entry, 83);
	archive_entry_set_gid(entry, 93);
	archive_entry_set_dev(entry, 13);
	archive_entry_set_ino(entry, 88);
	archive_entry_set_nlink(entry, 1);
	archive_entry_set_symlink(entry,"a");
	assertEqualIntA(a, 0, archive_write_header(a, entry));
	archive_entry_free(entry);

	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/*
	 * Verify the archive format.
	 */
	e = buff;

	/* First entry is "file" */
	file = e;
	assert(is_hex(e, 110)); /* Entire header is hex digits. */
	assertEqualMem(e + 0, "070701", 6); /* Magic */
	assert(memcmp(e + 6, "00000000", 8) != 0); /* ino != 0 */
	assertEqualMem(e + 14, "000081b4", 8); /* Mode */
	assertEqualMem(e + 22, "00000050", 8); /* uid */
	assertEqualMem(e + 30, "0000005a", 8); /* gid */
	assertEqualMem(e + 38, "00000001", 8); /* nlink */
	assertEqualMem(e + 46, "00000001", 8); /* mtime */
	assertEqualMem(e + 54, "0000000a", 8); /* File size */
	assertEqualMem(e + 62, "00000000", 8); /* devmajor */
	assertEqualMem(e + 70, "0000000c", 8); /* devminor */
	assertEqualMem(e + 78, "00000000", 8); /* rdevmajor */
	assertEqualMem(e + 86, "00000000", 8); /* rdevminor */
	assertEqualMem(e + 94, "00000005", 8); /* Name size */
	assertEqualMem(e + 102, "00000000", 8); /* CRC */
	assertEqualMem(e + 110, "file\0\0", 6); /* Name contents */
	assertEqualMem(e + 116, "1234567890", 10); /* File body */
	assertEqualMem(e + 126, "\0\0", 2); /* Pad to multiple of 4 */
	e += 128; /* Must be multiple of four here! */

	/* Second entry is "dir" */
	assert(is_hex(e, 110));
	assertEqualMem(e + 0, "070701", 6); /* Magic */
	assertEqualMem(e + 6, "00000000", 8); /* ino */
	assertEqualMem(e + 14, "000041fd", 8); /* Mode */
	assertEqualMem(e + 22, "00000000", 8); /* uid */
	assertEqualMem(e + 30, "00000000", 8); /* gid */
	assertEqualMem(e + 38, "00000002", 8); /* nlink */
	assertEqualMem(e + 46, "00000002", 8); /* mtime */
	assertEqualMem(e + 54, "00000000", 8); /* File size */
	assertEqualMem(e + 62, "00000000", 8); /* devmajor */
	assertEqualMem(e + 70, "00000000", 8); /* devminor */
	assertEqualMem(e + 78, "00000000", 8); /* rdevmajor */
	assertEqualMem(e + 86, "00000000", 8); /* rdevminor */
	assertEqualMem(e + 94, "00000004", 8); /* Name size */
	assertEqualMem(e + 102, "00000000", 8); /* CRC */
	assertEqualMem(e + 110, "dir\0", 4); /* name */
	assertEqualMem(e + 114, "\0\0", 2); /* Pad to multiple of 4 */
	e += 116; /* Must be multiple of four here! */

	/* Third entry is "lnk" */
	assert(is_hex(e, 110)); /* Entire header is hex digits. */
	assertEqualMem(e + 0, "070701", 6); /* Magic */
	assert(memcmp(e + 6, file + 6, 8) != 0); /* ino != file ino */
	assert(memcmp(e + 6, "00000000", 8) != 0); /* ino != 0 */
	assertEqualMem(e + 14, "0000a1b4", 8); /* Mode */
	assertEqualMem(e + 22, "00000053", 8); /* uid */
	assertEqualMem(e + 30, "0000005d", 8); /* gid */
	assertEqualMem(e + 38, "00000001", 8); /* nlink */
	assertEqualMem(e + 46, "00000003", 8); /* mtime */
	assertEqualMem(e + 54, "00000001", 8); /* File size */
	assertEqualMem(e + 62, "00000000", 8); /* devmajor */
	assertEqualMem(e + 70, "0000000d", 8); /* devminor */
	assertEqualMem(e + 78, "00000000", 8); /* rdevmajor */
	assertEqualMem(e + 86, "00000000", 8); /* rdevminor */
	assertEqualMem(e + 94, "00000004", 8); /* Name size */
	assertEqualMem(e + 102, "00000000", 8); /* CRC */
	assertEqualMem(e + 110, "lnk\0\0\0", 6); /* Name contents */
	assertEqualMem(e + 116, "a\0\0\0", 4); /* File body + pad */
	e += 120; /* Must be multiple of four here! */

	/* TODO: Verify other types of entries. */

	/* Last entry is end-of-archive marker. */
	assert(is_hex(e, 76));
	assertEqualMem(e + 0, "070701", 6); /* Magic */
	assertEqualMem(e + 6, "00000000", 8); /* ino */
	assertEqualMem(e + 14, "00000000", 8); /* Mode */
	assertEqualMem(e + 22, "00000000", 8); /* uid */
	assertEqualMem(e + 30, "00000000", 8); /* gid */
	assertEqualMem(e + 38, "00000001", 8); /* nlink */
	assertEqualMem(e + 46, "00000000", 8); /* mtime */
	assertEqualMem(e + 54, "00000000", 8); /* File size */
	assertEqualMem(e + 62, "00000000", 8); /* devmajor */
	assertEqualMem(e + 70, "00000000", 8); /* devminor */
	assertEqualMem(e + 78, "00000000", 8); /* rdevmajor */
	assertEqualMem(e + 86, "00000000", 8); /* rdevminor */
	assertEqualMem(e + 94, "0000000b", 8); /* Name size */
	assertEqualMem(e + 102, "00000000", 8); /* CRC */
	assertEqualMem(e + 110, "TRAILER!!!\0", 11); /* Name */
	assertEqualMem(e + 121, "\0\0\0", 3); /* Pad to multiple of 4 bytes */
	e += 124; /* Must be multiple of four here! */

	assertEqualInt((int)used, e - buff);

	free(buff);
}

/*
 * Verify the identity contract used when source inode numbers do not fit in
 * newc's 32-bit inode field.  In-range, unlinked entries retain their inode;
 * distinct overflow identities receive distinct nonzero values; repeated
 * hardlink identities remain stable; and device identity is part of the key.
 */
DEFINE_TEST(test_write_format_cpio_newc_large_inode_identity)
{
	struct archive *a;
	struct archive_entry *entry;
	char buff[4096];
	char *small_entry, *large_a, *large_b, *large_a_link, *large_other_dev;
	size_t used;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_cpio_newc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "small");
	archive_entry_set_mode(entry, S_IFREG | 0644);
	archive_entry_set_size(entry, 0);
	archive_entry_set_devmajor(entry, 0);
	archive_entry_set_devminor(entry, 7);
	archive_entry_set_ino64(entry, 1);
	archive_entry_set_nlink(entry, 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "big-a");
	archive_entry_set_mode(entry, S_IFREG | 0644);
	archive_entry_set_size(entry, 0);
	archive_entry_set_devmajor(entry, 0);
	archive_entry_set_devminor(entry, 7);
	archive_entry_set_ino64(entry, INT64_C(0x100000001));
	archive_entry_set_nlink(entry, 2);
	assertEqualIntA(a, ARCHIVE_WARN, archive_write_header(a, entry));
	archive_entry_free(entry);

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "big-b");
	archive_entry_set_mode(entry, S_IFREG | 0644);
	archive_entry_set_size(entry, 0);
	archive_entry_set_devmajor(entry, 0);
	archive_entry_set_devminor(entry, 7);
	archive_entry_set_ino64(entry, INT64_C(0x200000001));
	archive_entry_set_nlink(entry, 2);
	assertEqualIntA(a, ARCHIVE_WARN, archive_write_header(a, entry));
	archive_entry_free(entry);

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "big-a2");
	archive_entry_set_mode(entry, S_IFREG | 0644);
	archive_entry_set_size(entry, 0);
	archive_entry_set_devmajor(entry, 0);
	archive_entry_set_devminor(entry, 7);
	archive_entry_set_ino64(entry, INT64_C(0x100000001));
	archive_entry_set_nlink(entry, 2);
	assertEqualIntA(a, ARCHIVE_WARN, archive_write_header(a, entry));
	archive_entry_free(entry);

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "big-c");
	archive_entry_set_mode(entry, S_IFREG | 0644);
	archive_entry_set_size(entry, 0);
	archive_entry_set_devmajor(entry, 0);
	archive_entry_set_devminor(entry, 8);
	archive_entry_set_ino64(entry, INT64_C(0x100000001));
	archive_entry_set_nlink(entry, 2);
	assertEqualIntA(a, ARCHIVE_WARN, archive_write_header(a, entry));
	archive_entry_free(entry);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	small_entry = buff;
	large_a = small_entry + 116;
	large_b = large_a + 116;
	large_a_link = large_b + 116;
	large_other_dev = large_a_link + 120;

	assert(is_hex(small_entry, 110));
	assert(is_hex(large_a, 110));
	assert(is_hex(large_b, 110));
	assert(is_hex(large_a_link, 110));
	assert(is_hex(large_other_dev, 110));
	assertEqualMem(small_entry, "070701", 6);
	assertEqualMem(large_a, "070701", 6);
	assertEqualMem(large_b, "070701", 6);
	assertEqualMem(large_a_link, "070701", 6);
	assertEqualMem(large_other_dev, "070701", 6);
	assertEqualMem(small_entry + 110, "small\0", 6);
	assertEqualMem(large_a + 110, "big-a\0", 6);
	assertEqualMem(large_b + 110, "big-b\0", 6);
	assertEqualMem(large_a_link + 110, "big-a2\0", 7);
	assertEqualMem(large_other_dev + 110, "big-c\0", 6);

	assertEqualMem(small_entry + 6, "00000001", 8);
	assert(memcmp(large_a + 6, "00000000", 8) != 0);
	assert(memcmp(large_b + 6, "00000000", 8) != 0);
	assert(memcmp(large_other_dev + 6, "00000000", 8) != 0);
	assert(memcmp(small_entry + 6, large_a + 6, 8) != 0);
	assert(memcmp(large_a + 6, large_b + 6, 8) != 0);
	assertEqualMem(large_a + 6, large_a_link + 6, 8);
	assert(memcmp(large_a + 6, large_other_dev + 6, 8) != 0);
}

/*
 * Inode zero remains reserved for ordinary unlinked entries, but multiply
 * linked entries still require a stable device/inode identity.  Verify that
 * one zero-inode hardlink group is stable, a group on another device remains
 * distinct, and an ordinary zero-inode entry stays serialized as zero.
 */
DEFINE_TEST(test_write_format_cpio_newc_zero_inode_hardlinks)
{
	struct archive *a;
	struct archive_entry *entry;
	char buff[2048];
	char *zero_single, *zero_a, *zero_a_link, *zero_b;
	size_t used;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_cpio_newc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "zero");
	archive_entry_set_mode(entry, S_IFREG | 0644);
	archive_entry_set_size(entry, 0);
	archive_entry_set_devmajor(entry, 0);
	archive_entry_set_devminor(entry, 7);
	archive_entry_set_ino64(entry, 0);
	archive_entry_set_nlink(entry, 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "zero-a");
	archive_entry_set_mode(entry, S_IFREG | 0644);
	archive_entry_set_size(entry, 0);
	archive_entry_set_devmajor(entry, 0);
	archive_entry_set_devminor(entry, 7);
	archive_entry_set_ino64(entry, 0);
	archive_entry_set_nlink(entry, 2);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "zero-a2");
	archive_entry_set_mode(entry, S_IFREG | 0644);
	archive_entry_set_size(entry, 0);
	archive_entry_set_devmajor(entry, 0);
	archive_entry_set_devminor(entry, 7);
	archive_entry_set_ino64(entry, 0);
	archive_entry_set_nlink(entry, 2);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "zero-b");
	archive_entry_set_mode(entry, S_IFREG | 0644);
	archive_entry_set_size(entry, 0);
	archive_entry_set_devmajor(entry, 0);
	archive_entry_set_devminor(entry, 8);
	archive_entry_set_ino64(entry, 0);
	archive_entry_set_nlink(entry, 2);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	zero_single = buff;
	zero_a = zero_single + 116;
	zero_a_link = zero_a + 120;
	zero_b = zero_a_link + 120;

	assertEqualMem(zero_single + 110, "zero\0", 5);
	assertEqualMem(zero_a + 110, "zero-a\0", 7);
	assertEqualMem(zero_a_link + 110, "zero-a2\0", 8);
	assertEqualMem(zero_b + 110, "zero-b\0", 7);
	assertEqualMem(zero_single + 6, "00000000", 8);
	assert(memcmp(zero_a + 6, "00000000", 8) != 0);
	assertEqualMem(zero_a + 6, zero_a_link + 6, 8);
	assert(memcmp(zero_a + 6, zero_b + 6, 8) != 0);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cpio(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("zero", archive_entry_pathname(entry));
	assert(archive_entry_hardlink(entry) == NULL);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("zero-a", archive_entry_pathname(entry));
	assert(archive_entry_hardlink(entry) == NULL);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("zero-a2", archive_entry_pathname(entry));
	assertEqualString("zero-a", archive_entry_hardlink(entry));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("zero-b", archive_entry_pathname(entry));
	assert(archive_entry_hardlink(entry) == NULL);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &entry));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

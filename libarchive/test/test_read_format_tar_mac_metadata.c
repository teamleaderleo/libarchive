/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Zhaofeng Li
 * All rights reserved.
 */
#include "test.h"

#include <locale.h>

static void
test_standalone_mac_metadata(void)
{
	static const struct {
		const char *name;
		const char *data;
	} entries[] = {
		{"._fileC", "metadata for file C"},
		{"fileC", "content of file C"},
		{"._fileA", "content of file A"},
		{"._fileB", "content of file B"},
	};
	char buff[10240], data[32];
	struct archive *a;
	struct archive_entry *ae;
	size_t i, metadata_size, used;
	const void *metadata;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	assert((ae = archive_entry_new()) != NULL);
	for (i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
		archive_entry_clear(ae);
		archive_entry_set_pathname(ae, entries[i].name);
		archive_entry_set_mode(ae, AE_IFREG | 0644);
		archive_entry_set_size(ae, strlen(entries[i].data));
		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		assertEqualIntA(a, (int)strlen(entries[i].data),
		    (int)archive_write_data(a, entries[i].data,
		    strlen(entries[i].data)));
	}
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_option(a, "tar", "mac-ext", "1"));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("fileC", archive_entry_pathname(ae));
	metadata = archive_entry_mac_metadata(ae, &metadata_size);
	assertEqualInt(sizeof("metadata for file C") - 1, metadata_size);
	assertEqualMem("metadata for file C", metadata, metadata_size);
	assertEqualIntA(a, sizeof("content of file C") - 1,
	    archive_read_data(a, data, sizeof(data)));
	assertEqualMem("content of file C", data, sizeof("content of file C") - 1);

	for (i = 2; i < sizeof(entries) / sizeof(entries[0]); i++) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualString(entries[i].name, archive_entry_pathname(ae));
		assertEqualIntA(a, (int)strlen(entries[i].data),
		    (int)archive_read_data(a, data, sizeof(data)));
		assertEqualMem(entries[i].data, data, strlen(entries[i].data));
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_standalone_mac_metadata_wide_pathname(void)
{
	static const char metadata_name[] = "._\xcf\x80";
	static const wchar_t metadata_name_w[] = L"._\x03c0";
	static const char metadata_data[] = "standalone metadata";
	static const char next_name[] = "unrelated";
	static const char next_data[] = "ordinary data";
	char buff[10240], data[32];
	char *saved_locale;
	const char *current_locale;
	struct archive *a;
	struct archive_entry *ae;
	size_t used;
	int r;

	current_locale = setlocale(LC_ALL, NULL);
	saved_locale = current_locale == NULL ? NULL : strdup(current_locale);
	if (setlocale(LC_ALL, "en_US.UTF-8") == NULL) {
		free(saved_locale);
		skipping("en_US.UTF-8 locale not available on this system");
		return;
	}

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_options(a, "hdrcharset=UTF-8"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	assert((ae = archive_entry_new()) != NULL);

	archive_entry_set_pathname(ae, metadata_name);
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, sizeof(metadata_data) - 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, sizeof(metadata_data) - 1,
	    archive_write_data(a, metadata_data, sizeof(metadata_data) - 1));

	archive_entry_clear(ae);
	archive_entry_set_pathname(ae, next_name);
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, sizeof(next_data) - 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, sizeof(next_data) - 1,
	    archive_write_data(a, next_data, sizeof(next_data) - 1));

	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	if (setlocale(LC_ALL, "C") == NULL) {
		if (saved_locale != NULL)
			setlocale(LC_ALL, saved_locale);
		free(saved_locale);
		skipping("C locale not available on this system");
		return;
	}

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_option(a, "tar", "mac-ext", "1"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_option(a, "tar", "hdrcharset", "UTF-8"));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));

	r = archive_read_next_header(a, &ae);
	assert(r == ARCHIVE_OK || r == ARCHIVE_WARN);
	assertEqualWString(metadata_name_w, archive_entry_pathname_w(ae));
	assertEqualIntA(a, sizeof(metadata_data) - 1,
	    archive_read_data(a, data, sizeof(data)));
	assertEqualMem(metadata_data, data, sizeof(metadata_data) - 1);

	r = archive_read_next_header(a, &ae);
	assert(r == ARCHIVE_OK || r == ARCHIVE_WARN);
	assertEqualString(next_name, archive_entry_pathname(ae));
	assertEqualIntA(a, sizeof(next_data) - 1,
	    archive_read_data(a, data, sizeof(data)));
	assertEqualMem(next_data, data, sizeof(next_data) - 1);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	if (saved_locale != NULL)
		setlocale(LC_ALL, saved_locale);
	free(saved_locale);
}

DEFINE_TEST(test_read_format_tar_mac_metadata)
{
	/*
	 This test tar file is crafted with two files in a specific order:

	 1. A ._-prefixed file with pax header containing the path attribute.
	 2. A file with a pax header but without the path attribute.

	 It's designed to trigger the case encountered in:
	 <https://github.com/libarchive/libarchive/pull/2636>

	 GNU tar is required to reproduce this tar file:

	 ```sh
	 NAME1="._101_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	 NAME2="goodname"
	 OUT="test_read_format_tar_mac_metadata_1.tar"

	 echo "content of badname" >"${NAME1}"
	 echo "content of goodname" >"${NAME2}"

	 rm -f "${OUT}"
	 gnutar \
	 	--mtime="@0" \
	 	--owner=0 --group=0 --numeric-owner \
	 	--pax-option=exthdr.name=%d/PaxHeaders/%f,atime:=0,ctime:=0,foo:=bar \
	 	--format=pax \
	 	-cf "${OUT}" \
	 	"${NAME1}" \
	 	"${NAME2}"
	 uuencode "${OUT}" "${OUT}" >"${OUT}.uu"

	 sha256sum "${OUT}"
	 sha256sum "${OUT}.uu"
	 ```
	*/
	const char *refname = "test_read_format_tar_mac_metadata_1.tar";
	char *p;
	size_t s;
	struct archive *a;
	struct archive_entry *ae;

	/*
	 * This is not a valid AppleDouble metadata file. It is merely to test that
	 * the correct bytes are read.
	 */
	const unsigned char appledouble[] = {
		0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x20, 0x6f, 0x66, 0x20, 0x62,
		0x61, 0x64, 0x6e, 0x61, 0x6d, 0x65, 0x0a
	};

	extract_reference_file(refname);
	p = slurpfile(&s, "%s", refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_set_option(a, "tar", "mac-ext", "1"));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, p, s, 1));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	/* Correct name and metadata bytes */
	assertEqualString("goodname", archive_entry_pathname(ae));

	const void *metadata = archive_entry_mac_metadata(ae, &s);
	if (assert(metadata != NULL)) {
		assertEqualMem(metadata, appledouble,
			sizeof(appledouble));
	}

	/* ... and nothing else */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(p);
	test_standalone_mac_metadata();
	test_standalone_mac_metadata_wide_pathname();
}

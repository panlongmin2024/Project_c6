/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <test_fat.h>

const char test_str[] = "hello world! i am testing now, do you know? can i help you?";

int check_file_dir_exists(const char *path)
{
	int res;
	struct fs_dirent entry;

	/* Verify fs_stat() */
	res = fs_stat(path, &entry);

	return !res;
}


/*
 * test_bin_info.c
 *
 * Babeltrace SO info tests
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Antoine Busque <abusque@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <babeltrace/assert-internal.h>
#include <lttng-utils/debug-info/bin-info.h>

#include "tap/tap.h"

#define NR_TESTS 36
#define SO_NAME "libhello_so"

#define DWARF_DIR_NAME "dwarf_full"
#define ELF_DIR_NAME "elf_only"
#define BUILDID_DIR_NAME "build_id"
#define DEBUGLINK_DIR_NAME "debug_link"

#define SO_LOW_ADDR 0x400000
#define SO_MEMSZ 0x400000
#define FUNC_FOO_ADDR 0x402367
#define FUNC_FOO_LINE_NO 36
#define FUNC_FOO_FILENAME "./libhello.c"
#define FUNC_FOO_TP_ADDR 0x402300
#define FUNC_FOO_TP_LINE_NO 35
#define FUNC_FOO_TP_FILENAME "./libhello.c"
#define FUNC_FOO_ADDR_ELF 0x402367
#define FUNC_FOO_ADDR_DBG_LINK 0x402367
#define FUNC_FOO_NAME "foo+0xf0"
#define FUNC_FOO_NAME_ELF "foo+0xf0"
#define BUILD_ID_LEN 20

char *opt_debug_info_dir;
char *opt_debug_info_target_prefix;

static
void test_bin_info_build_id(const char *bin_info_dir)
{
	int ret;
	char *data_dir, *bin_path;
	char *func_name = NULL;
	struct bin_info *bin = NULL;
	struct source_location *src_loc = NULL;
	struct bt_fd_cache fdc;
	uint8_t build_id[BUILD_ID_LEN] = {
		0xcd, 0xd9, 0x8c, 0xdd, 0x87, 0xf7, 0xfe, 0x64, 0xc1, 0x3b,
		0x6d, 0xaa, 0xd5, 0x53, 0x98, 0x7e, 0xaf, 0xd4, 0x0c, 0xbb
	};

	diag("bin-info tests - separate DWARF via build ID");

	data_dir = g_build_filename(bin_info_dir, BUILDID_DIR_NAME, NULL);
	bin_path = g_build_filename(bin_info_dir, BUILDID_DIR_NAME, SO_NAME, NULL);

	if (data_dir == NULL || bin_path == NULL) {
		exit(EXIT_FAILURE);
	}

	ret = bt_fd_cache_init(&fdc);
	BT_ASSERT(ret == 0);
	bin = bin_info_create(&fdc, bin_path, SO_LOW_ADDR, SO_MEMSZ, true, data_dir, NULL);
	ok(bin != NULL, "bin_info_create successful");

	/* Test setting build_id */
	ret = bin_info_set_build_id(bin, build_id, BUILD_ID_LEN);
	ok(ret == 0, "bin_info_set_build_id successful");

	/* Test function name lookup (with DWARF) */
	ret = bin_info_lookup_function_name(bin, FUNC_FOO_ADDR, &func_name);
	ok(ret == 0, "bin_info_lookup_function_name successful");
	if (func_name) {
		ok(strcmp(func_name, FUNC_FOO_NAME) == 0,
			"bin_info_lookup_function_name - correct func_name value");
		free(func_name);
	} else {
		skip(1, "bin_info_lookup_function_name - func_name is NULL");
	}

	/* Test source location lookup */
	ret = bin_info_lookup_source_location(bin, FUNC_FOO_ADDR, &src_loc);
	ok(ret == 0, "bin_info_lookup_source_location successful");
	if (src_loc) {
		ok(src_loc->line_no == FUNC_FOO_LINE_NO,
			"bin_info_lookup_source_location - correct line_no");
		ok(strcmp(src_loc->filename, FUNC_FOO_FILENAME) == 0,
			"bin_info_lookup_source_location - correct filename");
		source_location_destroy(src_loc);
	} else {
		skip(2, "bin_info_lookup_source_location - src_loc is NULL");
	}

	bin_info_destroy(bin);
	bt_fd_cache_fini(&fdc);
	g_free(data_dir);
	g_free(bin_path);
}

static
void test_bin_info_debug_link(const char *bin_info_dir)
{
	int ret;
	char *data_dir, *bin_path;
	char *func_name = NULL;
	struct bin_info *bin = NULL;
	struct source_location *src_loc = NULL;
	char *dbg_filename = "libhello_so.debug";
	uint32_t crc = 0x289a8fdc;
	struct bt_fd_cache fdc;

	diag("bin-info tests - separate DWARF via debug link");

	data_dir = g_build_filename(bin_info_dir, DEBUGLINK_DIR_NAME, NULL);
	bin_path = g_build_filename(bin_info_dir, DEBUGLINK_DIR_NAME, SO_NAME, NULL);

	if (data_dir == NULL || bin_path == NULL) {
		exit(EXIT_FAILURE);
	}

	ret = bt_fd_cache_init(&fdc);
	BT_ASSERT(ret == 0);
	bin = bin_info_create(&fdc, bin_path, SO_LOW_ADDR, SO_MEMSZ, true, data_dir,
			NULL);
	ok(bin != NULL, "bin_info_create successful");

	/* Test setting debug link */
	ret = bin_info_set_debug_link(bin, dbg_filename, crc);
	ok(ret == 0, "bin_info_set_debug_link successful");

	/* Test function name lookup (with DWARF) */
	ret = bin_info_lookup_function_name(bin, FUNC_FOO_ADDR_DBG_LINK,
					&func_name);
	ok(ret == 0, "bin_info_lookup_function_name successful");
	if (func_name) {
		ok(strcmp(func_name, FUNC_FOO_NAME) == 0,
			"bin_info_lookup_function_name - correct func_name value");
		free(func_name);
	} else {
		skip(1, "bin_info_lookup_function_name - func_name is NULL");
	}

	/* Test source location lookup */
	ret = bin_info_lookup_source_location(bin, FUNC_FOO_ADDR_DBG_LINK,
					&src_loc);
	ok(ret == 0, "bin_info_lookup_source_location successful");
	if (src_loc) {
		ok(src_loc->line_no == FUNC_FOO_LINE_NO,
			"bin_info_lookup_source_location - correct line_no");
		ok(strcmp(src_loc->filename, FUNC_FOO_FILENAME) == 0,
			"bin_info_lookup_source_location - correct filename");
		source_location_destroy(src_loc);
	} else {
		skip(2, "bin_info_lookup_source_location - src_loc is NULL");
	}

	bin_info_destroy(bin);
	bt_fd_cache_fini(&fdc);
	g_free(data_dir);
	g_free(bin_path);
}

static
void test_bin_info_elf(const char *bin_info_dir)
{
	int ret;
	char *data_dir, *bin_path;
	char *func_name = NULL;
	struct bin_info *bin = NULL;
	struct source_location *src_loc = NULL;
	struct bt_fd_cache fdc;

	diag("bin-info tests - ELF only");

	data_dir = g_build_filename(bin_info_dir, ELF_DIR_NAME, NULL);
	bin_path = g_build_filename(bin_info_dir, ELF_DIR_NAME, SO_NAME, NULL);

	if (data_dir == NULL || bin_path == NULL) {
		exit(EXIT_FAILURE);
	}

	ret = bt_fd_cache_init(&fdc);
	BT_ASSERT(ret == 0);
	bin = bin_info_create(&fdc, bin_path, SO_LOW_ADDR, SO_MEMSZ, true, data_dir, NULL);
	ok(bin != NULL, "bin_info_create successful");

	/* Test function name lookup (with ELF) */
	ret = bin_info_lookup_function_name(bin, FUNC_FOO_ADDR_ELF, &func_name);
	ok(ret == 0, "bin_info_lookup_function_name successful");
	if (func_name) {
		ok(strcmp(func_name, FUNC_FOO_NAME_ELF) == 0,
			"bin_info_lookup_function_name - correct func_name value");
		free(func_name);
		func_name = NULL;
	} else {
		skip(1, "bin_info_lookup_function_name - func_name is NULL");
	}

	/* Test function name lookup - erroneous address */
	ret = bin_info_lookup_function_name(bin, 0, &func_name);
	ok(ret == -1 && func_name == NULL,
		"bin_info_lookup_function_name - fail on addr not found");

	/* Test source location location - should fail on ELF only file  */
	ret = bin_info_lookup_source_location(bin, FUNC_FOO_ADDR_ELF, &src_loc);
	ok(ret == -1, "bin_info_lookup_source_location - fail on ELF only file");

	source_location_destroy(src_loc);
	bin_info_destroy(bin);
	bt_fd_cache_fini(&fdc);
	g_free(data_dir);
	g_free(bin_path);
}

static
void test_bin_info(const char *bin_info_dir)
{
	int ret;
	char *data_dir, *bin_path;
	char *func_name = NULL;
	struct bin_info *bin = NULL;
	struct source_location *src_loc = NULL;
	struct bt_fd_cache fdc;

	diag("bin-info tests - DWARF bundled with SO file");


	data_dir = g_build_filename(bin_info_dir, DWARF_DIR_NAME, NULL);
	bin_path = g_build_filename(bin_info_dir, DWARF_DIR_NAME, SO_NAME, NULL);

	if (data_dir == NULL || bin_path == NULL) {
		exit(EXIT_FAILURE);
	}

	ret = bt_fd_cache_init(&fdc);
	BT_ASSERT(ret == 0);
	bin = bin_info_create(&fdc, bin_path, SO_LOW_ADDR, SO_MEMSZ, true, data_dir, NULL);
	ok(bin != NULL, "bin_info_create successful");

	/* Test bin_info_has_address */
	ret = bin_info_has_address(bin, 0);
	ok(ret == 0, "bin_info_has_address - address under so's range");
	ret = bin_info_has_address(bin, SO_LOW_ADDR);
	ok(ret == 1, "bin_info_has_address - lower bound of so's range");
	ret = bin_info_has_address(bin, FUNC_FOO_ADDR);
	ok(ret == 1, "bin_info_has_address - address in so's range");
	ret = bin_info_has_address(bin, SO_LOW_ADDR + SO_MEMSZ - 1);
	ok(ret == 1, "bin_info_has_address - upper bound of so's range");
	ret = bin_info_has_address(bin, SO_LOW_ADDR + SO_MEMSZ);
	ok(ret == 0, "bin_info_has_address - address above so's range");

	/* Test function name lookup (with DWARF) */
	ret = bin_info_lookup_function_name(bin, FUNC_FOO_ADDR, &func_name);
	ok(ret == 0, "bin_info_lookup_function_name successful");
	if (func_name) {
		ok(strcmp(func_name, FUNC_FOO_NAME) == 0,
			"bin_info_lookup_function_name - correct func_name value");
		free(func_name);
		func_name = NULL;
	} else {
		skip(1, "bin_info_lookup_function_name - func_name is NULL");
	}

	/* Test function name lookup - erroneous address */
	ret = bin_info_lookup_function_name(bin, 0, &func_name);
	ok(ret == -1 && func_name == NULL,
		"bin_info_lookup_function_name - fail on addr not found");

	/* Test source location lookup */
	ret = bin_info_lookup_source_location(bin, FUNC_FOO_ADDR, &src_loc);
	ok(ret == 0, "bin_info_lookup_source_location successful");
	if (src_loc) {
		ok(src_loc->line_no == FUNC_FOO_LINE_NO,
			"bin_info_lookup_source_location - correct line_no");
		ok(strcmp(src_loc->filename, FUNC_FOO_FILENAME) == 0,
			"bin_info_lookup_source_location - correct filename");
		source_location_destroy(src_loc);
		src_loc = NULL;
	} else {
		skip(2, "bin_info_lookup_source_location - src_loc is NULL");
	}

	/* Test source location lookup - inlined function */
	ret = bin_info_lookup_source_location(bin, FUNC_FOO_TP_ADDR, &src_loc);
	ok(ret == 0,
		"bin_info_lookup_source_location (inlined func) successful");
	if (src_loc) {
		ok(src_loc->line_no == FUNC_FOO_TP_LINE_NO,
			"bin_info_lookup_source_location (inlined func) - correct line_no");
		ok(strcmp(src_loc->filename, FUNC_FOO_TP_FILENAME) == 0,
			"bin_info_lookup_source_location (inlined func) - correct filename");
		source_location_destroy(src_loc);
		src_loc = NULL;
	} else {
		skip(2, "bin_info_lookup_source_location (inlined func) - src_loc is NULL");
	}

	/* Test source location lookup - erroneous address */
	ret = bin_info_lookup_source_location(bin, 0, &src_loc);
	ok(ret == -1 && src_loc == NULL,
		"bin_info_lookup_source_location - fail on addr not found");

	bin_info_destroy(bin);
	bt_fd_cache_fini(&fdc);
	g_free(data_dir);
	g_free(bin_path);
}

int main(int argc, char **argv)
{
	int ret;

	plan_tests(NR_TESTS);

	if (argc != 2) {
		return EXIT_FAILURE;
	} else {
		opt_debug_info_dir = argv[1];
	}

	ret = bin_info_init();
	ok(ret == 0, "bin_info_init successful");

	test_bin_info(opt_debug_info_dir);
	test_bin_info_elf(opt_debug_info_dir);
	test_bin_info_build_id(opt_debug_info_dir);
	test_bin_info_debug_link(opt_debug_info_dir);

	return EXIT_SUCCESS;
}

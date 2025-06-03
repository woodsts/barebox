// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: © 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

/* cp.c - copy files */

#include <common.h>
#include <command.h>
#include <xfuncs.h>
#include <linux/stat.h>
#include <libbb.h>
#include <fs.h>
#include <malloc.h>
#include <libgen.h>
#include <getopt.h>
#include <libfile.h>

/**
 * @param[in] argc Argument count from command line
 * @param[in] argv List of input arguments
 */
static int do_cp(int argc, char *argv[])
{
	int ret = 1;
	struct stat statbuf;
	int last_is_dir = 0;
	int i;
	int opt;
	int flags = 0, recursive = 0;
	int argc_min;

	while ((opt = getopt(argc, argv, "vnr")) > 0) {
		switch (opt) {
		case 'v':
			flags |= COPY_FILE_VERBOSE;
			break;
		case 'n':
			flags |= COPY_FILE_NO_OVERWRITE;
			break;
		case 'r':
			recursive = 1;
			break;
		}
	}

	argc_min = optind + 2;

	if (argc < argc_min)
		return COMMAND_ERROR_USAGE;

	if (!stat(argv[argc - 1], &statbuf)) {
		if (S_ISDIR(statbuf.st_mode))
			last_is_dir = 1;
	}

	if (((recursive && argc - optind > 2) || (argc > argc_min)) && !last_is_dir) {
		printf("cp: target `%s' is not a directory\n", argv[argc - 1]);
		return 1;
	}

	if (recursive && argc - optind == 2 && !last_is_dir) {
		ret = make_directory(argv[argc - 1]);
		if (ret)
			goto out;
	}

	for (i = optind; i < argc - 1; i++) {
		char *dst;

		dst = concat_path_file(argv[argc - 1], posix_basename(argv[i]));

		if (recursive)
			ret = copy_recursive(argv[i], dst, flags);
		else if (last_is_dir)
			ret = copy_file(argv[i], dst, flags);
		else
			ret = copy_file(argv[i], argv[argc - 1], flags);

		free(dst);
		if (ret)
			goto out;
	}

	ret = 0;
out:
	return ret;
}

BAREBOX_CMD_HELP_START(cp)
BAREBOX_CMD_HELP_TEXT("Copy file from SRC to DEST.")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-r", "recursive")
BAREBOX_CMD_HELP_OPT ("-n", "do not overwrite an existing file")
BAREBOX_CMD_HELP_OPT ("-v", "verbose")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(cp)
	.cmd		= do_cp,
	BAREBOX_CMD_DESC("copy files")
	BAREBOX_CMD_OPTS("[-rnv] SRC DEST")
	BAREBOX_CMD_GROUP(CMD_GRP_FILE)
	BAREBOX_CMD_HELP(cmd_cp_help)
BAREBOX_CMD_END

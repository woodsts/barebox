// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: © 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

/*
 * partition.c - parse a linux-like mtd partition definition and register
 *               the new partitions
 */

/**
 * @file
 * @brief partition handling and addpart and delpart command
 */

#include <common.h>
#include <command.h>
#include <complete.h>
#include <driver.h>
#include <malloc.h>
#include <errno.h>
#include <xfuncs.h>
#include <fs.h>
#include <linux/stat.h>
#include <libgen.h>
#include <getopt.h>
#include <cmdlinepart.h>
#include <linux/err.h>

static int do_addpart(int argc, char *argv[])
{
	const char *devname;
	loff_t devsize;
	struct stat s;
	int opt;
	unsigned int flags = CMDLINEPART_ADD_DEVNAME;

	while ((opt = getopt(argc, argv, "nf")) > 0) {
		switch (opt) {
		case 'n':
			flags &= ~CMDLINEPART_ADD_DEVNAME;
			break;
		case 'f':
			flags |= CMDLINEPART_FORCE;
			break;
		}
	}

	if (argc != optind + 2)
		return COMMAND_ERROR_USAGE;

	if (stat(argv[optind], &s)) {
		perror("addpart");
		return 1;
	}
	devsize = s.st_size;

	devname = kbasename(argv[optind]);

	return cmdlinepart_do_parse(devname, argv[optind + 1], devsize, flags);
}

BAREBOX_CMD_HELP_START(addpart)

BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-n", "do not use the device name as prefix of the partition name")
BAREBOX_CMD_HELP_OPT ("-f", "force partition creation, even if it overlaps existing ones")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Create partitions on device DEVICE using the partition description")
BAREBOX_CMD_HELP_TEXT("from PART.")
BAREBOX_CMD_HELP_TEXT("PART contains a partition description compatible to the Kernel mtd")
BAREBOX_CMD_HELP_TEXT("commandline partition description:")
BAREBOX_CMD_HELP_TEXT("SIZE1[@OFFSET1](NAME1)[RO],SIZE2[@OFFSET2](NAME2)[RO],...")
BAREBOX_CMD_HELP_TEXT("The size and the offset can be given in decimal (without any prefix) and")
BAREBOX_CMD_HELP_TEXT("in hex (prefixed with 0x). Both can have an optional suffix K, M or G.")
BAREBOX_CMD_HELP_TEXT("The size of the last partition can be specified as '-' for the remaining")
BAREBOX_CMD_HELP_TEXT("space on the device.")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(addpart)
	.cmd = do_addpart,
	BAREBOX_CMD_DESC("add a partition description to a device")
	BAREBOX_CMD_OPTS("[-nf] DEVICE PART")
	BAREBOX_CMD_GROUP(CMD_GRP_PART)
	BAREBOX_CMD_HELP(cmd_addpart_help)
BAREBOX_CMD_END

static int do_delpart(int argc, char *argv[])
{
	int i, err;

	for (i = 1; i < argc; i++) {
		err = devfs_del_partition(kbasename(argv[i]));
		if (err) {
			printf("cannot delete %s: %pe\n", argv[i], ERR_PTR(err));
			break;
		}
	}

	return 1;
}

BAREBOX_CMD_HELP_START(delpart)
BAREBOX_CMD_HELP_TEXT("Delete partitions previously added to a device with addpart.")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(delpart)
	.cmd = do_delpart,
	BAREBOX_CMD_DESC("delete partition(s)")
	BAREBOX_CMD_OPTS("PART...")
	BAREBOX_CMD_GROUP(CMD_GRP_PART)
	BAREBOX_CMD_HELP(cmd_delpart_help)
	BAREBOX_CMD_COMPLETE(devfs_partition_complete)
BAREBOX_CMD_END

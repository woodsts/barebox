/*
 * misc.c - various assorted functions
 *
 * Copyright (c) 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <fs.h>
#include <string.h>
#include <linux/ctype.h>
#include <getopt.h>
#include <libfile.h>

/*
 * Like simple_strtoull() but handles an optional G, M, K or k
 * suffix for Gigabyte, Megabyte or Kilobyte
 */
unsigned long long strtoull_suffix(const char *str, char **endp, int base)
{
	unsigned long long val;
	char *end;

	val = simple_strtoull(str, &end, base);

	switch (*end) {
	case 'G':
		val *= 1024;
		fallthrough;
	case 'M':
		val *= 1024;
		fallthrough;
	case 'k':
	case 'K':
		val *= 1024;
		end++;
		break;
	default:
		break;
	}

	if (strncmp(end, "iB", 2) == 0)
		end += 2;

	if (endp)
		*endp = end;

	return val;
}
EXPORT_SYMBOL(strtoull_suffix);

unsigned long strtoul_suffix(const char *str, char **endp, int base)
{
	return strtoull_suffix(str, endp, base);
}
EXPORT_SYMBOL(strtoul_suffix);

/*
 * This function parses strings in the form <startadr>[-endaddr]
 * or <startadr>[+size] and fills in start and size accordingly.
 * <startadr> and <endadr> can be given in decimal or hex (with 0x prefix)
 * and can have an optional G, M, K or k suffix.
 *
 * examples:
 * 0x1000-0x2000 -> start = 0x1000, size = 0x1001
 * 0x1000+0x1000 -> start = 0x1000, size = 0x1000
 * 0x1000        -> start = 0x1000, size = ~0
 * 1M+1k         -> start = 0x100000, size = 0x400
 */
int parse_area_spec(const char *str, loff_t *start, loff_t *size)
{
	char *endp;
	loff_t end, _start, _size;

	if (!isdigit(*str))
		return -1;

	_start = strtoull_suffix(str, &endp, 0);

	str = endp;

	if (!*str) {
		/* beginning given, but no size, assume maximum size */
		_size = ~0;
		goto success;
	}

	if (*str == '-') {
		/* beginning and end given */
		if (!isdigit(*(str + 1)))
			return -1;

		end = strtoull_suffix(str + 1, &endp, 0);
		str = endp;
		if (end < _start) {
			printf("end < start\n");
			return -1;
		}
		_size = end - _start + 1;
		goto success;
	}

	if (*str == '+') {
		/* beginning and size given */
		if (!isdigit(*(str + 1)))
			return -1;

		_size = strtoull_suffix(str + 1, &endp, 0);
		str = endp;
		goto success;
	}

	return -1;

success:
	if (*str && !isspace(*str))
		return -1;
	*start = _start;
	*size = _size;
	return 0;
}
EXPORT_SYMBOL(parse_area_spec);

/*
 * Common function for parsing options for the 'md', 'mw', 'memcpy', 'memcmp'
 * commands.
 */
int mem_parse_options(int argc, char *argv[], char *optstr, int *mode,
		      char **sourcefile, char **destfile, int *swab)
{
	int opt;

	while((opt = getopt(argc, argv, optstr)) > 0) {
		switch(opt) {
		case 'b':
			*mode = O_RWSIZE_1;
			break;
		case 'w':
			*mode = O_RWSIZE_2;
			break;
		case 'l':
			*mode = O_RWSIZE_4;
			break;
		case 'q':
			*mode = O_RWSIZE_8;
			break;
		case 's':
			*sourcefile = optarg;
			break;
		case 'd':
			*destfile = optarg;
			break;
		case 'x':
			if (!swab)
				return -EINVAL;
			*swab = 1;
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

int memcpy_parse_options(int argc, char *argv[], int *sourcefd,
			 int *destfd, loff_t *count,
			 int rwsize, int destmode)
{
	loff_t dest, src;
	int mode = rwsize;
	char *sourcefile = NULL;
	char *destfile = NULL;
	struct  stat statbuf;

	if (mem_parse_options(argc, argv, "bwlqs:d:", &mode, &sourcefile,
			      &destfile, NULL) < 0)
		return -EINVAL;

	if (optind + 2 > argc)
		return -EINVAL;

	src = strtoull_suffix(argv[optind], NULL, 0);
	dest = strtoull_suffix(argv[optind + 1], NULL, 0);

	if (optind + 2 == argc) {
		if (!sourcefile) {
			printf("source and count not given\n");
			return -EINVAL;
		}
		if (stat(sourcefile, &statbuf)) {
			perror("stat");
			return -1;
		}
		*count = statbuf.st_size - src;
	} else {
		*count = strtoull_suffix(argv[optind + 2], NULL, 0);
	}

	sourcefile = sourcefile ?: "/dev/mem";
	destfile = destfile ?: "/dev/mem";

	*sourcefd = open_and_lseek(sourcefile, mode | O_RDONLY, src);
	if (*sourcefd < 0) {
		printf("Could not open source file \"%s\": %m\n", sourcefile);
		return -1;
	}

	*destfd = open_and_lseek(destfile, mode | destmode, dest);
	if (*destfd < 0) {
		printf("Could not open destination file \"%s\": %m\n", destfile);
		close(*sourcefd);
		return -1;
	}

	return 0;
}

/*
 * Copyright (C) 2016 Pengutronix, Steffen Trumtrar <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __BLOBGEN_H__
#define __BLOBGEN_H__

#include <common.h>
#include <crypto/sha.h>

enum access_rights {
	KERNEL,
	KERNEL_EVM,
	USERSPACE,
};

enum operation {
	ENCRYPT,
	DECRYPT,
};

#define KEYMOD_LENGTH		16
#define MAX_BLOB_LEN		4096
#define BLOCKSIZE_BYTES		8

struct blob_param {
	struct param_d *param;
	char *value;
};

struct blob_data_blob {
	u8 data[MAX_BLOB_LEN];
};

struct blobgen {
	struct device_d dev;
	void *priv;
	struct ablkcipher_request *req;
	int (*run)(struct blobgen* bg, enum operation op);
	int (*pre_encryption)(struct blobgen* bg);
	int (*post_encryption)(struct blobgen* bg);
	int (*pre_decryption)(struct blobgen* bg);
	int (*post_decryption)(struct blobgen* bg);

	struct blob_data_blob *plaintext;
	struct blob_data_blob *ciphertext;

	struct blob_param modifier;
	struct blob_param payload;
	struct blob_param blob;

	bool busy;
	enum access_rights access;
	unsigned int payload_size;
	unsigned int max_payload_size;
};

int blob_gen_register(struct device_d *dev, struct blobgen *bg);

#endif

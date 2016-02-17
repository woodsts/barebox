/*
 * Copyright (C) 2016 Pengutronix, Steffen Trumtrar <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#include <common.h>
#include <dma.h>
#include <digest.h>
#include <driver.h>
#include <init.h>
#include <blobgen.h>
#include <stdlib.h>
#include <crypto.h>
#include <crypto/mxc_scc.h>

#define MAX_IVLEN		BLOCKSIZE_BYTES
#define BLOB_FOOTER_LEN		(MAX_IVLEN + SHA256_DIGEST_SIZE)

static int blob_run_crypto(struct blobgen *bg, enum operation op)
{
	int ret = 0;

	ret = op == ENCRYPT ?
		mxc_scc_cbc_des_encrypt(bg->req) :
		mxc_scc_cbc_des_decrypt(bg->req);

	return ret;
}

static int blob_hash_data(char *algo, u8 *src, u8 *dst, u16 size)
{
	struct digest *d;
	int ret;
	int cur = 0;
	int offset = 0;

	d = digest_alloc(algo);
	if (!d) {
		pr_err("Unable to allocate digest %s\n", algo);
		return -EINVAL;
	}

	ret = digest_init(d);
	if (ret)
		return ret;

	while (size) {
		cur = min((u16) MAX_BLOB_LEN, size);

		ret = digest_update(d, src + offset, cur);
		size -= cur;
		offset += cur;
	}

	ret = digest_final(d, dst);

	return 0;
}

/* Fixed padding for appending to plaintext to fill out a block */
static char block_padding[8] = { 0x80, 0, 0, 0, 0, 0, 0, 0 };

static int blob_gen_pre_encryption(struct blobgen *bg)
{
	int padding_byte_count = ((bg->payload_size + BLOCKSIZE_BYTES - 1) &
				  ~(BLOCKSIZE_BYTES - 1)) - bg->payload_size;
	u8 padding_buffer[sizeof(u16) + sizeof(block_padding)];
	struct blob_param *payload = &bg->payload;
	u8 __iomem *hash;

	memcpy(bg->plaintext->data, bg->modifier.value, KEYMOD_LENGTH);
	memcpy(bg->plaintext->data + KEYMOD_LENGTH, payload->value, bg->payload_size);

	bg->payload_size += KEYMOD_LENGTH;

	if (padding_byte_count) {
		memcpy(padding_buffer, block_padding, padding_byte_count);
		memcpy(bg->plaintext->data + bg->payload_size, padding_buffer,
		       padding_byte_count);
		bg->payload_size += padding_byte_count;
	}

#ifdef DEBUG
	printk("Plaintext (%d=%d+%d):\n", bg->payload_size, KEYMOD_LENGTH,
	       bg->payload_size - KEYMOD_LENGTH);
	memory_display(bg->plaintext->data, 0, bg->payload_size, 0x40 >> 3, 0);
#endif

	hash = xzalloc(sizeof(*hash)*SHA256_DIGEST_SIZE);

	blob_hash_data("sha256", bg->plaintext->data, hash, bg->payload_size);

	memcpy(bg->plaintext->data + bg->payload_size, hash, SHA256_DIGEST_SIZE);

	bg->payload_size += SHA256_DIGEST_SIZE;

#ifdef DEBUG
	printk("Plaintext (%d=%d+%d+%d):\n", bg->payload_size, KEYMOD_LENGTH,
	       bg->payload_size - (KEYMOD_LENGTH + SHA256_DIGEST_SIZE) ,
	       SHA256_DIGEST_SIZE);
	memory_display(bg->plaintext->data, 0, bg->payload_size, 0x40 >> 3, 0);
#endif

	if (bg->req && bg->req->info)
		get_random_bytes(bg->req->info, MAX_IVLEN);
	else
		return -ENOMEM;

	return 0;
}

static int blob_gen_post_encryption(struct blobgen *bg)
{
	int padding_byte_count = ((bg->payload_size + BLOCKSIZE_BYTES - 1) &
				  ~(BLOCKSIZE_BYTES - 1)) - bg->payload_size;

	bg->payload_size += padding_byte_count;

	memcpy(bg->ciphertext->data + bg->payload_size, bg->req->info, MAX_IVLEN);

	bg->payload_size += MAX_IVLEN;

#ifdef DEBUG
	printk("Ciphertext (%d=%d+%d+%d+%d):\n", bg->payload_size, KEYMOD_LENGTH,
	       bg->payload_size - (KEYMOD_LENGTH + SHA256_DIGEST_SIZE + MAX_IVLEN),
	       SHA256_DIGEST_SIZE, MAX_IVLEN);
	memory_display(bg->ciphertext->data, 0, bg->payload_size, 0x40 >> 3, 0);
#endif

	return 0;
}

static int blob_gen_pre_decryption(struct blobgen *bg)
{
	bg->req->info = xzalloc(sizeof(*bg->req->info)*MAX_IVLEN);

	/* the IV is not part of the ciphered text */
	bg->req->nbytes -= MAX_IVLEN;

#ifdef DEBUG
	printk("Ciphertext (%d=%d+%d+%d):\n", bg->req->nbytes, KEYMOD_LENGTH,
	       bg->req->nbytes - (KEYMOD_LENGTH + SHA256_DIGEST_SIZE),
	       SHA256_DIGEST_SIZE);
	memory_display(bg->ciphertext->data, 0, bg->req->nbytes, 0x40 >> 3, 0);
#endif

	memcpy(bg->req->info, bg->ciphertext->data + bg->req->nbytes, MAX_IVLEN);

	return 0;
}

static int blob_gen_post_decryption(struct blobgen *bg)
{
	int padding_byte_count = ((bg->payload_size + BLOCKSIZE_BYTES - 1) &
				  ~(BLOCKSIZE_BYTES - 1)) - bg->payload_size;
	struct blob_param *payload = &bg->payload;
	struct blob_param *modifier = &bg->modifier;
	u8 __iomem *hash;
	int ret = 0;

	hash = xzalloc(sizeof(*hash)*SHA256_DIGEST_SIZE);

	bg->payload_size -= (SHA256_DIGEST_SIZE + padding_byte_count);
	blob_hash_data("sha256", bg->plaintext->data, hash, bg->payload_size);

	if (memcmp(bg->plaintext->data + bg->payload_size, hash,
		   SHA256_DIGEST_SIZE)) {
		pr_err("%s: Corrupted SHA256 digest. Can't continue.\n",
		       bg->dev.name);
		pr_err("%s: Calculated hash:\n", bg->dev.name);
		memory_display(hash, 0, SHA256_DIGEST_SIZE, 0x40 >> 3, 0);
		pr_err("%s: Received hash:\n", bg->dev.name);
		memory_display(bg->plaintext->data + bg->payload_size,
			       0, SHA256_DIGEST_SIZE, 0x40 >> 3, 0);

		ret = -EILSEQ;
	}

	modifier->value = xstrndup(bg->plaintext->data, KEYMOD_LENGTH);

	bg->payload_size -= KEYMOD_LENGTH;

	payload->value = xstrndup(bg->plaintext->data + KEYMOD_LENGTH, bg->payload_size);

	return ret;
}

static int blob_gen_probe(struct device_d *dev)
{
	struct blobgen *bg;
	int ret;

	bg = xzalloc(sizeof(*bg));

	bg->max_payload_size = MAX_BLOB_LEN - (BLOB_FOOTER_LEN + KEYMOD_LENGTH);
	bg->pre_encryption = blob_gen_pre_encryption;
	bg->post_encryption = blob_gen_post_encryption;
	bg->pre_decryption = blob_gen_pre_decryption;
	bg->post_decryption = blob_gen_post_decryption;
	bg->run = blob_run_crypto;

	ret = blob_gen_register(dev, bg);

	return ret;
}

static struct of_device_id blob_gen_ids[] = {
	{
		.compatible = "scc_blob",
	},
	{},
};

static struct driver_d blob_gen_driver = {
	.name = "blob",
	.of_compatible = DRV_OF_COMPAT(blob_gen_ids),
	.probe = blob_gen_probe,
};
device_platform_driver(blob_gen_driver);

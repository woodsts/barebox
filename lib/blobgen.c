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

#include <blobgen.h>
#include <base64.h>
#include <malloc.h>
#include <crypto.h>

static int blob_gen_blob_get(struct param_d *p, void *arg)
{
	struct blobgen *bg = arg;
	struct blob_param *blob = &bg->blob;
	int length;
	int ret;

	if (bg->busy)
		return -EBUSY;

	if (bg->payload_size <= 0) {
		dev_err(&bg->dev, "No payload given\n");
		ret = -EINVAL;
		goto out;
	}

	bg->busy = true;

	memset(bg->plaintext->data, 0x0, sizeof(bg->plaintext->data));
	memset(bg->ciphertext->data, 0x0, sizeof(bg->ciphertext->data));

	bg->req = xzalloc(sizeof(*bg->req));
	bg->req->info = xzalloc(sizeof(*bg->req->info));

	if (bg->pre_encryption) {
		ret = bg->pre_encryption(bg);
		if (ret) {
			dev_err(&bg->dev, "error in pre-encryption hook (ret=%d)\n", ret);
			goto out;
		}
	}

	bg->req->src = bg->plaintext->data;
	bg->req->dst = bg->ciphertext->data;
	bg->req->nbytes = bg->payload_size;

	if (bg->run) {
		ret = bg->run(bg, ENCRYPT);
		if (ret) {
			dev_err(&bg->dev, "error in run hook (ret=%d)\n", ret);
			goto out;
		}
	} else {
		dev_err(&bg->dev, "No run function specified. Can not execute encryption.\n");
		ret = -EINVAL;
		goto out;
	}

	if (bg->post_encryption) {
		ret = bg->post_encryption(bg);
		if (ret) {
			dev_err(&bg->dev, "error in post-encryption hook (ret=%d)\n", ret);
			goto out;
		}
	}

#ifdef DEBUG
	printk("Encrypted data:\n");
	memory_display(bg->req->dst, 0, bg->payload_size, 0x40 >> 3, 0);
#endif

	free(blob->value);

	length = bg->payload_size;
	blob->value = xzalloc(BASE64_LENGTH(length) + 1);

	uuencode(blob->value, bg->ciphertext->data, length);

	bg->busy = false;

	return 0;

out:
	bg->busy = false;

	return ret;
}

static int blob_gen_blob_set(struct param_d *p, void *arg)
{
	struct blobgen *bg = arg;
	struct blob_param *blob = &bg->blob;
	int length;
	int ret;

	if (bg->busy)
		return -EBUSY;

	length = strlen(blob->value);
	if (length <= 0) {
		dev_err(&bg->dev, "blob can't be 0\n");
		ret = -EINVAL;
		goto out;
	}

	bg->busy = true;

	memset(bg->plaintext->data, 0x0, sizeof(bg->plaintext->data));
	memset(bg->ciphertext->data, 0x0, sizeof(bg->ciphertext->data));

	length = decode_base64(bg->ciphertext->data, sizeof(bg->ciphertext->data), blob->value);

	if (length > MAX_BLOB_LEN) {
		dev_err(&bg->dev, "blob to long\n");
		ret = -ENOSPC;
		goto out;
	}

	bg->req = xzalloc(sizeof(*bg->req));
	bg->req->src = bg->ciphertext->data;
	bg->req->dst = bg->plaintext->data;
	bg->req->nbytes = length;

	if (bg->pre_decryption) {
		ret = bg->pre_decryption(bg);
		if (ret) {
			dev_err(&bg->dev, "error in pre-decryption hook (ret=%d)\n", ret);
			goto out;
		}
	}

#ifdef DEBUG
	printk("Encrypted data:\n");
	memory_display(bg->req->src, 0, bg->req->nbytes, 0x40 >> 3, 0);
#endif

	bg->payload_size = bg->req->nbytes;

	if (bg->run) {
		ret = bg->run(bg, DECRYPT);
		if (ret) {
			dev_err(&bg->dev, "error in run hook (ret=%d)\n", ret);
			goto out;
		}
	} else {
		dev_err(&bg->dev, "No run function specified. Can not execute decryption.\n");
		goto out;
	}

	if (bg->post_decryption) {
		ret = bg->post_decryption(bg);
		if (ret) {
			dev_err(&bg->dev, "error in post-decryption hook (ret=%d)\n", ret);
			goto out;
		}
	}

	bg->busy = false;

	return 0;

out:
	bg->busy = false;
	return ret;
}


static int blob_gen_payload_get(struct param_d *p, void *arg)
{
	struct blobgen *bg = arg;

	if (bg->payload_size == 0)
		return -EINVAL;

	if (bg->access != USERSPACE)
		return -EPERM;

	return 0;
}

static int blob_gen_payload_set(struct param_d *p, void *arg)
{
	struct blobgen *bg = arg;
	struct blob_param *payload = &bg->payload;
	size_t len;

	if (bg->busy)
		return -EBUSY;

	len = strlen(payload->value);
	if (len > bg->max_payload_size)
		return -ENOSPC;

	bg->payload_size = len;

	return 0;
}


static int blob_gen_modifier_set(struct param_d *p, void *arg)
{
	struct blobgen *bg = arg;
	struct blob_param *modifier = &bg->modifier;
	ssize_t len;

	len = strlen(modifier->value);

	if (len > KEYMOD_LENGTH) {
		dev_err(&bg->dev, "modifier to long (%d)\n", len);
		return -EINVAL;
	}

	if (bg->busy)
		return -EBUSY;

	if (!strncmp(modifier->value, "kernel:evm", 10))
		bg->access = KERNEL_EVM;
	else if (!strncmp(modifier->value, "kernel:", 7))
		bg->access = KERNEL;
	else if (!strncmp(modifier->value, "user:", 5))
		bg->access = USERSPACE;

	return 0;
}

int blob_gen_register(struct device_d *dev, struct blobgen *bg)
{
	int ret = 0;

	strcpy(bg->dev.name, "blob");
	bg->dev.parent = dev;
	register_device(&bg->dev);

	bg->plaintext = xzalloc(sizeof(*bg->plaintext));
	bg->ciphertext = xzalloc(sizeof(*bg->ciphertext));

	bg->modifier.param = dev_add_param_string(&(bg->dev), "modifier",
						  blob_gen_modifier_set,
						  NULL,
						  &bg->modifier.value,
						  bg);
	if (IS_ERR(bg->modifier.param)) {
		ret = PTR_ERR(&bg->modifier.param);
		goto out;
	}

	bg->payload.param = dev_add_param_string(&(bg->dev), "payload",
						 blob_gen_payload_set,
						 blob_gen_payload_get,
						 &bg->payload.value,
						 bg);
	if (IS_ERR(bg->payload.param)) {
		ret = PTR_ERR(&bg->payload.param);
		goto out;
	}

	bg->blob.param = dev_add_param_string(&(bg->dev), "blob",
					      blob_gen_blob_set,
					      blob_gen_blob_get,
					      &bg->blob.value,
					      bg);
	if (IS_ERR(bg->blob.param)) {
		ret = PTR_ERR(&bg->blob.param);
		goto out;
	}

	bg->busy = false;

out:
	return ret;
}

/*
 * Copyright (C) 2015 Pengutronix, Steffen Trumtrar <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#include <common.h>
#include <asm/io.h>
#include <base64.h>
#include <blobgen.h>
#include <crypto.h>
#include <crypto/mxc_scc.h>
#include <dma.h>
#include <driver.h>
#include <init.h>
#include <fs.h>
#include <fcntl.h>
#include "intern.h"
#include "desc.h"
#include "desc_constr.h"
#include "error.h"
#include "jr.h"

#define DRIVERNAME	"blob"

/*
 * Upon completion, desc points to a buffer containing a CAAM job
 * descriptor which encapsulates data into an externally-storable
 * blob.
 */
#define INITIAL_DESCSZ		16
/* 32 bytes key blob + 16 bytes HMAC identifier */
#define BLOB_OVERHEAD		(32 + 16)
#define KEYMOD_LENGTH		16
#define RED_BLOB_LENGTH		64
#define MAX_BLOB_LEN		4096
#define DESC_LEN		64

struct blob_job_result {
        int err;
};

struct blob_priv {
	u32 desc[DESC_LEN];
	dma_addr_t dma_modifier;
	dma_addr_t dma_plaintext;
	dma_addr_t dma_ciphertext;
};

static void jr_jobdesc_blob_decap(void *priv, u8 modlen, u16 input_size)
{
	struct blob_priv *ctx = (struct blob_priv *) priv;
	u32 *desc = ctx->desc;
	u16 in_sz;
	u16 out_sz;

	in_sz = input_size;
	out_sz = input_size - BLOB_OVERHEAD;

	init_job_desc(desc, 0);
	/*
	 * The key modifier can be used to differentiate specific data.
	 * Or to prevent replay attacks.
	 */
	append_key(desc, ctx->dma_modifier, modlen, CLASS_2);
	append_seq_in_ptr(desc, ctx->dma_ciphertext, in_sz, 0);
	append_seq_out_ptr(desc, ctx->dma_plaintext, out_sz, 0);
	append_operation(desc, OP_TYPE_DECAP_PROTOCOL | OP_PCLID_BLOB);
}

static void jr_jobdesc_blob_encap(void *priv, u8 modlen, u16 input_size)
{
	struct blob_priv *ctx = (struct blob_priv *) priv;
	u32 *desc = ctx->desc;
	u16 in_sz;
	u16 out_sz;

	in_sz = input_size;
	out_sz = input_size + BLOB_OVERHEAD;

	init_job_desc(desc, 0);
	/*
	 * The key modifier can be used to differentiate specific data.
	 * Or to prevent replay attacks.
	 */
	append_key(desc, ctx->dma_modifier, modlen, CLASS_2);
	append_seq_in_ptr(desc, ctx->dma_plaintext, in_sz, 0);
	append_seq_out_ptr(desc, ctx->dma_ciphertext, out_sz, 0);
	append_operation(desc, OP_TYPE_ENCAP_PROTOCOL | OP_PCLID_BLOB);
}

static void blob_job_done(struct device_d *dev, u32 *desc, u32 err, void *arg)
{
	struct blob_job_result *res = arg;

	if (!res)
		return;

	if (err)
		caam_jr_strstatus(dev, readl(0x2101044));

	res->err = err;
}

static int blob_run_crypto(struct blobgen *bg, enum operation op)
{
	struct blob_priv *ctx = (struct blob_priv *) bg->priv;
	struct blob_data_blob *ciphertext = bg->ciphertext;
	struct blob_data_blob *plaintext = bg->plaintext;
	struct blob_param *modifier = &bg->modifier;
	struct device_d *jrdev = bg->dev.parent;
	struct blob_job_result testres;
	u8 modifier_len = strlen(modifier->value);
	u32 *desc = ctx->desc;
	int ret;

	memset(desc, 0, DESC_LEN);

	ctx->dma_modifier =   (dma_addr_t)modifier->value;
	ctx->dma_plaintext =  (dma_addr_t)plaintext->data;
	ctx->dma_ciphertext = (dma_addr_t)ciphertext->data;

	if (op == DECRYPT)
		jr_jobdesc_blob_decap(ctx, modifier_len, bg->payload_size);
	else
		jr_jobdesc_blob_encap(ctx, modifier_len, bg->payload_size);

	pr_debug("modifier:\n");
	pr_hex_dump_debug("modifier: ",
			  DUMP_PREFIX_OFFSET, 16, 1, modifier->value,
			  KEYMOD_LENGTH, false);

	dma_sync_single_for_device((unsigned long)desc, desc_bytes(desc), DMA_TO_DEVICE);

	dma_sync_single_for_device((unsigned long)modifier->value, modifier_len, DMA_TO_DEVICE);
	dma_sync_single_for_device((unsigned long)plaintext->data, sizeof(plaintext->data), DMA_TO_DEVICE);
	dma_sync_single_for_device((unsigned long)ciphertext->data, sizeof(ciphertext->data), DMA_TO_DEVICE);

	testres.err = 0;

	if (op == ENCRYPT) {
		pr_debug("plaintext:\n");
		pr_hex_dump_debug("plaintext: ",
				  DUMP_PREFIX_OFFSET, 16, 1, plaintext->data,
				  bg->payload_size, false);
	} else {
		pr_debug("ciphertext:\n");
		pr_hex_dump_debug("ciphertext: ",
				  DUMP_PREFIX_OFFSET, 16, 1, ciphertext->data,
				  bg->payload_size, false);
	}

	pr_debug("jobdesc:\n");
	pr_hex_dump_debug("jobdesc: ",
			  DUMP_PREFIX_OFFSET, 16, 1, desc,
			  desc_bytes(desc), false);

	ret = caam_jr_enqueue(jrdev, desc, blob_job_done, &testres);
	if (ret) {
		dev_err(jrdev, "%s error\n", op == ENCRYPT ? "encryption" : "decryption");
	}

	ret = testres.err;

	dma_sync_single_for_cpu((unsigned long)modifier->value, modifier_len, DMA_FROM_DEVICE);
	dma_sync_single_for_cpu((unsigned long)plaintext->data, sizeof(plaintext->data), DMA_FROM_DEVICE);
	dma_sync_single_for_cpu((unsigned long)ciphertext->data, sizeof(ciphertext->data), DMA_FROM_DEVICE);

	if (op == ENCRYPT) {
		pr_debug("ciphertext:\n");
		pr_hex_dump_debug("ciphertert",
				  DUMP_PREFIX_OFFSET, 16, 1, ciphertext->data,
				  bg->payload_size + BLOB_OVERHEAD, false);
	} else {
		pr_debug("plaintext:\n");
		pr_hex_dump_debug("ciphertert",
				  DUMP_PREFIX_OFFSET, 16, 1, ciphertext->data,
				  bg->payload_size - BLOB_OVERHEAD, false);
	}

	return ret;
}

static int blob_gen_pre_encryption(struct blobgen *bg)
{
	struct blob_data_blob *plaintext = bg->plaintext;
	struct blob_param *payload = &bg->payload;

	memcpy(plaintext->data, payload->value, strlen(payload->value));

	return 0;
}

static int blob_gen_post_encryption(struct blobgen *bg)
{
	bg->payload_size += BLOB_OVERHEAD;

	return 0;
}

static int blob_gen_pre_decryption(struct blobgen *bg)
{
	return 0;
}

static int blob_gen_post_decryption(struct blobgen *bg)
{
	struct blob_data_blob *plaintext = bg->plaintext;
	struct blob_param *payload = &bg->payload;

	bg->payload_size -= BLOB_OVERHEAD;

	free(payload->value);
	payload->value = xstrndup(plaintext->data, bg->payload_size);

	return 0;
}

static int blob_gen_probe(struct device_d *dev)
{
	struct device_node *dev_node;
	struct device_d *jrdev;
	struct blob_priv *priv;
	struct blobgen *bg;
	int ret;

	dev_node = of_find_compatible_node(NULL, NULL, "fsl,sec-v4.0-job-ring");
	if (!dev_node) {
		dev_node = of_find_compatible_node(NULL, NULL, "fsl,sec4.0-job-ring");
		if (!dev_node)
			return -ENODEV;
	}

	jrdev = of_find_device_by_node(dev_node);
	if (!jrdev)
		return -ENODEV;

	bg = xzalloc(sizeof(*bg));

	bg->max_payload_size = MAX_BLOB_LEN - BLOB_OVERHEAD;

	bg->pre_encryption = blob_gen_pre_encryption;
	bg->post_encryption = blob_gen_post_encryption;
	bg->pre_decryption = blob_gen_pre_decryption;
	bg->post_decryption = blob_gen_post_decryption;
	bg->run = blob_run_crypto;

	ret = blob_gen_register(jrdev, bg);

	priv = xzalloc(sizeof(*priv));

	bg->priv = priv;

	return ret;
}

static struct of_device_id blob_gen_ids[] = {
	{
		.compatible = "caam_blob",
	},
	{},
};

static struct driver_d blob_gen_driver = {
	.name = DRIVERNAME,
	.of_compatible = DRV_OF_COMPAT(blob_gen_ids),
	.probe = blob_gen_probe,
};
device_platform_driver(blob_gen_driver);

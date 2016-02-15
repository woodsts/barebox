/*
 * Copyright (C) 2016 Pengutronix, Steffen Trumtrar <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */

struct ablkcipher_request;

int mxc_scc_cbc_des_encrypt(struct ablkcipher_request *req);
int mxc_scc_cbc_des_decrypt(struct ablkcipher_request *req);

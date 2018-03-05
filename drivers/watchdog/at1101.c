/*
 * Copyright (C) 2018 Pengutronix, Jan Luebbe <jlu@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <driver.h>
#include <restart.h>
#include <init.h>
#include <param.h>
#include <i2c/i2c.h>
#include <malloc.h>
#include <notifier.h>
#include <reset_source.h>
#include <watchdog.h>
#include <crc.h>

struct at1101wd {
	struct restart_handler	restart;
	struct watchdog		wd;
	struct i2c_client	*client;
	struct device_d		*dev;

	int			progress;
	int			timeout;
	int			kernel_reboot_req;
	int			previous_uptime;
	int			clear;

	char 			sw_type[11]; /* 10 bytes from HW + 0-byte */
	int			sw_major, sw_minor, sw_patch;

	int			exchange_layout, exchange_offset;

};

/* Values for block markers */
#define AT1101WD_BLOCK_SW_VERSION		0x30
#define AT1101WD_BLOCK_EXCHANGE_INFO		0x70

/* Registers */
#define AT1101WD_REG_BOOT_REASON		0x00
#define AT1101WD_REG_TRIGGER			0x01
#define AT1101WD_REG_REBOOT_REQ_WR		0x02
#define AT1101WD_REG_REBOOT_REQ_RD		0x03
#define AT1101WD_REG_PLANNED_BOOT_REASON	0x04
#define AT1101WD_REG_PREVIOUS_UPTIME		0x05
#define AT1101WD_REG_GET_SEC_KEY		0x10 /* 4 bytes */
#define AT1101WD_REG_SET_SEC_KEY		0x14 /* 4 bytes */
#define AT1101WD_REG_TIMEOUT			0x20
#define AT1101WD_REG_CLEAR			0x30

/* Values for AT1101WD_REG_REBOOT_REQ_WR (exchange_offset+0x02) */
#define AT1101WD_REBOOT_REQ_KERNEL		0x24

static int at1101wd_watchdog_set_timeout(struct watchdog *wd, unsigned int
		timeout)
{
	struct at1101wd *priv = container_of(wd, struct at1101wd, wd);
	uint32_t secret;
	uint8_t val;
	int ret;

	if (timeout) {
		if (timeout > 255) {
			dev_warn(priv->dev, "timeout too large, using 255 seconds\n");
			timeout = 255;
		} else if (timeout && timeout < 30) {
			dev_warn(priv->dev, "timeout too small, using 30 seconds\n");
			timeout = 30;
		};
	};

	/* update timeout when needed */
	if (priv->timeout != timeout) {
		/* unlock access to timeout register */
		ret = i2c_read_reg(priv->client,
				priv->exchange_offset + AT1101WD_REG_GET_SEC_KEY,
				(uint8_t *)&secret, sizeof(secret));
		if (ret < 0) {
			dev_err(priv->dev, "failed to get secret key\n");
			return ret;
		}
		ret = i2c_write_reg(priv->client,
				priv->exchange_offset + AT1101WD_REG_SET_SEC_KEY,
				(uint8_t *)&secret, sizeof(secret));
		if (ret < 0) {
			dev_err(priv->dev, "failed to set secret key\n");
			return ret;
		}

		/* update the timeout */
		val = timeout;
		ret = i2c_write_reg(priv->client,
				priv->exchange_offset + AT1101WD_REG_TIMEOUT,
				&val, sizeof(val));
		if (ret < 0)
			return ret;
		priv->timeout = timeout;
	};

	val = priv->progress;
	ret = i2c_write_reg(priv->client,
			priv->exchange_offset + AT1101WD_REG_TRIGGER,
			&val, sizeof(val));
	if (ret < 0)
		return ret;

	return 0;
}

static const char *at1101wd_decode_boot_reason(uint8_t reason)
{
	switch (reason) {
	case 0x00:
		return "non critical/power/power up";
	case 0x01:
		return "non critical/power/hot plug";
	case 0x02 ... 0x1f:
		return "non critical/power/unknown";

	case 0x20:
		return "non critical/reboot/request by application";
	case 0x21:
		return "non critical/reboot/software install";
	case 0x22:
		return "non critical/reboot/power cycle request";
	case 0x23:
		return "non critical/reboot/command line request";
	case 0x24:
		return "non critical/reboot/kernel request";
	case 0x25 ... 0x3f:
		return "non critical/reboot/unkown";

	case 0x40:
		return "critical/power/over current";
	case 0x41:
		return "critical/power/over voltage";
	case 0x42 ... 0x5f:
		return "critical/power/unknown";

	case 0x60:
		return "critical/watchdog/timeout in boot";
	case 0x61:
		return "critical/watchdog/OS timeout";
	case 0x62:
		return "critical/watchdog/app timeout";
	case 0x63:
		return "critical/watchdog/cpu reset";
	case 0x64 ... 0x7f:
		return "critical/watchdog/unknown";

	default:
		return "unknown";
	};
}

static void at1101wd_detect_reset_source(struct at1101wd *priv)
{
	enum reset_src_type type;
	const char *reason;
	uint8_t val;
	int ret;

	ret = i2c_read_reg(priv->client,
			priv->exchange_offset + AT1101WD_REG_BOOT_REASON,
			&val, sizeof(val));
	if (ret < 0)
		return;

	reason = at1101wd_decode_boot_reason(val);

	dev_info(priv->dev, "%s (0x%02x)\n", reason, val);

	/* only the group is relevant */
	val &= 0xe0;

	if (val == 0x00) /* non critical power */
		type = RESET_POR;
	else if (val == 0x20) /* non critical reboot */
		type = RESET_RST;
	else if (val == 0x40) /* critical power */
		type = RESET_EXT;
	else if (val == 0x60) /* critical watchdog */
		type = RESET_WDG;
	else
		return;

	reset_source_set_priority(type, 300);
}

static void at1101wd_restart(struct restart_handler *rst)
{
	struct at1101wd *priv = container_of(rst, struct at1101wd, restart);
	uint32_t secret;
	uint8_t val = AT1101WD_REBOOT_REQ_KERNEL;
	int ret;

	dev_info(priv->dev, "restarting via watchdog\n");

	/* unlock access to timeout register */
	ret = i2c_read_reg(priv->client,
			priv->exchange_offset + AT1101WD_REG_GET_SEC_KEY,
			(uint8_t *)&secret, sizeof(secret));
	if (ret < 0) {
		dev_err(priv->dev, "failed to get secret key\n");
		return;
	}
	ret = i2c_write_reg(priv->client,
			priv->exchange_offset + AT1101WD_REG_SET_SEC_KEY,
			(uint8_t *)&secret, sizeof(secret));
	if (ret < 0) {
		dev_err(priv->dev, "failed to set secret key\n");
		return;
	}

	i2c_write_reg(priv->client,
			priv->exchange_offset + AT1101WD_REG_REBOOT_REQ_WR,
			&val, sizeof(val));

	mdelay(1000);
}

static int at1101wd_read_static_block(struct at1101wd *priv, uint8_t *buf, unsigned int len)
{
	switch (buf[0x00]) {
	case AT1101WD_BLOCK_SW_VERSION:
		if (len < 16) {
			dev_err(priv->dev, "SW-VERSION BLOCK TOO SMALL\n");
			return -EINVAL;
		}
		memcpy(priv->sw_type, &buf[0x02], 10);
		priv->sw_major = buf[0x0c] << 8 | buf[0x0d];
		priv->sw_minor = buf[0x0e] << 8 | buf[0x0f];
		priv->sw_patch = buf[0x10] << 8 | buf[0x11];
		return 0;
	case AT1101WD_BLOCK_EXCHANGE_INFO:
		if (len < 2) {
			dev_err(priv->dev, "EXCHANGE-INFO BLOCK TOO SMALL\n");
			return -EINVAL;
		}
		priv->exchange_layout = buf[0x02];
		priv->exchange_offset = buf[0x03];
		return 0;
	default:
		dev_warn(priv->dev, "UNKNOWN BLOCK MARKER (0x%02x)\n", buf[0x00]);
		return -EINVAL;
	}
}

static const uint32_t at1101wd_crc_table[256] = {
	0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
	0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
	0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75,
	0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
	0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
	0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
	0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
	0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
	0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072,
	0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
	0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02,
	0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
	0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692,
	0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
	0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2,
	0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
	0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
	0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
	0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B,
	0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF, 0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
	0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B,
	0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
	0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B,
	0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
	0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
	0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
	0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC,
	0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
	0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C,
	0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
	0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C,
	0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4,
};

static uint32_t at1101wd_crc32(uint32_t crc, void *_buf, int length)
{
	uint8_t *buf = _buf;

	while (length--)
		crc = crc << 8 ^ at1101wd_crc_table[(crc >> 24 ^ *(buf++)) & 0xff];

	return crc;
}

static int at1101wd_read_static(struct at1101wd *priv) {
	unsigned int crc_off, block_off, block_len;
	uint8_t buf[256];
	uint32_t crc;
	int ret;

	ret = i2c_read_reg(priv->client, 0x00, &buf[0x00], 1);
	if (ret < 0)
		return ret;

	/* check if data + 1 byte size + 4 bytes CRC will fit */
	crc_off = 0x01 + buf[0x00];

	if (crc_off > (0xff - 4))
		return -EINVAL;

	ret = i2c_read_reg(priv->client, 0x01, &buf[0x01], buf[0x00] + 4);
	if (ret < 0)
		return ret;

	crc = ~0;
	crc = at1101wd_crc32(crc, buf, crc_off + 4);
	if (crc) {
		dev_err(priv->dev, "CRC ERROR (result 0x%08x)\n", crc);
		return -EINVAL;
	};

	block_off = 0x01;
	for (;;) {
		if (block_off == crc_off)
			break;
		if ((block_off + 1) >= crc_off) {
			dev_err(priv->dev, "INCOMPLETE BLOCK HEADER (offset 0x%02x)\n", block_off);
			return -EINVAL;
		}
		block_len = buf[block_off + 1];
		if ((block_off+2+block_len) > crc_off) {
			dev_err(priv->dev, "INCOMPLETE BLOCK (offset 0x%02x, len 0x%02x)\n", block_off, block_len);
			return -EINVAL;
		}
		ret = at1101wd_read_static_block(priv, &buf[block_off], block_len);
		if (ret < 0)
			return ret;
		block_off += 2 + block_len;
	};

	if (priv->exchange_layout != 1) {
		dev_err(priv->dev, "UNKNOWN EXCHANGE-LAYOUT (version %d)\n", priv->exchange_layout);
		return -EINVAL;
	};

	return 0;
}

static int at1101wd_set_progress(struct param_d *param, void *data)
{
	struct at1101wd *priv = data;

	priv->progress &= 0xff;

	return at1101wd_watchdog_set_timeout(&priv->wd, priv->timeout);
}

static int at1101wd_set_timeout(struct param_d *param, void *data)
{
	struct at1101wd *priv = data;

	priv->timeout &= 0xff;

	return 0;
}

static int at1101wd_set_clear(struct param_d *param, void *data)
{
	struct at1101wd *priv = data;
	uint32_t secret;
	uint8_t val;
	int ret;

	priv->clear &= 0xff;
	val = priv->clear;

	dev_info(priv->dev, "clearing watchdog state\n");

	/* unlock access to timeout register */
	ret = i2c_read_reg(priv->client,
			priv->exchange_offset + AT1101WD_REG_GET_SEC_KEY,
			(uint8_t *)&secret, sizeof(secret));
	if (ret < 0) {
		dev_err(priv->dev, "failed to get secret key\n");
		return ret;
	}
	ret = i2c_write_reg(priv->client,
			priv->exchange_offset + AT1101WD_REG_SET_SEC_KEY,
			(uint8_t *)&secret, sizeof(secret));
	if (ret < 0) {
		dev_err(priv->dev, "failed to set secret key\n");
		return ret;
	}

	ret = i2c_write_reg(priv->client,
			priv->exchange_offset + AT1101WD_REG_CLEAR,
			&val, sizeof(val));
	if (ret < 0) {
		dev_err(priv->dev, "failed to clear watchdog state\n");
		return ret;
	}

	return 0;
}

static int at1101wd_probe(struct device_d *dev)
{
	struct at1101wd *priv = NULL;
	uint8_t val;
	int ret;

	priv = xzalloc(sizeof(struct at1101wd));
	priv->wd.set_timeout = at1101wd_watchdog_set_timeout;
	priv->client = to_i2c_client(dev);
	priv->dev = dev;

	ret = at1101wd_read_static(priv);
	if (ret < 0)
		goto on_error;

	ret = i2c_read_reg(priv->client,
			priv->exchange_offset+AT1101WD_REG_TIMEOUT,
			&val, sizeof(val));
	if (ret < 0)
		goto on_error;
	priv->timeout = val;

	ret = i2c_read_reg(priv->client,
			priv->exchange_offset+AT1101WD_REG_REBOOT_REQ_RD,
			&val, sizeof(val));
	if (ret < 0)
		goto on_error;
	priv->kernel_reboot_req = val;

	ret = i2c_read_reg(priv->client,
			priv->exchange_offset+AT1101WD_REG_PREVIOUS_UPTIME,
			&val, sizeof(val));
	if (ret < 0)
		goto on_error;
	priv->previous_uptime = val;

	ret = watchdog_register(&priv->wd);
	if (ret)
		goto on_error;

	at1101wd_detect_reset_source(priv);

	priv->restart.priority = 300;
	priv->restart.name = "at1101wd";
	priv->restart.restart = &at1101wd_restart;

	restart_handler_register(&priv->restart);

	dev_add_param_int(dev, "progress", at1101wd_set_progress, NULL,
			&priv->progress, "%d", priv);
	dev_add_param_int(dev, "timeout", at1101wd_set_timeout, NULL,
			&priv->timeout, "%d", priv);
	dev_add_param_int(dev, "clear", at1101wd_set_clear, NULL,
			&priv->clear, "%d", priv);
	dev_add_param_fixed(dev, "sw_type", priv->sw_type);
	dev_add_param_int_fixed(dev, "sw_major", priv->sw_major, "%d");
	dev_add_param_int_fixed(dev, "sw_minor", priv->sw_minor, "%d");
	dev_add_param_int_fixed(dev, "sw_patch", priv->sw_patch, "%d");
	dev_add_param_int_fixed(dev, "kernel_reboot_req", priv->kernel_reboot_req, "%d");
	dev_add_param_int_fixed(dev, "previous_uptime", priv->previous_uptime, "%d");

	return 0;

on_error:
	if (priv)
		free(priv);
	return ret;
}

static struct platform_device_id at1101wd_id[] = {
        { "at1101wd", },
	{ }
};

static struct driver_d at1101wd_driver = {
	.name = "at1101wd",
	.probe = at1101wd_probe,
	.id_table = at1101wd_id,
};

static int at1101wd_init(void)
{
	return i2c_driver_register(&at1101wd_driver);
}

device_initcall(at1101wd_init);

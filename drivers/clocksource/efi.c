// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2017 Jean-Christophe PLAGNIOL-VILLARD <plagnio@jcrosoft.com>
 */
#include <common.h>
#include <init.h>
#include <driver.h>
#include <clock.h>
#include <efi.h>
#include <efi/efi-payload.h>
#include <efi/efi-device.h>
#include <linux/err.h>

static uint64_t ticks = 1;
static struct efi_event *efi_cs_evt;

static uint64_t efi_cs_read(void)
{
	return ticks;
}

static void efi_cs_inc(struct efi_event *event, void *ctx)
{
	ticks++;
}

/* count ticks during a 1dms */
static uint64_t ticks_freq(void)
{
	uint64_t ticks_start, ticks_end;

	ticks_start = ticks;
	BS->stall(1000);
	ticks_end = ticks;

	return (ticks_end - ticks_start) * 1000;
}

/* count ticks during a 20ms delay as on qemu x86_64 the max is 100Hz */
static uint64_t ticks_freq_x86(void)
{
	uint64_t ticks_start, ticks_end;

	ticks_start = ticks;
	BS->stall(20 * 1000);
	ticks_end = ticks;

	return (ticks_end - ticks_start) * 50;
}

static int efi_cs_init(struct clocksource *cs)
{
	efi_status_t efiret;
	uint64_t freq;

	efiret = BS->create_event(EFI_EVT_TIMER | EFI_EVT_NOTIFY_SIGNAL,
			EFI_TPL_CALLBACK, efi_cs_inc, NULL, &efi_cs_evt);

	if (EFI_ERROR(efiret))
		return -efi_errno(efiret);

	efiret = BS->set_timer(efi_cs_evt, EFI_TIMER_PERIODIC, 10);
	if (EFI_ERROR(efiret)) {
		BS->close_event(efi_cs_evt);
		return -efi_errno(efiret);
	}

	freq = 1000 * 1000;
	if (ticks_freq() < 800 * 1000) {
		uint64_t nb_100ns;

		freq = ticks_freq_x86();
		if (freq == 0) {
			BS->close_event(efi_cs_evt);
			return -ENODEV;
		}
		nb_100ns = DIV_ROUND_DOWN_ULL(10 * 1000 * 1000, freq);
		pr_warn("EFI Event timer too slow freq = %llu Hz\n", freq);
		efiret = BS->set_timer(efi_cs_evt, EFI_TIMER_PERIODIC, nb_100ns);
		if (EFI_ERROR(efiret)) {
			BS->close_event(efi_cs_evt);
			return -efi_errno(efiret);
		}
	}

	cs->mult = clocksource_hz2mult(freq, cs->shift);

	return 0;
}

static struct clocksource efi_cs = {
	.read   = efi_cs_read,
	.mask   = CLOCKSOURCE_MASK(64),
	.shift  = 0,
	.init   = efi_cs_init,
	.priority = 80,
};

static int efi_cs_probe(struct device *dev)
{
	return init_clock(&efi_cs);
}

static struct driver efi_cs_driver = {
	.name = "efi-cs",
	.probe = efi_cs_probe,
};

core_platform_driver(efi_cs_driver);

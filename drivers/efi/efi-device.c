// SPDX-License-Identifier: GPL-2.0-only
/*
 * efi-device.c - barebox EFI payload support
 *
 * Copyright (c) 2014 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 */

#include <bootsource.h>
#include <common.h>
#include <driver.h>
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <linux/sizes.h>
#include <wchar.h>
#include <init.h>
#include <efi.h>
#include <efi/efi-payload.h>
#include <efi/efi-device.h>
#include <efi/device-path.h>
#include <linux/err.h>

static int efi_locate_handle(enum efi_locate_search_type search_type,
		efi_guid_t *protocol,
		void *search_key,
		size_t *no_handles,
		efi_handle_t **buffer)
{
	return __efi_locate_handle(BS, search_type, protocol, search_key, no_handles,
				   buffer);
}

static struct efi_device *efi_find_device(efi_handle_t handle)
{
	struct device *dev;
	struct efi_device *efidev;

	bus_for_each_device(&efi_bus, dev) {
		efidev = container_of(dev, struct efi_device, dev);

		if (efidev->handle == handle)
			return efidev;
	}

	return NULL;
}

static void efi_devinfo(struct device *dev)
{
	struct efi_device *efidev = to_efi_device(dev);
	int i;

	printf("Protocols:\n");

	for (i = 0; i < efidev->num_guids; i++)
		printf("  %d: %pUl: %s\n", i, &efidev->guids[i],
					efi_guid_string(&efidev->guids[i]));
}

static efi_handle_t efi_find_parent(efi_handle_t handle)
{
	size_t i, handle_count = 0;
	efi_handle_t *handles = NULL, parent;
	size_t j, num_guids;
	efi_guid_t **guids;
	int ret;
	efi_status_t efiret;
	struct efi_open_protocol_information_entry *entry_buffer;
	size_t k, entry_count;

	ret = efi_locate_handle(BY_PROTOCOL, &efi_device_path_protocol_guid,
			NULL, &handle_count, &handles);
	if (ret)
		return NULL;

	/*
	 * Normally one would expect a function/pointer to retrieve the parent.
	 * With EFI we have to:
	 * - get all handles
	 * - for each handle get the registered protocols
	 * - for each protocol get the users
	 * - the user which matches the input handle is the parent
	 */
	for (i = 0; i < handle_count; i++) {
		efiret = BS->open_protocol(handles[i], &efi_device_path_protocol_guid,
				NULL, NULL, NULL, EFI_OPEN_PROTOCOL_TEST_PROTOCOL);
		if (EFI_ERROR(efiret))
			continue;

		BS->protocols_per_handle(handles[i], &guids, &num_guids);
		for (j = 0; j < num_guids; j++) {
			efiret = BS->open_protocol_information(handles[i], guids[j],
				&entry_buffer, &entry_count);
			for (k = 0; k < entry_count; k++) {
				if (entry_buffer[k].controller_handle == NULL)
					continue;
				if (entry_buffer[k].controller_handle == handles[i])
					continue;
				if (entry_buffer[k].controller_handle == handle) {
					parent = handles[i];
					goto out;
				}
			}
		}
	}

	parent = NULL;

	free(handles);
out:
	return parent;
}

static struct efi_device *efi_add_device(efi_handle_t handle, efi_guid_t **guids,
		int num_guids)
{
	struct efi_device *efidev;
	int i;
	efi_guid_t *guidarr;
	efi_status_t efiret;
	void *devpath;

	efidev = efi_find_device(handle);
	if (efidev)
		return ERR_PTR(-EEXIST);

	efiret = BS->open_protocol(handle, &efi_device_path_protocol_guid,
			NULL, NULL, NULL, EFI_OPEN_PROTOCOL_TEST_PROTOCOL);
	if (EFI_ERROR(efiret))
		return ERR_PTR(-EINVAL);

	guidarr = malloc(sizeof(efi_guid_t) * num_guids);

	for (i = 0; i < num_guids; i++)
		memcpy(&guidarr[i], guids[i], sizeof(efi_guid_t));

	efiret = BS->open_protocol(handle, &efi_device_path_protocol_guid,
			&devpath, NULL, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(efiret)) {
		free(guidarr);
		return ERR_PTR(-EINVAL);
	}

	efidev = xzalloc(sizeof(*efidev));

	efidev->guids = guidarr;
	efidev->num_guids = num_guids;
	efidev->handle = handle;
	efidev->dev.bus = &efi_bus;
	efidev->dev.id = DEVICE_ID_SINGLE;
	devinfo_add(&efidev->dev, efi_devinfo);
	efidev->devpath = devpath;

	dev_set_name(&efidev->dev, "handle-%p", handle);

	efidev->parent_handle = efi_find_parent(efidev->handle);

	return efidev;
}

static int efi_register_device(struct efi_device *efidev)
{
	char *dev_path_str;
	struct efi_device *parent;
	int ret;

	/*
	 * Some UEFI instances create IPv4 and IPv6 messaging devices as children
	 * of the main MAC messaging device. Don't register these in barebox as
	 * they would show up as duplicate ethernet devices.
	 */
	if (device_path_to_type(efidev->devpath) == DEVICE_PATH_TYPE_MESSAGING_DEVICE) {
		u8 subtype = device_path_to_subtype(efidev->devpath);

		if (subtype == DEVICE_PATH_SUB_TYPE_MSG_IPv4 ||
		    subtype == DEVICE_PATH_SUB_TYPE_MSG_IPv6)
			return -EINVAL;
	}

	if (efi_find_device(efidev->handle))
		return -EEXIST;

	if (efidev->parent_handle) {
		parent = efi_find_device(efidev->parent_handle);
		if (!parent)
			return -EINVAL;

		efidev->dev.parent = &parent->dev;
	}

	ret = register_device(&efidev->dev);
	if (ret)
		return ret;

	dev_path_str = device_path_to_str(efidev->devpath);
	if (dev_path_str) {
		dev_add_param_fixed(&efidev->dev, "devpath", dev_path_str);
		free(dev_path_str);
	}

	debug("registered efi device %s\n", dev_name(&efidev->dev));

	return 0;
}

/**
 * efi_register_devices - iterate over all EFI handles and register
 *                        the devices found
 *
 * in barebox we treat all EFI handles which support the device_path
 * protocol as devices. This function iterates over all handles and
 * registers the corresponding devices. efi_register_devices is safe
 * to call multiple times. Already registered devices will be ignored.
 *
 */
void efi_register_devices(void)
{
	size_t handle_count = 0;
	efi_handle_t *handles = NULL;
	size_t num_guids;
	efi_guid_t **guids;
	int ret, i;
	struct efi_device **efidevs;
	int registered;

	ret = efi_locate_handle(BY_PROTOCOL, &efi_device_path_protocol_guid,
			NULL, &handle_count, &handles);
	if (ret)
		return;

	efidevs = xzalloc(handle_count * sizeof(struct efi_device *));

	for (i = 0; i < handle_count; i++) {
		BS->protocols_per_handle(handles[i], &guids, &num_guids);

		efidevs[i] = efi_add_device(handles[i], guids, num_guids);
	}

	/*
	 * We have a list of devices we want to register, but can only
	 * register a device when all parents are registered already.
	 * Do this by continiously iterating over the list until no
	 * further devices are registered.
	 */
	do {
		registered = 0;

		for (i = 0; i < handle_count; i++) {
			if (IS_ERR(efidevs[i]))
				continue;

			ret = efi_register_device(efidevs[i]);
			if (!ret) {
				efidevs[i] = ERR_PTR(-EEXIST);
				registered = 1;
			}
		}
	} while (registered);

	free(efidevs);
	free(handles);
}

int efi_connect_all(void)
{
	efi_status_t  efiret;
	size_t i, handle_count;
	efi_handle_t *handle_buffer;

	efiret = BS->locate_handle_buffer(ALL_HANDLES, NULL, NULL, &handle_count,
			&handle_buffer);
	if (EFI_ERROR(efiret))
		return -efi_errno(efiret);

	for (i = 0; i < handle_count; i++)
		efiret = BS->connect_controller(handle_buffer[i], NULL, NULL, true);

	if (handle_buffer)
		BS->free_pool(handle_buffer);

	return 0;
}

static int efi_bus_match(struct device *dev, const struct driver *drv)
{
	const struct efi_driver *efidrv = to_efi_driver(drv);
	struct efi_device *efidev = to_efi_device(dev);
	int i;

	for (i = 0; i < efidev->num_guids; i++) {
		if (!efi_guidcmp(efidrv->guid, efidev->guids[i])) {
			BS->handle_protocol(efidev->handle, &efidev->guids[i],
					&efidev->protocol);
			return true;
		}
	}

	return false;
}

static int efi_bus_probe(struct device *dev)
{
	struct efi_driver *efidrv = to_efi_driver(dev->driver);
	struct efi_device *efidev = to_efi_device(dev);

	return efidrv->probe(efidev);
}

static void efi_bus_remove(struct device *dev)
{
	struct efi_driver *efidrv = to_efi_driver(dev->driver);
	struct efi_device *efidev = to_efi_device(dev);

	if (efidrv->remove)
		efidrv->remove(efidev);
}

struct bus_type efi_bus = {
	.name = "efi",
	.match = efi_bus_match,
	.probe = efi_bus_probe,
	.remove = efi_bus_remove,
};

static void efi_businfo(struct device *dev)
{
	struct efi_config_table *t;
	int i = 0;

	printf("Tables:\n");
	for_each_efi_config_table(t) {
		printf("  %d: %pUl: %s\n", i++, &t->guid,
					efi_guid_string(&t->guid));
	}
}

static int efi_is_secure_boot(void)
{
	uint8_t *val;
	int ret = 0;

	val = efi_get_variable("SecureBoot", &efi_global_variable_guid, NULL);
	if (!IS_ERR(val)) {
		ret = *val;
		free(val);
	}

	return ret != 1;
}

static int efi_is_setup_mode(void)
{
	uint8_t *val;
	int ret = 0;

	val = efi_get_variable("SetupMode", &efi_global_variable_guid, NULL);
	if (!IS_ERR(val)) {
		ret = *val;
		free(val);
	}

	return ret != 1;
}

static bool is_bio_usbdev(struct efi_device *efidev)
{
	return efi_device_has_guid(efidev, EFI_USB_IO_PROTOCOL_GUID);
}

static struct efi_device *bootdev;

struct efi_device *efi_get_bootsource(void)
{
	return bootdev;
}

static void efi_set_bootsource(void)
{
	enum bootsource src = BOOTSOURCE_UNKNOWN;
	int instance = BOOTSOURCE_INSTANCE_UNKNOWN;

	efi_handle_t efi_parent;

	efi_parent = efi_find_parent(efi_loaded_image->device_handle);

	if (!efi_parent)
		goto out;

	bootdev = efi_find_device(efi_parent);

	if (!bootdev)
		goto out;

	if (is_bio_usbdev(bootdev)) {
		src = BOOTSOURCE_USB;
	} else {
		src = BOOTSOURCE_HD;
	}

out:

	bootsource_set_raw(src, instance);
}

static int efi_init_devices(void)
{
	char *fw_vendor = NULL;
	u16 sys_major = efi_sys_table->hdr.revision >> 16;
	u16 sys_minor = efi_sys_table->hdr.revision & 0xffff;
	int secure_boot = efi_is_secure_boot();
	int setup_mode = efi_is_setup_mode();

	fw_vendor = strdup_wchar_to_char((const wchar_t *)efi_sys_table->fw_vendor);

	pr_info("EFI v%u.%.02u by %s v%u\n",
		sys_major, sys_minor,
		fw_vendor, efi_sys_table->fw_revision);

	bus_register(&efi_bus);

	dev_add_param_fixed(efi_bus.dev, "fw_vendor", fw_vendor);
	free(fw_vendor);

	dev_add_param_uint32_fixed(efi_bus.dev, "major", sys_major, "%u");
	dev_add_param_uint32_fixed(efi_bus.dev, "minor", sys_minor, "%u");
	dev_add_param_uint32_fixed(efi_bus.dev, "fw_revision", efi_sys_table->fw_revision, "%u");
	dev_add_param_bool_fixed(efi_bus.dev, "secure_boot", secure_boot);
	dev_add_param_bool_fixed(efi_bus.dev, "secure_mode",
				 secure_boot & setup_mode);

	devinfo_add(efi_bus.dev, efi_businfo);

	efi_register_devices();

	efi_set_bootsource();

	return 0;
}
core_efi_initcall(efi_init_devices);

void efi_pause_devices(void)
{
	struct device *dev;

	bus_for_each_device(&efi_bus, dev) {
		struct driver *drv = dev->driver;
		struct efi_device *efidev = to_efi_device(dev);
		struct efi_driver *efidrv;

		if (!drv)
			continue;

		efidrv = to_efi_driver(drv);

		if (efidrv->dev_pause)
			efidrv->dev_pause(efidev);
	}
}

void efi_continue_devices(void)
{
	struct device *dev;

	bus_for_each_device(&efi_bus, dev) {
		struct driver *drv = dev->driver;
		struct efi_device *efidev = to_efi_device(dev);
		struct efi_driver *efidrv;

		if (!drv)
			continue;

		efidrv = to_efi_driver(drv);

		if (efidrv->dev_continue)
			efidrv->dev_continue(efidev);
	}
}

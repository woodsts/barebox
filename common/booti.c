// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2018 Sascha Hauer <s.hauer@pengutronix.de>

#define pr_fmt(fmt) "booti: " fmt

#include <common.h>
#include <filetype.h>
#include <memory.h>
#include <bootm.h>
#include <linux/sizes.h>

static unsigned long get_kernel_address(unsigned long os_address,
					unsigned long text_offset)
{
	resource_size_t start, end;
	int ret;

	if (!UIMAGE_IS_ADDRESS_VALID(os_address)) {
		ret = memory_bank_first_find_space(&start, &end);
		if (ret)
			return UIMAGE_INVALID_ADDRESS;

		return ALIGN(start, SZ_2M) + text_offset;
	}

	if (os_address >= text_offset && IS_ALIGNED(os_address - text_offset, SZ_2M))
		return os_address;

	return ALIGN(os_address, SZ_2M) + text_offset;
}

void *booti_load_image(struct image_data *data, phys_addr_t *oftree)
{
	const void *kernel_header =
			data->os_fit ? data->fit_kernel : data->os_header;
	unsigned long text_offset, image_size, kernel;
	unsigned long image_end;
	int ret;
	void *fdt;

	print_hex_dump_bytes("header ", DUMP_PREFIX_OFFSET, kernel_header, 80);

	if ((IS_ENABLED(CONFIG_RISCV) && !is_riscv_linux_bootimage(kernel_header)) ||
	    (IS_ENABLED(CONFIG_ARM64) && !is_arm64_linux_bootimage(kernel_header))) {
		pr_err("Unexpected magic at offset 0x38!\n");
		return ERR_PTR(-EINVAL);
	}

	text_offset = le64_to_cpup(kernel_header + 8);
	image_size = le64_to_cpup(kernel_header + 16);

	kernel = get_kernel_address(data->os_address, text_offset);

	pr_debug("Kernel to be loaded to %lx+%lx\n", kernel, image_size);

	if (kernel == UIMAGE_INVALID_ADDRESS)
		return ERR_PTR(-ENOENT);

	ret = bootm_load_os(data, kernel);
	if (ret)
		return ERR_PTR(ret);

	image_end = PAGE_ALIGN(kernel + image_size);

	if (oftree) {
		unsigned long devicetree;
		const struct resource *initrd_res;

		fdt = bootm_get_devicetree(data);
		if (IS_ERR(fdt))
			return fdt;
		if (!fdt)
			goto out;

		initrd_res = bootm_load_initrd(data, image_end);
		if (IS_ERR(initrd_res)) {
			return ERR_CAST(initrd_res);
		} else if (initrd_res) {
			image_end += resource_size(data->initrd_res);
			image_end = PAGE_ALIGN(image_end);
		}

		devicetree = image_end;

		ret = bootm_load_devicetree(data, fdt, devicetree);
		free(fdt);

		if (ret)
			return ERR_PTR(ret);

		*oftree = devicetree;
	}

out:
	printf("Loaded kernel to 0x%08lx", kernel);
	if (oftree && *oftree)
		printf(", devicetree at %pa", oftree);
	printf("\n");

	return (void *)kernel;
}

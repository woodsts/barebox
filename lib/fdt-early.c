#include <common.h>
#include <fdt.h>
#include <libfdt.h>
#include <asm/sections.h>
#include <sizes.h>
#include <asm/barebox-arm.h>

static void *fdt;
static u64 mem_start;
static u64 mem_size;

static char *of_fdt_get_string(struct fdt_header *blob, u32 offset)
{
	return ((char *)blob) +
		be32_to_cpu(blob->off_dt_strings) + offset;
}

static void *of_fdt_get_property(struct fdt_header *blob,
		       unsigned long node, const char *name,
		       unsigned long *size)
{
	unsigned long p = node;

	do {
		u32 tag = be32_to_cpup((__be32 *)p);
		u32 sz, noff;
		const char *nstr;

		p += 4;
		if (tag == FDT_NOP)
			continue;
		if (tag != FDT_PROP)
			return NULL;

		sz = be32_to_cpup((__be32 *)p);
		noff = be32_to_cpup((__be32 *)(p + 4));
		p += 8;
		if (be32_to_cpu(blob->version) < 0x10)
			p = ALIGN(p, sz >= 8 ? 8 : 4);

		nstr = of_fdt_get_string(blob, noff);
		if (nstr == NULL) {
			return NULL;
		}
		if (strcmp(name, nstr) == 0) {
			if (size)
				*size = sz;
			return (void *)p;
		}
		p += sz;
		p = ALIGN(p, 4);
	} while (1);
}

static void *of_get_flat_dt_prop(unsigned long node,
		const char *name, unsigned long *size)
{
	return of_fdt_get_property(fdt, node, name, size);
}

static int of_scan_flat_dt(void *fdt, int (*it)(unsigned long node,
				     const char *uname, int depth,
				     void *data),
			   void *data)
{
	unsigned long p;
	int rc = 0;
	int depth = -1;

	p = (unsigned long)fdt + fdt_off_dt_struct(fdt);

	do {
		u32 tag = be32_to_cpup((__be32 *)p);
		char *pathp;

		p += 4;
		if (tag == FDT_END_NODE) {
			depth--;
			continue;
		}
		if (tag == FDT_NOP)
			continue;
		if (tag == FDT_END)
			break;
		if (tag == FDT_PROP) {
			u32 sz = be32_to_cpup((__be32 *)p);
			p += 8;
			p += sz;
			p = ALIGN(p, 4);
			continue;
		}
		if (tag != FDT_BEGIN_NODE) {
			return -EINVAL;
		}
		depth++;
		pathp = (char *)p;
		p = ALIGN(p + strlen(pathp) + 1, 4);
		if ((*pathp) == '/') {
			char *lp, *np;
			for (lp = NULL, np = pathp; *np; np++)
				if ((*np) == '/')
					lp = np+1;
			if (lp != NULL)
				pathp = lp;
		}
		rc = it(p, pathp, depth, data);
		if (rc != 0)
			break;
	} while (1);

	return rc;
}

static int dt_root_size_cells;
static int dt_root_addr_cells;

/**
 * early_init_dt_scan_root - fetch the top level address and size cells
 */
static int early_init_dt_scan_root(unsigned long node, const char *uname,
		int depth, void *data)
{
	__be32 *prop;

	if (depth != 0)
		return 0;

	dt_root_size_cells = OF_ROOT_NODE_SIZE_CELLS_DEFAULT;
	dt_root_addr_cells = OF_ROOT_NODE_ADDR_CELLS_DEFAULT;

	prop = of_get_flat_dt_prop(node, "#size-cells", NULL);
	if (prop)
		dt_root_size_cells = be32_to_cpup(prop);
	pr_debug("dt_root_size_cells = %x\n", dt_root_size_cells);

	prop = of_get_flat_dt_prop(node, "#address-cells", NULL);
	if (prop)
		dt_root_addr_cells = be32_to_cpup(prop);
	pr_debug("dt_root_addr_cells = %x\n", dt_root_addr_cells);

	/* break now */
	return 1;
}

static u64 __init dt_mem_next_cell(int s, __be32 **cellp)
{
	__be32 *p = *cellp;

	*cellp = p + s;
	return of_read_number(p, s);
}

/**
 * early_init_dt_scan_memory - Look for and parse memory nodes
 */
static int early_init_dt_scan_memory(unsigned long node, const char *uname, int depth,
		void *data)
{
	uint32_t *reg;
	char *type = of_get_flat_dt_prop(node,
			"device_type", NULL);

	if (!type)
		return 0;

	reg = of_get_flat_dt_prop(node, "reg", NULL);
	if (!reg)
		return 0;

	mem_start = dt_mem_next_cell(dt_root_addr_cells, &reg);
	mem_size = dt_mem_next_cell(dt_root_size_cells, &reg);

	/* Break here if we found memory */
	if (mem_size)
		return 1;

	return 0;
}

int of_fdt_early_scan(void *_fdt, u64 *_mem_start, u64 *_mem_size)
{
	fdt = _fdt;

	of_scan_flat_dt(fdt, early_init_dt_scan_root, NULL);
	of_scan_flat_dt(fdt, early_init_dt_scan_memory, NULL);

	*_mem_start = mem_start;
	*_mem_size = mem_size;

	return 0;
}

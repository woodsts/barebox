// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <io.h>
#include <init.h>
#include <of_address.h>
#include <soc/fsl/immap_lsch2.h>
#include <soc/fsl/immap_lsch3.h>
#include <soc/fsl/fsl_qbman.h>
#include <soc/fsl/fsl_fman.h>
#include <mach/layerscape/layerscape.h>

/*
 * Stream IDs on Chassis-2 (for example ls1043a, ls1046a, ls1012) devices
 * are not hardwired and are programmed by sw.  There are a limited number
 * of stream IDs available, and the partitioning of them is scenario
 * dependent. This header defines the partitioning between legacy, PCI,
 * and DPAA1 devices.
 *
 * This partitioning can be customized in this file depending
 * on the specific hardware config:
 *
 *  -non-PCI legacy, platform devices (USB, SDHC, SATA, DMA, QE etc)
 *     -all legacy devices get a unique stream ID assigned and programmed in
 *      their AMQR registers by u-boot
 *
 *  -PCIe
 *     -there is a range of stream IDs set aside for PCI in this
 *      file.  U-boot will scan the PCI bus and for each device discovered:
 *         -allocate a streamID
 *         -set a PEXn LUT table entry mapping 'requester ID' to 'stream ID'
 *         -set a msi-map entry in the PEXn controller node in the
 *          device tree (see Documentation/devicetree/bindings/pci/pci-msi.txt
 *          for more info on the msi-map definition)
 *         -set a iommu-map entry in the PEXn controller node in the
 *          device tree (see Documentation/devicetree/bindings/pci/pci-iommu.txt
 *          for more info on the iommu-map definition)
 *
 *  -DPAA1
 *     - Stream ids for DPAA1 use are reserved for future usecase.
 *
 */

/* legacy devices */
#define FSL_USB1_STREAM_ID		1
#define FSL_USB2_STREAM_ID		2
#define FSL_USB3_STREAM_ID		3
#define FSL_SDHC_STREAM_ID		4
#define FSL_SATA_STREAM_ID		5
#define FSL_QE_STREAM_ID		6
#define FSL_QDMA_STREAM_ID		7
#define FSL_EDMA_STREAM_ID		8
#define FSL_ETR_STREAM_ID		9
#define FSL_DEBUG_STREAM_ID		10

/* PCI - programmed in PEXn_LUT */
#define FSL_PEX_STREAM_ID_START		11
#define FSL_PEX_STREAM_ID_END		26

/* DPAA1 - Stream-ID that can be programmed in DPAA1 h/w */
#define DPAA1_SID_START	27
#define DPAA1_SID_END	63

struct icid_id_table {
	const char *compat;
	u32 id;
	u32 reg;
	phys_addr_t compat_addr;
	phys_addr_t reg_addr;
};

static void of_set_iommu_prop(struct device_node *np, phandle iommu_handle,
			       int stream_id)
{
	u32 prop[] = {
		iommu_handle,
		stream_id
	};

	of_property_write_u32_array(np, "iommus", prop, ARRAY_SIZE(prop));
}

static phandle of_get_iommu_handle(struct device_node *root)
{
	struct device_node *iommu;

	iommu = of_find_compatible_node(root, NULL, "arm,mmu-500");
	if (!iommu) {
		pr_info("No \"arm,mmu-500\" node found, won't fixup\n");
		return 0;
	}

	return of_node_create_phandle(iommu);
}

static int of_fixup_icid(struct device_node *root, phandle iommu_handle,
			 const struct icid_id_table *icid_table, int num_icid)
{
	int i;

	for (i = 0; i < num_icid; i++) {
		const struct icid_id_table *icid = &icid_table[i];
		struct device_node *np;

		if (!icid->compat)
			continue;

		for_each_compatible_node_from(np, root, NULL, icid->compat) {
			struct resource res;

			if (of_address_to_resource(np, 0, &res))
				continue;

			if (res.start == icid->compat_addr) {
				of_set_iommu_prop(np, iommu_handle, icid->id);
				break;
			}
		}
	}

	return 0;
}

static void setup_icid_offsets(const struct icid_id_table *icid_table, int num_icids, bool le)
{
	int i;

	for (i = 0; i < num_icids; i++) {
		const struct icid_id_table *icid = &icid_table[i];

		if (le)
			out_le32((u32 *)(icid->reg_addr), icid->reg);
		else
			out_be32((u32 *)(icid->reg_addr), icid->reg);
	}
}

struct fman_icid_id_table {
	u32 port_id;
	u32 icid;
};

#define QMAN_CQSIDR_REG        0x20a80

#define SEC_JRnICID_LS(n)	((0x10 + (n) * 0x8) + 0x4)
#define SEC_RTICnICID_LS(n)	((0x60 + (n) * 0x8) + 0x4)
#define SEC_DECOnICID_LS(n)	((0xa0 + (n) * 0x8) + 0x4)
#define SEC_QIIC_LS	0x70024
#define	SEC_IRBAR_JRn(n) 	(0x10000 * ((n) + 1))

static const struct icid_id_table icid_tbl_ls1046a[] = {
	{
		.compat = "fsl,qman",
		.id = DPAA1_SID_START,
		.reg = DPAA1_SID_START,
		.compat_addr = LSCH2_QMAN_ADDR,
		.reg_addr = offsetof(struct ccsr_qman_v3, liodnr) + LSCH2_QMAN_ADDR,
	}, {
		.compat = "fsl,bman",
		.id = DPAA1_SID_START + 1,
		.reg = DPAA1_SID_START + 1,
		.compat_addr = LSCH2_BMAN_ADDR,
		.reg_addr = offsetof(struct ccsr_bman, liodnr) + LSCH2_BMAN_ADDR,
	}, {
		.compat = "fsl,esdhc",
		.id = FSL_SDHC_STREAM_ID,
		.reg = (((FSL_SDHC_STREAM_ID) << 24) | (1 << 23)),
		.compat_addr = LSCH2_ESDHC_ADDR,
		.reg_addr = offsetof(struct ccsr_scfg, sdhc_icid) + LSCH2_SCFG_ADDR,
	}, {
		.compat = "snps,dwc3",
		.id = FSL_USB1_STREAM_ID,
		.reg = (((FSL_USB1_STREAM_ID) << 24) | (1 << 23)),
		.compat_addr = LSCH2_XHCI_USB1_ADDR,
		.reg_addr = offsetof(struct ccsr_scfg, usb1_icid) + LSCH2_SCFG_ADDR,
	}, {
		.compat = "snps,dwc3",
		.id = FSL_USB2_STREAM_ID,
		.reg = (((FSL_USB2_STREAM_ID) << 24) | (1 << 23)),
		.compat_addr = LSCH2_XHCI_USB2_ADDR,
		.reg_addr = offsetof(struct ccsr_scfg, usb2_icid) + LSCH2_SCFG_ADDR,
	}, {
		.compat = "snps,dwc3",
		.id = FSL_USB3_STREAM_ID,
		.reg = (((FSL_USB3_STREAM_ID) << 24) | (1 << 23)),
		.compat_addr = LSCH2_XHCI_USB3_ADDR,
		.reg_addr = offsetof(struct ccsr_scfg, usb3_icid) + LSCH2_SCFG_ADDR,
	}, {
		.compat = "fsl,ls1046a-ahci",
		.id = FSL_SATA_STREAM_ID,
		.reg = (((FSL_SATA_STREAM_ID) << 24) | (1 << 23)),
		.compat_addr = LSCH2_HCI_BASE_ADDR,
		.reg_addr = offsetof(struct ccsr_scfg, sata_icid) + LSCH2_SCFG_ADDR,
	}, {
		.compat = "fsl,ls1046a-qdma",
		.id = FSL_QDMA_STREAM_ID,
		.reg = (1 << 31) | (FSL_QDMA_STREAM_ID),
		.compat_addr = LSCH2_QDMA_BASE_ADDR,
		.reg_addr = LSCH2_QDMA_BASE_ADDR + QMAN_CQSIDR_REG,
	}, {
		.id = FSL_QDMA_STREAM_ID,
		.reg = (1 << 31) | (FSL_QDMA_STREAM_ID),
		.compat_addr = LSCH2_QDMA_BASE_ADDR,
		.reg_addr = LSCH2_QDMA_BASE_ADDR + QMAN_CQSIDR_REG + 4,
	}, {
		.compat = "fsl,vf610-edma",
		.id = FSL_EDMA_STREAM_ID,
		.reg = (((FSL_EDMA_STREAM_ID) << 24) | (1 << 23)),
		.compat_addr = LSCH2_EDMA_BASE_ADDR,
		.reg_addr = offsetof(struct ccsr_scfg, edma_icid) + LSCH2_SCFG_ADDR,
	}, {
		.id = FSL_ETR_STREAM_ID,
		.reg = (((FSL_ETR_STREAM_ID) << 24) | (1 << 23)),
		.reg_addr = offsetof(struct ccsr_scfg, etr_icid) + LSCH2_SCFG_ADDR,
	}, {
		.id = FSL_DEBUG_STREAM_ID,
		.reg = (((FSL_DEBUG_STREAM_ID) << 24) | (1 << 23)),
		.reg_addr = offsetof(struct ccsr_scfg, debug_icid) + LSCH2_SCFG_ADDR,
	}, {
		.compat = "fsl,sec-v4.0",
		.id = DPAA1_SID_END,
		.compat_addr = LSCH2_SEC_ADDR,
		.reg_addr = SEC_QIIC_LS + LSCH2_SEC_ADDR,
	}, {
		.compat = "fsl,sec-v4.0-job-ring",
		.id = DPAA1_SID_START + 3,
		.reg = (((DPAA1_SID_START + 3) << 16) | (DPAA1_SID_START + 3)),
		.compat_addr = LSCH2_SEC_ADDR + SEC_IRBAR_JRn(0),
		.reg_addr = SEC_JRnICID_LS(0) + LSCH2_SEC_ADDR,
	}, {
		.compat = "fsl,sec-v4.0-job-ring",
		.id = DPAA1_SID_START + 4,
		.reg = (((DPAA1_SID_START + 4) << 16) | (DPAA1_SID_START + 4)),
		.compat_addr = LSCH2_SEC_ADDR + SEC_IRBAR_JRn(1),
		.reg_addr = SEC_JRnICID_LS(1) + LSCH2_SEC_ADDR,
	}, {
		.compat = "fsl,sec-v4.0-job-ring",
		.id = DPAA1_SID_START + 5,
		.reg = (((DPAA1_SID_START + 5) << 16) | (DPAA1_SID_START + 5)),
		.compat_addr = LSCH2_SEC_ADDR + SEC_IRBAR_JRn(2),
		.reg_addr = SEC_JRnICID_LS(2) + LSCH2_SEC_ADDR,
	}, {
		.compat = "fsl,sec-v4.0-job-ring",
		.id = DPAA1_SID_START + 6,
		.reg = (((DPAA1_SID_START + 6) << 16) | (DPAA1_SID_START + 6)),
		.compat_addr = LSCH2_SEC_ADDR + SEC_IRBAR_JRn(3),
		.reg_addr = SEC_JRnICID_LS(3) + LSCH2_SEC_ADDR,
	}, {
		.id = DPAA1_SID_START + 7,
		.reg = (((DPAA1_SID_START + 7) << 16) | (DPAA1_SID_START + 7)),
		.reg_addr = SEC_RTICnICID_LS(0) + LSCH2_SEC_ADDR,
	}, {
		.id = DPAA1_SID_START + 8,
		.reg = (((DPAA1_SID_START + 8) << 16) | (DPAA1_SID_START + 8)),
		.reg_addr = SEC_RTICnICID_LS(1) + LSCH2_SEC_ADDR,
	},{
		.id = DPAA1_SID_START + 9,
		.reg = (((DPAA1_SID_START + 9) << 16) | (DPAA1_SID_START + 9)),
		.reg_addr = SEC_RTICnICID_LS(2) + LSCH2_SEC_ADDR,
	}, {
		.id = DPAA1_SID_START + 10,
		.reg = (((DPAA1_SID_START + 10) << 16) | (DPAA1_SID_START + 10)),
		.reg_addr = SEC_RTICnICID_LS(3) + LSCH2_SEC_ADDR,
	}, {
		.id = DPAA1_SID_START + 11,
		.reg = (((DPAA1_SID_START + 11) << 16) | (DPAA1_SID_START + 11)),
		.reg_addr = SEC_DECOnICID_LS(0) + LSCH2_SEC_ADDR,
	}, {
		.id = DPAA1_SID_START + 12,
		.reg = (((DPAA1_SID_START + 12) << 16) | (DPAA1_SID_START + 12)),
		.reg_addr = SEC_DECOnICID_LS(1) + LSCH2_SEC_ADDR,
	}, {
		.id = DPAA1_SID_START + 13,
		.reg = (((DPAA1_SID_START + 13) << 16) | (DPAA1_SID_START + 13)),
		.reg_addr = SEC_DECOnICID_LS(2) + LSCH2_SEC_ADDR,
	},
};

static const struct fman_icid_id_table fman_icid_tbl_ls1046a[] = {
	{
		.port_id = 0x02,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x03,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x04,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x05,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x06,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x07,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x08,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x09,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x0a,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x0b,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x0c,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x0d,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x28,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x29,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x2a,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x2b,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x2c,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x2d,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x10,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x11,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x30,
		.icid = DPAA1_SID_END,
	}, {
		.port_id = 0x31,
		.icid = DPAA1_SID_END,
	},
};

static int get_fman_port_icid(int port_id, const struct fman_icid_id_table *tbl,
		       const int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (tbl[i].port_id == port_id)
			return tbl[i].icid;
	}

	return -ENODEV;
}

static void of_fixup_fman_port_icid_by_compat(struct device_node *root,
					       phandle iommu_handle,
					       const char *compat)
{
	struct device_node *np;
	int ret, icid;
	u32 cell_index;

	for_each_compatible_node_from(np, root, NULL, compat) {
		ret = of_property_read_u32(np, "cell-index", &cell_index);
		if (ret)
			continue;

		icid = get_fman_port_icid(cell_index, fman_icid_tbl_ls1046a,
					  ARRAY_SIZE(fman_icid_tbl_ls1046a));
		if (icid < 0) {
			printf("WARNING unknown ICID for fman port %u\n",
			       cell_index);
			continue;
		}

		of_set_iommu_prop(np, iommu_handle, icid);
	}
}

static void of_fixup_fman_icids(struct device_node *root, phandle iommu_handle)
{
	static const char * const compats[] = {
		"fsl,fman-v3-port-oh",
		"fsl,fman-v3-port-rx",
		"fsl,fman-v3-port-tx",
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(compats); i++)
		of_fixup_fman_port_icid_by_compat(root, iommu_handle, compats[i]);
}

struct qportal_info {
	u16 dicid;  /* DQRR ICID */
	u16 ficid;  /* frame data ICID */
	u16 icid;
	u8 sdest;
};

static const struct qportal_info qp_info[] = {
	{
		.dicid = DPAA1_SID_END,
		.ficid = DPAA1_SID_END,
		.icid = DPAA1_SID_END,
		.sdest = 0,
	}, {
		.dicid = DPAA1_SID_END,
		.ficid = DPAA1_SID_END,
		.icid = DPAA1_SID_END,
		.sdest = 0,
	}, {
		.dicid = DPAA1_SID_END,
		.ficid = DPAA1_SID_END,
		.icid = DPAA1_SID_END,
		.sdest = 0,
	}, {
		.dicid = DPAA1_SID_END,
		.ficid = DPAA1_SID_END,
		.icid = DPAA1_SID_END,
		.sdest = 0,
	}, {
		.dicid = DPAA1_SID_END,
		.ficid = DPAA1_SID_END,
		.icid = DPAA1_SID_END,
		.sdest = 0,
	}, {
		.dicid = DPAA1_SID_END,
		.ficid = DPAA1_SID_END,
		.icid = DPAA1_SID_END,
		.sdest = 0,
	}, {
		.dicid = DPAA1_SID_END,
		.ficid = DPAA1_SID_END,
		.icid = DPAA1_SID_END,
		.sdest = 0,
	}, {
		.dicid = DPAA1_SID_END,
		.ficid = DPAA1_SID_END,
		.icid = DPAA1_SID_END,
		.sdest = 0,
	}, {
		.dicid = DPAA1_SID_END,
		.ficid = DPAA1_SID_END,
		.icid = DPAA1_SID_END,
		.sdest = 0,
	}, {
		.dicid = DPAA1_SID_END,
		.ficid = DPAA1_SID_END,
		.icid = DPAA1_SID_END,
		.sdest = 0,
	},
};

#define BMAN_NUM_PORTALS     10
#define BMAN_MEM_BASE        0x508000000
#define BMAN_MEM_SIZE        0x08000000
#define BMAN_SP_CINH_SIZE    0x10000
#define BMAN_CENA_SIZE       (BMAN_MEM_SIZE >> 1)
#define BMAN_CINH_BASE       (BMAN_MEM_BASE + BMAN_CENA_SIZE)
#define BMAN_SWP_ISDR_REG    0x3e80
#define QMAN_MEM_BASE        0x500000000
#define QMAN_MEM_PHYS        QMAN_MEM_BASE
#define QMAN_MEM_SIZE        0x08000000
#define QMAN_SP_CINH_SIZE    0x10000
#define QMAN_CENA_SIZE       (QMAN_MEM_SIZE >> 1)
#define QMAN_CINH_BASE       (QMAN_MEM_BASE + QMAN_CENA_SIZE)
#define QMAN_SWP_ISDR_REG    0x3680

static void inhibit_portals(void __iomem *addr, int max_portals,
			    int portal_cinh_size)
{
	int i;

	for (i = 0; i < max_portals; i++) {
		out_be32(addr, -1);
		addr += portal_cinh_size;
	}
}

static void setup_qbman_portals(void)
{
	void __iomem *bpaddr = (void *)BMAN_CINH_BASE + BMAN_SWP_ISDR_REG;
	void __iomem *qpaddr = (void *)QMAN_CINH_BASE + QMAN_SWP_ISDR_REG;
	struct ccsr_qman_v3 *qman = IOMEM(LSCH2_QMAN_ADDR);
	int i;

	/* Set the Qman initiator BAR to match the LAW (for DQRR stashing) */
	out_be32(&qman->qcsp_bare, (u32)(QMAN_MEM_PHYS >> 32));
	out_be32(&qman->qcsp_bar, (u32)QMAN_MEM_PHYS);

	for (i = 0; i < ARRAY_SIZE(qp_info); i++) {
		const struct qportal_info *qi = &qp_info[i];

		out_be32(&qman->qcsp[i].qcsp_lio_cfg, (qi->icid << 16) | qi->dicid);
		/* set frame icid */
		out_be32(&qman->qcsp[i].qcsp_io_cfg, (qi->sdest << 16) | qi->ficid);
	}

	/* Change default state of BMan ISDR portals to all 1s */
	inhibit_portals(bpaddr, BMAN_NUM_PORTALS, BMAN_SP_CINH_SIZE);
	inhibit_portals(qpaddr, ARRAY_SIZE(qp_info), QMAN_SP_CINH_SIZE);
}

static void of_set_qportal_iommu_prop(struct device_node *np, phandle iommu_handle,
			       const struct qportal_info *qp_info)
{
	u32 prop[] = {
		iommu_handle,
		qp_info->icid,
		iommu_handle,
		qp_info->dicid,
		iommu_handle,
		qp_info->ficid
	};

	of_property_write_u32_array(np, "iommus", prop, ARRAY_SIZE(prop));
}

static void of_fixup_qportals(struct device_node *root, phandle iommu_handle)
{
	struct device_node *np;
	unsigned int maj, min;
	unsigned int ip_cfg;
	struct ccsr_qman_v3 *qman = IOMEM(LSCH2_QMAN_ADDR);
	u32 rev_1 = in_be32(&qman->ip_rev_1);
	u32 rev_2 = in_be32(&qman->ip_rev_2);
	u32 cell_index;
	int ret;

	maj = (rev_1 >> 8) & 0xff;
	min = rev_1 & 0xff;
	ip_cfg = rev_2 & 0xff;

	for_each_compatible_node_from(np, root, NULL, "fsl,qman-portal") {
		ret = of_property_read_u32(np, "cell-index", &cell_index);
		if (ret)
			continue;

		of_set_qportal_iommu_prop(np, iommu_handle, &qp_info[cell_index]);
	}
}

static int of_fixup_ls1046a(struct device_node *root, void *context)
{
	phandle iommu_handle;

	iommu_handle = of_get_iommu_handle(root);
	if (!iommu_handle)
		return 0;

	of_fixup_icid(root, iommu_handle, icid_tbl_ls1046a, ARRAY_SIZE(icid_tbl_ls1046a));
	of_fixup_fman_icids(root, iommu_handle);
	of_fixup_qportals(root, iommu_handle);

	return 0;
}

void ls1046a_setup_icids(void)
{
	struct ccsr_fman *fm = (void *)LSCH2_FM1_ADDR;
	int i;

	setup_icid_offsets(icid_tbl_ls1046a, ARRAY_SIZE(icid_tbl_ls1046a), false);

	/* setup fman icids */
	for (i = 0; i < ARRAY_SIZE(fman_icid_tbl_ls1046a); i++) {
		const struct fman_icid_id_table *icid = &fman_icid_tbl_ls1046a[i];

		out_be32(&fm->fm_bmi_common.fmbm_ppid[icid->port_id - 1],
			 icid->icid);
	}

	setup_qbman_portals();

	of_register_fixup(of_fixup_ls1046a, NULL);
}

static const struct icid_id_table icid_tbl_ls1028a[] = {
	{
		.compat = "snps,dwc3",
		.id = 1,
		.reg = 1,
		.compat_addr = LSCH3_XHCI_USB1_ADDR,
		.reg_addr = offsetof(struct lsch3_ccsr_gur, usb1_amqr) + LSCH3_GUTS_ADDR,
	}, {
		.compat = "snps,dwc3",
		.id = 2,
		.reg = 2,
		.compat_addr = LSCH3_XHCI_USB2_ADDR,
		.reg_addr = offsetof(struct lsch3_ccsr_gur, usb2_amqr) + LSCH3_GUTS_ADDR,
	}, {
		.compat = "fsl,esdhc",
		.id = 3,
		.reg = 3,
		.compat_addr = LSCH3_ESDHC1_BASE_ADDR,
		.reg_addr = offsetof(struct lsch3_ccsr_gur, sdmm1_amqr) + LSCH3_GUTS_ADDR,
	}, {
		.compat = "fsl,esdhc",
		.id = 69,
		.reg = 69,
		.compat_addr = LSCH3_ESDHC2_BASE_ADDR,
		.reg_addr = offsetof(struct lsch3_ccsr_gur, sdmm2_amqr) + LSCH3_GUTS_ADDR,
	}, {
		.compat = "fsl,ls1028a-ahci",
		.id = 4,
		.reg = 4,
		.compat_addr = LSCH3_AHCI1_ADDR,
		.reg_addr = offsetof(struct lsch3_ccsr_gur, sata1_amqr) + LSCH3_GUTS_ADDR,
	}, {
		.compat = "fsl,vf610-edma",
		.id = 40,
		.reg = 40,
		.compat_addr = LSCH3_EDMA_ADDR,
		.reg_addr = offsetof(struct lsch3_ccsr_gur, spare3_amqr) + LSCH3_GUTS_ADDR,
	},  {
		.compat = "fsl,ls1028a-qdma",
		.id = 5,
		.reg = (1 << 31) | 5,
		.compat_addr = LSCH3_QDMA_ADDR,
		.reg_addr = LSCH3_QDMA_ADDR + QMAN_CQSIDR_REG,
	}, {
		.compat = NULL,
		.id = 5,
		.reg = (1 << 31) | 5,
		.compat_addr = LSCH3_QDMA_ADDR,
		.reg_addr = LSCH3_QDMA_ADDR + QMAN_CQSIDR_REG + 4,
	}, {
		.compat = "vivante,gc",
		.id = 71,
		.reg = 71,
		.compat_addr = LSCH3_GPU_ADDR,
		.reg_addr = offsetof(struct lsch3_ccsr_gur, misc1_amqr) + LSCH3_GUTS_ADDR,
	}, {
		.compat = "arm,mali-dp500",
		.id = 72,
		.reg = 72,
		.compat_addr = LSCH3_DISPLAY_ADDR,
		.reg_addr = offsetof(struct lsch3_ccsr_gur, spare2_amqr) + LSCH3_GUTS_ADDR,
	}, {
		.compat = "fsl,sec-v4.0-job-ring",
		.id = 65,
		.reg = 65,
		.compat_addr = LSCH3_SEC_JR0_ADDR,
		.reg_addr = offsetof(struct ccsr_sec, jrliodnr[0].ls) + LSCH3_SEC_ADDR,
        }, {
		.compat = "fsl,sec-v4.0-job-ring",
		.id = 66,
		.reg = 66,
		.compat_addr = LSCH3_SEC_JR1_ADDR,
		.reg_addr = offsetof(struct ccsr_sec, jrliodnr[1].ls) + LSCH3_SEC_ADDR,
	}, {
		.id = 67,
		.reg = 67,
		.compat_addr = LSCH3_SEC_JR2_ADDR,
		.reg_addr = offsetof(struct ccsr_sec, jrliodnr[2].ls) + LSCH3_SEC_ADDR,
	}, {
		.compat = "fsl,sec-v4.0-job-ring",
		.id = 68,
		.reg = 68,
		.compat_addr = LSCH3_SEC_JR3_ADDR,
		.reg_addr = offsetof(struct ccsr_sec, jrliodnr[3].ls) + LSCH3_SEC_ADDR,
	}, {
		.id = 64,
		.reg = 64,
		.compat_addr = 0,
		.reg_addr = offsetof(struct ccsr_sec, rticliodnr[0].ls) + LSCH3_SEC_ADDR,
	}, {
		.id = 64,
		.reg = 64,
		.compat_addr = 0,
		.reg_addr = offsetof(struct ccsr_sec, rticliodnr[1].ls) + LSCH3_SEC_ADDR,
	}, {
		.id = 64,
		.reg = 64,
		.compat_addr = 0,
		.reg_addr = offsetof(struct ccsr_sec, rticliodnr[2].ls) + LSCH3_SEC_ADDR,
	}, {
		.id = 64,
		.reg = 64,
		.compat_addr = 0,
		.reg_addr = offsetof(struct ccsr_sec, rticliodnr[3].ls) + LSCH3_SEC_ADDR,
	}, {
		.id = 64,
		.reg = 64,
		.compat_addr = 0,
		.reg_addr = offsetof(struct ccsr_sec, decoliodnr[0].ls) + LSCH3_SEC_ADDR,
	}, {
		.id = 64,
		.reg = 64,
		.compat_addr = 0,
		.reg_addr = offsetof(struct ccsr_sec, decoliodnr[1].ls) + LSCH3_SEC_ADDR,
	}
};

static int of_fixup_icid_ls1028a(struct device_node *root, void *context)
{
	phandle iommu_handle;

	iommu_handle = of_get_iommu_handle(root);
	if (!iommu_handle)
		return 0;

	of_fixup_icid(root, iommu_handle, icid_tbl_ls1028a, ARRAY_SIZE(icid_tbl_ls1028a));

	return 0;
}

/* offset of IERB config register per PCI function */
static const int ierb_offset[] = {
	0x0800,
	0x1800,
	0x2800,
	0x3800,
	0x4800,
	0x5800,
	0x6800,
	-1,
	0x0804,
	0x0808,
	0x1804,
	0x1808,
};

#define ECAM_IERB_BASE                0x1f0800000ULL
#define ECAM_IERB_MSICAR              (ECAM_IERB_BASE + 0xa400)
#define ECAM_IERB_MSICAR_VALUE                0x30
#define FSL_ECAM_STREAM_ID_START 41

/*
 * Use a custom function for LS1028A, for now this is the only SoC with IERB
 * and we're currently considering reorganizing IERB for future SoCs.
 */
static void ls1028a_set_ecam_icids(void)
{
	int i;

	out_le32(ECAM_IERB_MSICAR, ECAM_IERB_MSICAR_VALUE);

	for (i = 0; i < ARRAY_SIZE(ierb_offset); i++) {
		if (ierb_offset[i] < 0)
			continue;

		out_le32(ECAM_IERB_BASE + ierb_offset[i],
			 FSL_ECAM_STREAM_ID_START + i);
	}
}

static int of_fixup_ecam_ls1028a(struct device_node *root, void *context)
{
	struct device_node *np;
	int i, ret;
	const char *props[] = { "msi-map", "iommu-map" };
	uint32_t map[4];

	np = of_find_compatible_node(root, NULL, "pci-host-ecam-generic");
	if (!np) {
		pr_info("No \"pci-host-ecam-generic\" node found, won't fixup\n");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(props); i++) {
		ret = of_property_read_u32_array(np, props[i], map, 4);
		if (ret) {
			pr_err("Cannot read \"%s\" property: %pe", props[i], ERR_PTR(ret));
			return ret;
		}

		map[2] = FSL_ECAM_STREAM_ID_START;
		map[3] = ARRAY_SIZE(ierb_offset);
		ret = of_property_write_u32_array(np, props[i], map, 4);
		if (ret) {
			pr_err("Cannot write \"%s\" property: %pe", props[i], ERR_PTR(ret));
			return ret;
		}
	}

	return 0;
}

void ls1028a_setup_icids(void)
{
	setup_icid_offsets(icid_tbl_ls1028a, ARRAY_SIZE(icid_tbl_ls1028a), true);

	ls1028a_set_ecam_icids();

	of_register_fixup(of_fixup_icid_ls1028a, NULL);
	of_register_fixup(of_fixup_ecam_ls1028a, NULL);
}

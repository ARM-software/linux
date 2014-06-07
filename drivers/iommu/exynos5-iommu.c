/* linux/drivers/iommu/exynos_iommu.c
 *
 * Copyright (c) 2013-2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef CONFIG_EXYNOS_IOMMU_DEBUG
#define DEBUG
#endif

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/device.h>

#include <asm/pgtable.h>

#include "exynos-iommu.h"

#define CFG_LRU		0x1
#define CFG_MASK	0x0150FFFF /* Selecting bit 0-15, 20, 22 and 24 */
#define CFG_SYSSEL	(1 << 22) /* System MMU 3.2 only */

#define PB_INFO_NUM(reg)	((reg) & 0xFF) /* System MMU 3.3 only */

#define REG_MMU_FLUSH		0x00C
#define REG_MMU_FLUSH_ENTRY	0x010
#define REG_PT_BASE_ADDR	0x014
#define REG_INT_STATUS		0x018
#define REG_INT_CLEAR		0x01C
#define REG_PB_INFO		0x400
#define REG_PB_LMM		0x404
#define REG_PB_INDICATE		0x408
#define REG_PB_CFG		0x40C
#define REG_PB_START_ADDR	0x410
#define REG_PB_END_ADDR		0x414
#define REG_SPB_BASE_VPN	0x418

#define REG_PAGE_FAULT_ADDR	0x024
#define REG_AW_FAULT_ADDR	0x028
#define REG_AR_FAULT_ADDR	0x02C
#define REG_DEFAULT_SLAVE_ADDR	0x030
#define REG_FAULT_TRANS_INFO	0x04C
#define REG_L1TLB_READ_ENTRY	0x040
#define REG_L1TLB_ENTRY_PPN	0x044
#define REG_L1TLB_ENTRY_VPN	0x048

#define MAX_NUM_PBUF		6
#define SINGLE_PB_SIZE		16

#define NUM_MINOR_OF_SYSMMU_V3	4

#define MMU_TLB_ENT_NUM(val)	((val) & 0x7F)

enum exynos_sysmmu_inttype {
	SYSMMU_PAGEFAULT,
	SYSMMU_AR_MULTIHIT,
	SYSMMU_AW_MULTIHIT,
	SYSMMU_BUSERROR,
	SYSMMU_AR_SECURITY,
	SYSMMU_AR_ACCESS,
	SYSMMU_AW_SECURITY,
	SYSMMU_AW_PROTECTION, /* 7 */
	SYSMMU_FAULT_UNDEF,
	SYSMMU_FAULTS_NUM
};

static unsigned short fault_reg_offset[9] = {
	REG_PAGE_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AW_FAULT_ADDR,
	REG_PAGE_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AW_FAULT_ADDR,
	REG_AW_FAULT_ADDR
};

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PAGE FAULT",
	"AR MULTI-HIT FAULT",
	"AW MULTI-HIT FAULT",
	"BUS ERROR",
	"AR SECURITY PROTECTION FAULT",
	"AR ACCESS PROTECTION FAULT",
	"AW SECURITY PROTECTION FAULT",
	"AW ACCESS PROTECTION FAULT",
	"UNDEFINED FAULT"
};

static bool has_sysmmu_capable_pbuf(void __iomem *sfrbase, int *min)
{
	unsigned int ver;

	ver = __raw_sysmmu_version(sfrbase);
	if (min)
		*min = MMU_MIN_VER(ver);
	return MMU_MAJ_VER(ver) == 3;
}

void __sysmmu_tlb_invalidate(void __iomem *sfrbase,
				dma_addr_t iova, size_t size)
{
	if (!WARN_ON(!sysmmu_block(sfrbase)))
		__raw_writel(0x1, sfrbase + REG_MMU_FLUSH);
	sysmmu_unblock(sfrbase);
}

void __sysmmu_tlb_invalidate_flpdcache(void __iomem *sfrbase, dma_addr_t iova)
{
	if (__raw_sysmmu_version(sfrbase) == MAKE_MMU_VER(3, 3))
		__raw_writel(iova | 0x1, sfrbase + REG_MMU_FLUSH_ENTRY);
}

void __sysmmu_tlb_invalidate_entry(void __iomem *sfrbase, dma_addr_t iova)
{
	__raw_writel(iova | 0x1, sfrbase + REG_MMU_FLUSH_ENTRY);
}

void __sysmmu_set_ptbase(void __iomem *sfrbase, phys_addr_t pfn_pgtable)
{
	__raw_writel(pfn_pgtable * PAGE_SIZE, sfrbase + REG_PT_BASE_ADDR);

	__raw_writel(0x1, sfrbase + REG_MMU_FLUSH);
}

static void __sysmmu_set_prefbuf(void __iomem *pbufbase, unsigned long base,
					unsigned long size, int idx)
{
	__raw_writel(base, pbufbase + idx * 8);
	__raw_writel(size - 1 + base,  pbufbase + 4 + idx * 8);
}

/*
 * Offset of prefetch buffer setting registers are different
 * between SysMMU 3.1 and 3.2. 3.3 has a single prefetch buffer setting.
 */
static unsigned short
	pbuf_offset[NUM_MINOR_OF_SYSMMU_V3] = {0x04C, 0x04C, 0x070, 0x410};


static void __sysmmu_set_pbuf_ver31(struct sysmmu_drvdata *drvdata,
				struct sysmmu_prefbuf prefbuf[], int num_bufs)
{
	unsigned long cfg =
		__raw_readl(drvdata->sfrbase + REG_MMU_CFG) & CFG_MASK;

	/* Only the first 2 buffers are set to PB */
	if (num_bufs >= 2) {
		/* Separate PB mode */
		cfg |= 2 << 28;

		if (prefbuf[1].size == 0)
			prefbuf[1].size = 1;
		__sysmmu_set_prefbuf(drvdata->sfrbase + pbuf_offset[1],
					prefbuf[1].base, prefbuf[1].size, 1);
	} else {
		/* Combined PB mode */
		cfg |= 3 << 28;
		drvdata->num_pbufs = 1;
		drvdata->pbufs[0] = prefbuf[0];
	}

	__raw_writel(cfg, drvdata->sfrbase + REG_MMU_CFG);

	if (prefbuf[0].size == 0)
		prefbuf[0].size = 1;
	__sysmmu_set_prefbuf(drvdata->sfrbase + pbuf_offset[1],
				prefbuf[0].base, prefbuf[0].size, 0);
}

static void __sysmmu_set_pbuf_ver32(struct sysmmu_drvdata *drvdata,
				struct sysmmu_prefbuf prefbuf[], int num_bufs)
{
	int i;
	unsigned long cfg =
		__raw_readl(drvdata->sfrbase + REG_MMU_CFG) & CFG_MASK;

	cfg |= 7 << 16; /* enabling PB0 ~ PB2 */

	switch (num_bufs) {
	case 1:
		/* Combined PB mode (0 ~ 2) */
		cfg |= 1 << 19;
		break;
	case 2:
		/* Combined PB mode (0 ~ 1) */
		cfg |= 1 << 21;
		break;
	case 3:
		break;
	default:
		num_bufs = 3; /* Only the first 3 buffers are set to PB */
	}

	for (i = 0; i < num_bufs; i++) {
		if (prefbuf[i].size == 0) {
			dev_err(drvdata->sysmmu,
				"%s: Trying to init PB[%d/%d]with zero-size\n",
				__func__, i, num_bufs);
			prefbuf[i].size = 1;
		}
		__sysmmu_set_prefbuf(drvdata->sfrbase + pbuf_offset[2],
			prefbuf[i].base, prefbuf[i].size, i);
	}

	__raw_writel(cfg, drvdata->sfrbase + REG_MMU_CFG);
}

static unsigned int find_lmm_preset(unsigned int num_pb, unsigned int num_bufs)
{
	static char lmm_preset[4][6] = {  /* [num of PB][num of buffers] */
	/*	  1,  2,  3,  4,  5,  6 */
		{ 1,  1,  0, -1, -1, -1}, /* num of pb: 3 */
		{ 3,  2,  1,  0, -1, -1}, /* num of pb: 4 */
		{-1, -1, -1, -1, -1, -1},
		{ 5,  5,  4,  2,  1,  0}, /* num of pb: 6 */
		};
	unsigned int lmm;

	BUG_ON(num_bufs > 6);
	lmm = lmm_preset[num_pb - 3][num_bufs - 1];
	BUG_ON(lmm == -1);
	return lmm;
}

static unsigned int find_num_pb(unsigned int num_pb, unsigned int lmm)
{
	static char lmm_preset[6][6] = { /* [pb_num - 1][pb_lmm] */
		{0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0},
		{3, 2, 0, 0, 0, 0},
		{4, 3, 2, 1, 0, 0},
		{0, 0, 0, 0, 0, 0},
		{6, 5, 4, 3, 3, 2},
	};

	num_pb = lmm_preset[num_pb - 1][lmm];
	BUG_ON(num_pb == 0);
	return num_pb;
}

static void __sysmmu_set_pbuf_ver33(struct sysmmu_drvdata *drvdata,
				struct sysmmu_prefbuf prefbuf[], int num_bufs)
{
	unsigned int i, num_pb, lmm;

	num_pb = PB_INFO_NUM(__raw_readl(drvdata->sfrbase + REG_PB_INFO));

	lmm = find_lmm_preset(num_pb, (unsigned int)num_bufs);
	num_pb = find_num_pb(num_pb, lmm);

	__raw_writel(lmm, drvdata->sfrbase + REG_PB_LMM);

	for (i = 0; i < num_pb; i++) {
		__raw_writel(i, drvdata->sfrbase + REG_PB_INDICATE);
		__raw_writel(0, drvdata->sfrbase + REG_PB_CFG);
		if (prefbuf[i].size == 0) {
			dev_err(drvdata->sysmmu,
				"%s: Trying to init PB[%d/%d]with zero-size\n",
				__func__, i, num_bufs);
			continue;
		}
		if (num_bufs <= i)
			continue; /* unused PB */
		__sysmmu_set_prefbuf(drvdata->sfrbase + pbuf_offset[3],
					prefbuf[i].base, prefbuf[i].size, 0);
		__raw_writel(prefbuf[i].config | 1,
					drvdata->sfrbase + REG_PB_CFG);
	}
}

static void (*func_set_pbuf[NUM_MINOR_OF_SYSMMU_V3])
		(struct sysmmu_drvdata *, struct sysmmu_prefbuf *, int) = {
		__sysmmu_set_pbuf_ver31,
		__sysmmu_set_pbuf_ver31,
		__sysmmu_set_pbuf_ver32,
		__sysmmu_set_pbuf_ver33,
};


static void __sysmmu_disable_pbuf_ver31(void __iomem *sfrbase)
{
	__raw_writel(__raw_readl(sfrbase + REG_MMU_CFG) & CFG_MASK,
			sfrbase + REG_MMU_CFG);
}

#define __sysmmu_disable_pbuf_ver32 __sysmmu_disable_pbuf_ver31

static void __sysmmu_disable_pbuf_ver33(void __iomem *sfrbase)
{
	unsigned int i, num_pb;
	num_pb = PB_INFO_NUM(__raw_readl(sfrbase + REG_PB_INFO));

	__raw_writel(0, sfrbase + REG_PB_LMM);

	for (i = 0; i < num_pb; i++) {
		__raw_writel(i, sfrbase + REG_PB_INDICATE);
		__raw_writel(0, sfrbase + REG_PB_CFG);
	}
}

static void (*func_disable_pbuf[NUM_MINOR_OF_SYSMMU_V3]) (void __iomem *) = {
		__sysmmu_disable_pbuf_ver31,
		__sysmmu_disable_pbuf_ver31,
		__sysmmu_disable_pbuf_ver32,
		__sysmmu_disable_pbuf_ver33,
};

static unsigned int __sysmmu_get_num_pb(struct sysmmu_drvdata *drvdata,
					int *min)
{
	if (!has_sysmmu_capable_pbuf(drvdata->sfrbase, min))
		return 0;

	switch (*min) {
	case 0:
	case 1:
		return 2;
	case 2:
		return 3;
	case 3:
		return PB_INFO_NUM(__raw_readl(drvdata->sfrbase + REG_PB_INFO));
	default:
		BUG();
	}

	return 0;
}

void __exynos_sysmmu_set_prefbuf_by_region(struct sysmmu_drvdata *drvdata,
			struct sysmmu_prefbuf pb_reg[], unsigned int num_reg)
{
	unsigned int i;
	int num_bufs = 0;
	struct sysmmu_prefbuf prefbuf[6];
	unsigned int version;

	version = __raw_sysmmu_version(drvdata->sfrbase);
	if (version < MAKE_MMU_VER(3, 0))
		return;

	if ((num_reg == 0) || (pb_reg == NULL)) {
		/* Disabling prefetch buffers */
		func_disable_pbuf[MMU_MIN_VER(version)](drvdata->sfrbase);
		return;
	}

	for (i = 0; i < num_reg; i++) {
		if (((pb_reg[i].config & SYSMMU_PBUFCFG_WRITE) &&
					(drvdata->prop & SYSMMU_PROP_WRITE)) ||
			(!(pb_reg[i].config & SYSMMU_PBUFCFG_WRITE) &&
				 (drvdata->prop & SYSMMU_PROP_READ)))
			prefbuf[num_bufs++] = pb_reg[i];
	}

	func_set_pbuf[MMU_MIN_VER(version)](drvdata, prefbuf, num_bufs);
}

void __exynos_sysmmu_set_prefbuf_by_plane(struct sysmmu_drvdata *drvdata,
			unsigned int inplanes, unsigned int onplanes,
			unsigned int ipoption, unsigned int opoption)
{
	unsigned int num_pb;
	int num_bufs, min;
	struct sysmmu_prefbuf prefbuf[6];

	num_pb = __sysmmu_get_num_pb(drvdata, &min);
	if (num_pb == 0) /* No Prefetch buffers */
		return;

	num_bufs = __prepare_prefetch_buffers_by_plane(drvdata,
			prefbuf, num_pb, inplanes, onplanes,
			ipoption, opoption);

	if (num_bufs == 0)
		func_disable_pbuf[min](drvdata->sfrbase);
	else
		func_set_pbuf[min](drvdata, prefbuf, num_bufs);
}

void dump_sysmmu_tlb_pb(void __iomem *sfrbase)
{
	unsigned int i, capa, lmm, tlb_ent_num;

	lmm = MMU_RAW_VER(__raw_readl(sfrbase + REG_MMU_VERSION));

	pr_crit("---------- System MMU Status -----------------------------\n");
	pr_crit("VERSION %d.%d, MMU_CFG: %#010x, MMU_STATUS: %#010x\n",
		MMU_MAJ_VER(lmm), MMU_MIN_VER(lmm),
		__raw_readl(sfrbase + REG_MMU_CFG),
		__raw_readl(sfrbase + REG_MMU_STATUS));


	pr_crit("---------- Level 1 TLB -----------------------------------\n");

	tlb_ent_num = MMU_TLB_ENT_NUM(__raw_readl(sfrbase + REG_MMU_VERSION));
	for (i = 0; i < tlb_ent_num; i++) {
		__raw_writel(i, sfrbase + REG_L1TLB_READ_ENTRY);
		pr_crit("[%02d] VPN: %#010x, PPN: %#010x\n",
			i, __raw_readl(sfrbase + REG_L1TLB_ENTRY_VPN),
			__raw_readl(sfrbase + REG_L1TLB_ENTRY_PPN));
	}

	capa = __raw_readl(sfrbase + REG_PB_INFO);
	lmm = __raw_readl(sfrbase + REG_PB_LMM);

	pr_crit("---------- Prefetch Buffers ------------------------------\n");
	pr_crit("PB_INFO: %#010x, PB_LMM: %#010x\n", capa, lmm);

	capa = find_num_pb(capa & 0xFF, lmm);

	for (i = 0; i < capa; i++) {
		__raw_writel(i, sfrbase + REG_PB_INDICATE);
		pr_crit("PB[%d] = CFG: %#010x, START: %#010x, END: %#010x\n", i,
			__raw_readl(sfrbase + REG_PB_CFG),
			__raw_readl(sfrbase + REG_PB_START_ADDR),
			__raw_readl(sfrbase + REG_PB_END_ADDR));

		pr_crit("PB[%d]_SUB0 BASE_VPN = %#010x\n", i,
			__raw_readl(sfrbase + REG_SPB_BASE_VPN));
		__raw_writel(i | 0x100, sfrbase + REG_PB_INDICATE);
		pr_crit("PB[%d]_SUB1 BASE_VPN = %#010x\n", i,
			__raw_readl(sfrbase + REG_SPB_BASE_VPN));
	}

	/* Reading L2TLB is not provided by H/W */
}

static void show_fault_information(struct sysmmu_drvdata *drvdata,
					enum exynos_sysmmu_inttype itype,
					unsigned long fault_addr)
{
	unsigned int info;
	phys_addr_t pgtable;
	unsigned int version;

	pgtable = __raw_readl(drvdata->sfrbase + REG_PT_BASE_ADDR);

	pr_crit("----------------------------------------------------------\n");
	pr_crit("%s %s at %#010lx by %s (page table @ %#010x)\n",
		dev_name(drvdata->sysmmu),
		sysmmu_fault_name[itype], fault_addr,
		dev_name(drvdata->master), pgtable);

	if (itype== SYSMMU_FAULT_UNDEF) {
		pr_crit("The fault is not caused by this System MMU.\n");
		pr_crit("Please check IRQ and SFR base address.\n");
		goto finish;
	}

	version = __raw_sysmmu_version(drvdata->sfrbase);
	if (version == MAKE_MMU_VER(3, 3)) {
		info = __raw_readl(drvdata->sfrbase +
				REG_FAULT_TRANS_INFO);
		pr_crit("AxID: %#x, AxLEN: %#x RW: %s\n",
			info & 0xFFFF, (info >> 16) & 0xF,
			(info >> 20) ? "WRITE" : "READ");
	}

	if (pgtable != drvdata->pgtable)
		pr_crit("Page table base of driver: %#010x\n",
			drvdata->pgtable);

	if (itype == SYSMMU_BUSERROR) {
		pr_crit("System MMU has failed to access page table\n");
		goto finish;
	}

	if (!pfn_valid(pgtable >> PAGE_SHIFT)) {
		pr_crit("Page table base is not in a valid memory region\n");
	} else {
		sysmmu_pte_t *ent;
		ent = section_entry(phys_to_virt(pgtable), fault_addr);
		pr_crit("Lv1 entry: %#010x\n", *ent);

		if (lv1ent_page(ent)) {
			ent = page_entry(ent, fault_addr);
			pr_crit("Lv2 entry: %#010x\n", *ent);
		}
	}

	if (MMU_MIN_VER(version) == 3)
		dump_sysmmu_tlb_pb(drvdata->sfrbase);

finish:
	pr_crit("----------------------------------------------------------\n");
}

irqreturn_t exynos_sysmmu_irq(int irq, void *dev_id)
{
	/* SYSMMU is in blocked when interrupt occurred. */
	struct sysmmu_drvdata *drvdata = dev_id;
	unsigned int itype;
	unsigned long addr = -1;
	int ret = -ENOSYS;
	int flags = 0;

	WARN(!is_sysmmu_active(drvdata),
		"Fault occurred while System MMU %s is not enabled!\n",
		dev_name(drvdata->sysmmu));

	itype =  __ffs(__raw_readl(drvdata->sfrbase + REG_INT_STATUS));

	if (WARN_ON(!((itype >= 0) && (itype < SYSMMU_FAULT_UNDEF))))
		itype = SYSMMU_FAULT_UNDEF;
	else
		addr = __raw_readl(drvdata->sfrbase + fault_reg_offset[itype]);

	show_fault_information(drvdata, itype, addr);

	if (drvdata->domain) /* master is set if drvdata->domain exists */
		ret = report_iommu_fault(drvdata->domain,
					drvdata->master, addr, flags);

	panic("Unrecoverable System MMU Fault!!");

	return IRQ_HANDLED;
}

void __sysmmu_init_config(struct sysmmu_drvdata *drvdata)
{
	unsigned long cfg = CFG_LRU | CFG_QOS(drvdata->qos);
	unsigned int version;

	__raw_writel(0, drvdata->sfrbase + REG_MMU_CTRL);

	version = __raw_sysmmu_version(drvdata->sfrbase);
	if (version < MAKE_MMU_VER(3, 0))
		goto set_cfg;

	if (MMU_MAJ_VER(version) != 3)
		panic("%s: Failed to read version (%d.%d), master: %s\n",
			dev_name(drvdata->sysmmu), MMU_MAJ_VER(version),
			MMU_MIN_VER(version), dev_name(drvdata->master));


	if (MMU_MIN_VER(version) < 2)
		goto set_pb;

	BUG_ON(MMU_MIN_VER(version) > 3);

	cfg |= CFG_FLPDCACHE;
	cfg |= (MMU_MIN_VER(version) == 2) ? CFG_SYSSEL : CFG_ACGEN;
	cfg |= CFG_QOS_OVRRIDE;

set_pb:
	__exynos_sysmmu_set_prefbuf_by_plane(drvdata, 0, 0,
				SYSMMU_PBUFCFG_DEFAULT_INPUT,
				SYSMMU_PBUFCFG_DEFAULT_OUTPUT);
set_cfg:
	cfg |= __raw_readl(drvdata->sfrbase + REG_MMU_CFG) & ~CFG_MASK;
	__raw_writel(cfg, drvdata->sfrbase + REG_MMU_CFG);
}

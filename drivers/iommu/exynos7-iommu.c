/* linux/drivers/iommu/exynos7_iommu.c
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
#include <linux/sched.h>

#include <asm/pgtable.h>

#include "exynos-iommu.h"

#define CFG_MASK	0x01101FBC /* Selecting bit 24, 20, 12-7, 5-2 */

#define PB_INFO_NUM(reg)	((reg) & 0xFF)
#define L1TLB_ATTR_IM	(1 << 16)

#define REG_PT_BASE_PPN		0x00C
#define REG_MMU_FLUSH		0x010
#define REG_MMU_FLUSH_ENTRY	0x014
#define REG_MMU_FLUSH_RANGE	0x018
#define REG_FLUSH_RANGE_START	0x020
#define REG_FLUSH_RANGE_END	0x024
#define REG_MMU_CAPA		0x030
#define REG_INT_STATUS		0x060
#define REG_INT_CLEAR		0x064
#define REG_FAULT_AR_ADDR	0x070
#define REG_FAULT_AR_TRANS_INFO	0x078
#define REG_FAULT_AW_ADDR	0x080
#define REG_FAULT_AW_TRANS_INFO	0x088
#define REG_L1TLB_CFG		0x100 /* sysmmu v5.1 only */
#define REG_L1TLB_CTRL		0x108 /* sysmmu v5.1 only */
#define REG_L2TLB_CFG		0x200 /* sysmmu that has L2TLB only*/
#define REG_PB_LMM		0x300
#define REG_PB_INDICATE		0x308
#define REG_PB_CFG		0x310
#define REG_PB_START_ADDR	0x320
#define REG_PB_END_ADDR		0x328
#define REG_PB_INFO		0x350
#define REG_SW_DF_VPN		0x400 /* sysmmu v5.1 only */
#define REG_SW_DF_VPN_CMD_NUM	0x408 /* sysmmu v5.1 only */
#define REG_L1TLB_READ_ENTRY	0x750
#define REG_L1TLB_ENTRY_VPN	0x754
#define REG_L1TLB_ENTRY_PPN	0x75C
#define REG_L1TLB_ENTRY_ATTR	0x764

/* 'reg' argument must be the value of REG_MMU_CAPA register */
#define MMU_NUM_L1TLB_ENTRIES(reg) (reg & 0xFF)
#define MMU_HAVE_PB(reg)	(!!((reg >> 20) & 0xF))
#define MMU_HAVE_L2TLB(reg)	(!!((reg >> 8) & 0xFFF))

#define MMU_MAX_DF_CMD		8

#define SYSMMU_FAULTS_NUM         (SYSMMU_FAULT_UNKNOWN + 1)

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PTW ACCESS FAULT",
	"PAGE FAULT",
	"L1TLB MULTI-HIT FAULT",
	"ACCESS FAULT",
	"SECURITY FAULT",
	"UNKNOWN FAULT"
};

static bool has_sysmmu_capable_pbuf(void __iomem *sfrbase)
{
	unsigned long cfg = __raw_readl(sfrbase + REG_MMU_CAPA);

	return MMU_HAVE_PB(cfg) ? true : false;
}

void __sysmmu_tlb_invalidate_flpdcache(void __iomem *sfrbase, dma_addr_t iova)
{
	if (has_sysmmu_capable_pbuf(sfrbase))
		__raw_writel(iova | 0x1, sfrbase + REG_MMU_FLUSH_ENTRY);
}

void __sysmmu_tlb_invalidate_entry(void __iomem *sfrbase, dma_addr_t iova)
{
	__raw_writel(iova | 0x1, sfrbase + REG_MMU_FLUSH_ENTRY);
}

void __sysmmu_tlb_invalidate(void __iomem *sfrbase,
				dma_addr_t iova, size_t size)
{
	if (__raw_sysmmu_version(sfrbase) >= MAKE_MMU_VER(5, 1)) {
		__raw_writel(iova, sfrbase + REG_FLUSH_RANGE_START);
		__raw_writel(size - 1 + iova, sfrbase + REG_FLUSH_RANGE_END);
		__raw_writel(0x1, sfrbase + REG_MMU_FLUSH_RANGE);
	} else {
		if (sysmmu_block(sfrbase))
			__raw_writel(0x1, sfrbase + REG_MMU_FLUSH);
		sysmmu_unblock(sfrbase);
	}
}

void __sysmmu_set_ptbase(void __iomem *sfrbase, phys_addr_t pfn_pgtable)
{
	__raw_writel(pfn_pgtable, sfrbase + REG_PT_BASE_PPN);

	__raw_writel(0x1, sfrbase + REG_MMU_FLUSH);
}

static void __sysmmu_disable_pbuf(void __iomem *sfrbase)
{
	unsigned int i, num_pb;

	num_pb = PB_INFO_NUM(__raw_readl(sfrbase + REG_PB_INFO));

	__raw_writel(0, sfrbase + REG_PB_LMM);

	for (i = 0; i < num_pb; i++) {
		__raw_writel(i, sfrbase + REG_PB_INDICATE);
		__raw_writel(0, sfrbase + REG_PB_CFG);
	}
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

static void __sysmmu_set_pbuf(struct sysmmu_drvdata *drvdata,
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
		__raw_writel(prefbuf[i].base,
			     drvdata->sfrbase + REG_PB_START_ADDR);
		__raw_writel(prefbuf[i].size - 1 + prefbuf[i].base,
				drvdata->sfrbase + REG_PB_END_ADDR);
		__raw_writel(prefbuf[i].config | 1,
					drvdata->sfrbase + REG_PB_CFG);
	}
}

void __exynos_sysmmu_set_prefbuf_by_region(struct sysmmu_drvdata *drvdata,
		struct sysmmu_prefbuf pb_reg[], unsigned int num_reg)
{
	unsigned int i;
	int num_bufs = 0;
	struct sysmmu_prefbuf prefbuf[6];

	if (!has_sysmmu_capable_pbuf(drvdata->sfrbase))
		return;

	if ((num_reg == 0) || (pb_reg == NULL)) {
		/* Disabling prefetch buffers */
		__sysmmu_disable_pbuf(drvdata->sfrbase);
		return;
	}

	for (i = 0; i < num_reg; i++) {
		if (((pb_reg[i].config & SYSMMU_PBUFCFG_WRITE) &&
					(drvdata->prop & SYSMMU_PROP_WRITE)) ||
			(!(pb_reg[i].config & SYSMMU_PBUFCFG_WRITE) &&
				 (drvdata->prop & SYSMMU_PROP_READ)))
			prefbuf[num_bufs++] = pb_reg[i];
	}

	__sysmmu_set_pbuf(drvdata, prefbuf, num_bufs);
}

void __exynos_sysmmu_set_prefbuf_by_plane(struct sysmmu_drvdata *drvdata,
			unsigned int inplanes, unsigned int onplanes,
			unsigned int ipoption, unsigned int opoption)
{
	unsigned int num_pb;
	int num_bufs;
	struct sysmmu_prefbuf prefbuf[6];

	if (!has_sysmmu_capable_pbuf(drvdata->sfrbase))
		return;

	num_pb = PB_INFO_NUM(__raw_readl(drvdata->sfrbase + REG_PB_INFO));
	if ((num_pb != 3) && (num_pb != 4) && (num_pb != 6)) {
		dev_err(drvdata->master,
				"%s: Read invalid PB information from %s\n",
				__func__, dev_name(drvdata->sysmmu));
		return;
	}

	num_bufs = __prepare_prefetch_buffers_by_plane(drvdata,
			prefbuf, num_pb, inplanes, onplanes,
			ipoption, opoption);

	if (num_bufs == 0)
		__sysmmu_disable_pbuf(drvdata->sfrbase);
	else
		__sysmmu_set_pbuf(drvdata, prefbuf, num_bufs);
}

static void __sysmmu_set_df(void __iomem *sfrbase,
				dma_addr_t iova)
{
	__raw_writel(iova, sfrbase + REG_SW_DF_VPN);
}

void __exynos_sysmmu_set_df(struct sysmmu_drvdata *drvdata, dma_addr_t iova)
{
#ifdef CONFIG_EXYNOS7_IOMMU_CHECK_DF
	int i, num_l1tlb, df_cnt = 0;
#endif
	u32 cfg;

	if (MAKE_MMU_VER(5, 1) > __raw_sysmmu_version(drvdata->sfrbase)) {
		dev_err(drvdata->sysmmu, "%s: SW direct fetch not supported\n",
			__func__);
		return;
	}

#ifdef CONFIG_EXYNOS7_IOMMU_CHECK_DF
	num_l1tlb = MMU_NUM_L1TLB_ENTRIES(__raw_readl(drvdata->sfrbase +
				REG_MMU_CAPA));
	for (i = 0; i < num_l1tlb; i++) {
		__raw_writel(i, drvdata->sfrbase + REG_L1TLB_READ_ENTRY);
		cfg = __raw_readl(drvdata->sfrbase + REG_L1TLB_ENTRY_ATTR);
		if (cfg & L1TLB_ATTR_IM)
			df_cnt++;
	}

	if (df_cnt == num_l1tlb) {
		dev_err(drvdata->sysmmu, "%s: All TLBs are special slots", __func__);
		return;
	}

	cfg = __raw_readl(drvdata->sfrbase + REG_SW_DF_VPN_CMD_NUM);

	if ((cfg & 0xFF) > 9)
		dev_info(drvdata->sysmmu,
			"%s: DF command queue is full\n", __func__);
	else
#endif
		__sysmmu_set_df(drvdata->sfrbase, iova);
}

void __exynos_sysmmu_release_df(struct sysmmu_drvdata *drvdata)
{
	if (__raw_sysmmu_version(drvdata->sfrbase) >= MAKE_MMU_VER(5, 1))
		__raw_writel(0x1, drvdata->sfrbase + REG_L1TLB_CTRL);
	else
		dev_err(drvdata->sysmmu, "DF is not supported");
}

void dump_sysmmu_tlb_pb(void __iomem *sfrbase)
{
	unsigned int i, capa, lmm;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pr_crit("---------- System MMU Status -----------------------------\n");

	pgd = pgd_offset_k((unsigned long)sfrbase);
	if (!pgd) {
		pr_crit("Invalid virtual address %p\n", sfrbase);
		return;
	}

	pud = pud_offset(pgd, (unsigned long)sfrbase);
	if (!pud) {
		pr_crit("Invalid virtual address %p\n", sfrbase);
		return;
	}

	pmd = pmd_offset(pud, (unsigned long)sfrbase);
	if (!pmd) {
		pr_crit("Invalid virtual address %p\n", sfrbase);
		return;
	}

	pte = pte_offset_kernel(pmd, (unsigned long)sfrbase);
	if (!pte) {
		pr_crit("Invalid virtual address %p\n", sfrbase);
		return;
	}

	capa = __raw_readl(sfrbase + REG_MMU_CAPA);
	lmm = MMU_RAW_VER(__raw_readl(sfrbase + REG_MMU_VERSION));

	pr_crit("ADDR: %#010lx(VA: 0x%p), MMU_CTRL: %#010x, PT_BASE: %#010x\n",
		pte_pfn(*pte) << PAGE_SHIFT, sfrbase,
		__raw_readl(sfrbase + REG_MMU_CTRL),
		__raw_readl(sfrbase + REG_PT_BASE_PPN));
	pr_crit("VERSION %d.%d, MMU_CFG: %#010x, MMU_STATUS: %#010x\n",
		MMU_MAJ_VER(lmm), MMU_MIN_VER(lmm),
		__raw_readl(sfrbase + REG_MMU_CFG),
		__raw_readl(sfrbase + REG_MMU_STATUS));

	if (lmm == MAKE_MMU_VER(5, 1))
		pr_crit("TLB hit notify : %s\n",
			(__raw_readl(sfrbase + REG_L1TLB_CFG) == 2) ?
				"on" : "off");

	if (MMU_HAVE_L2TLB(capa))
		pr_crit("Level 2 TLB: %s\n",
			(__raw_readl(sfrbase + REG_L2TLB_CFG) == 1) ?
				"on" : "off");

	pr_crit("---------- Level 1 TLB -----------------------------------\n");

	for (i = 0; i < MMU_NUM_L1TLB_ENTRIES(capa); i++) {
		__raw_writel(i, sfrbase + REG_L1TLB_READ_ENTRY);
		pr_crit("[%02d] VPN: %#010x, PPN: %#010x, ATTR: %#010x\n",
			i, __raw_readl(sfrbase + REG_L1TLB_ENTRY_VPN),
			__raw_readl(sfrbase + REG_L1TLB_ENTRY_PPN),
			__raw_readl(sfrbase + REG_L1TLB_ENTRY_ATTR));
	}

	if (!MMU_HAVE_PB(capa))
		return;

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
	}

	/* Reading L2TLB is not provided by H/W */
}

static void show_fault_information(struct sysmmu_drvdata *drvdata,
				   int flags, unsigned long fault_addr)
{
	unsigned int info;
	phys_addr_t pgtable;
	int fault_id = SYSMMU_FAULT_ID(flags);

	pgtable = __raw_readl(drvdata->sfrbase + REG_PT_BASE_PPN);
	pgtable <<= PAGE_SHIFT;

	pr_crit("----------------------------------------------------------\n");
	pr_crit("%s %s %s at %#010lx by %s (page table @ %#010x)\n",
		dev_name(drvdata->sysmmu),
		(flags & IOMMU_FAULT_WRITE) ? "WRITE" : "READ",
		sysmmu_fault_name[fault_id], fault_addr,
		dev_name(drvdata->master), pgtable);

	if (fault_id == SYSMMU_FAULT_UNKNOWN) {
		pr_crit("The fault is not caused by this System MMU.\n");
		pr_crit("Please check IRQ and SFR base address.\n");
		goto finish;
	}

	info = __raw_readl(drvdata->sfrbase +
			((flags & IOMMU_FAULT_WRITE) ?
			REG_FAULT_AW_TRANS_INFO : REG_FAULT_AR_TRANS_INFO));
	pr_crit("AxID: %#x, AxLEN: %#x\n", info & 0xFFFF, (info >> 16) & 0xF);

	if (pgtable != drvdata->pgtable)
		pr_crit("Page table base of driver: %#010x\n",
			drvdata->pgtable);

	if (fault_id == SYSMMU_FAULT_PTW_ACCESS) {
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

	dump_sysmmu_tlb_pb(drvdata->sfrbase);

finish:
	pr_crit("----------------------------------------------------------\n");
}

#define REG_INT_STATUS_WRITE_BIT 16

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
	if (itype >= REG_INT_STATUS_WRITE_BIT) {
		itype -= REG_INT_STATUS_WRITE_BIT;
		flags = IOMMU_FAULT_WRITE;
	}

	if (WARN_ON(!(itype < SYSMMU_FAULT_UNKNOWN)))
		itype = SYSMMU_FAULT_UNKNOWN;
	else
		addr = __raw_readl(drvdata->sfrbase +
				((flags & IOMMU_FAULT_WRITE) ?
				 REG_FAULT_AW_ADDR : REG_FAULT_AR_ADDR));
	flags |= SYSMMU_FAULT_FLAG(itype);

	show_fault_information(drvdata, flags, addr);

	if (drvdata->domain) /* master is set if drvdata->domain exists */
		ret = report_iommu_fault(drvdata->domain,
					drvdata->master, addr, flags);

#if 0 /* Recovering System MMU fault is available from System MMU v6 */
	if ((ret == 0) &&
		((itype == SYSMMU_FAULT_PAGE_FAULT) ||
		 (itype == SYSMMU_FAULT_ACCESS))) {
		if (flags & IOMMU_FAULT_WRITE)
			itype += REG_INT_STATUS_WRITE_BIT;
		__raw_writel(1 << itype, drvdata->sfrbase + REG_INT_CLEAR);

		sysmmu_unblock(drvdata->sfrbase);
	} else
#endif

	panic("Unrecoverable System MMU Fault!!");

	return IRQ_HANDLED;
}

void __sysmmu_init_config(struct sysmmu_drvdata *drvdata)
{
	unsigned long cfg;

	__raw_writel(0, drvdata->sfrbase + REG_MMU_CTRL);

	cfg = CFG_FLPDCACHE | CFG_ACGEN;
	if (!(drvdata->qos < 0))
		cfg |= CFG_QOS_OVRRIDE | CFG_QOS(drvdata->qos);

	if (has_sysmmu_capable_pbuf(drvdata->sfrbase))
		__exynos_sysmmu_set_prefbuf_by_plane(drvdata, 0, 0,
					SYSMMU_PBUFCFG_DEFAULT_INPUT,
					SYSMMU_PBUFCFG_DEFAULT_OUTPUT);

	cfg |= __raw_readl(drvdata->sfrbase + REG_MMU_CFG) & ~CFG_MASK;
	__raw_writel(cfg, drvdata->sfrbase + REG_MMU_CFG);
}

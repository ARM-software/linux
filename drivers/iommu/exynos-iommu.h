/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Data structure definition for Exynos IOMMU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/iommu.h>
#include <linux/irq.h>
#include <linux/clk.h>

#include <linux/exynos_iovmm.h>

#define TRACE_LOG trace_printk
#define TRACE_LOG_DEV(dev, fmt, args...)  \
		TRACE_LOG("%s: " fmt, dev_name(dev), ##args)

#define MODULE_NAME "exynos-sysmmu"

#define IOVA_START 0x10000000
#define IOVM_SIZE (SZ_2G + SZ_1G + SZ_256M) /* last 4K is for error values */

#define IOVM_NUM_PAGES(vmsize) (vmsize / PAGE_SIZE)
#define IOVM_BITMAP_SIZE(vmsize) \
		((IOVM_NUM_PAGES(vmsize) + BITS_PER_BYTE) / BITS_PER_BYTE)

#define SPSECT_ORDER	24
#define DSECT_ORDER	21
#define SECT_ORDER	20
#define LPAGE_ORDER	16
#define SPAGE_ORDER	12

#define SPSECT_SIZE	(1 << SPSECT_ORDER)
#define DSECT_SIZE	(1 << DSECT_ORDER)
#define SECT_SIZE	(1 << SECT_ORDER)
#define LPAGE_SIZE	(1 << LPAGE_ORDER)
#define SPAGE_SIZE	(1 << SPAGE_ORDER)

#define SPSECT_MASK	~(SPSECT_SIZE - 1)
#define DSECT_MASK	~(DSECT_SIZE - 1)
#define SECT_MASK	~(SECT_SIZE - 1)
#define LPAGE_MASK	~(LPAGE_SIZE - 1)
#define SPAGE_MASK	~(SPAGE_SIZE - 1)

#define SPSECT_ENT_MASK	~((SPSECT_SIZE >> PG_ENT_SHIFT) - 1)
#define DSECT_ENT_MASK	~((DSECT_SIZE >> PG_ENT_SHIFT) - 1)
#define SECT_ENT_MASK	~((SECT_SIZE >> PG_ENT_SHIFT) - 1)
#define LPAGE_ENT_MASK	~((LPAGE_SIZE >> PG_ENT_SHIFT) - 1)
#define SPAGE_ENT_MASK	~((SPAGE_SIZE >> PG_ENT_SHIFT) - 1)

#define SECT_PER_SPSECT		(SPSECT_SIZE / SECT_SIZE)
#define SECT_PER_DSECT		(DSECT_SIZE / SECT_SIZE)
#define SPAGES_PER_LPAGE	(LPAGE_SIZE / SPAGE_SIZE)

#define PGBASE_TO_PHYS(pgent)	(phys_addr_t)((pgent) << PG_ENT_SHIFT)

#define MAX_NUM_PBUF	6
#define MAX_NUM_PLANE	6

#define NUM_LV1ENTRIES 4096
#define NUM_LV2ENTRIES (SECT_SIZE / SPAGE_SIZE)

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) ((iova & ~SECT_MASK) >> SPAGE_ORDER)

typedef u32 sysmmu_pte_t;
typedef u32 exynos_iova_t;

#define LV2TABLE_SIZE (NUM_LV2ENTRIES * sizeof(sysmmu_pte_t))

#define spsection_phys(sent) PGBASE_TO_PHYS(*(sent) & SPSECT_ENT_MASK)
#define spsection_offs(iova) ((iova) & (SPSECT_SIZE - 1))
#define section_phys(sent) PGBASE_TO_PHYS(*(sent) & SECT_ENT_MASK)
#define section_offs(iova) ((iova) & (SECT_SIZE - 1))
#define lpage_phys(pent) PGBASE_TO_PHYS(*(pent) & LPAGE_ENT_MASK)
#define lpage_offs(iova) ((iova) & (LPAGE_SIZE - 1))
#define spage_phys(pent) PGBASE_TO_PHYS(*(pent) & SPAGE_ENT_MASK)
#define spage_offs(iova) ((iova) & (SPAGE_SIZE - 1))

#define lv2table_base(sent) ((*((phys_addr_t *)(sent)) & ~0x3F) << PG_ENT_SHIFT)

#define SYSMMU_BLOCK_POLLING_COUNT 120

#define REG_MMU_CTRL		0x000
#define REG_MMU_CFG		0x004
#define REG_MMU_STATUS		0x008
#define REG_MMU_VERSION		0x034

#define CTRL_ENABLE	0x5
#define CTRL_BLOCK	0x7
#define CTRL_DISABLE	0x0

#define CFG_ACGEN	(1 << 24) /* System MMU 3.3+ */
#define CFG_FLPDCACHE	(1 << 20) /* System MMU 3.2+ */
#define CFG_SHAREABLE	(1 << 12) /* System MMU 3.0+ */
#define CFG_QOS_OVRRIDE (1 << 11) /* System MMU 3.3+ */
#define CFG_QOS(n)	(((n) & 0xF) << 7)

/*
 * Metadata attached to the owner of a group of System MMUs that belong
 * to the same owner device.
 */
struct exynos_iommu_owner {
	struct list_head entry; /* entry of exynos_iommu_owner list */
	struct list_head client; /* entry of exynos_iommu_domain.clients */
	struct device *dev;
	void *vmm_data;         /* IO virtual memory manager's data */
	spinlock_t lock;        /* Lock to preserve consistency of System MMU */
	struct list_head mmu_list; /* head of sysmmu_list_data.node */
};

struct exynos_vm_region {
	struct list_head node;
	dma_addr_t start;
	size_t size;
};

struct exynos_iovmm {
	struct iommu_domain *domain; /* iommu domain for this iovmm */
	size_t iovm_size[MAX_NUM_PLANE]; /* iovm bitmap size per plane */
	dma_addr_t iova_start[MAX_NUM_PLANE]; /* iovm start address per plane */
	unsigned long *vm_map[MAX_NUM_PLANE]; /* iovm biatmap per plane */
	struct list_head regions_list;	/* list of exynos_vm_region */
	spinlock_t vmlist_lock; /* lock for updating regions_list */
	spinlock_t bitmap_lock; /* lock for manipulating bitmaps */
	struct device *dev; /* peripheral device that has this iovmm */
	size_t allocated_size[MAX_NUM_PLANE];
	int num_areas[MAX_NUM_PLANE];
	int inplanes;
	int onplanes;
	unsigned int num_map;
	unsigned int num_unmap;
};

void exynos_sysmmu_tlb_invalidate(struct device *dev, dma_addr_t start,
				  size_t size);

#define SYSMMU_FAULT_WRITE	(1 << SYSMMU_FAULTS_NUM)

enum sysmmu_property {
	SYSMMU_PROP_RESERVED,
	SYSMMU_PROP_READ,
	SYSMMU_PROP_WRITE,
	SYSMMU_PROP_READWRITE = SYSMMU_PROP_READ | SYSMMU_PROP_WRITE,
	SYSMMU_PROP_RW_MASK = SYSMMU_PROP_READWRITE,
	SYSMMU_PROP_WINDOW_SHIFT = 16,
	SYSMMU_PROP_WINDOW_MASK = 0x1F << SYSMMU_PROP_WINDOW_SHIFT,
};

/*
 * Metadata attached to each System MMU devices.
 */
struct sysmmu_drvdata {
	struct list_head entry;	/* entry of sysmmu debug drvdata list */
	struct list_head node;	/* entry of exynos_iommu_owner.mmu_list */
	struct device *sysmmu;	/* System MMU's device descriptor */
	struct device *master;	/* Client device that needs System MMU */
	void __iomem *sfrbase;
	struct clk *clk;
	struct clk *clk_master;
	int activations;
	struct iommu_domain *domain; /* domain given to iommu_attach_device() */
	phys_addr_t pgtable;
	spinlock_t lock;
	struct sysmmu_prefbuf pbufs[MAX_NUM_PBUF];
	short qos;
	short num_pbufs;
	bool runtime_active;
	bool suspended;
	enum sysmmu_property prop; /* mach/sysmmu.h */
};

struct exynos_iommu_domain {
	struct list_head clients; /* list of sysmmu_drvdata.node */
	sysmmu_pte_t *pgtable; /* lv1 page table, 16KB */
	short *lv2entcnt; /* free lv2 entry counter for each section */
	spinlock_t lock; /* lock for this structure */
	spinlock_t pgtablelock; /* lock for modifying page table @ pgtable */
};

#if defined(CONFIG_EXYNOS7_IOMMU) && defined(CONFIG_EXYNOS5_IOMMU)
#error "CONFIG_IOMMU_EXYNOS5 and CONFIG_IOMMU_EXYNOS7 defined together"
#endif

#if defined(CONFIG_EXYNOS5_IOMMU) /* System MMU v1/2/3 */

#define SYSMMU_OF_COMPAT_STRING "samsung,exynos4210-sysmmu"
#define DEFAULT_QOS_VALUE	8
#define PG_ENT_SHIFT 0 /* 32bit PA, 32bit VA */
#define lv1ent_fault(sent) ((*(sent) == ZERO_LV2LINK) || \
			   ((*(sent) & 3) == 0) || ((*(sent) & 3) == 3))
#define lv1ent_page(sent) ((*(sent) != ZERO_LV2LINK) && \
			  ((*(sent) & 3) == 1))
#define lv1ent_section(sent) ((*(sent) & 3) == 2)
#define lv1ent_dsection(sent) 0 /* Large section is not defined */
#define lv1ent_spsection(sent) (((*(sent) & 3) == 2) && \
			       (((*(sent) >> 18) & 1) == 1))
#define lv2ent_fault(pent) ((*(pent) & 3) == 0 || \
			   ((*(pent) & SPAGE_ENT_MASK) == fault_page))
#define lv2ent_small(pent) ((*(pent) & 2) == 2)
#define lv2ent_large(pent) ((*(pent) & 3) == 1)
#define dsection_phys(sent) ({ BUG(); 0; }) /* large section is not defined */
#define dsection_offs(iova) ({ BUG(); 0; })
#define mk_lv1ent_spsect(pa) ((sysmmu_pte_t) ((pa) | 0x40002))
#define mk_lv1ent_dsect(pa) ({ BUG(); 0; })
#define mk_lv1ent_sect(pa) ((sysmmu_pte_t) ((pa) | 2))
#define mk_lv1ent_page(pa) ((sysmmu_pte_t) ((pa) | 1))
#define mk_lv2ent_lpage(pa) ((sysmmu_pte_t) ((pa) | 1))
#define mk_lv2ent_spage(pa) ((sysmmu_pte_t) ((pa) | 2))

#define PGSIZE_BITMAP (SECT_SIZE | LPAGE_SIZE | SPAGE_SIZE)

#define __exynos_sysmmu_set_df(drvdata, iova) do { } while (0)
#define __exynos_sysmmu_release_df(drvdata) do { } while (0)

#define __sysmmu_show_status(drvdata) do { } while (0) /* TODO */

#elif defined(CONFIG_EXYNOS7_IOMMU) /* System MMU v5 ~ */

#define SYSMMU_OF_COMPAT_STRING "samsung,exynos5430-sysmmu"
#define DEFAULT_QOS_VALUE	-1 /* Inherited from master */
#define PG_ENT_SHIFT 4 /* 36bit PA, 32bit VA */
#define lv1ent_fault(sent) ((*(sent) == ZERO_LV2LINK) || \
			   ((*(sent) & 7) == 0))
#define lv1ent_page(sent) ((*(sent) != ZERO_LV2LINK) && \
			  ((*(sent) & 7) == 1))
#define lv1ent_section(sent) ((*(sent) & 7) == 2)
#define lv1ent_dsection(sent) ((*(sent) & 7) == 4)
#define lv1ent_spsection(sent) ((*(sent) & 7) == 6)
#define lv2ent_fault(pent) ((*(pent) & 3) == 0 || \
			   (PGBASE_TO_PHYS(*(pent) & SPAGE_ENT_MASK) == fault_page))
#define lv2ent_small(pent) ((*(pent) & 2) == 2)
#define lv2ent_large(pent) ((*(pent) & 3) == 1)
#define dsection_phys(sent) PGBASE_TO_PHYS(*(sent) & DSECT_ENT_MASK)
#define dsection_offs(iova) ((iova) & (DSECT_SIZE - 1))
#define mk_lv1ent_spsect(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 6)
#define mk_lv1ent_dsect(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 4)
#define mk_lv1ent_sect(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 2)
#define mk_lv1ent_page(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 1)
#define mk_lv2ent_lpage(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 1)
#define mk_lv2ent_spage(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 2)

#define PGSIZE_BITMAP (DSECT_SIZE | SECT_SIZE | LPAGE_SIZE | SPAGE_SIZE)

void __sysmmu_show_status(struct sysmmu_drvdata *drvdata);
void __exynos_sysmmu_set_df(struct sysmmu_drvdata *drvdata, dma_addr_t iova);
void __exynos_sysmmu_release_df(struct sysmmu_drvdata *drvdata);

#else
#error "Neither CONFIG_IOMMU_EXYNOS5 nor CONFIG_IOMMU_EXYNOS7 is defined"
#endif

#define __sysmmu_clk_enable(drvdata)	if (drvdata->clk) \
						clk_enable(drvdata->clk)
#define __sysmmu_clk_disable(drvdata)	if (drvdata->clk) \
						clk_disable(drvdata->clk)
#define __master_clk_enable(drvdata)	if (drvdata->clk_master) \
						clk_enable(drvdata->clk_master)
#define __master_clk_disable(drvdata)	if (drvdata->clk_master) \
						clk_disable(drvdata->clk_master)

#if defined(CONFIG_EXYNOS7_IOMMU) && defined(CONFIG_EXYNOS5_IOMMU)
#error "CONFIG_IOMMU_EXYNOS5 and CONFIG_IOMMU_EXYNOS7 defined together"
#endif

static inline bool set_sysmmu_active(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU was not active previously
	   and it needs to be initialized */
	return ++data->activations == 1;
}

static inline bool set_sysmmu_inactive(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU is needed to be disabled */
	BUG_ON(data->activations < 1);
	return --data->activations == 0;
}

static inline bool is_sysmmu_active(struct sysmmu_drvdata *data)
{
	return data->activations > 0;
}

static inline bool is_sysmmu_really_enabled(struct sysmmu_drvdata *data)
{
	return is_sysmmu_active(data) && data->runtime_active;
}

#define MMU_MAJ_VER(val)	((val) >> 7)
#define MMU_MIN_VER(val)	((val) & 0x7F)
#define MMU_RAW_VER(reg)	(((reg) >> 21) & ((1 << 11) - 1)) /* 11 bits */

#define MAKE_MMU_VER(maj, min)	((((maj) & 0xF) << 7) | ((min) & 0x7F))

static inline unsigned int __raw_sysmmu_version(void __iomem *sfrbase)
{
	return MMU_RAW_VER(__raw_readl(sfrbase + REG_MMU_VERSION));
}

static inline void __raw_sysmmu_disable(void __iomem *sfrbase)
{
	__raw_writel(0, sfrbase + REG_MMU_CFG);
	__raw_writel(CTRL_DISABLE, sfrbase + REG_MMU_CTRL);
}

static inline void __raw_sysmmu_enable(void __iomem *sfrbase)
{
	__raw_writel(CTRL_ENABLE, sfrbase + REG_MMU_CTRL);
}

#define sysmmu_unblock __raw_sysmmu_enable

void dump_sysmmu_tlb_pb(void __iomem *sfrbase);

static inline bool sysmmu_block(void __iomem *sfrbase)
{
	int i = SYSMMU_BLOCK_POLLING_COUNT;

	__raw_writel(CTRL_BLOCK, sfrbase + REG_MMU_CTRL);
	while ((i > 0) && !(__raw_readl(sfrbase + REG_MMU_STATUS) & 1))
		--i;

	if (!(__raw_readl(sfrbase + REG_MMU_STATUS) & 1)) {
		dump_sysmmu_tlb_pb(sfrbase);
		panic("Failed to block System MMU!");
		sysmmu_unblock(sfrbase);
		return false;
	}

	return true;
}

void __sysmmu_init_config(struct sysmmu_drvdata *drvdata);
void __sysmmu_set_ptbase(void __iomem *sfrbase, phys_addr_t pfn_pgtable);

extern unsigned long *zero_lv2_table;
#define ZERO_LV2LINK mk_lv1ent_page(__pa(zero_lv2_table))

static inline sysmmu_pte_t *page_entry(sysmmu_pte_t *sent, unsigned long iova)
{
	return (sysmmu_pte_t *)(__va(lv2table_base(sent))) +
				lv2ent_offset(iova);
}

static inline sysmmu_pte_t *section_entry(
				sysmmu_pte_t *pgtable, unsigned long iova)
{
	return (sysmmu_pte_t *)(pgtable + lv1ent_offset(iova));
}

irqreturn_t exynos_sysmmu_irq(int irq, void *dev_id);
void __sysmmu_tlb_invalidate_flpdcache(void __iomem *sfrbase, dma_addr_t iova);
void __exynos_sysmmu_set_prefbuf_by_plane(struct sysmmu_drvdata *drvdata,
			unsigned int inplanes, unsigned int onplanes,
			unsigned int ipoption, unsigned int opoption);
void __exynos_sysmmu_set_prefbuf_by_region(struct sysmmu_drvdata *drvdata,
			struct sysmmu_prefbuf pb_reg[],
			unsigned int num_reg);
int __prepare_prefetch_buffers_by_plane(struct sysmmu_drvdata *drvdata,
				struct sysmmu_prefbuf prefbuf[], int num_pb,
				int inplanes, int onplanes,
				int ipoption, int opoption);
void __sysmmu_tlb_invalidate_entry(void __iomem *sfrbase, dma_addr_t iova);
void __sysmmu_tlb_invalidate(void __iomem *sfrbase,
				dma_addr_t iova, size_t size);

void dump_sysmmu_tlb_pb(void __iomem *sfrbase);

#ifdef CONFIG_EXYNOS_IOVMM
static inline struct exynos_iovmm *exynos_get_iovmm(struct device *dev)
{
	return ((struct exynos_iommu_owner *)dev->archdata.iommu)->vmm_data;
}

struct exynos_vm_region *find_iovm_region(struct exynos_iovmm *vmm,
						dma_addr_t iova);

static inline int find_iovmm_plane(struct exynos_iovmm *vmm, dma_addr_t iova)
{
	int i;

	for (i = 0; i < (vmm->inplanes + vmm->onplanes); i++)
		if ((iova >= vmm->iova_start[i]) &&
			(iova < (vmm->iova_start[i] + vmm->iovm_size[i])))
			return i;
	return -1;
}

#else
static inline struct exynos_iovmm *exynos_get_iovmm(struct device *dev)
{
	return NULL;
}

struct exynos_vm_region *find_iovm_region(struct exynos_iovmm *vmm,
						dma_addr_t iova)
{
	return NULL;
}

static inline int find_iovmm_plane(struct exynos_iovmm *vmm, dma_addr_t iova)
{
	return -1;
}
#endif

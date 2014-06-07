/* linux/arch/arm/plat-s5p/include/plat/iovmm.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_PLAT_IOVMM_H
#define __ASM_PLAT_IOVMM_H

#ifdef CONFIG_EXYNOS_IOVMM
#include <linux/dma-direction.h>
#include <linux/iommu.h>

struct scatterlist;
struct device;

int iovmm_activate(struct device *dev);
void iovmm_deactivate(struct device *dev);

/* iovmm_map() - Maps a list of physical memory chunks
 * @dev: the owner of the IO address space where the mapping is created
 * @sg: list of physical memory chunks to map
 * @offset: length in bytes where the mapping starts
 * @size: how much memory to map in bytes. @offset + @size must not exceed
 *        total size of @sg
 * @direction: dma data direction for iova
 * @id: From where iovmm allocates iova
 *
 * This function returns mapped IO address in the address space of @dev.
 * Returns minus error number if mapping fails.
 * Caller must check its return code with IS_ERROR_VALUE() if the function
 * succeeded.
 *
 * The caller of this function must ensure that iovmm_cleanup() is not called
 * while this function is called.
 *
 */
dma_addr_t iovmm_map(struct device *dev, struct scatterlist *sg, off_t offset,
		size_t size, enum dma_data_direction direction, int id);

/* iovmm_unmap() - unmaps the given IO address
 * @dev: the owner of the IO address space where @iova belongs
 * @iova: IO address that needs to be unmapped and freed.
 *
 * The caller of this function must ensure that iovmm_cleanup() is not called
 * while this function is called.
 */
void iovmm_unmap(struct device *dev, dma_addr_t iova);

/* iovmm_map_oto - create one to one mapping for the given physical address
 * @dev: the owner of the IO address space to map
 * @phys: physical address to map
 * @size: size of the mapping to create
 *
 * This function return 0 if mapping is successful. Otherwise, minus error
 * value.
 */
int iovmm_map_oto(struct device *dev, phys_addr_t phys, size_t size);

/* iovmm_unmap_oto - remove one to one mapping
 * @dev: the owner ofthe IO address space
 * @phys: physical address to remove mapping
 */
void iovmm_unmap_oto(struct device *dev, phys_addr_t phys);

int exynos_create_iovmm(struct device *dev, int inplanes, int onplanes);

#define SYSMMU_FAULT_BITS	4
#define SYSMMU_FAULT_SHIFT	16
#define SYSMMU_FAULT_MASK	((1 << SYSMMU_FAULT_BITS) - 1)
#define SYSMMU_FAULT_FLAG(id) (((id) & SYSMMU_FAULT_MASK) << SYSMMU_FAULT_SHIFT)
#define SYSMMU_FAULT_ID(fg)   (((fg) >> SYSMMU_FAULT_SHIFT) & SYSMMU_FAULT_MASK)

#define SYSMMU_FAULT_PTW_ACCESS   0
#define SYSMMU_FAULT_PAGE_FAULT   1
#define SYSMMU_FAULT_TLB_MULTIHIT 2
#define SYSMMU_FAULT_ACCESS       3
#define SYSMMU_FAULT_SECURITY     4
#define SYSMMU_FAULT_UNKNOWN      5

#define IOMMU_FAULT_EXYNOS_PTW_ACCESS SYSMMU_FAULT_FLAG(SYSMMU_FAULT_PTW_ACCESS)
#define IOMMU_FAULT_EXYNOS_PAGE_FAULT SYSMMU_FAULT_FLAG(SYSMMU_FAULT_PAGE_FAULT)
#define IOMMU_FAULT_EXYNOS_TLB_MULTIHIT \
				SYSMMU_FAULT_FLAG(SYSMMU_FAULT_TLB_MULTIHIT)
#define IOMMU_FAULT_EXYNOS_ACCESS     SYSMMU_FAULT_FLAG(SYSMMU_FAULT_ACCESS)
#define IOMMU_FAULT_EXYNOS_SECURITY   SYSMMU_FAULT_FLAG(SYSMMU_FAULT_SECURITY)
#define IOMMU_FAULT_EXYNOS_UNKNOWN    SYSMMU_FAULT_FLAG(SYSMMU_FAULT_UNKOWN)

/*
 * iovmm_set_fault_handler - register fault handler of dev to iommu controller
 * @dev: the device that wants to register fault handler
 * @handler: fault handler
 * @token: any data the device driver needs to get when fault occurred
 */
void iovmm_set_fault_handler(struct device *dev,
			     iommu_fault_handler_t handler, void *token);

/*
 * flags to option_iplanes and option_oplanes.
 * inplanes and onplanes is 'input planes' and 'output planes', respectively.
 *
 * default value to option_iplanes:
 *    (TLB_UPDATE | ASCENDING | PREFETCH)
 * default value to option_oplanes:
 *    (TLB_UPDATE | ASCENDING | PREFETCH | WRITE)
 *
 * SYSMMU_PBUFCFG_READ and SYSMMU_PBUFCFG_WRITE are ignored because they are
 * implicitly set from 'inplanes' and 'onplanes' arguments to
 * iovmm_set_prefetch_buffer().
 *
 * Guide to setting flags:
 * - Clear SYSMMU_BUFCFG_TLB_UPDATE if a buffer is accessed by the device
 *   for rotation.
 * - Set SYSMMU_PBUFCFG_DESCENDING if the device access a buffer in reversed
 *   order
 * - Clear SYSMMU_PBUFCFG_PREFETCH if access to a buffer has poor locality.
 * - Otherwise, always set flags as default value.
 */
#define SYSMMU_PBUFCFG_TLB_UPDATE      (1 << 16)
#define SYSMMU_PBUFCFG_ASCENDING       (1 << 12)
#define SYSMMU_PBUFCFG_DESCENDING      (0 << 12)
#define SYSMMU_PBUFCFG_PREFETCH                (1 << 8)
#define SYSMMU_PBUFCFG_WRITE           (1 << 4)
#define SYSMMU_PBUFCFG_READ            (0 << 4)

#define SYSMMU_PBUFCFG_DEFAULT_INPUT   (SYSMMU_PBUFCFG_TLB_UPDATE | \
					SYSMMU_PBUFCFG_ASCENDING |  \
					SYSMMU_PBUFCFG_PREFETCH |   \
					SYSMMU_PBUFCFG_READ)
#define SYSMMU_PBUFCFG_DEFAULT_OUTPUT  (SYSMMU_PBUFCFG_TLB_UPDATE | \
					SYSMMU_PBUFCFG_ASCENDING |  \
					SYSMMU_PBUFCFG_PREFETCH |   \
					SYSMMU_PBUFCFG_WRITE)

/*
 * sysmmu_set_prefetch_buffer_by_plane() -
 *                               set prefetch buffer configuration by plane
 *
 * @dev: device descriptor of master device
 * @inplanes: number of input planes that uses prefetch buffers.
 * @onplanes: number of output planes that uses prefetch buffers.
 * @option_iplanes: prefetch buffer configuration to input planes.
 * @option_oplanes: prefetch buffer configuration to output planes.
 *
 * Returns 0 if setting is successful. -EINVAL if the argument is invalid.
 *
 * @inplanes and @onplanes must not exceed the values to exynos_create_iovmm().
 * The setting is reset if System MMU is reset.
 * The situation that System MMU is reset are:
 * - iovmm_deactivate()
 * - local power down due to suspend to ram, pm_rutime_put() or its equivalent.
 */
int sysmmu_set_prefetch_buffer_by_plane(struct device *dev,
			unsigned int inplanes, unsigned int onplanes,
			unsigned int ipoption, unsigned int opoption);
#else
#define iovmm_activate(dev)		(-ENOSYS)
#define iovmm_deactivate(dev)		do { } while (0)
#define iovmm_map(dev, sg, offset, size) (-ENOSYS)
#define iovmm_unmap(dev, iova)		do { } while (0)
#define iovmm_map_oto(dev, phys, size)	(-ENOSYS)
#define iovmm_unmap_oto(dev, phys)	do { } while (0)
#define exynos_create_iovmm(sysmmu, inplanes, onplanes) 0
#define iovmm_set_fault_handler(dev, handler, token) do { } while (0)

int sysmmu_set_prefetch_buffer_by_plane(struct device *dev,
			unsigned int inplanes, unsigned int onplanes,
			unsigned int ipoption, unsigned int opoption)
{
	return -ENOSYS;
}
#endif /* CONFIG_EXYNOS_IOVMM */

#ifdef CONFIG_EXYNOS_IOMMU
/**
 * exynos_sysmmu_enable() - enable system mmu
 * @dev: The device whose System MMU is about to be enabled.
 * @pgd: Base physical address of the 1st level page table
 *
 * This function enable system mmu to transfer address
 * from virtual address to physical address.
 * Return non-zero if it fails to enable System MMU.
 */
int exynos_sysmmu_enable(struct device *dev, unsigned long pgd);

/**
 * exynos_sysmmu_disable() - disable sysmmu mmu of ip
 * @dev: The device whose System MMU is about to be disabled.
 *
 * This function disable system mmu to transfer address
 * from virtual address to physical address
 */
bool exynos_sysmmu_disable(struct device *dev);

/**
 * exynos_sysmmu_map_user_pages() - maps all pages by fetching from
 * user page table entries.
 * @dev: The device whose System MMU is about to be disabled.
 * @mm: mm struct of user requested to map
 * @vaddr: start vaddr in valid vma
 * @size: size to map
 * @write: set if buffer may be written
 *
 * This function maps all user pages into sysmmu page table.
 */
int exynos_sysmmu_map_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr, size_t size,
					int write);

/**
 * exynos_sysmmu_unmap_user_pages() - unmaps all mapped pages
 * @dev: The device whose System MMU is about to be disabled.
 * @mm: mm struct of user requested to map
 * @vaddr: start vaddr in valid vma
 * @size: size to map
 *
 * This function unmaps all user pages mapped in sysmmu page table.
 */
int exynos_sysmmu_unmap_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr, size_t size);

/*
 * The handle_pte_fault() is called by exynos_sysmmu_map_user_pages().
 * Driver cannot include include/linux/huge_mm.h because
 * CONFIG_TRANSPARENT_HUGEPAGE is disabled.
 */
extern int handle_pte_fault(struct mm_struct *mm,
			    struct vm_area_struct *vma, unsigned long address,
			    pte_t *pte, pmd_t *pmd, unsigned int flags);

struct sysmmu_prefbuf {
	unsigned long base;
	unsigned long size;
	unsigned long config;
};

/*
 * sysmmu_set_prefetch_buffer_by_region() - set prefetch buffer configuration
 *
 * @dev: device descriptor of master device
 * @pb_reg: array of regions where prefetch buffer contains.
 *
 * If @dev is NULL or @pb_reg is 0, prefetch buffers is disabled.
 *
 */
void sysmmu_set_prefetch_buffer_by_region(struct device *dev,
			struct sysmmu_prefbuf pb_reg[], unsigned int num_reg);

/*
 * sysmmu_set_qos() - change PTW_QOS of the System MMUs of the given device
 *
 * @dev: device descriptor of master device
 * @qos: QoS value of Page table walking in the range of 0 ~ 15
 *
 * The changed QoS value is kept until it is changed to other value or
 * reset to the default value with sysmmu_reset_qos().
 */
void sysmmu_set_qos(struct device *dev, unsigned int qos);
/*
 * sysmmu_reset_qos() - reset PTW_QOS of the System MMUs to the default value
 *
 * @dev: device descriptor of master device
 *
 * PTW_QOS value of the System MMUs of @dev is reset to the default value that
 * is defined in compile time or booting time.
 */
void sysmmu_reset_qos(struct device *dev);

void exynos_sysmmu_show_status(struct device *dev);

void exynos_sysmmu_set_df(struct device *dev, dma_addr_t iova);
void exynos_sysmmu_release_df(struct device *dev);

#else
static inline int exynos_sysmmu_enable(struct device *owner, unsigned long *pgd)
{
	return -ENODEV;
}

static inline bool exynos_sysmmu_disable(struct device *owner)
{
	return false;
}

int exynos_sysmmu_map_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr, size_t size,
					int write)
{
	return -ENODEV;
}

int exynos_sysmmu_unmap_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr, size_t size)
{
	return -ENODEV;
}

#define sysmmu_set_qos(dev, qos) do { } while (0)
#define sysmmu_reset_qos(dev) do { } while (0)

#define exynos_sysmmu_show_status(dev) do { } while (0)
#define exynos_sysmmu_set_df(dev, iova)	do { } while (0)
#define exynos_sysmmu_release_df(dev)	do { } while (0)
#endif
#endif /*__ASM_PLAT_IOVMM_H*/

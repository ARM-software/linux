/*
 *
 * (C) COPYRIGHT 2010, 2012-2013 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



/**
 * @file mali_kbase_mem_linux.h
 * Base kernel memory APIs, Linux implementation.
 */

#ifndef _KBASE_MEM_LINUX_H_
#define _KBASE_MEM_LINUX_H_

/* This define is used by the gator kernel module compile to select which DDK
 * API calling convention to use. If not defined (legacy DDK) gator assumes
 * version 1. The version to DDK release mapping is:
 *     Version 1 API: DDK versions r1px, r2px
 *     Version 2 API: DDK versions r3px and newer
 **/
#define MALI_DDK_GATOR_API_VERSION 2

/** A HWC dump mapping */
typedef struct kbase_hwc_dma_mapping {
	void *     cpu_va;
	dma_addr_t dma_pa;
	size_t     size;
} kbase_hwc_dma_mapping;

struct kbase_va_region *kbase_pmem_alloc(kbase_context *kctx, u32 size, u32 flags, u16 * const pmem_cookie);
int kbase_mmap(struct file *file, struct vm_area_struct *vma);

/** @brief Allocate memory from kernel space and map it onto the GPU
 *
 * @param kctx   The context used for the allocation/mapping
 * @param size   The size of the allocation in bytes
 * @param handle An opaque structure used to contain the state needed to free the memory
 * @return the VA for kernel space and GPU MMU
 */
void * kbase_va_alloc(kbase_context *kctx, u32 size, kbase_hwc_dma_mapping * handle);

/** @brief Free/unmap memory allocated by kbase_va_alloc
 *
 * @param kctx   The context used for the allocation/mapping
 * @param handle An opaque structure returned by the kbase_va_alloc function.
 */
void kbase_va_free(kbase_context *kctx, kbase_hwc_dma_mapping * handle);

#endif				/* _KBASE_MEM_LINUX_H_ */

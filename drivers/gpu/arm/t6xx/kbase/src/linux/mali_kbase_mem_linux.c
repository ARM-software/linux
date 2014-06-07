/*
 *
 * (C) COPYRIGHT 2010-2013 ARM Limited. All rights reserved.
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
 * @file mali_kbase_mem_linux.c
 * Base kernel memory APIs, Linux implementation.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
#endif				/* defined(CONFIG_DMA_SHARED_BUFFER) */

#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/linux/mali_kbase_mem_linux.h>

static int kbase_tracking_page_setup(struct kbase_context * kctx, struct vm_area_struct * vma);

struct kbase_va_region *kbase_pmem_alloc(kbase_context *kctx, u32 size, u32 flags, u16 * const pmem_cookie)
{
	struct kbase_va_region *reg;
	u16 cookie;

	KBASE_DEBUG_ASSERT(kctx != NULL);
	KBASE_DEBUG_ASSERT(pmem_cookie != NULL);

	if (0 == size)
		goto out1;

	if (!kbase_check_alloc_flags(flags))
		goto out1;

	reg = kbase_alloc_free_region(kctx, 0, size, KBASE_REG_ZONE_PMEM);
	if (!reg)
		goto out1;

	reg->flags &= ~KBASE_REG_FREE;

	kbase_update_region_flags(reg, flags, MALI_FALSE);

	if (kbase_alloc_phy_pages(reg, size, size))
		goto out2;

	reg->nr_alloc_pages = size;
	reg->extent = 0;

	kbase_gpu_vm_lock(kctx);
	if (!kctx->osctx.cookies)
		goto out3;

	cookie = __ffs(kctx->osctx.cookies);
	kctx->osctx.cookies &= ~(1UL << cookie);
	reg->flags &= ~KBASE_REG_COOKIE_MASK;
	reg->flags |= KBASE_REG_COOKIE(cookie);

	list_add(&reg->link, &kctx->osctx.reg_pending);

	*pmem_cookie = cookie;
	kbase_gpu_vm_unlock(kctx);

	return reg;

 out3:
	kbase_gpu_vm_unlock(kctx);
	kbase_free_phy_pages(reg);
 out2:
	kfree(reg);
 out1:
	return NULL;

}

KBASE_EXPORT_TEST_API(kbase_pmem_alloc)

/*
 * Callback for munmap(). PMEM receives a special treatment, as it
 * frees the memory at the same time it gets unmapped. This avoids the
 * map/unmap race where map reuses a memory range that has been
 * unmapped from CPU, but still mapped on GPU.
 */
STATIC void kbase_cpu_vm_close(struct vm_area_struct *vma)
{
	struct kbase_va_region *reg = vma->vm_private_data;
	kbase_context *kctx = reg->kctx;
	mali_error err;

	kbase_gpu_vm_lock(kctx);

	err = kbase_cpu_free_mapping(reg, vma);
	if (!err && (reg->flags & KBASE_REG_ZONE_MASK) == KBASE_REG_ZONE_PMEM)
		kbase_mem_free_region(kctx, reg);

	kbase_gpu_vm_unlock(kctx);
}

KBASE_EXPORT_TEST_API(kbase_cpu_vm_close)

static const struct vm_operations_struct kbase_vm_ops = {
	.close = kbase_cpu_vm_close,
};

static int kbase_cpu_mmap(struct kbase_va_region *reg, struct vm_area_struct *vma, void *kaddr, u32 nr_pages)
{
	struct kbase_cpu_mapping *map;
	u64 start_off = vma->vm_pgoff - reg->start_pfn;
	phys_addr_t *page_array;
	int err = 0;
	int i;

	map = kzalloc(sizeof(*map), GFP_KERNEL);

	if (!map) {
		WARN_ON(1);
		err = -ENOMEM;
		goto out;
	}

	/*
	 * VM_DONTCOPY - don't make this mapping available in fork'ed processes
	 * VM_DONTEXPAND - disable mremap on this region
	 * VM_IO - disables paging
	 * VM_DONTDUMP - Don't include in core dumps (3.7 only)
	 * VM_MIXEDMAP - Support mixing struct page*s and raw pfns.
	 *               This is needed to support using the dedicated and
	 *               the OS based memory backends together.
	 */
	/*
	 * This will need updating to propagate coherency flags
	 * See MIDBASE-1057
	 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	vma->vm_flags |= VM_DONTCOPY | VM_DONTDUMP | VM_DONTEXPAND | VM_IO | VM_MIXEDMAP;
#else
	vma->vm_flags |= VM_DONTCOPY | VM_DONTEXPAND | VM_RESERVED | VM_IO | VM_MIXEDMAP;
#endif
	vma->vm_ops = &kbase_vm_ops;
	vma->vm_private_data = reg;

	page_array = kbase_get_phy_pages(reg);

	if (!(reg->flags & KBASE_REG_CPU_CACHED)) {
		/* We can't map vmalloc'd memory uncached.
		 * Other memory will have been returned from
		 * kbase_phy_pages_alloc which should have done the cache
		 * maintenance necessary to support an uncached mapping
		 */
		BUG_ON(kaddr);
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	}

	if (!kaddr) {
		for (i = 0; i < nr_pages; i++) {
			err = vm_insert_mixed(vma, vma->vm_start + (i << PAGE_SHIFT), page_array[i + start_off] >> PAGE_SHIFT);
			WARN_ON(err);
			if (err)
				break;
		}
	} else {
		/* vmalloc remaping is easy... */
		err = remap_vmalloc_range(vma, kaddr, 0);
		WARN_ON(err);
	}

	if (err) {
		kfree(map);
		goto out;
	}

	map->uaddr = (void *)vma->vm_start;
	map->nr_pages = nr_pages;
	map->page_off = start_off;
	map->private = vma;

	if ( (reg->flags & KBASE_REG_ZONE_MASK) == KBASE_REG_ZONE_TMEM)
	{
		kbase_process_page_usage_dec(reg->kctx, nr_pages);
	}

	list_add(&map->link, &reg->map_list);

 out:
	return err;
}

static int kbase_trace_buffer_mmap(kbase_context *kctx, struct vm_area_struct *vma, struct kbase_va_region **const reg, void **const kaddr)
{
	struct kbase_va_region *new_reg;
	u32 nr_pages;
	size_t size;
	int err = 0;
	u32 *tb;

	pr_debug("in %s\n", __func__);
	size = (vma->vm_end - vma->vm_start);
	nr_pages = size >> PAGE_SHIFT;

	if (!kctx->jctx.tb) {
		KBASE_DEBUG_ASSERT(0 != size);
		tb = vmalloc_user(size);

		if (NULL == tb) {
			err = -ENOMEM;
			goto out;
		}

		kbase_device_trace_buffer_install(kctx, tb, size);
	} else {
		err = -EINVAL;
		goto out;
	}

	*kaddr = kctx->jctx.tb;

	new_reg = kbase_alloc_free_region(kctx, 0, nr_pages, KBASE_REG_ZONE_PMEM);
	if (!new_reg) {
		err = -ENOMEM;
		WARN_ON(1);
		goto out_disconnect;
	}

	new_reg->flags &= ~KBASE_REG_FREE;
	new_reg->flags |= KBASE_REG_IS_TB | KBASE_REG_CPU_CACHED;
	new_reg->nr_alloc_pages = nr_pages;
	if (MALI_ERROR_NONE != kbase_add_va_region(kctx, new_reg, vma->vm_start, nr_pages, 1)) {
		err = -ENOMEM;
		WARN_ON(1);
		goto out_va_region;
	}

	*reg = new_reg;

	/* map read only, noexec */
	vma->vm_flags &= ~(VM_WRITE | VM_EXEC);
	/* the rest of the flags is added by the cpu_mmap handler */

	pr_debug("%s done\n", __func__);
	return 0;

 out_va_region:
	kbase_free_alloced_region(new_reg);
 out_disconnect:
	kbase_device_trace_buffer_uninstall(kctx);
	vfree(tb);
 out:
	return err;

}

static int kbase_mmu_dump_mmap(kbase_context *kctx, struct vm_area_struct *vma, struct kbase_va_region **const reg, void **const kmap_addr)
{
	struct kbase_va_region *new_reg;
	void *kaddr;
	u32 nr_pages;
	size_t size;
	int err = 0;

	pr_debug("in kbase_mmu_dump_mmap\n");
	size = (vma->vm_end - vma->vm_start);
	nr_pages = size >> PAGE_SHIFT;

	kaddr = kbase_mmu_dump(kctx, nr_pages);

	if (!kaddr) {
		err = -ENOMEM;
		goto out;
	}

	new_reg = kbase_alloc_free_region(kctx, 0, nr_pages, KBASE_REG_ZONE_PMEM);
	if (!new_reg) {
		err = -ENOMEM;
		WARN_ON(1);
		goto out;
	}

	new_reg->flags &= ~KBASE_REG_FREE;
	new_reg->flags |= KBASE_REG_IS_MMU_DUMP | KBASE_REG_CPU_CACHED;
	new_reg->nr_alloc_pages = nr_pages;
	if (MALI_ERROR_NONE != kbase_add_va_region(kctx, new_reg, vma->vm_start, nr_pages, 1)) {
		err = -ENOMEM;
		WARN_ON(1);
		goto out_va_region;
	}

	*kmap_addr = kaddr;
	*reg = new_reg;

	pr_debug("kbase_mmu_dump_mmap done\n");
	return 0;

 out_va_region:
	kbase_free_alloced_region(new_reg);
 out:
	return err;
}

/* must be called with the gpu vm lock held */

struct kbase_va_region *kbase_lookup_cookie(kbase_context *kctx, mali_addr64 cookie)
{
	struct kbase_va_region *reg;
	struct list_head *pos;
	mali_addr64 test_cookie;

	KBASE_DEBUG_ASSERT(kctx != NULL);
	BUG_ON(!mutex_is_locked(&kctx->reg_lock));

	test_cookie = KBASE_REG_COOKIE(cookie);

	list_for_each(pos, &kctx->osctx.reg_pending) {
		reg = list_entry(pos, struct kbase_va_region, link);
		if ((reg->flags & KBASE_REG_COOKIE_MASK) == test_cookie)
			return reg;
	}

	return NULL;		/* not found */
}

KBASE_EXPORT_TEST_API(kbase_lookup_cookie)

void kbase_unlink_cookie(kbase_context *kctx, mali_addr64 cookie, struct kbase_va_region *reg)
{
	KBASE_DEBUG_ASSERT(kctx != NULL);
	KBASE_DEBUG_ASSERT(reg != NULL);
	KBASE_DEBUG_ASSERT(MALI_TRUE == kbasep_list_member_of(&kctx->osctx.reg_pending, &reg->link));
	KBASE_DEBUG_ASSERT(KBASE_REG_COOKIE(cookie) == (reg->flags & KBASE_REG_COOKIE_MASK));
	KBASE_DEBUG_ASSERT((kctx->osctx.cookies & (1UL << cookie)) == 0);

	list_del(&reg->link);
	kctx->osctx.cookies |= (1UL << cookie);	/* mark as resolved */
}

KBASE_EXPORT_TEST_API(kbase_unlink_cookie)

void kbase_os_mem_map_lock(kbase_context *kctx)
{
	struct mm_struct *mm = current->mm;
	(void)kctx;
	down_read(&mm->mmap_sem);
}

void kbase_os_mem_map_unlock(kbase_context *kctx)
{
	struct mm_struct *mm = current->mm;
	(void)kctx;
	up_read(&mm->mmap_sem);
}

int kbase_mmap(struct file *file, struct vm_area_struct *vma)
{
	kbase_context *kctx = file->private_data;
	struct kbase_va_region *reg;
	void *kaddr = NULL;
	u32 nr_pages;
	int err = 0;

	pr_debug("kbase_mmap\n");
	nr_pages = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;

	if (0 == nr_pages) {
		err = -EINVAL;
		goto out;
	}

	kbase_gpu_vm_lock(kctx);

	if (vma->vm_pgoff == KBASE_REG_COOKIE_MTP)
	{
		/* The non-mapped tracking helper page */
		err = kbase_tracking_page_setup(kctx, vma);
		goto out_unlock;
	}

	/* if not the MTP, verify that the MTP has been mapped */
	rcu_read_lock();
	/* catches both when the special page isn't present or when we've forked */
	if (rcu_dereference(kctx->process_mm) != current->mm)
	{
		err = -EINVAL;
		rcu_read_unlock();
		goto out_unlock;
	}
	rcu_read_unlock();

	if (vma->vm_pgoff == KBASE_REG_COOKIE_RB) {
		/* Ring buffer doesn't exist any more */
		err = -EINVAL;
		goto out_unlock;
	} else if (vma->vm_pgoff == KBASE_REG_COOKIE_TB) {
		err = kbase_trace_buffer_mmap(kctx, vma, &reg, &kaddr);
		if (0 != err)
			goto out_unlock;
		pr_debug("kbase_trace_buffer_mmap ok\n");
		goto map;
	} else if (vma->vm_pgoff == KBASE_REG_COOKIE_MMU_DUMP) {
		/* MMU dump */
		err = kbase_mmu_dump_mmap(kctx, vma, &reg, &kaddr);
		if (0 != err)
			goto out_unlock;

		goto map;
	}

	if (vma->vm_pgoff < PAGE_SIZE) {	/* first page is reserved for cookie resolution */
		/* PMEM stuff, fetch the right region */
		reg = kbase_lookup_cookie(kctx, vma->vm_pgoff);

		if (NULL != reg) {
			if (reg->nr_pages != nr_pages) {
				/* incorrect mmap size */
				/* leave the cookie for a potential later mapping, or to be reclaimed later when the context is freed */
				err = -ENOMEM;
				goto out_unlock;
			}

			kbase_unlink_cookie(kctx, vma->vm_pgoff, reg);

			if (MALI_ERROR_NONE != kbase_gpu_mmap(kctx, reg, vma->vm_start, nr_pages, 1)) {
				/* Unable to map in GPU space. Recover from kbase_unlink_cookie */
				list_add(&reg->link, &kctx->osctx.reg_pending);
				kctx->osctx.cookies &= ~(1UL << vma->vm_pgoff);
				WARN_ON(1);
				err = -ENOMEM;
				goto out_unlock;
			}

			/*
			 * Overwrite the offset with the
			 * region start_pfn, so we effectively
			 * map from offset 0 in the region.
			 */
			vma->vm_pgoff = reg->start_pfn;
			goto map;
		}

		err = -ENOMEM;
		goto out_unlock;
	} else if (vma->vm_pgoff < KBASE_REG_ZONE_EXEC_BASE) {
		/* invalid offset as it identifies an already mapped pmem */
		err = -ENOMEM;
		goto out_unlock;
	} else {
		u32 zone;

		/* TMEM case or EXEC case */
		if (vma->vm_pgoff < KBASE_REG_ZONE_TMEM_BASE)
			zone = KBASE_REG_ZONE_EXEC;
		else
			zone = KBASE_REG_ZONE_TMEM;

		reg = kbase_region_tracker_find_region_enclosing_range(kctx, vma->vm_pgoff, nr_pages);
		if (reg && (reg->flags & (KBASE_REG_ZONE_MASK | KBASE_REG_FREE)) == zone) {
#ifdef CONFIG_DMA_SHARED_BUFFER
			if (reg->imported_type == BASE_TMEM_IMPORT_TYPE_UMM)
				goto dma_map;
#endif				/* CONFIG_DMA_SHARED_BUFFER */
			goto map;
		}

		err = -ENOMEM;
		goto out_unlock;
	}
 map:
	err = kbase_cpu_mmap(reg, vma, kaddr, nr_pages);

	if (vma->vm_pgoff == KBASE_REG_COOKIE_MMU_DUMP) {
		/* MMU dump - userspace should now have a reference on
		 * the pages, so we can now free the kernel mapping */
		vfree(kaddr);
	}
	goto out_unlock;

#ifdef CONFIG_DMA_SHARED_BUFFER
 dma_map:
	err = dma_buf_mmap(reg->imported_metadata.umm.dma_buf, vma, vma->vm_pgoff - reg->start_pfn);
#endif				/* CONFIG_DMA_SHARED_BUFFER */
 out_unlock:
	kbase_gpu_vm_unlock(kctx);
 out:
	if (err)
		pr_err("mmap failed %d\n", err);

	return err;
}

KBASE_EXPORT_TEST_API(kbase_mmap)

mali_error kbase_create_os_context(kbase_os_context * const osctx)
{
	KBASE_DEBUG_ASSERT(osctx != NULL);

	INIT_LIST_HEAD(&osctx->reg_pending);
	osctx->cookies = ~KBASE_REG_RESERVED_COOKIES;
	osctx->tgid = current->tgid;
	init_waitqueue_head(&osctx->event_queue);

	return MALI_ERROR_NONE;
}

KBASE_EXPORT_TEST_API(kbase_create_os_context)

static void kbase_reg_pending_dtor(struct kbase_va_region *reg)
{
	kbase_free_phy_pages(reg);
	pr_info("Freeing pending unmapped region\n");
	kfree(reg);
}

void kbase_destroy_os_context(kbase_os_context *osctx)
{
	struct kbase_va_region *reg;

	KBASE_DEBUG_ASSERT(osctx != NULL);

	while (!list_empty(&osctx->reg_pending)) {
		reg = list_entry(osctx->reg_pending.next, struct kbase_va_region, link);
		list_del(osctx->reg_pending.next);
		kbase_reg_pending_dtor(reg);
	}
}

KBASE_EXPORT_TEST_API(kbase_destroy_os_context)

void *kbase_va_alloc(kbase_context *kctx, u32 size, kbase_hwc_dma_mapping * handle)
{
	int i;
	void *va;
	dma_addr_t  dma_pa;
	struct kbase_va_region *reg;
	phys_addr_t *page_array;

	u32 pages = ((size - 1) >> PAGE_SHIFT) + 1;
	u32 flags = BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_CPU_WR | BASE_MEM_PROT_GPU_RD | BASE_MEM_PROT_GPU_WR;
	
	KBASE_DEBUG_ASSERT(kctx != NULL);
	KBASE_DEBUG_ASSERT(0 != size);

	if (size == 0)
		goto err;

	va = dma_alloc_writecombine(NULL, size, &dma_pa, GFP_KERNEL);
	if (!va)
		goto err;

	memset(va, 0x0, size);

	/* Store the state so we can free it later. */
	handle->cpu_va = va;
	handle->dma_pa = dma_pa;
	handle->size   = size;

	kbase_gpu_vm_lock(kctx);

	reg = kbase_alloc_free_region(kctx, 0, pages, KBASE_REG_ZONE_PMEM);
	if (!reg)
		goto vm_unlock;

	reg->flags &= ~KBASE_REG_FREE;
	kbase_update_region_flags(reg, flags, MALI_FALSE);

	reg->nr_alloc_pages = pages;
	reg->extent = 0;

	KBASE_DEBUG_ASSERT(0 != pages);
	page_array = vmalloc_user(pages * sizeof(*page_array));

	if (!page_array)
		goto free_reg;

	for (i = 0; i < pages; i++) {
		page_array[i] = dma_pa + (i << PAGE_SHIFT);
	}

	kbase_set_phy_pages(reg, page_array);

	if (kbase_gpu_mmap(kctx, reg, (uintptr_t) va, pages, 1))
		goto free_array;

	kbase_gpu_vm_unlock(kctx);

	return va;

 free_array:
	vfree(page_array);
 free_reg:
	kfree(reg);
 vm_unlock:
	kbase_gpu_vm_unlock(kctx);
	dma_free_writecombine(NULL, size, va, dma_pa);
 err:
	return NULL;
}
KBASE_EXPORT_SYMBOL(kbase_va_alloc)

void kbasep_os_process_page_usage_update( kbase_context *kctx, int pages )
{
	struct mm_struct *mm;

	rcu_read_lock();
	mm = rcu_dereference(kctx->process_mm);
	if (mm)
	{
		atomic_add(pages, &kctx->nonmapped_pages);
#ifdef SPLIT_RSS_COUNTING
		add_mm_counter(mm, MM_FILEPAGES, pages);
#else
		spin_lock(&mm->page_table_lock);
		add_mm_counter(mm, MM_FILEPAGES, pages);
		spin_unlock(&mm->page_table_lock);
#endif
	}
	rcu_read_unlock();
}

static void kbasep_os_process_page_usage_drain(kbase_context * kctx)
{
	int pages;
	struct mm_struct * mm;

	spin_lock(&kctx->mm_update_lock);
	mm = rcu_dereference_protected(kctx->process_mm, lockdep_is_held(&kctx->mm_update_lock));
	if (!mm)
	{
		spin_unlock(&kctx->mm_update_lock);
		return;
	}

	rcu_assign_pointer(kctx->process_mm, NULL);
	spin_unlock(&kctx->mm_update_lock);
	synchronize_rcu();

	pages = atomic_xchg(&kctx->nonmapped_pages, 0);
#ifdef SPLIT_RSS_COUNTING
	add_mm_counter(mm, MM_FILEPAGES, -pages);
#else
	spin_lock(&mm->page_table_lock);
	add_mm_counter(mm, MM_FILEPAGES, -pages);
	spin_unlock(&mm->page_table_lock);
#endif
}

static void kbase_special_vm_close(struct vm_area_struct *vma)
{
	kbase_context * kctx;
	kctx = vma->vm_private_data;
	kbasep_os_process_page_usage_drain(kctx);
}

static const struct vm_operations_struct kbase_vm_special_ops = {
	.close = kbase_special_vm_close,
};

static int kbase_tracking_page_setup(struct kbase_context * kctx, struct vm_area_struct * vma)
{
	/* check that this is the only tracking page */
	spin_lock(&kctx->mm_update_lock);
	if (rcu_dereference_protected(kctx->process_mm, lockdep_is_held(&kctx->mm_update_lock)))
	{
		spin_unlock(&kctx->mm_update_lock);
		return -EFAULT;
	}

	rcu_assign_pointer(kctx->process_mm, current->mm);

	spin_unlock(&kctx->mm_update_lock);

	/* no real access */
	vma->vm_flags &= ~(VM_READ | VM_WRITE | VM_EXEC);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	vma->vm_flags |= VM_DONTCOPY | VM_DONTEXPAND | VM_DONTDUMP | VM_IO;
#else
	vma->vm_flags |= VM_DONTCOPY | VM_DONTEXPAND | VM_RESERVED | VM_IO;
#endif
	vma->vm_ops = &kbase_vm_special_ops;
	vma->vm_private_data = kctx;

	return 0;
}

void kbase_va_free(kbase_context *kctx, kbase_hwc_dma_mapping * handle)
{
	struct kbase_va_region *reg;
	phys_addr_t *page_array;
	mali_error err;

	KBASE_DEBUG_ASSERT(kctx != NULL);
	KBASE_DEBUG_ASSERT(handle->cpu_va != NULL);

	kbase_gpu_vm_lock(kctx);

	reg = kbase_region_tracker_find_region_base_address(kctx, (uintptr_t)handle->cpu_va);
	KBASE_DEBUG_ASSERT(reg);

	err = kbase_gpu_munmap(kctx, reg);
	KBASE_DEBUG_ASSERT(err == MALI_ERROR_NONE);

	page_array = kbase_get_phy_pages(reg);
	vfree(page_array);

	kfree(reg);

	kbase_gpu_vm_unlock(kctx);

	dma_free_writecombine(NULL, handle->size, handle->cpu_va, handle->dma_pa);
}
KBASE_EXPORT_SYMBOL(kbase_va_free)

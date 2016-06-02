/*
 *
 * (C) COPYRIGHT 2011-2015 ARM Limited. All rights reserved.
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





#include <mali_kbase.h>

#include <linux/dma-mapping.h>
#ifdef CONFIG_SYNC
#include "sync.h"
#include <linux/syscalls.h>
#include "mali_kbase_sync.h"
#endif
#include <mali_kbase_hwaccess_time.h>
#include <linux/version.h>
#include <linux/ktime.h>

/* Mask to check cache alignment of data structures */
#define KBASE_CACHE_ALIGNMENT_MASK		((1<<L1_CACHE_SHIFT)-1)

/**
 * @file mali_kbase_softjobs.c
 *
 * This file implements the logic behind software only jobs that are
 * executed within the driver rather than being handed over to the GPU.
 */

void kbasep_add_waiting_soft_job(struct kbase_jd_atom *katom)
{
	struct kbase_context *kctx = katom->kctx;
	unsigned long lflags;

	spin_lock_irqsave(&kctx->waiting_soft_jobs_lock, lflags);
	list_add_tail(&katom->dep_item[0], &kctx->waiting_soft_jobs);
	spin_unlock_irqrestore(&kctx->waiting_soft_jobs_lock, lflags);
}

static struct page *kbasep_translate_gpu_addr_to_kernel_page(
		struct kbase_context *kctx, u64 gpu_addr)
{
	u64 pfn;
	struct kbase_va_region *reg;
	phys_addr_t addr = 0;

	KBASE_DEBUG_ASSERT(NULL != kctx);

	pfn = gpu_addr >> PAGE_SHIFT;

	kbase_gpu_vm_lock(kctx);
	reg = kbase_region_tracker_find_region_enclosing_address(
			kctx, gpu_addr);
	if (!reg || (reg->flags & KBASE_REG_FREE))
		goto err_vm_unlock;
	addr = reg->cpu_alloc->pages[pfn - reg->start_pfn];
	kbase_gpu_vm_unlock(kctx);

	if (!addr)
		goto err;

	return pfn_to_page(PFN_DOWN(addr));

err_vm_unlock:
	kbase_gpu_vm_unlock(kctx);
err:
	return NULL;
}

int kbasep_read_soft_event_status(
		struct kbase_context *kctx, u64 evt, unsigned char *status)
{
	struct page *pg = kbasep_translate_gpu_addr_to_kernel_page(
			kctx, evt);
	unsigned char *mapped_pg;
	u32 offset = evt & ~PAGE_MASK;

	KBASE_DEBUG_ASSERT(NULL != status);

	if (!pg)
		return -1;

	mapped_pg = (unsigned char *)kmap_atomic(pg);
	KBASE_DEBUG_ASSERT(NULL != mapped_pg); /* kmap_atomic() must not fail */
	*status = *(mapped_pg + offset);
	kunmap_atomic(mapped_pg);

	return 0;
}

int kbasep_write_soft_event_status(
		struct kbase_context *kctx, u64 evt, unsigned char new_status)
{
	struct page *pg = kbasep_translate_gpu_addr_to_kernel_page(
			kctx, evt);
	unsigned char *mapped_pg;
	u32 offset = evt & ~PAGE_MASK;

	KBASE_DEBUG_ASSERT((new_status == BASE_JD_SOFT_EVENT_SET) ||
			   (new_status == BASE_JD_SOFT_EVENT_RESET));

	if (!pg)
		return -1;

	mapped_pg = (unsigned char *)kmap_atomic(pg);
	KBASE_DEBUG_ASSERT(NULL != mapped_pg); /* kmap_atomic() must not fail */
	*(mapped_pg + offset) = new_status;
	kunmap_atomic(mapped_pg);

	return 0;
}

static int kbase_dump_cpu_gpu_time(struct kbase_jd_atom *katom)
{
	struct kbase_va_region *reg;
	phys_addr_t addr = 0;
	u64 pfn;
	u32 offset;
	char *page;
	struct timespec ts;
	struct base_dump_cpu_gpu_counters data;
	u64 system_time;
	u64 cycle_counter;
	u64 jc = katom->jc;
	struct kbase_context *kctx = katom->kctx;
	int pm_active_err;

	memset(&data, 0, sizeof(data));

	/* Take the PM active reference as late as possible - otherwise, it could
	 * delay suspend until we process the atom (which may be at the end of a
	 * long chain of dependencies */
	pm_active_err = kbase_pm_context_active_handle_suspend(kctx->kbdev, KBASE_PM_SUSPEND_HANDLER_DONT_REACTIVATE);
	if (pm_active_err) {
		struct kbasep_js_device_data *js_devdata = &kctx->kbdev->js_data;

		/* We're suspended - queue this on the list of suspended jobs
		 * Use dep_item[1], because dep_item[0] is in use for 'waiting_soft_jobs' */
		mutex_lock(&js_devdata->runpool_mutex);
		list_add_tail(&katom->dep_item[1], &js_devdata->suspended_soft_jobs_list);
		mutex_unlock(&js_devdata->runpool_mutex);

		/* Also adding this to the list of waiting soft job */
		kbasep_add_waiting_soft_job(katom);

		return pm_active_err;
	}

	kbase_backend_get_gpu_time(kctx->kbdev, &cycle_counter, &system_time,
									&ts);

	kbase_pm_context_idle(kctx->kbdev);

	data.sec = ts.tv_sec;
	data.usec = ts.tv_nsec / 1000;
	data.system_time = system_time;
	data.cycle_counter = cycle_counter;

	pfn = jc >> PAGE_SHIFT;
	offset = jc & ~PAGE_MASK;

	/* Assume this atom will be cancelled until we know otherwise */
	katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;
	if (offset > 0x1000 - sizeof(data)) {
		/* Wouldn't fit in the page */
		return 0;
	}

	kbase_gpu_vm_lock(kctx);
	reg = kbase_region_tracker_find_region_enclosing_address(kctx, jc);
	if (reg &&
	    (reg->flags & KBASE_REG_GPU_WR) &&
	    reg->cpu_alloc && reg->cpu_alloc->pages)
		addr = reg->cpu_alloc->pages[pfn - reg->start_pfn];

	kbase_gpu_vm_unlock(kctx);
	if (!addr)
		return 0;

	page = kmap(pfn_to_page(PFN_DOWN(addr)));
	if (!page)
		return 0;

	kbase_sync_single_for_cpu(katom->kctx->kbdev,
			kbase_dma_addr(pfn_to_page(PFN_DOWN(addr))) +
			offset, sizeof(data),
			DMA_BIDIRECTIONAL);

	memcpy(page + offset, &data, sizeof(data));

	kbase_sync_single_for_device(katom->kctx->kbdev,
			kbase_dma_addr(pfn_to_page(PFN_DOWN(addr))) +
			offset, sizeof(data),
			DMA_BIDIRECTIONAL);

	kunmap(pfn_to_page(PFN_DOWN(addr)));

	/* Atom was fine - mark it as done */
	katom->event_code = BASE_JD_EVENT_DONE;

	return 0;
}

#ifdef CONFIG_SYNC

/* Complete an atom that has returned '1' from kbase_process_soft_job (i.e. has waited)
 *
 * @param katom     The atom to complete
 */
static void complete_soft_job(struct kbase_jd_atom *katom)
{
	struct kbase_context *kctx = katom->kctx;

	mutex_lock(&kctx->jctx.lock);
	list_del(&katom->dep_item[0]);
	kbase_finish_soft_job(katom);
	if (jd_done_nolock(katom, NULL))
		kbase_js_sched_all(kctx->kbdev);
	mutex_unlock(&kctx->jctx.lock);
}

static enum base_jd_event_code kbase_fence_trigger(struct kbase_jd_atom *katom, int result)
{
	struct sync_pt *pt;
	struct sync_timeline *timeline;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
	if (!list_is_singular(&katom->fence->pt_list_head)) {
#else
	if (katom->fence->num_fences != 1) {
#endif
		/* Not exactly one item in the list - so it didn't (directly) come from us */
		return BASE_JD_EVENT_JOB_CANCELLED;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
	pt = list_first_entry(&katom->fence->pt_list_head, struct sync_pt, pt_list);
#else
	pt = container_of(katom->fence->cbs[0].sync_pt, struct sync_pt, base);
#endif
	timeline = sync_pt_parent(pt);

	if (!kbase_sync_timeline_is_ours(timeline)) {
		/* Fence has a sync_pt which isn't ours! */
		return BASE_JD_EVENT_JOB_CANCELLED;
	}

	kbase_sync_signal_pt(pt, result);

	sync_timeline_signal(timeline);

	return (result < 0) ? BASE_JD_EVENT_JOB_CANCELLED : BASE_JD_EVENT_DONE;
}

static void kbase_fence_wait_worker(struct work_struct *data)
{
	struct kbase_jd_atom *katom;
	struct kbase_context *kctx;

	katom = container_of(data, struct kbase_jd_atom, work);
	kctx = katom->kctx;

	complete_soft_job(katom);
}

static void kbase_fence_wait_callback(struct sync_fence *fence, struct sync_fence_waiter *waiter)
{
	struct kbase_jd_atom *katom = container_of(waiter, struct kbase_jd_atom, sync_waiter);
	struct kbase_context *kctx;

	KBASE_DEBUG_ASSERT(NULL != katom);

	kctx = katom->kctx;

	KBASE_DEBUG_ASSERT(NULL != kctx);

	/* Propagate the fence status to the atom.
	 * If negative then cancel this atom and its dependencies.
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
	if (fence->status < 0)
#else
	if (atomic_read(&fence->status) < 0)
#endif
		katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;

	/* To prevent a potential deadlock we schedule the work onto the job_done_wq workqueue
	 *
	 * The issue is that we may signal the timeline while holding kctx->jctx.lock and
	 * the callbacks are run synchronously from sync_timeline_signal. So we simply defer the work.
	 */

	KBASE_DEBUG_ASSERT(0 == object_is_on_stack(&katom->work));
	INIT_WORK(&katom->work, kbase_fence_wait_worker);
	queue_work(kctx->jctx.job_done_wq, &katom->work);
}

static int kbase_fence_wait(struct kbase_jd_atom *katom)
{
	int ret;

	KBASE_DEBUG_ASSERT(NULL != katom);
	KBASE_DEBUG_ASSERT(NULL != katom->kctx);

	sync_fence_waiter_init(&katom->sync_waiter, kbase_fence_wait_callback);

	ret = sync_fence_wait_async(katom->fence, &katom->sync_waiter);

	if (ret == 1) {
		/* Already signalled */
		return 0;
	}

	if (ret < 0) {
		katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;
		/* We should cause the dependent jobs in the bag to be failed,
		 * to do this we schedule the work queue to complete this job */
		KBASE_DEBUG_ASSERT(0 == object_is_on_stack(&katom->work));
		INIT_WORK(&katom->work, kbase_fence_wait_worker);
		queue_work(katom->kctx->jctx.job_done_wq, &katom->work);
	}

	kbasep_add_waiting_soft_job(katom);

	return 1;
}

static void kbase_fence_cancel_wait(struct kbase_jd_atom *katom)
{
	if (sync_fence_cancel_async(katom->fence, &katom->sync_waiter) != 0) {
		/* The wait wasn't cancelled - leave the cleanup for kbase_fence_wait_callback */
		return;
	}

	/* Wait was cancelled - zap the atoms */
	katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;

	kbase_finish_soft_job(katom);

	if (jd_done_nolock(katom, NULL))
		kbase_js_sched_all(katom->kctx->kbdev);
}
#endif /* CONFIG_SYNC */

static void kbasep_soft_event_complete_job(struct work_struct *work)
{
	struct kbase_jd_atom *katom = container_of(work, struct kbase_jd_atom,
			work);
	struct kbase_context *kctx = katom->kctx;
	int resched;

	mutex_lock(&kctx->jctx.lock);
	resched = jd_done_nolock(katom, NULL);
	mutex_unlock(&kctx->jctx.lock);

	if (resched)
		kbase_js_sched_all(kctx->kbdev);
}

void kbasep_complete_triggered_soft_events(struct kbase_context *kctx, u64 evt)
{
	int cancel_timer = 1;
	struct list_head *entry, *tmp;
	unsigned long lflags;

	spin_lock_irqsave(&kctx->waiting_soft_jobs_lock, lflags);
	list_for_each_safe(entry, tmp, &kctx->waiting_soft_jobs) {
		struct kbase_jd_atom *katom = list_entry(
				entry, struct kbase_jd_atom, dep_item[0]);

		if ((katom->core_req & BASEP_JD_REQ_ATOM_TYPE) ==
		    BASE_JD_REQ_SOFT_EVENT_WAIT) {
			if (katom->jc == evt) {
				list_del(&katom->dep_item[0]);

				katom->event_code = BASE_JD_EVENT_DONE;
				INIT_WORK(&katom->work,
					  kbasep_soft_event_complete_job);
				queue_work(kctx->jctx.job_done_wq,
					   &katom->work);
			} else {
				/* There are still other waiting jobs, we cannot
				 * cancel the timer yet */
				cancel_timer = 0;
			}
		}
	}

	if (cancel_timer)
		hrtimer_try_to_cancel(&kctx->soft_event_timeout);
	spin_unlock_irqrestore(&kctx->waiting_soft_jobs_lock, lflags);
}

enum hrtimer_restart kbasep_soft_event_timeout_worker(struct hrtimer *timer)
{
	struct kbase_context *kctx = container_of(timer, struct kbase_context,
			soft_event_timeout);
	u32 timeout_ms = (u32)atomic_read(
			&kctx->kbdev->js_data.soft_event_timeout_ms);
	ktime_t cur_time = ktime_get();
	enum hrtimer_restart restarting = HRTIMER_NORESTART;
	unsigned long lflags;
	struct list_head *entry, *tmp;

	spin_lock_irqsave(&kctx->waiting_soft_jobs_lock, lflags);
	list_for_each_safe(entry, tmp, &kctx->waiting_soft_jobs) {
		struct kbase_jd_atom *katom = list_entry(
				entry, struct kbase_jd_atom, dep_item[0]);

		if ((katom->core_req & BASEP_JD_REQ_ATOM_TYPE) ==
		    BASE_JD_REQ_SOFT_EVENT_WAIT) {
			s64 elapsed_time =
				ktime_to_ms(ktime_sub(cur_time,
						      katom->start_timestamp));
			if (elapsed_time > (s64)timeout_ms) {
				/* Take it out of the list to ensure that it
				 * will be cancelled in all cases */
				list_del(&katom->dep_item[0]);

				katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;
				INIT_WORK(&katom->work,
					  kbasep_soft_event_complete_job);
				queue_work(kctx->jctx.job_done_wq,
					   &katom->work);
			} else {
				restarting = HRTIMER_RESTART;
			}
		}
	}

	if (restarting)
		hrtimer_add_expires(timer, HR_TIMER_DELAY_MSEC(timeout_ms));
	spin_unlock_irqrestore(&kctx->waiting_soft_jobs_lock, lflags);

	return restarting;
}

static int kbasep_soft_event_wait(struct kbase_jd_atom *katom)
{
	struct kbase_context *kctx = katom->kctx;
	ktime_t remaining;
	unsigned char status;

	/* The status of this soft-job is stored in jc */
	if (kbasep_read_soft_event_status(kctx, katom->jc, &status) != 0) {
		katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;
		return 0;
	}

	if (status == BASE_JD_SOFT_EVENT_SET)
		return 0; /* Event already set, nothing to do */

	/* Record the start time of this atom so we could cancel it at
	 * the right time */
	katom->start_timestamp = ktime_get();

	/* Add the atom to the waiting list before the timer is
	 * (re)started to make sure that it gets processed */
	kbasep_add_waiting_soft_job(katom);

	/* Schedule cancellation of this atom after a period if it is
	 * not active */
	remaining = hrtimer_get_remaining(&kctx->soft_event_timeout);
	if (remaining.tv64 <= 0) {
		int timeout_ms = atomic_read(
				&kctx->kbdev->js_data.soft_event_timeout_ms);
		hrtimer_start(&kctx->soft_event_timeout,
			      HR_TIMER_DELAY_MSEC((u64)timeout_ms),
			      HRTIMER_MODE_REL);
	}

	return 1;
}

static void kbasep_soft_event_update(struct kbase_jd_atom *katom,
				     unsigned char new_status)
{
	/* Complete jobs waiting on the same event */
	struct kbase_context *kctx = katom->kctx;

	if (kbasep_write_soft_event_status(kctx, katom->jc, new_status) != 0) {
		katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;
		return;
	}

	if (new_status == BASE_JD_SOFT_EVENT_SET)
		kbasep_complete_triggered_soft_events(kctx, katom->jc);
}

static void kbasep_soft_event_cancel_job(struct kbase_jd_atom *katom)
{
	katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;
	if (jd_done_nolock(katom, NULL))
		kbase_js_sched_all(katom->kctx->kbdev);
}

int kbase_process_soft_job(struct kbase_jd_atom *katom)
{
	switch (katom->core_req & BASEP_JD_REQ_ATOM_TYPE) {
	case BASE_JD_REQ_SOFT_DUMP_CPU_GPU_TIME:
		return kbase_dump_cpu_gpu_time(katom);
#ifdef CONFIG_SYNC
	case BASE_JD_REQ_SOFT_FENCE_TRIGGER:
		KBASE_DEBUG_ASSERT(katom->fence != NULL);
		katom->event_code = kbase_fence_trigger(katom, katom->event_code == BASE_JD_EVENT_DONE ? 0 : -EFAULT);
		/* Release the reference as we don't need it any more */
		sync_fence_put(katom->fence);
		katom->fence = NULL;
		break;
	case BASE_JD_REQ_SOFT_FENCE_WAIT:
		return kbase_fence_wait(katom);
#endif				/* CONFIG_SYNC */
	case BASE_JD_REQ_SOFT_REPLAY:
		return kbase_replay_process(katom);
	case BASE_JD_REQ_SOFT_EVENT_WAIT:
		return kbasep_soft_event_wait(katom);
	case BASE_JD_REQ_SOFT_EVENT_SET:
		kbasep_soft_event_update(katom, BASE_JD_SOFT_EVENT_SET);
		break;
	case BASE_JD_REQ_SOFT_EVENT_RESET:
		kbasep_soft_event_update(katom, BASE_JD_SOFT_EVENT_RESET);
		break;
	}

	/* Atom is complete */
	return 0;
}

void kbase_cancel_soft_job(struct kbase_jd_atom *katom)
{
	switch (katom->core_req & BASEP_JD_REQ_ATOM_TYPE) {
#ifdef CONFIG_SYNC
	case BASE_JD_REQ_SOFT_FENCE_WAIT:
		kbase_fence_cancel_wait(katom);
		break;
#endif
	case BASE_JD_REQ_SOFT_EVENT_WAIT:
		kbasep_soft_event_cancel_job(katom);
		break;
	default:
		/* This soft-job doesn't support cancellation! */
		KBASE_DEBUG_ASSERT(0);
	}
}

int kbase_prepare_soft_job(struct kbase_jd_atom *katom)
{
	switch (katom->core_req & BASEP_JD_REQ_ATOM_TYPE) {
	case BASE_JD_REQ_SOFT_DUMP_CPU_GPU_TIME:
		{
			if (0 != (katom->jc & KBASE_CACHE_ALIGNMENT_MASK))
				return -EINVAL;
		}
		break;
#ifdef CONFIG_SYNC
	case BASE_JD_REQ_SOFT_FENCE_TRIGGER:
		{
			struct base_fence fence;
			int fd;

			if (0 != copy_from_user(&fence, (__user void *)(uintptr_t) katom->jc, sizeof(fence)))
				return -EINVAL;

			fd = kbase_stream_create_fence(fence.basep.stream_fd);
			if (fd < 0)
				return -EINVAL;

			katom->fence = sync_fence_fdget(fd);

			if (katom->fence == NULL) {
				/* The only way the fence can be NULL is if userspace closed it for us.
				 * So we don't need to clear it up */
				return -EINVAL;
			}
			fence.basep.fd = fd;
			if (0 != copy_to_user((__user void *)(uintptr_t) katom->jc, &fence, sizeof(fence))) {
				katom->fence = NULL;
				sys_close(fd);
				return -EINVAL;
			}
		}
		break;
	case BASE_JD_REQ_SOFT_FENCE_WAIT:
		{
			struct base_fence fence;

			if (0 != copy_from_user(&fence, (__user void *)(uintptr_t) katom->jc, sizeof(fence)))
				return -EINVAL;

			/* Get a reference to the fence object */
			katom->fence = sync_fence_fdget(fence.basep.fd);
			if (katom->fence == NULL)
				return -EINVAL;
		}
		break;
#endif				/* CONFIG_SYNC */
	case BASE_JD_REQ_SOFT_REPLAY:
		break;
	case BASE_JD_REQ_SOFT_EVENT_WAIT:
	case BASE_JD_REQ_SOFT_EVENT_SET:
	case BASE_JD_REQ_SOFT_EVENT_RESET:
		if (katom->jc == 0)
			return -EINVAL;
		break;
	default:
		/* Unsupported soft-job */
		return -EINVAL;
	}
	return 0;
}

void kbase_finish_soft_job(struct kbase_jd_atom *katom)
{
	switch (katom->core_req & BASEP_JD_REQ_ATOM_TYPE) {
	case BASE_JD_REQ_SOFT_DUMP_CPU_GPU_TIME:
		/* Nothing to do */
		break;
#ifdef CONFIG_SYNC
	case BASE_JD_REQ_SOFT_FENCE_TRIGGER:
		/* If fence has not yet been signalled, do it now */
		if (katom->fence) {
			kbase_fence_trigger(katom, katom->event_code ==
					BASE_JD_EVENT_DONE ? 0 : -EFAULT);
			sync_fence_put(katom->fence);
			katom->fence = NULL;
		}
		break;
	case BASE_JD_REQ_SOFT_FENCE_WAIT:
		/* Release the reference to the fence object */
		sync_fence_put(katom->fence);
		katom->fence = NULL;
		break;
#endif				/* CONFIG_SYNC */
	}
}

void kbase_resume_suspended_soft_jobs(struct kbase_device *kbdev)
{
	LIST_HEAD(local_suspended_soft_jobs);
	struct kbase_jd_atom *tmp_iter;
	struct kbase_jd_atom *katom_iter;
	struct kbasep_js_device_data *js_devdata;
	bool resched = false;

	KBASE_DEBUG_ASSERT(kbdev);

	js_devdata = &kbdev->js_data;

	/* Move out the entire list */
	mutex_lock(&js_devdata->runpool_mutex);
	list_splice_init(&js_devdata->suspended_soft_jobs_list,
			&local_suspended_soft_jobs);
	mutex_unlock(&js_devdata->runpool_mutex);

	/*
	 * Each atom must be detached from the list and ran separately -
	 * it could be re-added to the old list, but this is unlikely
	 */
	list_for_each_entry_safe(katom_iter, tmp_iter,
			&local_suspended_soft_jobs, dep_item[1]) {
		struct kbase_context *kctx = katom_iter->kctx;

		mutex_lock(&kctx->jctx.lock);

		/* Remove from the global list */
		list_del(&katom_iter->dep_item[1]);
		/* Remove from the context's list of waiting soft jobs */
		list_del(&katom_iter->dep_item[0]);

		if (kbase_process_soft_job(katom_iter) == 0) {
			kbase_finish_soft_job(katom_iter);
			resched |= jd_done_nolock(katom_iter, NULL);
		} else {
			KBASE_DEBUG_ASSERT((katom_iter->core_req &
					BASEP_JD_REQ_ATOM_TYPE)
					!= BASE_JD_REQ_SOFT_REPLAY);
		}

		mutex_unlock(&kctx->jctx.lock);
	}

	if (resched)
		kbase_js_sched_all(kbdev);
}

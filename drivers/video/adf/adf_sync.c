/*
 * Sync File validation framework
 *
 * Copyright (C) 2012 Google, Inc.
 * portions Copyright 2017 ARM Ltd.
 * These code is mostly copied from drivers/dma-buf/sw_sync.c
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sync_file.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <video/adf_sync.h>

#define CREATE_TRACE_POINTS

static const struct fence_ops timeline_fence_ops;

static inline struct sync_pt *fence_to_sync_pt(struct fence *fence)
{
	if (fence->ops != &timeline_fence_ops)
		return NULL;
	return container_of(fence, struct sync_pt, base);
}

/**
 * sync_timeline_create() - creates a sync object
 * @name:	sync_timeline name
 *
 * Creates a new sync_timeline. Returns the sync_timeline object or NULL in
 * case of error.
 */
struct sync_timeline *__sync_timeline_create(const char *name)
{
	struct sync_timeline *obj;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return NULL;

	kref_init(&obj->kref);
	obj->context = fence_context_alloc(1);
	strlcpy(obj->name, name, sizeof(obj->name));

	INIT_LIST_HEAD(&obj->child_list_head);
	INIT_LIST_HEAD(&obj->active_list_head);
	spin_lock_init(&obj->child_list_lock);

	return obj;
}
EXPORT_SYMBOL(__sync_timeline_create);

static void sync_timeline_free(struct kref *kref)
{
	struct sync_timeline *obj =
		container_of(kref, struct sync_timeline, kref);

	kfree(obj);
}

void sync_timeline_get(struct sync_timeline *obj)
{
	kref_get(&obj->kref);
}
EXPORT_SYMBOL(sync_timeline_get);

void sync_timeline_put(struct sync_timeline *obj)
{
	kref_put(&obj->kref, sync_timeline_free);
}
EXPORT_SYMBOL(sync_timeline_put);

/**
 * sync_timeline_signal() - signal a status change on a sync_timeline
 * @obj:	sync_timeline to signal
 * @inc:	num to increment on timeline->value
 *
 * A sync implementation should call this any time one of it's fences
 * has signaled or has an error condition.
 */
void sync_timeline_signal(struct sync_timeline *obj, unsigned int inc)
{
	unsigned long flags;
	struct sync_pt *pt, *next;

	spin_lock_irqsave(&obj->child_list_lock, flags);

	obj->value += inc;

	list_for_each_entry_safe(pt, next, &obj->active_list_head,
				 active_list) {
		if (fence_is_signaled_locked(&pt->base))
			list_del_init(&pt->active_list);
	}

	spin_unlock_irqrestore(&obj->child_list_lock, flags);
}
EXPORT_SYMBOL(sync_timeline_signal);


/**
 * sync_pt_create() - creates a sync pt
 * @parent:	fence's parent sync_timeline
 * @size:	size to allocate for this pt
 * @inc:	value of the fence
 *
 * Creates a new sync_pt as a child of @parent.  @size bytes will be
 * allocated allowing for implementation specific data to be kept after
 * the generic sync_timeline struct. Returns the sync_pt object or
 * NULL in case of error.
 */
static struct sync_pt *sync_pt_create(struct sync_timeline *obj, int size,
			     unsigned int value)
{
	unsigned long flags;
	struct sync_pt *pt;

	if (size < sizeof(*pt))
		return NULL;

	pt = kzalloc(size, GFP_KERNEL);
	if (!pt)
		return NULL;

	spin_lock_irqsave(&obj->child_list_lock, flags);
	sync_timeline_get(obj);
	fence_init(&pt->base, &timeline_fence_ops, &obj->child_list_lock,
		   obj->context, value);
	list_add_tail(&pt->child_list, &obj->child_list_head);
	INIT_LIST_HEAD(&pt->active_list);
	spin_unlock_irqrestore(&obj->child_list_lock, flags);
	return pt;
}

static const char *timeline_fence_get_driver_name(struct fence *fence)
{
	return "sw_sync";
}

static const char *timeline_fence_get_timeline_name(struct fence *fence)
{
	struct sync_timeline *parent = fence_parent(fence);

	return parent->name;
}

static void timeline_fence_release(struct fence *fence)
{
	struct sync_pt *pt = fence_to_sync_pt(fence);
	struct sync_timeline *parent = fence_parent(fence);
	unsigned long flags;

	spin_lock_irqsave(fence->lock, flags);
	list_del(&pt->child_list);
	if (!list_empty(&pt->active_list))
		list_del(&pt->active_list);
	spin_unlock_irqrestore(fence->lock, flags);

	sync_timeline_put(parent);
	fence_free(fence);
}

static bool timeline_fence_signaled(struct fence *fence)
{
	struct sync_timeline *parent = fence_parent(fence);

	return (fence->seqno > parent->value) ? false : true;
}

static bool timeline_fence_enable_signaling(struct fence *fence)
{
	struct sync_pt *pt = fence_to_sync_pt(fence);
	struct sync_timeline *parent = fence_parent(fence);

	if (timeline_fence_signaled(fence))
		return false;

	list_add_tail(&pt->active_list, &parent->active_list_head);
	return true;
}

static void timeline_fence_disable_signaling(struct fence *fence)
{
	struct sync_pt *pt = container_of(fence, struct sync_pt, base);

	list_del_init(&pt->active_list);
}

static void timeline_fence_value_str(struct fence *fence,
				    char *str, int size)
{
	snprintf(str, size, "%d", fence->seqno);
}

static void timeline_fence_timeline_value_str(struct fence *fence,
					     char *str, int size)
{
	struct sync_timeline *parent = fence_parent(fence);

	snprintf(str, size, "%d", parent->value);
}

static const struct fence_ops timeline_fence_ops = {
	.get_driver_name = timeline_fence_get_driver_name,
	.get_timeline_name = timeline_fence_get_timeline_name,
	.enable_signaling = timeline_fence_enable_signaling,
	.disable_signaling = timeline_fence_disable_signaling,
	.signaled = timeline_fence_signaled,
	.wait = fence_default_wait,
	.release = timeline_fence_release,
	.fence_value_str = timeline_fence_value_str,
	.timeline_value_str = timeline_fence_timeline_value_str,
};

struct sw_sync_timeline *sw_sync_timeline_create(const char *name)
{
	return (struct sw_sync_timeline *)__sync_timeline_create(name);
}
EXPORT_SYMBOL(sw_sync_timeline_create);


void sw_sync_timeline_inc(struct sw_sync_timeline *obj, u32 inc)
{
	sync_timeline_signal(&obj->obj, inc);
}

EXPORT_SYMBOL(sw_sync_timeline_inc);

struct sync_pt *sw_sync_pt_create(struct sw_sync_timeline *obj, u32 value)
{
	return sync_pt_create(&obj->obj, sizeof(struct sync_pt), value);
}
EXPORT_SYMBOL(sw_sync_pt_create);


void sync_pt_free(struct sync_pt *pt)
{
        fence_put(&pt->base);
}
EXPORT_SYMBOL(sync_pt_free);


#define to_sync_fence(sync_file)	\
	container_of(sync_file, struct sync_fence, file)

/* TODO: implement a create which takes more that one sync_pt */
struct sync_fence *sync_fence_create(const char *name, struct sync_pt *pt)
{
         return to_sync_fence(sync_file_create(&pt->base));
}

EXPORT_SYMBOL(sync_fence_create);

void sync_fence_put(struct sync_fence *fence)
{
        fput(fence->file.file);
}
EXPORT_SYMBOL(sync_fence_put);

int sync_fence_wait(struct sync_fence *fence, long timeout)
{
        return fence_wait(fence->file.fence, true);
}
EXPORT_SYMBOL(sync_fence_wait);


/**
 * sync_file_fdget() - get a sync_file from an fd
 * @fd:		fd referencing a fence
 *
 * Ensures @fd references a valid sync_file, increments the refcount of the
 * backing file. Returns the sync_file or NULL in case of error.
 */
static struct sync_file *sync_file_fdget(int fd)
{
	struct file *filp = fget(fd);
	if (!filp)
		return NULL;
	return filp->private_data;
}

struct sync_fence *sync_fence_fdget(int fd)
{
        return to_sync_fence(sync_file_fdget(fd));
}


void sync_fence_install(struct sync_fence *fence, int fd)
{
        fd_install(fd, fence->file.file);
}
EXPORT_SYMBOL(sync_fence_install);

void sync_timeline_destroy(struct sync_timeline *obj)
{
	smp_wmb();

	sync_timeline_put(obj);
}
EXPORT_SYMBOL(sync_timeline_destroy);


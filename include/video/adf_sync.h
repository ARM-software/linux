/*
 * include/linux/sw_sync.h
 *
 * Copyright (C) 2012 Google, Inc.
 *
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
#ifndef  _ADF_SYNC_H
#define  _ADF_SYNC_H
#include <linux/sync_file.h>
#include <linux/fence.h>

/**
 * struct sync_timeline - sync object
 * @kref:		reference count on fence.
 * @name:		name of the sync_timeline. Useful for debugging
 * @child_list_head:	list of children sync_pts for this sync_timeline
 * @child_list_lock:	lock protecting @child_list_head and fence.status
 * @active_list_head:	list of active (unsignaled/errored) sync_pts
 * @sync_timeline_list:	membership in global sync_timeline_list
 */
struct sync_timeline {
	struct kref		kref;
	char			name[32];


	u64			context;
	int			value;

	struct list_head	child_list_head;
	spinlock_t		child_list_lock;

	struct list_head	active_list_head;

	struct list_head	sync_timeline_list;
};

static inline struct sync_timeline *fence_parent(struct fence *fence)
{
	return container_of(fence->lock, struct sync_timeline,
			    child_list_lock);
}

struct sw_sync_timeline {
       struct sync_timeline     obj;
};


struct sync_fence {
        struct sync_file        file;
};

/**
 * struct sync_pt - sync_pt object
 * @base: base fence object
 * @child_list: sync timeline child's list
 * @active_list: sync timeline active child's list
 */
struct sync_pt {
	struct fence base;
	struct list_head child_list;
	struct list_head active_list;
};

void sync_timeline_put(struct sync_timeline *obj);
void sync_timeline_get(struct sync_timeline *obj);
struct sync_timeline *__sync_timeline_create(const char *name);
void sync_timeline_signal(struct sync_timeline *obj, unsigned int inc);
struct sw_sync_timeline *sw_sync_timeline_create(const char *name);
void sw_sync_timeline_inc(struct sw_sync_timeline *obj, u32 inc);
struct sync_pt *sw_sync_pt_create(struct sw_sync_timeline *obj, u32 value);
void sync_pt_free(struct sync_pt *pt);
struct sync_fence *sync_fence_create(const char *name, struct sync_pt *pt);
void sync_fence_put(struct sync_fence *fence);
int sync_fence_wait(struct sync_fence *fence, long timeout);
struct sync_fence *sync_fence_fdget(int fd);
void sync_fence_install(struct sync_fence *fence, int fd);
void sync_timeline_destroy(struct sync_timeline *obj);

#endif

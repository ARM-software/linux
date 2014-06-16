/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>

/**
 * drm_atomic_helper_begin - start a sequence of atomic updates
 * @dev: DRM device
 * @flags: the modifier flags that userspace has requested
 *
 * Begin a sequence of atomic property sets.  Returns a driver
 * private state object that is passed back into the various
 * object's set_property() fxns, and into the remainder of the
 * atomic funcs.  The state object should accumulate the changes
 * from one o more set_property()'s.  At the end, the state can
 * be checked, and optionally committed.
 *
 * RETURNS
 *   a driver private state object, which is passed back in to
 *   the various other atomic fxns, or error (such as -EBUSY if
 *   there is still a pending async update)
 */
void *drm_atomic_helper_begin(struct drm_device *dev, uint32_t flags)
{
	struct drm_atomic_helper_state *state;
	int sz;
	void *ptr;

	sz = sizeof(*state);

	ptr = kzalloc(sz, GFP_KERNEL);

	state = ptr;
	ptr = &state[1];

	kref_init(&state->refcount);
	state->dev = dev;
	state->flags = flags;
	return state;
}
EXPORT_SYMBOL(drm_atomic_helper_begin);

/**
 * drm_atomic_helper_set_event - set a pending event on mode object
 * @dev: DRM device
 * @state: the driver private state object
 * @obj: the object to set the event on
 * @event: the event to send back
 *
 * Set pending event for an update on the specified object.  The
 * event is to be sent back to userspace after the update completes.
 */
int drm_atomic_helper_set_event(struct drm_device *dev,
		void *state, struct drm_mode_object *obj,
		struct drm_pending_vblank_event *event)
{
	return -EINVAL;  /* for now */
}
EXPORT_SYMBOL(drm_atomic_helper_set_event);

/**
 * drm_atomic_helper_check - validate state object
 * @dev: DRM device
 * @state: the driver private state object
 *
 * Check the state object to see if the requested state is
 * physically possible.
 *
 * RETURNS
 * Zero for success or -errno
 */
int drm_atomic_helper_check(struct drm_device *dev, void *state)
{
	return 0;  /* for now */
}
EXPORT_SYMBOL(drm_atomic_helper_check);

/**
 * drm_atomic_helper_commit - commit state
 * @dev: DRM device
 * @state: the driver private state object
 *
 * Commit the state.  This will only be called if atomic_check()
 * succeeds.
 *
 * RETURNS
 * Zero for success or -errno
 */
int drm_atomic_helper_commit(struct drm_device *dev, void *state)
{
	return 0;  /* for now */
}
EXPORT_SYMBOL(drm_atomic_helper_commit);

/**
 * drm_atomic_helper_end - conclude the atomic update
 * @dev: DRM device
 * @state: the driver private state object
 *
 * Release resources associated with the state object.
 */
void drm_atomic_helper_end(struct drm_device *dev, void *state)
{
	drm_atomic_helper_state_unreference(state);
}
EXPORT_SYMBOL(drm_atomic_helper_end);

void _drm_atomic_helper_state_free(struct kref *kref)
{
	struct drm_atomic_helper_state *state =
		container_of(kref, struct drm_atomic_helper_state, refcount);
	kfree(state);
}
EXPORT_SYMBOL(_drm_atomic_helper_state_free);


const struct drm_atomic_helper_funcs drm_atomic_helper_funcs = {
};
EXPORT_SYMBOL(drm_atomic_helper_funcs);

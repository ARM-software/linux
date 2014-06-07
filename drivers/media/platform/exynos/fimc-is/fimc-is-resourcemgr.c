#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "fimc-is-resourcemgr.h"
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"

struct pm_qos_request exynos_isp_qos_int;
struct pm_qos_request exynos_isp_qos_mem;
struct pm_qos_request exynos_isp_qos_cam;
struct pm_qos_request exynos_isp_qos_disp;

int fimc_is_resource_probe(struct fimc_is_resourcemgr *resourcemgr,
	void *private_data)
{
	int ret = 0;

	BUG_ON(!resourcemgr);
	BUG_ON(!private_data);

	resourcemgr->private_data = private_data;

	atomic_set(&resourcemgr->rsccount, 0);
	atomic_set(&resourcemgr->rsccount_sensor, 0);
	atomic_set(&resourcemgr->rsccount_ischain, 0);

#ifdef ENABLE_DVFS
	/* dvfs controller init */
	ret = fimc_is_dvfs_init(resourcemgr);
	if (ret)
		err("%s: fimc_is_dvfs_init failed!\n", __func__);
#endif

	info("%s\n", __func__);
	return ret;
}

int fimc_is_resource_get(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
	struct fimc_is_core *core;

	BUG_ON(!resourcemgr);

	core = (struct fimc_is_core *)resourcemgr->private_data;

	info("[RSC] %s: rsccount = %d\n", __func__, atomic_read(&core->rsccount));

	if (!atomic_read(&core->rsccount)) {
		core->debug_cnt = 0;

		/* 1. interface open */
		fimc_is_interface_open(&core->interface);

#ifdef ENABLE_DVFS
		/* dvfs controller init */
		ret = fimc_is_dvfs_init(resourcemgr);
		if (ret)
			err("%s: fimc_is_dvfs_init failed!\n", __func__);
#endif
	}

	atomic_inc(&core->rsccount);

	if (atomic_read(&core->rsccount) > 5) {
		err("[RSC] %s: Invalid rsccount(%d)\n", __func__,
			atomic_read(&core->rsccount));
		ret = -EMFILE;
	}

	return ret;
}

int fimc_is_resource_put(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
	struct fimc_is_core *core;

	BUG_ON(!resourcemgr);

	core = (struct fimc_is_core *)resourcemgr->private_data;

	if ((atomic_read(&core->rsccount) == 0) ||
		(atomic_read(&core->rsccount) > 5)) {
		err("[RSC] %s: Invalid rsccount(%d)\n", __func__,
			atomic_read(&core->rsccount));
		ret = -EMFILE;

		goto exit;
	}

	atomic_dec(&core->rsccount);

	 pr_info("[RSC] %s: rsccount = %d\n",
               __func__, atomic_read(&core->rsccount));

	if (!atomic_read(&core->rsccount)) {
		if (test_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state)) {
			/* 1. Stop a5 and other devices operation */
			ret = fimc_is_itf_power_down(&core->interface);
			if (ret)
				err("power down is failed, retry forcelly");

			/* 2. Power down */
			ret = fimc_is_ischain_power(&core->ischain[0], 0);
			if (ret)
				err("fimc_is_ischain_power is failed");
		}

		/* 3. Deinit variables */
		ret = fimc_is_interface_close(&core->interface);
		if (ret)
			err("fimc_is_interface_close is failed");

#ifndef RESERVED_MEM
		/* 5. Dealloc memroy */
		fimc_is_ishcain_deinitmem(&core->ischain[0]);
#endif
	}

exit:
	return ret;
}

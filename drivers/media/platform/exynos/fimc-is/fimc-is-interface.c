/*
 * drivers/media/video/exynos/fimc-is-mc2/fimc-is-interface.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * The header file related to camera
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/workqueue.h>
#include <linux/bug.h>

#include "fimc-is-core.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-groupmgr.h"

#include "fimc-is-interface.h"
#include "fimc-is-clk-gate.h"

u32 __iomem *notify_fcount_sen0;
u32 __iomem *notify_fcount_sen1;
u32 __iomem *notify_fcount_sen2;
u32 __iomem *last_fcount0;
u32 __iomem *last_fcount1;

#define init_request_barrier(itf) mutex_init(&itf->request_barrier)
#define enter_request_barrier(itf) mutex_lock(&itf->request_barrier);
#define exit_request_barrier(itf) mutex_unlock(&itf->request_barrier);
#define init_process_barrier(itf) spin_lock_init(&itf->process_barrier);
#define enter_process_barrier(itf) spin_lock_irq(&itf->process_barrier);
#define exit_process_barrier(itf) spin_unlock_irq(&itf->process_barrier);

extern struct fimc_is_sysfs_debug sysfs_debug;

int print_fre_work_list(struct fimc_is_work_list *this)
{
	struct list_head *temp;
	struct fimc_is_work *work;

	if (!(this->id & TRACE_WORK_ID_MASK))
		return 0;

	printk(KERN_ERR "[INF] fre(%02X, %02d) :",
		this->id, this->work_free_cnt);

	list_for_each(temp, &this->work_free_head) {
		work = list_entry(temp, struct fimc_is_work, list);
		printk(KERN_CONT "%X(%d)->", work->msg.command, work->fcount);
	}

	printk(KERN_CONT "X\n");

	return 0;
}

static int set_free_work(struct fimc_is_work_list *this,
	struct fimc_is_work *work)
{
	int ret = 0;
	unsigned long flags;

	if (work) {
		spin_lock_irqsave(&this->slock_free, flags);

		list_add_tail(&work->list, &this->work_free_head);
		this->work_free_cnt++;
#ifdef TRACE_WORK
		print_fre_work_list(this);
#endif

		spin_unlock_irqrestore(&this->slock_free, flags);
	} else {
		ret = -EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}

static int get_free_work(struct fimc_is_work_list *this,
	struct fimc_is_work **work)
{
	int ret = 0;
	unsigned long flags;

	if (work) {
		spin_lock_irqsave(&this->slock_free, flags);

		if (this->work_free_cnt) {
			*work = container_of(this->work_free_head.next,
					struct fimc_is_work, list);
			list_del(&(*work)->list);
			this->work_free_cnt--;
		} else
			*work = NULL;

		spin_unlock_irqrestore(&this->slock_free, flags);
	} else {
		ret = -EFAULT;
		err("item is null ptr");
	}

	return ret;
}

static int get_free_work_irq(struct fimc_is_work_list *this,
	struct fimc_is_work **work)
{
	int ret = 0;

	if (work) {
		spin_lock(&this->slock_free);

		if (this->work_free_cnt) {
			*work = container_of(this->work_free_head.next,
					struct fimc_is_work, list);
			list_del(&(*work)->list);
			this->work_free_cnt--;
		} else
			*work = NULL;

		spin_unlock(&this->slock_free);
	} else {
		ret = -EFAULT;
		err("item is null ptr");
	}

	return ret;
}

int print_req_work_list(struct fimc_is_work_list *this)
{
	struct list_head *temp;
	struct fimc_is_work *work;

	if (!(this->id & TRACE_WORK_ID_MASK))
		return 0;

	printk(KERN_ERR "[INF] req(%02X, %02d) :",
		this->id, this->work_request_cnt);

	list_for_each(temp, &this->work_request_head) {
		work = list_entry(temp, struct fimc_is_work, list);
		printk(KERN_CONT "%X(%d)->", work->msg.command, work->fcount);
	}

	printk(KERN_CONT "X\n");

	return 0;
}

static int set_req_work(struct fimc_is_work_list *this,
	struct fimc_is_work *work)
{
	int ret = 0;
	unsigned long flags;

	if (work) {
		spin_lock_irqsave(&this->slock_request, flags);

		list_add_tail(&work->list, &this->work_request_head);
		this->work_request_cnt++;
#ifdef TRACE_WORK
		print_req_work_list(this);
#endif

		spin_unlock_irqrestore(&this->slock_request, flags);
	} else {
		ret = -EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}

static int set_req_work_irq(struct fimc_is_work_list *this,
	struct fimc_is_work *work)
{
	int ret = 0;

	if (work) {
		spin_lock(&this->slock_request);

		list_add_tail(&work->list, &this->work_request_head);
		this->work_request_cnt++;
#ifdef TRACE_WORK
		print_req_work_list(this);
#endif

		spin_unlock(&this->slock_request);
	} else {
		ret = -EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}

static int get_req_work(struct fimc_is_work_list *this,
	struct fimc_is_work **work)
{
	int ret = 0;
	unsigned long flags;

	if (work) {
		spin_lock_irqsave(&this->slock_request, flags);

		if (this->work_request_cnt) {
			*work = container_of(this->work_request_head.next,
					struct fimc_is_work, list);
			list_del(&(*work)->list);
			this->work_request_cnt--;
		} else
			*work = NULL;

		spin_unlock_irqrestore(&this->slock_request, flags);
	} else {
		ret = -EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}

static void init_work_list(struct fimc_is_work_list *this, u32 id, u32 count)
{
	u32 i;

	this->id = id;
	this->work_free_cnt	= 0;
	this->work_request_cnt	= 0;
	INIT_LIST_HEAD(&this->work_free_head);
	INIT_LIST_HEAD(&this->work_request_head);
	spin_lock_init(&this->slock_free);
	spin_lock_init(&this->slock_request);
	for (i = 0; i < count; ++i)
		set_free_work(this, &this->work[i]);

	init_waitqueue_head(&this->wait_queue);
}

static int set_busystate(struct fimc_is_interface *this,
	u32 command)
{
	int ret;

	ret = test_and_set_bit(IS_IF_STATE_BUSY, &this->state);
	if (ret)
		warn("%d command : busy state is already set", command);

	return ret;
}

static int clr_busystate(struct fimc_is_interface *this,
	u32 command)
{
	int ret;

	ret = test_and_clear_bit(IS_IF_STATE_BUSY, &this->state);
	if (!ret)
		warn("%d command : busy state is already clr", command);

	return !ret;
}

static int test_busystate(struct fimc_is_interface *this)
{
	int ret = 0;

	ret = test_bit(IS_IF_STATE_BUSY, &this->state);

	return ret;
}

static int wait_lockstate(struct fimc_is_interface *this)
{
	int ret = 0;

	ret = wait_event_timeout(this->lock_wait_queue,
		!atomic_read(&this->lock_pid), FIMC_IS_COMMAND_TIMEOUT);
	if (ret) {
		ret = 0;
	} else {
		err("timeout");
		ret = -ETIME;
	}

	return ret;
}

static int wait_idlestate(struct fimc_is_interface *this)
{
	int ret = 0;

	ret = wait_event_timeout(this->idle_wait_queue,
		!test_busystate(this), FIMC_IS_COMMAND_TIMEOUT);
	if (ret) {
		ret = 0;
	} else {
		err("timeout");
		ret = -ETIME;
	}

	return ret;
}

static int wait_initstate(struct fimc_is_interface *this)
{
	int ret = 0;

	ret = wait_event_timeout(this->init_wait_queue,
		test_bit(IS_IF_STATE_START, &this->state),
		FIMC_IS_STARTUP_TIMEOUT);
	if (ret) {
		ret = 0;
	} else {
		err("timeout");
		ret = -ETIME;
	}

	return ret;
}

static void testnclr_wakeup(struct fimc_is_interface *this,
	u32 command)
{
	int ret = 0;

	ret = clr_busystate(this, command);
	if (ret)
		err("current state is invalid(%ld)", this->state);

	wake_up(&this->idle_wait_queue);
}

static int waiting_is_ready(struct fimc_is_interface *interface)
{
	int ret = 0;
	u32 try_count = TRY_RECV_AWARE_COUNT;
	u32 cfg = readl(interface->regs + INTMSR0);
	u32 status = INTMSR0_GET_INTMSD0(cfg);

	while (status) {
		cfg = readl(interface->regs + INTMSR0);
		status = INTMSR0_GET_INTMSD0(cfg);
		udelay(100);
		dbg("Retry to read INTMSR0(%d)\n", try_count);

		if (--try_count == 0) {
			err("INTMSR0's 0 bit is not cleared.");
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

static void send_interrupt(struct fimc_is_interface *interface)
{
	writel(INTGR0_INTGD0, interface->regs + INTGR0);
}

static int fimc_is_set_cmd(struct fimc_is_interface *itf,
	struct fimc_is_msg *msg,
	struct fimc_is_msg *reply)
{
	int ret = 0;
	int wait_lock = 0;
	u32 lock_pid = 0;
	u32 id;
	volatile struct is_common_reg __iomem *com_regs;
#ifdef MEASURE_TIME
#ifdef INTERFACE_TIME
	struct timeval measure_str, measure_end;
#endif
#endif

	BUG_ON(!itf);
	BUG_ON(!msg);
	BUG_ON(msg->instance >= FIMC_IS_MAX_NODES);
	BUG_ON(msg->command >= HIC_COMMAND_END);
	BUG_ON(!reply);

	if (!test_bit(IS_IF_STATE_OPEN, &itf->state)) {
		warn("interface close, %d cmd is cancel", msg->command);
		goto exit;
	}

	lock_pid = atomic_read(&itf->lock_pid);
	if (lock_pid && (lock_pid != current->pid)) {
		pr_info("itf LOCK, %d(%d) wait\n", current->pid, msg->command);
		wait_lock = wait_lockstate(itf);
		pr_info("itf UNLOCK, %d(%d) go\n", current->pid, msg->command);
		if (wait_lock) {
			err("wait_lockstate is fail, lock reset");
			atomic_set(&itf->lock_pid, 0);
		}
	}

	dbg_interface("TP#1\n");
	enter_request_barrier(itf);
	dbg_interface("TP#2\n");

#ifdef MEASURE_TIME
#ifdef INTERFACE_TIME
	do_gettimeofday(&measure_str);
#endif
#endif

	switch (msg->command) {
	case HIC_STREAM_ON:
		if (itf->streaming[msg->instance] == IS_IF_STREAMING_ON)
			warn("already stream on");
		break;
	case HIC_STREAM_OFF:
		if (itf->streaming[msg->instance] == IS_IF_STREAMING_OFF)
			warn("already stream off");
		break;
	case HIC_PROCESS_START:
		if (itf->processing[msg->instance] == IS_IF_PROCESSING_ON)
			warn("already process on");
		break;
	case HIC_PROCESS_STOP:
		if (itf->processing[msg->instance] == IS_IF_PROCESSING_OFF)
			warn("already process off");
		break;
	case HIC_POWER_DOWN:
		if (itf->pdown_ready == IS_IF_POWER_DOWN_READY)
			warn("already powerdown ready");
		break;
	default:
		if (itf->pdown_ready == IS_IF_POWER_DOWN_READY) {
			exit_request_barrier(itf);
			warn("already powerdown ready, %d cmd is cancel",
				msg->command);
			goto exit;
		}
		break;
	}

	enter_process_barrier(itf);

	ret = waiting_is_ready(itf);
	if (ret) {
		exit_request_barrier(itf);
		exit_process_barrier(itf);
		err("waiting for ready is fail");
		ret = -EBUSY;
		goto exit;
	}

	set_busystate(itf, msg->command);
	com_regs = itf->com_regs;
	id = (msg->group << GROUP_ID_SHIFT) | msg->instance;
	writel(msg->command, &com_regs->hicmd);
	writel(id, &com_regs->hic_sensorid);
	writel(msg->parameter1, &com_regs->hic_param1);
	writel(msg->parameter2, &com_regs->hic_param2);
	writel(msg->parameter3, &com_regs->hic_param3);
	writel(msg->parameter4, &com_regs->hic_param4);
	send_interrupt(itf);

	exit_process_barrier(itf);

	ret = wait_idlestate(itf);
	if (ret) {
		exit_request_barrier(itf);
		err("%d command is timeout", msg->command);
		clr_busystate(itf, msg->command);
		ret = -ETIME;
		goto exit;
	}

	reply->command = itf->reply.command;
	reply->group = itf->reply.group;
	reply->instance = itf->reply.instance;
	reply->parameter1 = itf->reply.parameter1;
	reply->parameter2 = itf->reply.parameter2;
	reply->parameter3 = itf->reply.parameter3;
	reply->parameter4 = itf->reply.parameter4;

	if (reply->command == ISR_DONE) {
		if (msg->command != reply->parameter1) {
			exit_request_barrier(itf);
			err("invalid command reply(%d != %d)", msg->command, reply->parameter1);
			ret = -EINVAL;
			goto exit;
		}

		switch (msg->command) {
		case HIC_STREAM_ON:
			itf->streaming[msg->instance] = IS_IF_STREAMING_ON;
			break;
		case HIC_STREAM_OFF:
			itf->streaming[msg->instance] = IS_IF_STREAMING_OFF;
			break;
		case HIC_PROCESS_START:
			itf->processing[msg->instance] = IS_IF_PROCESSING_ON;
			break;
		case HIC_PROCESS_STOP:
			itf->processing[msg->instance] = IS_IF_PROCESSING_OFF;
			break;
		case HIC_POWER_DOWN:
			itf->pdown_ready = IS_IF_POWER_DOWN_READY;
			break;
		case HIC_OPEN_SENSOR:
			if (reply->parameter1 == HIC_POWER_DOWN) {
				err("firmware power down");
				itf->pdown_ready = IS_IF_POWER_DOWN_READY;
				ret = -ECANCELED;
			} else {
				itf->pdown_ready = IS_IF_POWER_DOWN_NREADY;
			}
			break;
		default:
			break;
		}
	} else {
		err("ISR_NDONE is occured");
		ret = -EINVAL;
	}

#ifdef MEASURE_TIME
#ifdef INTERFACE_TIME
	do_gettimeofday(&measure_end);
	measure_time(&itf->time[msg->command],
		msg->instance,
		msg->group,
		&measure_str,
		&measure_end);
#endif
#endif

	exit_request_barrier(itf);

exit:
	if (ret)
		fimc_is_hw_logdump(itf);

	return ret;
}

static int fimc_is_set_cmd_shot(struct fimc_is_interface *this,
	struct fimc_is_msg *msg)
{
	int ret = 0;
	u32 id;
	volatile struct is_common_reg __iomem *com_regs;

	BUG_ON(!this);
	BUG_ON(!msg);
	BUG_ON(msg->instance >= FIMC_IS_MAX_NODES);

	if (!test_bit(IS_IF_STATE_OPEN, &this->state)) {
		warn("interface close, %d cmd is cancel", msg->command);
		goto exit;
	}

	enter_process_barrier(this);

	ret = waiting_is_ready(this);
	if (ret) {
		exit_process_barrier(this);
		err("waiting for ready is fail");
		ret = -EBUSY;
		goto exit;
	}

	spin_lock_irq(&this->shot_check_lock);
	atomic_set(&this->shot_check[msg->instance], 1);
	spin_unlock_irq(&this->shot_check_lock);

	com_regs = this->com_regs;
	id = (msg->group << GROUP_ID_SHIFT) | msg->instance;
	writel(msg->command, &com_regs->hicmd);
	writel(id, &com_regs->hic_sensorid);
	writel(msg->parameter1, &com_regs->hic_param1);
	writel(msg->parameter2, &com_regs->hic_param2);
	writel(msg->parameter3, &com_regs->hic_param3);
	writel(msg->parameter4, &com_regs->hic_param4);
	send_interrupt(this);

	exit_process_barrier(this);

exit:
	return ret;
}

static int fimc_is_set_cmd_nblk(struct fimc_is_interface *this,
	struct fimc_is_work *work)
{
	int ret = 0;
	u32 id;
	struct fimc_is_msg *msg;
	volatile struct is_common_reg __iomem *com_regs;

	msg = &work->msg;
	switch (msg->command) {
	case HIC_SET_CAM_CONTROL:
		set_req_work(&this->nblk_cam_ctrl, work);
		break;
	default:
		err("unresolved command\n");
		break;
	}

	enter_process_barrier(this);

	ret = waiting_is_ready(this);
	if (ret) {
		err("waiting for ready is fail");
		ret = -EBUSY;
		goto exit;
	}

	com_regs = this->com_regs;
	id = (msg->group << GROUP_ID_SHIFT) | msg->instance;
	writel(msg->command, &com_regs->hicmd);
	writel(id, &com_regs->hic_sensorid);
	writel(msg->parameter1, &com_regs->hic_param1);
	writel(msg->parameter2, &com_regs->hic_param2);
	writel(msg->parameter3, &com_regs->hic_param3);
	writel(msg->parameter4, &com_regs->hic_param4);
	send_interrupt(this);

exit:
	exit_process_barrier(this);
	return ret;
}

static inline void fimc_is_get_cmd(struct fimc_is_interface *itf,
	struct fimc_is_msg *msg, u32 index)
{
	volatile struct is_common_reg __iomem *com_regs = itf->com_regs;

	switch (index) {
	case INTR_GENERAL:
		msg->id = 0;
		msg->command = readl(&com_regs->ihcmd);
		msg->instance = readl(&com_regs->ihc_sensorid);
		msg->parameter1 = readl(&com_regs->ihc_param1);
		msg->parameter2 = readl(&com_regs->ihc_param2);
		msg->parameter3 = readl(&com_regs->ihc_param3);
		msg->parameter4 = readl(&com_regs->ihc_param4);
		break;
	case INTR_3A0C_FDONE:
		msg->id = 0;
		msg->command = IHC_FRAME_DONE;
		msg->instance = readl(&com_regs->taa0c_sensor_id);
		msg->parameter1 = readl(&com_regs->taa0c_param1);
		msg->parameter2 = readl(&com_regs->taa0c_param2);
		msg->parameter3 = readl(&com_regs->taa0c_param3);
		msg->parameter4 = 0;
		break;
	case INTR_3A1C_FDONE:
		msg->id = 0;
		msg->command = IHC_FRAME_DONE;
		msg->instance = readl(&com_regs->taa1c_sensor_id);
		msg->parameter1 = readl(&com_regs->taa1c_param1);
		msg->parameter2 = readl(&com_regs->taa1c_param2);
		msg->parameter3 = readl(&com_regs->taa1c_param3);
		msg->parameter4 = 0;
		break;
	case INTR_SCC_FDONE:
		msg->id = 0;
		msg->command = IHC_FRAME_DONE;
		msg->instance = readl(&com_regs->scc_sensor_id);
		msg->parameter1 = readl(&com_regs->scc_param1);
		msg->parameter2 = readl(&com_regs->scc_param2);
		msg->parameter3 = readl(&com_regs->scc_param3);
		msg->parameter4 = 0;
		break;
	case INTR_DIS_FDONE:
		msg->id = 0;
		msg->command = IHC_FRAME_DONE;
		msg->instance = readl(&com_regs->dis_sensor_id);
		msg->parameter1 = readl(&com_regs->dis_param1);
		msg->parameter2 = readl(&com_regs->dis_param2);
		msg->parameter3 = readl(&com_regs->dis_param3);
		msg->parameter4 = 0;
		break;
	case INTR_SCP_FDONE:
		msg->id = 0;
		msg->command = IHC_FRAME_DONE;
		msg->instance = readl(&com_regs->scp_sensor_id);
		msg->parameter1 = readl(&com_regs->scp_param1);
		msg->parameter2 = readl(&com_regs->scp_param2);
		msg->parameter3 = readl(&com_regs->scp_param3);
		msg->parameter4 = 0;
		break;
	case INTR_SHOT_DONE:
		msg->id = 0;
		msg->command = IHC_FRAME_DONE;
		msg->instance = readl(&com_regs->shot_sensor_id);
		msg->parameter1 = readl(&com_regs->shot_param1);
		msg->parameter2 = readl(&com_regs->shot_param2);
		msg->parameter3 = readl(&com_regs->shot_param3);
		msg->parameter4 = 0;
		break;
	default:
		msg->id = 0;
		msg->command = 0;
		msg->instance = 0;
		msg->parameter1 = 0;
		msg->parameter2 = 0;
		msg->parameter3 = 0;
		msg->parameter4 = 0;
		err("unknown command getting\n");
		break;
	}

	msg->group = msg->instance >> GROUP_ID_SHIFT;
	msg->instance = msg->instance & GROUP_ID_MASK;
}

static inline u32 fimc_is_get_intr(struct fimc_is_interface *itf)
{
	u32 status = 0;
	volatile struct is_common_reg __iomem *com_regs = itf->com_regs;

	if (itf->need_iflag) {
		status = readl(&com_regs->ihcmd_iflag) |
			 readl(&com_regs->taa0c_iflag) |
			 readl(&com_regs->taa1c_iflag) |
			 readl(&com_regs->scc_iflag) |
			 readl(&com_regs->dis_iflag) |
			 readl(&com_regs->scp_iflag) |
			 readl(&com_regs->shot_iflag);
	}

	status |= readl(itf->regs + INTMSR1);

	return status;
}

static inline void fimc_is_clr_intr(struct fimc_is_interface *itf,
	u32 index)
{
	volatile struct is_common_reg __iomem *com_regs = itf->com_regs;

	writel((1 << index), itf->regs + INTCR1);

	if (itf->need_iflag) {
		switch (index) {
		case INTR_GENERAL:
			writel(0, &com_regs->ihcmd_iflag);
			break;
		case INTR_3A0C_FDONE:
			writel(0, &com_regs->taa0c_iflag);
			break;
		case INTR_3A1C_FDONE:
			writel(0, &com_regs->taa1c_iflag);
			break;
		case INTR_SCC_FDONE:
			writel(0, &com_regs->scc_iflag);
			break;
		case INTR_DIS_FDONE:
			writel(0, &com_regs->dis_iflag);
			break;
		case INTR_SCP_FDONE:
			writel(0, &com_regs->scp_iflag);
			break;
		case INTR_SHOT_DONE:
			writel(0, &com_regs->shot_iflag);
			break;
		default:
			err("unknown command clear\n");
			break;
		}
	}
}

static void wq_func_general(struct work_struct *data)
{
	struct fimc_is_interface *itf;
	struct fimc_is_msg *msg;
	struct fimc_is_work *work;
	struct fimc_is_work *nblk_work;

	itf = container_of(data, struct fimc_is_interface,
		work_wq[INTR_GENERAL]);

	get_req_work(&itf->work_list[INTR_GENERAL], &work);
	while (work) {
		msg = &work->msg;
		switch (msg->command) {
		case IHC_GET_SENSOR_NUMBER:
			info("IS Version: %d.%d [0x%02x]\n",
				ISDRV_VERSION, msg->parameter1,
				get_drv_clock_gate() |
				get_drv_dvfs());
			set_bit(IS_IF_STATE_START, &itf->state);
			itf->pdown_ready = IS_IF_POWER_DOWN_NREADY;
			wake_up(&itf->init_wait_queue);
			break;
		case ISR_DONE:
			switch (msg->parameter1) {
			case HIC_OPEN_SENSOR:
				dbg_interface("open done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_CLOSE_SENSOR:
				dbg_interface("close done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_GET_SET_FILE_ADDR:
				dbg_interface("saddr(%p) done\n",
					(void *)msg->parameter2);
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_LOAD_SET_FILE:
				dbg_interface("setfile done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_SET_A5_MAP:
				dbg_interface("mapping done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_SET_A5_UNMAP:
				dbg_interface("unmapping done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_PROCESS_START:
				dbg_interface("process_on done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_PROCESS_STOP:
				dbg_interface("process_off done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_STREAM_ON:
				dbg_interface("stream_on done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_STREAM_OFF:
				dbg_interface("stream_off done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_SET_PARAMETER:
				dbg_interface("s_param done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_GET_STATIC_METADATA:
				dbg_interface("g_capability done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_PREVIEW_STILL:
				dbg_interface("a_param(%dx%d) done\n",
					msg->parameter2,
					msg->parameter3);
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_POWER_DOWN:
				dbg_interface("powerdown done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			case HIC_I2C_CONTROL_LOCK:
				dbg_interface("i2c lock done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
#if (FW_HAS_SYS_CTRL_CMD)
			case HIC_SYSTEM_CONTROL:
				dbg_interface("system control done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
#endif
#if (FW_HAS_SENSOR_MODE_CMD)
			case HIC_SENSOR_MODE_CHANGE:
				dbg_interface("sensor mode change done\n");
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
#endif
			/*non-blocking command*/
			case HIC_SHOT:
				err("shot done is not acceptable\n");
				break;
			case HIC_SET_CAM_CONTROL:
				/* this code will be used latter */
#if 0
				dbg_interface("camctrl done\n");
				get_req_work(&itf->nblk_cam_ctrl , &nblk_work);
				if (nblk_work) {
					nblk_work->msg.command = ISR_DONE;
					set_free_work(&itf->nblk_cam_ctrl,
						nblk_work);
				} else {
					err("nblk camctrl request is empty");
					print_fre_work_list(
						&itf->nblk_cam_ctrl);
					print_req_work_list(
						&itf->nblk_cam_ctrl);
				}
#else
				err("camctrl is not acceptable\n");
#endif
				break;
			default:
				err("unknown done is invokded\n");
				break;
			}
			break;
		case ISR_NDONE:
			switch (msg->parameter1) {
			case HIC_SHOT:
				err("[ITF:%d] shot NDONE is not acceptable",
					msg->instance);
				break;
			case HIC_SET_CAM_CONTROL:
				dbg_interface("camctrl NOT done\n");
				get_req_work(&itf->nblk_cam_ctrl , &nblk_work);
				nblk_work->msg.command = ISR_NDONE;
				set_free_work(&itf->nblk_cam_ctrl, nblk_work);
				break;
			case HIC_SET_PARAMETER:
				err("s_param NOT done");
				err("param2 : 0x%08X", msg->parameter2);
				err("param3 : 0x%08X", msg->parameter3);
				err("param4 : 0x%08X", msg->parameter4);
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			default:
				err("a command(%d) not done", msg->parameter1);
				memcpy(&itf->reply, msg,
					sizeof(struct fimc_is_msg));
				testnclr_wakeup(itf, msg->parameter1);
				break;
			}
			break;
		case IHC_SET_FACE_MARK:
			err("FACE_MARK(%d,%d,%d) is not acceptable\n",
				msg->parameter1,
				msg->parameter2,
				msg->parameter3);
			break;
		case IHC_AA_DONE:
			err("AA_DONE(%d,%d,%d) is not acceptable\n",
				msg->parameter1,
				msg->parameter2,
				msg->parameter3);
			break;
		case IHC_FLASH_READY:
			err("IHC_FLASH_READY is not acceptable");
			break;
		case IHC_NOT_READY:
			err("IHC_NOT_READY is occured, need reset");
			fimc_is_hw_logdump(itf);
			break;
#if (FW_HAS_REPORT_ERR_CMD)
		case IHC_REPORT_ERR:
			err("IHC_REPORT_ERR is occured");
			fimc_is_hw_logdump(itf);
			break;
#endif
		default:
			err("func_general unknown(0x%08X) end\n", msg->command);
			break;
		}

		set_free_work(&itf->work_list[INTR_GENERAL], work);
		get_req_work(&itf->work_list[INTR_GENERAL], &work);
	}
}

static void wq_func_subdev(struct fimc_is_subdev *leader,
	struct fimc_is_subdev *subdev,
	u32 fcount, u32 rcount, u32 status, u32 instance)
{
	u32 findex, out_flag;
	u32 capture_vid, capture_id;
	char name;
	unsigned long flags;
	struct fimc_is_video_ctx *ldr_vctx, *sub_vctx;
	struct fimc_is_framemgr *ldr_framemgr, *sub_framemgr;
	struct fimc_is_frame *ldr_frame, *sub_frame;
	struct camera2_node *capture;

	BUG_ON(!leader);
	BUG_ON(!subdev);

	ldr_vctx = leader->vctx;
	if (!ldr_vctx) {
		err("ldr_vctx is NULL");
		return;
	}

	sub_vctx = subdev->vctx;
	if (!sub_vctx) {
		err("sub_vctx is NULL");
		return;
	}

	if (!sub_vctx->video) {
		err("video is NULL");
		return;
	}

	ldr_framemgr = GET_SRC_FRAMEMGR(ldr_vctx);
	sub_framemgr = GET_DST_FRAMEMGR(sub_vctx);

	capture_vid = sub_vctx->video->id;
	switch (capture_vid) {
	case FIMC_IS_VIDEO_SCC_NUM:
		out_flag = OUT_SCC_FRAME;
		name = 'C';
		break;
	case FIMC_IS_VIDEO_VDC_NUM:
		out_flag = OUT_DIS_FRAME;
		name = 'D';
		break;
	case FIMC_IS_VIDEO_SCP_NUM:
		out_flag = OUT_SCP_FRAME;
		name = 'P';
		break;
	case FIMC_IS_VIDEO_3A0C_NUM:
		out_flag = OUT_3AAC_FRAME;
		name = '0';
		break;
	case FIMC_IS_VIDEO_3A1C_NUM:
		out_flag = OUT_3AAC_FRAME;
		name = '1';
		break;
	default:
		err("video node is invalid(%d)", sub_vctx->video->id);
		return;
	}

	framemgr_e_barrier_irqs(sub_framemgr, 0, flags);

	fimc_is_frame_process_head(sub_framemgr, &sub_frame);
	if (sub_frame && test_bit(REQ_FRAME, &sub_frame->req_flag)) {

#ifdef DBG_STREAMING
		info("[%c:D:%d] %d(%d,%d)\n", name, instance,
			sub_frame->index, fcount, rcount);
#endif

		if (!sub_frame->stream) {
			err("stream is NULL, critical error");
			goto p_err;
		}

		findex = sub_frame->stream->findex;
		if (findex >= ldr_vctx->q_src.buf_maxcount) {
			err("findex(%d) is invalid(max : %d)",
				findex, ldr_vctx->q_src.buf_maxcount);
			sub_frame->stream->fvalid = 0;
			goto done;
		}

		if (status) {
			info("[%c:D:%d] FRM%d NOT DONE(%d)\n", name,
				instance, fcount, status);
			sub_frame->stream->fvalid = 0;
			goto done;
		}

		ldr_frame = &ldr_framemgr->frame[findex];
		if (ldr_frame->fcount != fcount) {
			err("%c frame mismatched(ldr%d, sub%d)", name,
				ldr_frame->fcount, fcount);
			sub_frame->stream->fvalid = 0;
		} else {
			sub_frame->stream->fvalid = 1;
			clear_bit(out_flag, &ldr_frame->out_flag);
		}

		for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
			capture = &ldr_frame->shot_ext->node_group.capture[capture_id];
			if (capture_vid == capture->vid) {
				sub_frame->stream->output_crop_region[0] = capture->output.cropRegion[0];
				sub_frame->stream->output_crop_region[1] = capture->output.cropRegion[1];
				sub_frame->stream->output_crop_region[2] = capture->output.cropRegion[2];
				sub_frame->stream->output_crop_region[3] = capture->output.cropRegion[3];

				sub_frame->stream->input_crop_region[0] = capture->input.cropRegion[0];
				sub_frame->stream->input_crop_region[1] = capture->input.cropRegion[1];
				sub_frame->stream->input_crop_region[2] = capture->input.cropRegion[2];
				sub_frame->stream->input_crop_region[3] = capture->input.cropRegion[3];
				break;
			}
		}

		if (capture_id >= CAPTURE_NODE_MAX) {
			err("can't find capture stream(%d > %d)", capture_id, CAPTURE_NODE_MAX);
			sub_frame->stream->output_crop_region[0] = 0;
			sub_frame->stream->output_crop_region[1] = 0;
			sub_frame->stream->output_crop_region[2] = 0;
			sub_frame->stream->output_crop_region[3] = 0;
		}

done:
		clear_bit(REQ_FRAME, &sub_frame->req_flag);
		sub_frame->stream->fcount = fcount;
		sub_frame->stream->rcount = rcount;

		fimc_is_frame_trans_pro_to_com(sub_framemgr, sub_frame);
		buffer_done(sub_vctx, sub_frame->index);
	} else
		err("done(%p) is occured without request", sub_frame);

p_err:
	framemgr_x_barrier_irqr(sub_framemgr, 0, flags);
}

static void wq_func_3a0c(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface,
		work_wq[INTR_3A0C_FDONE]);

	get_req_work(&itf->work_list[INTR_3A0C_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->parameter1;
		status = msg->parameter2;
		rcount = msg->parameter3;

		if (instance >= FIMC_IS_MAX_NODES) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		subdev = &device->taac;
		leader = subdev->leader;

		wq_func_subdev(leader, subdev, fcount, rcount, status, instance);

p_err:
		set_free_work(&itf->work_list[INTR_3A0C_FDONE], work);
		get_req_work(&itf->work_list[INTR_3A0C_FDONE], &work);
	}
}

static void wq_func_3a1c(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface,
		work_wq[INTR_3A1C_FDONE]);

	get_req_work(&itf->work_list[INTR_3A1C_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->parameter1;
		status = msg->parameter2;
		rcount = msg->parameter3;

		if (instance >= FIMC_IS_MAX_NODES) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		subdev = &device->taac;
		leader = subdev->leader;

		wq_func_subdev(leader, subdev, fcount, rcount, status, instance);

p_err:
		set_free_work(&itf->work_list[INTR_3A1C_FDONE], work);
		get_req_work(&itf->work_list[INTR_3A1C_FDONE], &work);
	}
}

static void wq_func_scc(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface,
		work_wq[INTR_SCC_FDONE]);

	get_req_work(&itf->work_list[INTR_SCC_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->parameter1;
		status = msg->parameter2;
		rcount = msg->parameter3;

		if (instance >= FIMC_IS_MAX_NODES) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		subdev = &device->scc;
		leader = subdev->leader;

		wq_func_subdev(leader, subdev, fcount, rcount, status, instance);

p_err:
		set_free_work(&itf->work_list[INTR_SCC_FDONE], work);
		get_req_work(&itf->work_list[INTR_SCC_FDONE], &work);
	}
}

static void wq_func_dis(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface,
		work_wq[INTR_DIS_FDONE]);

	get_req_work(&itf->work_list[INTR_DIS_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->parameter1;
		status = msg->parameter2;
		rcount = msg->parameter3;

		if (instance >= FIMC_IS_MAX_NODES) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		subdev = &device->dis;
		leader = subdev->leader;

		wq_func_subdev(leader, subdev, fcount, rcount, status, instance);

p_err:
		set_free_work(&itf->work_list[INTR_DIS_FDONE], work);
		get_req_work(&itf->work_list[INTR_DIS_FDONE], &work);
	}
}

static void wq_func_scp(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface,
		work_wq[INTR_SCP_FDONE]);

	get_req_work(&itf->work_list[INTR_SCP_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->parameter1;
		status = msg->parameter2;
		rcount = msg->parameter3;

		if (instance >= FIMC_IS_MAX_NODES) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		subdev = &device->scp;
		leader = subdev->leader;

		wq_func_subdev(leader, subdev, fcount, rcount, status, instance);

p_err:
		set_free_work(&itf->work_list[INTR_SCP_FDONE], work);
		get_req_work(&itf->work_list[INTR_SCP_FDONE], &work);
	}
}

static void wq_func_group_3a0(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *ldr_framemgr,
	struct fimc_is_frame *ldr_frame,
	struct fimc_is_framemgr *sub_framemgr,
	struct fimc_is_video_ctx *vctx,
	u32 status)
{
	u32 done_state = VB2_BUF_STATE_DONE;
	unsigned long flags;
	struct fimc_is_queue *src_queue, *dst_queue;
	struct fimc_is_frame *sub_frame;

	BUG_ON(!vctx);
	BUG_ON(!ldr_framemgr);
	BUG_ON(!ldr_frame);
	BUG_ON(!sub_framemgr);

	if (status != ISR_DONE) {
		info("[3A0:D:%d] GRP0 NOT DONE(%d, %d)\n", group->instance,
			ldr_frame->fcount, ldr_frame->index);
		done_state = VB2_BUF_STATE_ERROR;
	}

#ifdef DBG_STREAMING
	if (status == ISR_DONE)
		info("[3A0:D:%d] GRP0 DONE(%d)\n", group->instance,
			ldr_frame->fcount);
#endif

	src_queue = GET_SRC_QUEUE(vctx);
	dst_queue = GET_DST_QUEUE(vctx);

	/* 1. sub frame done */
	framemgr_e_barrier_irqs(sub_framemgr, 0, flags);

	fimc_is_frame_process_head(sub_framemgr, &sub_frame);
	if (sub_frame && test_bit(REQ_FRAME, &sub_frame->req_flag)) {
		clear_bit(REQ_FRAME, &sub_frame->req_flag);

		sub_frame->stream->fvalid = 1;
		sub_frame->stream->fcount = ldr_frame->fcount;
		sub_frame->stream->rcount = ldr_frame->rcount;

		fimc_is_frame_trans_pro_to_com(sub_framemgr, sub_frame);
		queue_done(vctx, dst_queue, sub_frame->index, done_state);
	} else
		err("done is occured without request(%p, %d)", sub_frame, ldr_frame->fcount);

	framemgr_x_barrier_irqr(sub_framemgr, 0, flags);

	/* 2. leader frame done */
	fimc_is_ischain_meta_invalid(ldr_frame);

	fimc_is_frame_trans_pro_to_com(ldr_framemgr, ldr_frame);
	fimc_is_group_done(groupmgr, group, ldr_frame, done_state);
	queue_done(vctx, src_queue, ldr_frame->index, done_state);
}

static void wq_func_group_3a1(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *ldr_framemgr,
	struct fimc_is_frame *ldr_frame,
	struct fimc_is_framemgr *sub_framemgr,
	struct fimc_is_video_ctx *vctx,
	u32 status)
{
	u32 done_state = VB2_BUF_STATE_DONE;
	unsigned long flags;
	struct fimc_is_queue *src_queue, *dst_queue;
	struct fimc_is_frame *sub_frame;

	BUG_ON(!vctx);
	BUG_ON(!ldr_framemgr);
	BUG_ON(!ldr_frame);
	BUG_ON(!sub_framemgr);

	if (status != ISR_DONE) {
		info("[3A1:D:%d] GRP1 NOT DONE(%d, %d)\n", group->instance,
			ldr_frame->fcount, ldr_frame->index);
		done_state = VB2_BUF_STATE_ERROR;
	}

#ifdef DBG_STREAMING
	if (status == ISR_DONE)
		info("[3A1:D:%d] GRP1 DONE(%d)\n", group->instance,
			ldr_frame->fcount);
#endif

	src_queue = GET_SRC_QUEUE(vctx);
	dst_queue = GET_DST_QUEUE(vctx);

	/* 1. sub frame done */
	framemgr_e_barrier_irqs(sub_framemgr, 0, flags);

	fimc_is_frame_process_head(sub_framemgr, &sub_frame);
	if (sub_frame && test_bit(REQ_FRAME, &sub_frame->req_flag)) {
		clear_bit(REQ_FRAME, &sub_frame->req_flag);

		sub_frame->stream->fvalid = 1;
		sub_frame->stream->fcount = ldr_frame->fcount;
		sub_frame->stream->rcount = ldr_frame->rcount;

		fimc_is_frame_trans_pro_to_com(sub_framemgr, sub_frame);
		queue_done(vctx, dst_queue, sub_frame->index, done_state);
	} else
		err("done is occured without request(%p)", sub_frame);

	framemgr_x_barrier_irqr(sub_framemgr, 0, flags);

	/* 2. leader frame done */
	fimc_is_ischain_meta_invalid(ldr_frame);

	fimc_is_frame_trans_pro_to_com(ldr_framemgr, ldr_frame);
	fimc_is_group_done(groupmgr, group, ldr_frame, done_state);
	queue_done(vctx, src_queue, ldr_frame->index, done_state);
}

static void wq_func_group_isp(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 status)
{
	u32 done_state = VB2_BUF_STATE_DONE;
	struct fimc_is_queue *queue;
#ifdef ENABLE_SENSOR_DRIVER
	struct camera2_lens_uctl *isp_lens_uctl;
	struct camera2_lens_uctl *lens_uctl;
	struct camera2_sensor_uctl *isp_sensor_uctl;
	struct camera2_sensor_uctl *sensor_uctl;
	struct camera2_flash_uctl *isp_flash_uctl;
	struct camera2_flash_uctl *flash_uctl;
#endif

	BUG_ON(!framemgr);
	BUG_ON(!frame);

	if (status != ISR_DONE) {
		info("[ISP:D:%d] GRP2 NOT DONE(%d, %d)\n", group->instance,
			frame->fcount, frame->index);
		done_state = VB2_BUF_STATE_ERROR;
	}

#ifdef DBG_STREAMING
	if (status == ISR_DONE)
		info("[ISP:D:%d] GRP2 DONE(%d)\n", group->instance,
			frame->fcount);
#endif

	queue = GET_SRC_QUEUE(vctx);

	/* Cache Invalidation */
	fimc_is_ischain_meta_invalid(frame);

#ifdef ENABLE_SENSOR_DRIVER
	isp_lens_uctl = &itf->isp_peri_ctl.lensUd;
	isp_sensor_uctl = &itf->isp_peri_ctl.sensorUd;
	isp_flash_uctl = &itf->isp_peri_ctl.flashUd;

	if (frame->shot->uctl.uUpdateBitMap & CAM_SENSOR_CMD) {
		sensor_uctl = &frame->shot->uctl.sensorUd;
		isp_sensor_uctl->ctl.exposureTime =
			sensor_uctl->ctl.exposureTime;
		isp_sensor_uctl->ctl.frameDuration =
			sensor_uctl->ctl.frameDuration;
		isp_sensor_uctl->ctl.sensitivity =
			sensor_uctl->ctl.sensitivity;

		frame->shot->uctl.uUpdateBitMap &=
			~CAM_SENSOR_CMD;
	}

	if (frame->shot->uctl.uUpdateBitMap & CAM_LENS_CMD) {
		lens_uctl = &frame->shot->uctl.lensUd;
		isp_lens_uctl->ctl.focusDistance =
			lens_uctl->ctl.focusDistance;
		isp_lens_uctl->maxPos =
			lens_uctl->maxPos;
		isp_lens_uctl->slewRate =
			lens_uctl->slewRate;

		frame->shot->uctl.uUpdateBitMap &=
			~CAM_LENS_CMD;
	}

	if (frame->shot->uctl.uUpdateBitMap & CAM_FLASH_CMD) {
		flash_uctl = &frame->shot->uctl.flashUd;
		isp_flash_uctl->ctl.flashMode =
			flash_uctl->ctl.flashMode;
		isp_flash_uctl->ctl.firingPower =
			flash_uctl->ctl.firingPower;
		isp_flash_uctl->ctl.firingTime =
			flash_uctl->ctl.firingTime;

		frame->shot->uctl.uUpdateBitMap &=
			~CAM_FLASH_CMD;
	}
#endif

	fimc_is_frame_trans_pro_to_com(framemgr, frame);
	fimc_is_group_done(groupmgr, group, frame, done_state);
	queue_done(vctx, queue, frame->index, done_state);
}

static void wq_func_group_dis(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 status)
{
	u32 done_state = VB2_BUF_STATE_DONE;
	struct fimc_is_queue *queue;

	BUG_ON(!framemgr);
	BUG_ON(!frame);

	if (status != ISR_DONE) {
		info("[DIS:D:%d] GRP3 NOT DONE(%d, %d)\n", group->instance,
			frame->fcount, frame->index);
		done_state = VB2_BUF_STATE_ERROR;
	}

#ifdef DBG_STREAMING
	if (status == ISR_DONE)
		info("[DIS:D:%d] GRP3 DONE(%d)\n", group->instance,
			frame->fcount);
#endif

	queue = GET_SRC_QUEUE(vctx);

	/* Cache Invalidation */
	fimc_is_ischain_meta_invalid(frame);

	fimc_is_frame_trans_pro_to_com(framemgr, frame);
	fimc_is_group_done(groupmgr, group, frame, done_state);
	queue_done(vctx, queue, frame->index, done_state);
}

void wq_func_group(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *ldr_framemgr,
	struct fimc_is_frame *ldr_frame,
	struct fimc_is_video_ctx *vctx,
	u32 status1, u32 status2, u32 fcount)
{
	u32 lindex, hindex;
	struct fimc_is_framemgr *sub_framemgr;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!ldr_framemgr);
	BUG_ON(!ldr_frame);
	BUG_ON(!vctx);

	/*
	 * complete count should be lower than 3 when
	 * buffer is queued or overflow can be occured
	 */
	if (ldr_framemgr->frame_com_cnt >= 2)
		warn("remained completes is %d(%X)", (ldr_framemgr->frame_com_cnt + 1), group->id);

	if (status2 != IS_SHOT_SUCCESS) {
		lindex = ldr_frame->shot->ctl.entry.lowIndexParam;
		lindex &= ~ldr_frame->shot->dm.entry.lowIndexParam;
		hindex = ldr_frame->shot->ctl.entry.highIndexParam;
		hindex &= ~ldr_frame->shot->dm.entry.highIndexParam;
		if (lindex || hindex)
			err("cause : %d : invalid parameter(%08X %08X)", status2, lindex, hindex);
		else
			err("cause : %d", status2);
	}

	switch (group->id) {
	case GROUP_ID_3A0:
		sub_framemgr = GET_DST_FRAMEMGR(vctx);

		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
			/*
			 * When OTF is enabled, 3A0 can have 2 more shots
			 * done is reported to hal if frame count is same
			 * not done is reported to hal if driver frame count is
			 * less than firmware frame count
			 * this correction is repeated
			 */
			if (fcount != ldr_frame->fcount) {
				while (ldr_frame) {
					if (fcount == ldr_frame->fcount) {
						status1 = ISR_DONE;
						wq_func_group_3a0(groupmgr, group,
							ldr_framemgr, ldr_frame,
							sub_framemgr, vctx, status1);
						break;
					} else if (fcount > ldr_frame->fcount) {
						err("cause: mismatch(%d != %d)",
							fcount,
							ldr_frame->fcount);
						status1 = ISR_NDONE;
						wq_func_group_3a0(groupmgr, group,
							ldr_framemgr, ldr_frame,
							sub_framemgr, vctx, status1);

						/* get next leader frame */
						fimc_is_frame_process_head(ldr_framemgr,
							&ldr_frame);
					} else {
						warn("%d done is ignored", fcount);
						break;
					}
				}
			} else {
				wq_func_group_3a0(groupmgr, group,
					ldr_framemgr, ldr_frame,
					sub_framemgr, vctx, status1);
			}
		} else {
			if (fcount != ldr_frame->fcount) {
				err("cause : mismatch(%d != %d)", fcount, ldr_frame->fcount);
				status1 = ISR_NDONE;
			}

			wq_func_group_3a0(groupmgr, group, ldr_framemgr, ldr_frame,
				sub_framemgr, vctx, status1);
		}
		break;
	case GROUP_ID_3A1:
		sub_framemgr = GET_DST_FRAMEMGR(vctx);

		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
			/*
			 * When OTF is enabled, 3A1 can have 2 more shots
			 * done is reported to hal if frame count is same
			 * not done is reported to hal if driver frame count is
			 * less than firmware frame count
			 * this correction is repeated
			 */
			if (fcount != ldr_frame->fcount) {

				while (ldr_frame) {
					if (fcount == ldr_frame->fcount) {
						status1 = ISR_DONE;
						wq_func_group_3a1(groupmgr, group,
							ldr_framemgr, ldr_frame,
							sub_framemgr, vctx, status1);
						break;
					} else if (fcount > ldr_frame->fcount) {
						err("cause : mismatch(%d != %d)", fcount,
							ldr_frame->fcount);
						status1 = ISR_NDONE;
						wq_func_group_3a1(groupmgr, group,
							ldr_framemgr, ldr_frame,
							sub_framemgr, vctx, status1);

						/* get next leader frame */
						fimc_is_frame_process_head(ldr_framemgr,
							&ldr_frame);
					} else {
						warn("%d done is ignored", fcount);
						break;
					}
				}
			} else {
				wq_func_group_3a1(groupmgr, group,
					ldr_framemgr, ldr_frame,
					sub_framemgr, vctx, status1);
			}
		} else {
			if (fcount != ldr_frame->fcount) {
				err("cause : mismatch(%d != %d)", fcount, ldr_frame->fcount);
				status1 = ISR_NDONE;
			}

			wq_func_group_3a1(groupmgr, group, ldr_framemgr, ldr_frame,
				sub_framemgr, vctx, status1);

		}
		break;
	case GROUP_ID_ISP:
		if (fcount != ldr_frame->fcount) {
			err("cause : mismatch(%d != %d)", fcount,
				ldr_frame->fcount);
			status1 = ISR_NDONE;
		}

		wq_func_group_isp(groupmgr, group, ldr_framemgr, ldr_frame,
			vctx, status1);
		break;
	case GROUP_ID_DIS:
		if (fcount != ldr_frame->fcount) {
			err("cause : mismatch(%d != %d)", fcount,
				ldr_frame->fcount);
			status1 = ISR_NDONE;
		}

		wq_func_group_dis(groupmgr, group, ldr_framemgr, ldr_frame,
			vctx, status1);
		break;
	default:
		err("unresolved group id %d", group->id);
		break;
	}
}

static void wq_func_shot(struct work_struct *data)
{
	struct fimc_is_device_ischain *device;
	struct fimc_is_interface *itf;
	struct fimc_is_msg *msg;
	struct fimc_is_framemgr *grp_framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_work_list *work_list;
	struct fimc_is_work *work;
	struct fimc_is_video_ctx *vctx;
	unsigned long flags;
	u32 req_flag;
	u32 fcount;
	u32 status1, status2;
	int instance;
	int group_id;
	struct fimc_is_core *core;

	BUG_ON(!data);

	itf = container_of(data, struct fimc_is_interface,
		work_wq[INTR_SHOT_DONE]);
	work_list = &itf->work_list[INTR_SHOT_DONE];
	group  = NULL;
	vctx = NULL;
	grp_framemgr = NULL;

	get_req_work(work_list, &work);
	while (work) {
		core = (struct fimc_is_core *)itf->core;
		instance = work->msg.instance;
		group_id = work->msg.group;
		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		groupmgr = device->groupmgr;

		msg = &work->msg;
		fcount = msg->parameter1;
		status1 = msg->parameter2;
		status2 = msg->parameter3;

		switch (group_id) {
		case GROUP_ID(GROUP_ID_3A0):
			/* if there's no entry to 3a1 */
			if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a0)) {
				req_flag = REQ_3AA_SHOT;
				group = &device->group_3aa;
			} else {
				req_flag = REQ_ISP_SHOT;
				group = &device->group_isp;
			}
			break;
		case GROUP_ID(GROUP_ID_3A1):
			req_flag = REQ_3AA_SHOT;
			group = &device->group_3aa;
			break;
		case GROUP_ID(GROUP_ID_ISP):
			req_flag = REQ_ISP_SHOT;
			group = &device->group_isp;
			break;
		case GROUP_ID(GROUP_ID_DIS):
			req_flag = REQ_DIS_SHOT;
			group = &device->group_dis;
			break;
		default:
			merr("unresolved group id %d", device, group_id);
			group = NULL;
			vctx = NULL;
			grp_framemgr = NULL;
			goto remain;
		}

		if (!group) {
			merr("group is NULL", device);
			goto remain;
		}

		vctx = group->leader.vctx;
		if (!vctx) {
			merr("vctx is NULL", device);
			goto remain;
		}

		grp_framemgr = GET_SRC_FRAMEMGR(vctx);
		if (!grp_framemgr) {
			merr("grp_framemgr is NULL", device);
			goto remain;
		}

		framemgr_e_barrier_irqs(grp_framemgr, FMGR_IDX_7, flags);

		fimc_is_frame_process_head(grp_framemgr, &frame);

		if (frame) {
#ifdef MEASURE_TIME
#ifdef INTERNAL_TIME
			do_gettimeofday(&frame->time_shotdone);
#endif
#ifdef EXTERNAL_TIME
			do_gettimeofday(&frame->tzone[TM_SHOT_D]);
#endif
#endif

			clear_bit(req_flag, &frame->req_flag);
			if (frame->req_flag)
				merr("group(%d) req flag is not clear all(%X)",
					device, group->id, (u32)frame->req_flag);

#ifdef ENABLE_CLOCK_GATE
			/* dynamic clock off */
			if (sysfs_debug.en_clk_gate &&
					sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
				fimc_is_clk_gate_set(core, group->id, false, false);
#endif
			wq_func_group(groupmgr, group, grp_framemgr, frame,
				vctx, status1, status2, fcount);
		} else {
			merr("GRP%d DONE(%d) is occured without request",
				device, group->id, fcount);
			fimc_is_frame_print_all(grp_framemgr);
		}

		framemgr_x_barrier_irqr(grp_framemgr, FMGR_IDX_7, flags);
#ifdef ENABLE_CLOCK_GATE
		if (fcount == 1 &&
				sysfs_debug.en_clk_gate &&
				sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
			fimc_is_clk_gate_lock_set(core, instance, false);
#endif
remain:
		set_free_work(work_list, work);
		get_req_work(work_list, &work);
	}
}

static inline void wq_func_schedule(struct fimc_is_interface *itf,
	struct work_struct *work_wq)
{
	if (itf->workqueue)
		queue_work(itf->workqueue, work_wq);
	else
		schedule_work(work_wq);
}

static void interface_timer(unsigned long data)
{
	u32 shot_count, scount_3ax, scount_isp;
	u32 fcount, i, j;
	unsigned long flags;
	void __iomem *regs;
	struct fimc_is_interface *itf = (struct fimc_is_interface *)data;
	struct fimc_is_core *core;
	struct fimc_is_device_ischain *device;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_work_list *work_list;
	struct work_struct *work_wq;

	BUG_ON(!itf);
	BUG_ON(!itf->core);

	if (!test_bit(IS_IF_STATE_OPEN, &itf->state)) {
		pr_info("shot timer is terminated\n");
		return;
	}

	core = itf->core;

	for (i = 0; i < FIMC_IS_MAX_NODES; ++i) {
		device = &core->ischain[i];
		shot_count = 0;
		scount_3ax = 0;
		scount_isp = 0;

		sensor = device->sensor;
		if (!sensor)
			continue;

		if (!test_bit(FIMC_IS_SENSOR_FRONT_START, &sensor->state))
			continue;

		if (test_bit(FIMC_IS_ISCHAIN_OPEN_SENSOR, &device->state)) {
			spin_lock_irq(&itf->shot_check_lock);
			if (atomic_read(&itf->shot_check[i])) {
				atomic_set(&itf->shot_check[i], 0);
				atomic_set(&itf->shot_timeout[i], 0);
				spin_unlock_irq(&itf->shot_check_lock);
				continue;
			}
			spin_unlock_irq(&itf->shot_check_lock);

			if (test_bit(FIMC_IS_GROUP_ACTIVE, &device->group_3aa.state)) {
				framemgr = GET_GROUP_FRAMEMGR(&device->group_3aa);
				framemgr_e_barrier_irqs(framemgr, 0, flags);
				scount_3ax = framemgr->frame_pro_cnt;
				shot_count += scount_3ax;
				framemgr_x_barrier_irqr(framemgr, 0, flags);
			}

			if (test_bit(FIMC_IS_GROUP_ACTIVE, &device->group_isp.state)) {
				framemgr = GET_GROUP_FRAMEMGR(&device->group_isp);
				framemgr_e_barrier_irqs(framemgr, 0, flags);
				scount_isp = framemgr->frame_pro_cnt;
				shot_count += scount_isp;
				framemgr_x_barrier_irqr(framemgr, 0, flags);
			}

			if (test_bit(FIMC_IS_GROUP_ACTIVE, &device->group_dis.state)) {
				framemgr = GET_GROUP_FRAMEMGR(&device->group_dis);
				framemgr_e_barrier_irqs(framemgr, 0, flags);
				shot_count += framemgr->frame_pro_cnt;
				framemgr_x_barrier_irqr(framemgr, 0, flags);
			}
		}

		if (shot_count) {
			atomic_inc(&itf->shot_timeout[i]);
			pr_err ("shot timer[%d] is increased to %d\n", i,
				atomic_read(&itf->shot_timeout[i]));
		}

		if (atomic_read(&itf->shot_timeout[i]) > TRY_TIMEOUT_COUNT) {
			merr("shot command is timeout(%d, %d(%d+%d))", device,
				atomic_read(&itf->shot_timeout[i]),
				shot_count, scount_3ax, scount_isp);

			pr_err("\n### 3ax framemgr info ###\n");
			if (scount_3ax) {
				framemgr = GET_GROUP_FRAMEMGR(&device->group_3aa);
				fimc_is_frame_print_all(framemgr);
			}

			pr_err("\n### isp framemgr info ###\n");
			if (scount_isp) {
				framemgr = GET_GROUP_FRAMEMGR(&device->group_isp);
				fimc_is_frame_print_all(framemgr);
			}

			pr_err("\n### work list info ###\n");
			work_list = &itf->work_list[INTR_SHOT_DONE];
			print_fre_work_list(work_list);
			print_req_work_list(work_list);

			if (work_list->work_request_cnt > 0) {
				pr_err("\n### processing work lately ###\n");
				work_wq = &itf->work_wq[INTR_SHOT_DONE];
				wq_func_shot(work_wq);

				atomic_set(&itf->shot_check[i], 0);
				atomic_set(&itf->shot_timeout[i], 0);
			} else {
				pr_err("\n### firmware messsage dump ###\n");
				fimc_is_hw_logdump(itf);

				pr_err("\n### MCUCTL dump ###\n");
				regs = itf->com_regs;
				for (j = 0; j < 64; ++j)
					pr_err("MCTL[%d] : %08X\n", j, readl(regs + (4 * j)));

				if (itf->need_iflag &&
					readl(&itf->com_regs->shot_iflag)) {
					pr_err("\n### MCUCTL check ###\n");
					fimc_is_clr_intr(itf, INTR_SHOT_DONE);

					for (j = 0; j < 64; ++j)
						pr_err("MCTL[%d] : %08X\n", j, readl(regs + (4 * j)));
				}
#ifdef BUG_ON_ENABLE
				BUG();
#endif
				return;
			}
		}
	}

	for (i = 0; i < FIMC_IS_MAX_NODES; ++i) {
		sensor = &core->sensor[i];

		if (!test_bit(FIMC_IS_SENSOR_FRONT_START, &sensor->state))
			continue;

		fcount = fimc_is_sensor_g_fcount(sensor);
		if (fcount == atomic_read(&itf->sensor_check[i])) {
			atomic_inc(&itf->sensor_timeout[i]);
			pr_err ("sensor timer[%d] is increased to %d(fcount : %d)\n", i,
				atomic_read(&itf->sensor_timeout[i]), fcount);
		} else {
			atomic_set(&itf->sensor_timeout[i], 0);
			atomic_set(&itf->sensor_check[i], fcount);
		}

		if (atomic_read(&itf->sensor_timeout[i]) > TRY_TIMEOUT_COUNT) {
			merr("sensor is timeout(%d, %d)", sensor,
				atomic_read(&itf->sensor_timeout[i]),
				atomic_read(&itf->sensor_check[i]));

			pr_err("\n### firmware messsage dump ###\n");
			fimc_is_hw_logdump(itf);

			pr_err("\n### MCUCTL dump ###\n");
			regs = itf->com_regs;
			for (j = 0; j < 64; ++j)
				pr_err("MCTL[%d] : %08X\n", j, readl(regs + (4 * j)));
#ifdef BUG_ON_ENABLE
			BUG();
#endif
			return;
		}
	}

	mod_timer(&itf->timer, jiffies +
		(FIMC_IS_COMMAND_TIMEOUT/TRY_TIMEOUT_COUNT));
}

static irqreturn_t interface_isr(int irq, void *data)
{
	struct fimc_is_interface *itf;
	struct work_struct *work_wq;
	struct fimc_is_work_list *work_list;
	struct fimc_is_work *work;
	u32 status;

	itf = (struct fimc_is_interface *)data;
	status = fimc_is_get_intr(itf);

	if (status & (1<<INTR_SHOT_DONE)) {
		work_wq = &itf->work_wq[INTR_SHOT_DONE];
		work_list = &itf->work_list[INTR_SHOT_DONE];

		get_free_work_irq(work_list, &work);
		if (work) {
			fimc_is_get_cmd(itf, &work->msg, INTR_SHOT_DONE);
			work->fcount = work->msg.parameter1;
			set_req_work_irq(work_list, work);

			if (!work_pending(work_wq))
				wq_func_schedule(itf, work_wq);
		} else
			err("free work item is empty5");

		status &= ~(1<<INTR_SHOT_DONE);
		fimc_is_clr_intr(itf, INTR_SHOT_DONE);
	}

	if (status & (1<<INTR_GENERAL)) {
		work_wq = &itf->work_wq[INTR_GENERAL];
		work_list = &itf->work_list[INTR_GENERAL];

		get_free_work_irq(&itf->work_list[INTR_GENERAL], &work);
		if (work) {
			fimc_is_get_cmd(itf, &work->msg, INTR_GENERAL);
			set_req_work_irq(work_list, work);

			if (!work_pending(work_wq))
				wq_func_schedule(itf, work_wq);
		} else
			err("free work item is empty1");

		status &= ~(1<<INTR_GENERAL);
		fimc_is_clr_intr(itf, INTR_GENERAL);
	}

	if (status & (1<<INTR_3A0C_FDONE)) {
		work_wq = &itf->work_wq[INTR_3A0C_FDONE];
		work_list = &itf->work_list[INTR_3A0C_FDONE];

		get_free_work_irq(work_list, &work);
		if (work) {
			fimc_is_get_cmd(itf, &work->msg, INTR_3A0C_FDONE);
			set_req_work_irq(work_list, work);

			if (!work_pending(work_wq))
				wq_func_schedule(itf, work_wq);
		} else
			err("free work item is empty2");

		status &= ~(1<<INTR_3A0C_FDONE);
		fimc_is_clr_intr(itf, INTR_3A0C_FDONE);
	}

	if (status & (1<<INTR_3A1C_FDONE)) {
		work_wq = &itf->work_wq[INTR_3A1C_FDONE];
		work_list = &itf->work_list[INTR_3A1C_FDONE];

		get_free_work_irq(work_list, &work);
		if (work) {
			fimc_is_get_cmd(itf, &work->msg, INTR_3A1C_FDONE);
			set_req_work_irq(work_list, work);

			if (!work_pending(work_wq))
				wq_func_schedule(itf, work_wq);
		} else
			err("free work item is empty2");

		status &= ~(1<<INTR_3A1C_FDONE);
		fimc_is_clr_intr(itf, INTR_3A1C_FDONE);
	}

	if (status & (1<<INTR_SCC_FDONE)) {
		work_wq = &itf->work_wq[INTR_SCC_FDONE];
		work_list = &itf->work_list[INTR_SCC_FDONE];

		get_free_work_irq(work_list, &work);
		if (work) {
			fimc_is_get_cmd(itf, &work->msg, INTR_SCC_FDONE);
			set_req_work_irq(work_list, work);

			if (!work_pending(work_wq))
				wq_func_schedule(itf, work_wq);
		} else
			err("free work item is empty2");

		status &= ~(1<<INTR_SCC_FDONE);
		fimc_is_clr_intr(itf, INTR_SCC_FDONE);
	}

	if (status & (1<<INTR_DIS_FDONE)) {
		work_wq = &itf->work_wq[INTR_DIS_FDONE];
		work_list = &itf->work_list[INTR_DIS_FDONE];

		get_free_work_irq(work_list, &work);
		if (work) {
			fimc_is_get_cmd(itf, &work->msg, INTR_DIS_FDONE);
			set_req_work_irq(work_list, work);

			if (!work_pending(work_wq))
				wq_func_schedule(itf, work_wq);
		} else {
			err("free work item is empty3");
		}

		status &= ~(1<<INTR_DIS_FDONE);
		fimc_is_clr_intr(itf, INTR_DIS_FDONE);
	}

	if (status & (1<<INTR_SCP_FDONE)) {
		work_wq = &itf->work_wq[INTR_SCP_FDONE];
		work_list = &itf->work_list[INTR_SCP_FDONE];

		get_free_work_irq(work_list, &work);
		if (work) {
			fimc_is_get_cmd(itf, &work->msg, INTR_SCP_FDONE);
			set_req_work_irq(work_list, work);

			if (!work_pending(work_wq))
				wq_func_schedule(itf, work_wq);
		} else {
			err("free work item is empty4");
		}

		status &= ~(1<<INTR_SCP_FDONE);
		fimc_is_clr_intr(itf, INTR_SCP_FDONE);
	}

	if (status != 0)
		err("status is NOT all clear(0x%08X)", status);

	return IRQ_HANDLED;
}

#define VERSION_OF_NO_NEED_IFLAG 221
int fimc_is_interface_probe(struct fimc_is_interface *this,
	u32 regs,
	u32 irq,
	void *core_data)
{
	int ret = 0;
	struct fimc_is_core *core = (struct fimc_is_core *)core_data;

	dbg_interface("%s\n", __func__);

	init_request_barrier(this);
	init_process_barrier(this);
	init_waitqueue_head(&this->lock_wait_queue);
	init_waitqueue_head(&this->init_wait_queue);
	init_waitqueue_head(&this->idle_wait_queue);
	spin_lock_init(&this->shot_check_lock);

	this->workqueue = alloc_workqueue("fimc-is/highpri", WQ_HIGHPRI, 0);
	if (!this->workqueue)
		warn("failed to alloc own workqueue, will be use global one");

	INIT_WORK(&this->work_wq[INTR_GENERAL], wq_func_general);
	INIT_WORK(&this->work_wq[INTR_3A0C_FDONE], wq_func_3a0c);
	INIT_WORK(&this->work_wq[INTR_3A1C_FDONE], wq_func_3a1c);
	INIT_WORK(&this->work_wq[INTR_SCC_FDONE], wq_func_scc);
	INIT_WORK(&this->work_wq[INTR_DIS_FDONE], wq_func_dis);
	INIT_WORK(&this->work_wq[INTR_SCP_FDONE], wq_func_scp);
	INIT_WORK(&this->work_wq[INTR_SHOT_DONE], wq_func_shot);

	this->regs = (void *)regs;
	this->com_regs = (struct is_common_reg *)(regs + ISSR0);

	if (GET_FIMC_IS_VER_OF_SUBIP(core, mcuctl) < VERSION_OF_NO_NEED_IFLAG)
		this->need_iflag = true;
	else
		this->need_iflag = false;

	ret = request_irq(irq, interface_isr, 0, "mcuctl", this);
	if (ret)
		err("request_irq failed\n");

	notify_fcount_sen0		= &this->com_regs->fcount_sen0;
	notify_fcount_sen1		= &this->com_regs->fcount_sen1;
	notify_fcount_sen2		= &this->com_regs->fcount_sen2;
	last_fcount1			= &this->com_regs->fcount_sen3;
	last_fcount0			= &this->com_regs->grp1_done_frame_num;
	this->core			= (void *)core;
	clear_bit(IS_IF_STATE_OPEN, &this->state);
	clear_bit(IS_IF_STATE_START, &this->state);
	clear_bit(IS_IF_STATE_BUSY, &this->state);

	init_work_list(&this->nblk_cam_ctrl,
		TRACE_WORK_ID_CAMCTRL, MAX_NBLOCKING_COUNT);
	init_work_list(&this->work_list[INTR_GENERAL],
		TRACE_WORK_ID_GENERAL, MAX_WORK_COUNT);
	init_work_list(&this->work_list[INTR_3A0C_FDONE],
		TRACE_WORK_ID_3A0C, MAX_WORK_COUNT);
	init_work_list(&this->work_list[INTR_3A1C_FDONE],
		TRACE_WORK_ID_3A1C, MAX_WORK_COUNT);
	init_work_list(&this->work_list[INTR_SCC_FDONE],
		TRACE_WORK_ID_SCC, MAX_WORK_COUNT);
	init_work_list(&this->work_list[INTR_DIS_FDONE],
		TRACE_WORK_ID_DIS, MAX_WORK_COUNT);
	init_work_list(&this->work_list[INTR_SCP_FDONE],
		TRACE_WORK_ID_SCP, MAX_WORK_COUNT);
	init_work_list(&this->work_list[INTR_SHOT_DONE],
		TRACE_WORK_ID_SHOT, MAX_WORK_COUNT);

#ifdef MEASURE_TIME
#ifdef INTERFACE_TIME
	{
		u32 i;
		for (i = 0; i < HIC_COMMAND_END; ++i)
			measure_init(&this->time[i], i);
	}
#endif
#endif

	return ret;
}

int fimc_is_interface_open(struct fimc_is_interface *this)
{
	int i;
	int ret = 0;

	if (test_bit(IS_IF_STATE_OPEN, &this->state)) {
		err("already open");
		ret = -EMFILE;
		goto exit;
	}

	dbg_interface("%s\n", __func__);

	for (i = 0; i < FIMC_IS_MAX_NODES; i++) {
		this->streaming[i] = IS_IF_STREAMING_INIT;
		this->processing[i] = IS_IF_PROCESSING_INIT;
		atomic_set(&this->shot_check[i], 0);
		atomic_set(&this->shot_timeout[i], 0);
		atomic_set(&this->sensor_check[i], 0);
		atomic_set(&this->sensor_timeout[i], 0);
	}
	this->pdown_ready = IS_IF_POWER_DOWN_READY;
	atomic_set(&this->lock_pid, 0);
	clear_bit(IS_IF_STATE_START, &this->state);
	clear_bit(IS_IF_STATE_BUSY, &this->state);

	init_timer(&this->timer);
	this->timer.expires = jiffies +
		(FIMC_IS_COMMAND_TIMEOUT/TRY_TIMEOUT_COUNT);
	this->timer.data = (unsigned long)this;
	this->timer.function = interface_timer;
	add_timer(&this->timer);

	set_bit(IS_IF_STATE_OPEN, &this->state);

exit:
	return ret;
}

int fimc_is_interface_close(struct fimc_is_interface *this)
{
	int ret = 0;
	int retry;

	if (!test_bit(IS_IF_STATE_OPEN, &this->state)) {
		err("already close");
		ret = -EMFILE;
		goto exit;
	}

	retry = 10;
	while (test_busystate(this) && retry) {
		err("interface is busy");
		msleep(20);
		retry--;
	}
	if (!retry)
		err("waiting idle is fail");

	del_timer_sync(&this->timer);
	dbg_interface("%s\n", __func__);

	clear_bit(IS_IF_STATE_OPEN, &this->state);

exit:
	return ret;
}

void fimc_is_interface_lock(struct fimc_is_interface *this)
{
	atomic_set(&this->lock_pid, current->pid);
}

void fimc_is_interface_unlock(struct fimc_is_interface *this)
{
	atomic_set(&this->lock_pid, 0);
	wake_up(&this->lock_wait_queue);
}

void fimc_is_interface_reset(struct fimc_is_interface *this)
{
	if (this->need_iflag) {
		writel(0, &this->com_regs->ihcmd_iflag);
		writel(0, &this->com_regs->taa0c_iflag);
		writel(0, &this->com_regs->taa1c_iflag);
		writel(0, &this->com_regs->scc_iflag);
		writel(0, &this->com_regs->dis_iflag);
		writel(0, &this->com_regs->scp_iflag);
		writel(0, &this->com_regs->shot_iflag);
	}
}

int fimc_is_hw_logdump(struct fimc_is_interface *this)
{
	int debug_cnt, sentence_i;
	char *debug;
	char letter;
	char sentence[250];
	int count = 0, i;
	struct fimc_is_core *core;

	if (!test_bit(IS_IF_STATE_OPEN, &this->state)) {
		err("interface is closed");
		return 0;
	}

	core = (struct fimc_is_core *)this->core;
	sentence_i = 0;

	vb2_ion_sync_for_device(core->minfo.fw_cookie,
		DEBUG_OFFSET, DEBUG_CNT, DMA_FROM_DEVICE);

	debug = (char *)(core->minfo.kvaddr + DEBUG_OFFSET);
	debug_cnt = *((int *)(core->minfo.kvaddr + DEBUGCTL_OFFSET))
			- DEBUG_OFFSET;

	if (core->debug_cnt > debug_cnt)
		count = (DEBUG_CNT - core->debug_cnt) + debug_cnt;
	else
		count = debug_cnt - core->debug_cnt;

	if (count) {
		printk(KERN_ERR "start(%d %d)\n", debug_cnt, count);
		for (i = core->debug_cnt; count > 0; count--) {
			letter = debug[i];
			if (letter) {
				sentence[sentence_i] = letter;
				if (sentence_i >= 247) {
					sentence[sentence_i + 1] = '\n';
					sentence[sentence_i + 2] = 0;
					printk(KERN_ERR "%s", sentence);
					sentence_i = 0;
				} else if (letter == '\n') {
					sentence[sentence_i + 1] = 0;
					printk(KERN_ERR "%s", sentence);
					sentence_i = 0;
				} else {
					sentence_i++;
				}
			}
			i++;
			if (i > DEBUG_CNT)
				i = 0;
		}
		core->debug_cnt = debug_cnt;
		printk(KERN_ERR "end\n");
	}

	return count;
}

int fimc_is_hw_memdump(struct fimc_is_interface *this,
	u32 start,
	u32 end)
{
	u32 *cur;
	u32 items, offset;
	char term[50], sentence[250];

	cur = (u32 *)start;
	items = 0;
	offset = 0;

	memset(sentence, 0, sizeof(sentence));
	printk(KERN_ERR "Memory Dump(0x%08X ~ 0x%08X)\n", start, end);

	while ((u32)cur <= end) {
		if (!(items % 8)) {
			printk(KERN_ERR "%s", sentence);
			offset = 0;
			snprintf(term, sizeof(term), "%p:      ", cur);
			snprintf(&sentence[offset], sizeof(sentence) - offset, "%s", term);
			offset += strlen(term);
		}

		snprintf(term, sizeof(term), "%08X   ", *cur);
		snprintf(&sentence[offset], sizeof(sentence) - offset, "%s", term);
		offset += strlen(term);
		cur++;
		items++;
	}

	return (u32)cur - end;
}

int fimc_is_hw_enum(struct fimc_is_interface *this)
{
	int ret = 0;
	struct fimc_is_msg msg;
	volatile struct is_common_reg __iomem *com_regs;

	dbg_interface("enum()\n");

	ret = wait_initstate(this);
	if (ret) {
		err("enum time out");
		ret = -ETIME;
		goto exit;
	}

	msg.id = 0;
	msg.command = ISR_DONE;
	msg.instance = 0;
	msg.group = 0;
	msg.parameter1 = IHC_GET_SENSOR_NUMBER;
	/*
	 * this mean sensor numbers
	 * signel sensor: 1, dual sensor: 2, triple sensor: 3
	 */
	msg.parameter2 = 3;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	waiting_is_ready(this);
	com_regs = this->com_regs;
	writel(msg.command, &com_regs->hicmd);
	writel(msg.instance, &com_regs->hic_sensorid);
	writel(msg.parameter1, &com_regs->hic_param1);
	writel(msg.parameter2, &com_regs->hic_param2);
	writel(msg.parameter3, &com_regs->hic_param3);
	writel(msg.parameter4, &com_regs->hic_param4);
	send_interrupt(this);

exit:
	return ret;
}

int fimc_is_hw_saddr(struct fimc_is_interface *this,
	u32 instance, u32 *setfile_addr)
{
	int ret = 0;
	struct fimc_is_msg msg, reply;

	dbg_interface("saddr(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_GET_SET_FILE_ADDR;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);
	*setfile_addr = reply.parameter2;

	return ret;
}

int fimc_is_hw_setfile(struct fimc_is_interface *this,
	u32 instance)
{
	int ret = 0;
	struct fimc_is_msg msg, reply;

	dbg_interface("setfile(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_LOAD_SET_FILE;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_open(struct fimc_is_interface *this,
	u32 instance,
	u32 module_id,
	u32 info,
	u32 group,
	u32 flag,
	u32 *mwidth,
	u32 *mheight)
{
	int ret;
	struct fimc_is_msg msg, reply;

	dbg_interface("open(%d,%d,%08X)\n", module_id, group, flag);

	msg.id = 0;
	msg.command = HIC_OPEN_SENSOR;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = module_id;
	msg.parameter2 = info;
	msg.parameter3 = group;
	msg.parameter4 = flag;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	*mwidth = reply.parameter2;
	*mheight = reply.parameter3;

	return ret;
}

int fimc_is_hw_close(struct fimc_is_interface *this,
	u32 instance)
{
	int ret;
	struct fimc_is_msg msg, reply;

	dbg_interface("close sensor(instance:%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_CLOSE_SENSOR;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_stream_on(struct fimc_is_interface *this,
	u32 instance)
{
	int ret;
	struct fimc_is_msg msg, reply;

	BUG_ON(!this);

	dbg_interface("stream_on(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_STREAM_ON;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_stream_off(struct fimc_is_interface *this,
	u32 instance)
{
	int ret;
	struct fimc_is_msg msg, reply;

	dbg_interface("stream_off(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_STREAM_OFF;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_process_on(struct fimc_is_interface *this,
	u32 instance, u32 group)
{
	int ret;
	struct fimc_is_msg msg, reply;

	dbg_interface("process_on(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_PROCESS_START;
	msg.instance = instance;
	msg.group = group;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_process_off(struct fimc_is_interface *this,
	u32 instance, u32 group, u32 mode)
{
	int ret;
	struct fimc_is_msg msg, reply;

	WARN_ON(mode >= 2);

	dbg_interface("process_off(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_PROCESS_STOP;
	msg.instance = instance;
	msg.group = group;
	msg.parameter1 = mode;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_i2c_lock(struct fimc_is_interface *this,
	u32 instance, int i2c_clk, bool lock)
{
	int ret;
	struct fimc_is_msg msg, reply;

	dbg_interface("i2c_lock(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_I2C_CONTROL_LOCK;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = lock;
	msg.parameter2 = i2c_clk;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_s_param(struct fimc_is_interface *this,
	u32 instance, u32 lindex, u32 hindex, u32 indexes)
{
	int ret;
	struct fimc_is_msg msg, reply;

	dbg_interface("s_param(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_SET_PARAMETER;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = ISS_PREVIEW_STILL;
	msg.parameter2 = indexes;
	msg.parameter3 = lindex;
	msg.parameter4 = hindex;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_a_param(struct fimc_is_interface *this,
	u32 instance, u32 group, u32 sub_mode)
{
	int ret = 0;
	struct fimc_is_msg msg, reply;

	dbg_interface("a_param(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_PREVIEW_STILL;
	msg.instance = instance;
	msg.group = group;
	msg.parameter1 = sub_mode;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_g_capability(struct fimc_is_interface *this,
	u32 instance, u32 address)
{
	int ret;
	struct fimc_is_msg msg, reply;

	dbg_interface("g_capability(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_GET_STATIC_METADATA;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = address;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_map(struct fimc_is_interface *this,
	u32 instance, u32 group, u32 address, u32 size)
{
	int ret;
	struct fimc_is_msg msg, reply;

	dbg_interface("[%d][%d] map(%08X, %X)\n", instance, group, address, size);

	msg.id = 0;
	msg.command = HIC_SET_A5_MAP;
	msg.instance = instance;
	msg.group = group;
	msg.parameter1 = address;
	msg.parameter2 = size;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_unmap(struct fimc_is_interface *this,
	u32 instance, u32 group)
{
	int ret;
	struct fimc_is_msg msg, reply;

	dbg_interface("[%d][%d] unmap\n", instance, group);

	msg.id = 0;
	msg.command = HIC_SET_A5_UNMAP;
	msg.instance = instance;
	msg.group = group;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

int fimc_is_hw_power_down(struct fimc_is_interface *this,
	u32 instance)
{
	int ret = 0;
	struct fimc_is_msg msg, reply;

	dbg_interface("pwr_down(%d)\n", instance);

	if (!test_bit(IS_IF_STATE_START, &this->state)) {
		warn("instance(%d): FW is not initialized, wait\n", instance);
		ret = fimc_is_hw_enum(this);
		if (ret)
			err("fimc_is_itf_enum is fail(%d)", ret);
	}

	msg.id = 0;
	msg.command = HIC_POWER_DOWN;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}

#if (FW_HAS_SYS_CTRL_CMD)
int fimc_is_hw_sys_ctl(struct fimc_is_interface *this,
	u32 instance, int cmd, int val)
{
	int ret;
	struct fimc_is_msg msg, reply;

	dbg_interface("sys_ctl(cmd(%d), val(%d))\n", cmd, val);

	msg.id = 0;
	msg.command = HIC_SYSTEM_CONTROL;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = cmd;
	msg.parameter2 = val;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}
#else
int fimc_is_hw_sys_ctl(struct fimc_is_interface *this,
	u32 instance, int cmd, int val)
{
	return 0;
}
#endif

#if (FW_HAS_SENSOR_MODE_CMD)
int fimc_is_hw_sensor_mode(struct fimc_is_interface *this,
	u32 instance, int cfg)
{
	int ret;
	struct fimc_is_msg msg, reply;

	dbg_interface("sensor mode(%d): %d\n", instance, cfg);

	msg.id = 0;
	msg.command = HIC_SENSOR_MODE_CHANGE;
	msg.instance = instance;
	msg.group = 0;
	msg.parameter1 = cfg;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(this, &msg, &reply);

	return ret;
}
#else
int fimc_is_hw_sensor_mode(struct fimc_is_interface *this,
	u32 instance, int cfg)
{
	return 0;
}
#endif

int fimc_is_hw_shot_nblk(struct fimc_is_interface *this,
	u32 instance, u32 group, u32 bayer, u32 shot, u32 fcount, u32 rcount)
{
	int ret = 0;
	struct fimc_is_msg msg;

	/*dbg_interface("shot_nblk(%d, %d)\n", instance, fcount);*/

	msg.id = 0;
	msg.command = HIC_SHOT;
	msg.instance = instance;
	msg.group = group;
	msg.parameter1 = bayer;
	msg.parameter2 = shot;
	msg.parameter3 = fcount;
	msg.parameter4 = rcount;

	ret = fimc_is_set_cmd_shot(this, &msg);

	return ret;
}

int fimc_is_hw_s_camctrl_nblk(struct fimc_is_interface *this,
	u32 instance, u32 address, u32 fcount)
{
	int ret = 0;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	dbg_interface("cam_ctrl_nblk(%d)\n", instance);

	get_free_work(&this->nblk_cam_ctrl, &work);

	if (work) {
		work->fcount = fcount;
		msg = &work->msg;
		msg->id = 0;
		msg->command = HIC_SET_CAM_CONTROL;
		msg->instance = instance;
		msg->group = 0;
		msg->parameter1 = address;
		msg->parameter2 = fcount;
		msg->parameter3 = 0;
		msg->parameter4 = 0;

		ret = fimc_is_set_cmd_nblk(this, work);
	} else {
		err("g_free_nblk return NULL");
		print_fre_work_list(&this->nblk_cam_ctrl);
		print_req_work_list(&this->nblk_cam_ctrl);
		ret = 1;
	}

	return ret;
}

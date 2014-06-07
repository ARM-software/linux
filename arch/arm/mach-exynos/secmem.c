/*
 * arch/arm/mach-exynos/secmem.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/export.h>
#include <linux/pm_qos.h>
#include <linux/dma-contiguous.h>
#include <linux/exynos_ion.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include <asm/memory.h>
#include <asm/cacheflush.h>

#include <plat/devs.h>

#include <mach/secmem.h>
#include <mach/smc.h>

#define SECMEM_DEV_NAME	"s5p-smem"
struct miscdevice secmem;
struct secmem_crypto_driver_ftn *crypto_driver;

uint32_t instance_count;

static uint32_t secmem_regions[] = {
	ION_EXYNOS_ID_MFC_SH,
	ION_EXYNOS_ID_G2D_WFD,
	ION_EXYNOS_ID_VIDEO,
	ION_EXYNOS_ID_MFC_INPUT,
	ION_EXYNOS_ID_SECTBL,
	ION_EXYNOS_ID_MFC_FW,
	ION_EXYNOS_ID_MFC_NFW,
};

static char *secmem_regions_name[] = {
	"mfc_sh",	/* 0 */
	"g2d_wfd",	/* 1 */
	"video",	/* 2 */
	"mfc_input",	/* 3 */
	"sectbl",	/* 4 */
	"mfc_fw",	/* 5 */
	"mfc_nfw",	/* 6 */
	NULL
};

static bool drm_onoff;
static DEFINE_MUTEX(drm_lock);
static DEFINE_MUTEX(smc_lock);

struct secmem_info {
	struct device	*dev;
	bool		drm_enabled;
};

struct protect_info {
	uint32_t dev;
	uint32_t enable;
};

#define SECMEM_IS_PAGE_ALIGNED(addr) (!((addr) & (~PAGE_MASK)))

static int secmem_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct device *dev = miscdev->this_device;
	struct secmem_info *info;

	info = kzalloc(sizeof(struct secmem_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	file->private_data = info;

	mutex_lock(&drm_lock);
	instance_count++;
	mutex_unlock(&drm_lock);

	return 0;
}

struct device_node *get_secmem_dev_node_and_size(size_t *index_np_size)
{
	size_t size;
	const __be32 *phandle;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "samsung,exynos-secmem");
	if (!of_device_is_available(np)) {
		pr_err("Fail to find compatible node for secmem\n");
		return NULL;
	}

	phandle = of_get_property(np, "secmem", &size);
	if (!phandle) {
		printk("fail to get phandle from semem\n");
		return NULL;
	}

	size = size / sizeof(*phandle);
	*index_np_size = size;

	return np;
}

struct platform_device *get_secmem_dt_index_pdev(struct device_node *np, int idx)
{
	struct device_node *np_idx;
	struct platform_device *pdev;

	np_idx = of_parse_phandle(np, "secmem", idx);
	if (!np_idx) {
		pr_err("secmem phandle node is not found\n");
		return NULL;
	}

	pdev = of_find_device_by_node(np_idx);
	if (!pdev) {
		pr_err("secmem node is not found\n");
		return NULL;
	}

	return pdev;
}

int drm_enable_locked(struct secmem_info *info, bool enable)
{
	int idx_np;
	size_t idx_np_size;
	struct platform_device *pdev;
	struct device_node *np = NULL;
	struct device *dev;

	if (drm_onoff == enable) {
		pr_err("%s: DRM is already %s\n", __func__, drm_onoff ? "on" : "off");
		return 0;
	}

	np = get_secmem_dev_node_and_size(&idx_np_size);
	if (!np) {
		pr_err("fail to get secmem dev node and size\n");
		return 0;
	}

	for (idx_np = 0; idx_np < idx_np_size; idx_np++) {
		pdev = get_secmem_dt_index_pdev(np, idx_np);
		if (pdev == NULL) {
			pr_err("fail to get secmem index pdev\n");
			return 0;
		}
		dev = &pdev->dev;

		if (enable)
			pm_runtime_get_sync(dev);
		else
			pm_runtime_put_sync(dev);
	}

	drm_onoff = enable;
	/*
	 * this will only allow this instance to turn drm_off either by
	 * calling the ioctl or by closing the fd
	 */
	info->drm_enabled = enable;

	return 0;
}

static int secmem_release(struct inode *inode, struct file *file)
{
	struct secmem_info *info = file->private_data;

	/* disable drm if we were the one to turn it on */
	mutex_lock(&drm_lock);
	instance_count--;
	if (instance_count == 0) {
		if (info->drm_enabled) {
			int ret;
			ret = drm_enable_locked(info, false);
			if (ret < 0)
				pr_err("fail to lock/unlock drm status. lock = %d\n", false);
		}
	}
	else {
		printk("%s: exist opened instance", __func__);
	}
	mutex_unlock(&drm_lock);

	kfree(info);
	return 0;
}

static long secmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct secmem_info *info = filp->private_data;

	static int nbufs = 0;

	switch (cmd) {
	case SECMEM_IOC_GET_CHUNK_NUM:
	{
		nbufs = sizeof(secmem_regions) / sizeof(uint32_t);

		if (nbufs == 0)
			return -ENOMEM;

		if (copy_to_user((void __user *)arg, &nbufs, sizeof(int)))
			return -EFAULT;
		break;
	}
	case SECMEM_IOC_CHUNKINFO:
	{
		struct secchunk_info minfo;

		if (copy_from_user(&minfo, (void __user *)arg, sizeof(minfo)))
			return -EFAULT;

		memset(&minfo.name, 0, MAX_NAME_LEN);

		if (minfo.index < 0)
			return -EINVAL;

		if (minfo.index >= nbufs) {
			minfo.index = -1; /* No more memory region */
		} else {

			if (ion_exynos_contig_heap_info(secmem_regions[minfo.index],
					&minfo.base, &minfo.size))
				return -EINVAL;

			memcpy(minfo.name, secmem_regions_name[minfo.index], MAX_NAME_LEN);
		}

		if (copy_to_user((void __user *)arg, &minfo, sizeof(minfo)))
			return -EFAULT;
		break;
	}
#if defined(CONFIG_ION)
	case SECMEM_IOC_GET_FD_PHYS_ADDR:
	{
		struct ion_client *client;
		struct secfd_info fd_info;
		struct ion_handle *handle;
		size_t len;

		if (copy_from_user(&fd_info, (int __user *)arg,
					sizeof(fd_info)))
			return -EFAULT;

		client = ion_client_create(ion_exynos, "DRM");
		if (IS_ERR(client)) {
			pr_err("%s: Failed to get ion_client of DRM\n",
				__func__);
			return -ENOMEM;
		}

		handle = ion_import_dma_buf(client, fd_info.fd);
		pr_debug("%s: fd from user space = %d\n",
				__func__, fd_info.fd);
		if (IS_ERR(handle)) {
			pr_err("%s: Failed to get ion_handle of DRM\n",
				__func__);
			ion_client_destroy(client);
			return -ENOMEM;
		}

		if (ion_phys(client, handle, &fd_info.phys, &len)) {
			pr_err("%s: Failed to get phys. addr of DRM\n",
				__func__);
			ion_client_destroy(client);
			ion_free(client, handle);
			return -ENOMEM;
		}

		pr_debug("%s: physical addr from kernel space = 0x%08x\n",
				__func__, (unsigned int)fd_info.phys);

		ion_free(client, handle);
		ion_client_destroy(client);

		if (copy_to_user((void __user *)arg, &fd_info, sizeof(fd_info)))
			return -EFAULT;
		break;
	}
#endif
	case SECMEM_IOC_GET_DRM_ONOFF:
		smp_rmb();
		if (copy_to_user((void __user *)arg, &drm_onoff, sizeof(int)))
			return -EFAULT;
		break;
	case SECMEM_IOC_SET_DRM_ONOFF:
	{
		int ret, val = 0;

		if (copy_from_user(&val, (int __user *)arg, sizeof(int)))
			return -EFAULT;

		mutex_lock(&drm_lock);
		if ((info->drm_enabled && !val) ||
		    (!info->drm_enabled && val)) {
			/*
			 * 1. if we enabled drm, then disable it
			 * 2. if we don't already hdrm enabled,
			 *    try to enable it.
			 */
			ret = drm_enable_locked(info, val);
			if (ret < 0)
				pr_err("fail to lock/unlock drm status. lock = %d\n", val);
		}
		mutex_unlock(&drm_lock);
		break;
	}
	case SECMEM_IOC_GET_CRYPTO_LOCK:
	{
		break;
	}
	case SECMEM_IOC_RELEASE_CRYPTO_LOCK:
	{
		break;
	}
	case SECMEM_IOC_SET_TZPC:
	{
#if !defined(CONFIG_SOC_EXYNOS5422) && !defined(CONFIG_SOC_EXYNOS5430)
		struct protect_info prot;

		if (copy_from_user(&prot, (void __user *)arg, sizeof(struct protect_info)))
			return -EFAULT;

		mutex_lock(&smc_lock);
		exynos_smc((uint32_t)(0x81000000), 0, prot.dev, prot.enable);
		mutex_unlock(&smc_lock);
#endif
		break;
	}
	default:
		return -ENOTTY;
	}

	return 0;
}

void secmem_crypto_register(struct secmem_crypto_driver_ftn *ftn)
{
	crypto_driver = ftn;
}
EXPORT_SYMBOL(secmem_crypto_register);

void secmem_crypto_deregister(void)
{
	crypto_driver = NULL;
}
EXPORT_SYMBOL(secmem_crypto_deregister);

static const struct file_operations secmem_fops = {
	.open		= secmem_open,
	.release	= secmem_release,
	.unlocked_ioctl = secmem_ioctl,
};

struct miscdevice secmem = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= SECMEM_DEV_NAME,
	.fops	= &secmem_fops,
};

static int __init secmem_init(void)
{
	int ret;
	static int iso_count = 0;
	static int count, nbufs = 0;

	ret = misc_register(&secmem);
	if (ret) {
		pr_err("%s: SECMEM can't register misc on minor=%d\n",
			__func__, MISC_DYNAMIC_MINOR);
		return ret;
	}

	crypto_driver = NULL;

	if (!iso_count) {
		nbufs = sizeof(secmem_regions) / sizeof(uint32_t);

		for (count = 0; count < nbufs; count++) {
			ret = ion_exynos_contig_heap_isolate(secmem_regions[count]);
			if (ret < 0) {
				pr_err("%s: Fail to isolate reserve region. id = %d\n",
						__func__, secmem_regions[count]);
				return -ENODEV;
			}
		}

		iso_count = 1;
		pr_debug("%s: reserve region is isolated.\n", __func__);
	}

	return 0;
}

static void __exit secmem_exit(void)
{
	misc_deregister(&secmem);
}

module_init(secmem_init);
module_exit(secmem_exit);

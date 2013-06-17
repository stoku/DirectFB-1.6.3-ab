/*
 * Copyright (C) 2013 Sosuke Tokunaga <sosuke.tokunaga@courier-systems.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the Lisence, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be helpful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MARCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "sh_du.h"

#define SH_DU_DSSR		(0x0008)
#define SH_DU_DSRCR		(0x000C)
#define SH_DU_DIER		(0x0010)

#define SH_DU_DSSR_VBK		(0x01 << 11)

struct sh_du_wait_flags_queue {
	struct sh_du_status	key;
	wait_queue_t 		wait;
};

struct sh_du_miscdevice {
	char 			name[8];
	u32			status;
	wait_queue_head_t	queue;
	int			irq;
	struct {
		u8		*base;
		resource_size_t	size;
	} reg;
	struct clk		*clk;
	struct miscdevice	misc;
};

static struct sh_du_miscdevice *sh_du_create(void)
{
	struct sh_du_miscdevice *ret;

	ret = kzalloc(sizeof(*ret), GFP_KERNEL);
	return ret;
}

static void sh_du_delete(struct sh_du_miscdevice *dev)
{
	kfree(dev);
}

static struct sh_du_miscdevice *sh_du_get(struct file *filp)
{
	struct miscdevice *d = (struct miscdevice *)filp->private_data;
	return (!d) ? NULL : container_of(d, struct sh_du_miscdevice, misc);
}

static long sh_du_access(struct sh_du_miscdevice *dev,
			unsigned int dir,
			unsigned long arg)
{
	struct sh_du_reg reg;

	if (copy_from_user(&reg, (void *)arg, sizeof(reg)))
		return -EFAULT;

	if (reg.offset + 4 > dev->reg.size)
		return -EFAULT;

	if (dir == _IOC_READ) {
		reg.value = ioread32(dev->reg.base + reg.offset);
		if (copy_to_user((void *)arg, &reg, sizeof(reg)))
			return -EFAULT;
	} else {
		iowrite32(reg.value, dev->reg.base + reg.offset);
	}

	return 0;
}

static bool sh_du_test_flags(const struct sh_du_status *key, u32 flags)
{
	flags &= key->flags;
	return (key->flags == flags) || (flags && (key->mode & SH_DU_WAIT_OR));
}

static int sh_du_wake_flags(wait_queue_t *wait,
				unsigned mode,
				int sync,
				void *key)
{
	struct sh_du_wait_flags_queue *wfq;
	u32 flags;

	wfq = container_of(wait, struct sh_du_wait_flags_queue, wait);
	flags = (u32)key;

	if (!sh_du_test_flags(&wfq->key, flags))
		return 0;

	wfq->key.flags = flags;
	return autoremove_wake_function(wait, mode, sync, key);
}

static long sh_du_wait(struct sh_du_miscdevice *dev,
			unsigned long arg)
{
	long ret;
	struct sh_du_wait_flags_queue wfq;
	
	if (copy_from_user(&wfq.key, (void *)arg, sizeof(wfq.key)))
		return -EFAULT;

	wfq.wait.private	= current;
	wfq.wait.func		= sh_du_wake_flags;
	wfq.wait.flags		= 0;
	INIT_LIST_HEAD(&wfq.wait.task_list);

	ret = -ERESTARTSYS;
	if (wfq.key.mode & SH_DU_CLEAR_BEFORE)
		dev->status &= ~wfq.key.flags;
	prepare_to_wait(&dev->queue, &wfq.wait, TASK_INTERRUPTIBLE);
	if (!signal_pending(current)) {
		schedule();
		if (!signal_pending(current))
			ret = 0;
	}
	finish_wait(&dev->queue, &wfq.wait);

	if (ret == 0) {
		if (wfq.key.mode & SH_DU_CLEAR_AFTER)
			dev->status &= ~wfq.key.flags;
		if (copy_to_user((void *)arg, &wfq.key, sizeof(wfq.key)))
			ret = -EFAULT;
	}

	return ret;
}

static irqreturn_t sh_du_interrupt(int irq, void *dev_id)
{
	struct sh_du_miscdevice *dev = dev_id;
	u32 status;

	status = ioread32(dev->reg.base + SH_DU_DSSR) &
		ioread32(dev->reg.base + SH_DU_DIER);

	if (status) {
		iowrite32(status, dev->reg.base + SH_DU_DSRCR);
		dev->status |= status;
		wake_up_interruptible_poll(&dev->queue, dev->status);
	}

	return IRQ_HANDLED;
}

static int sh_du_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static long sh_du_ioctl(struct file *filp,
			unsigned int cmd,
			unsigned long arg)
{
	struct sh_du_miscdevice *shdev;

	shdev = sh_du_get(filp);
	if (!shdev)
		return -ENODEV;

	switch (cmd) {
	case SH_DU_IOC_GETREG:
	case SH_DU_IOC_SETREG:
		return sh_du_access(shdev, _IOC_DIR(cmd), arg);
	case SH_DU_IOC_WAIT:
		return sh_du_wait(shdev, arg);
	default:
		return -EINVAL;
	}
}

static ssize_t sh_du_read(struct file *filp,
				char __user *buf,
				size_t count,
				loff_t *f_pos)
{
	struct sh_du_miscdevice *shdev;
	loff_t pos;

	shdev = sh_du_get(filp);
	if (!shdev)
		return -ENODEV;

	pos = *f_pos;
	if (pos >= shdev->reg.size)
		return 0;

	if (count > (shdev->reg.size - (size_t)pos))
		count = shdev->reg.size - (size_t)pos;

	count -= copy_to_user(buf, shdev->reg.base + pos, count);
	*f_pos = pos + count;

	return count;
}

static struct file_operations sh_du_fops = {
	.open		= sh_du_open,
	.unlocked_ioctl	= sh_du_ioctl,
	.read		= sh_du_read,
};

static int sh_du_probe(struct platform_device *pdev)
{
	int ret;
	struct sh_du_miscdevice *shdev;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "invalid resource\n");
		return -EINVAL;
	}

	shdev = sh_du_create();
	if (!shdev)
		return -ENOMEM;

	shdev->reg.size = resource_size(res);
	shdev->reg.base = (u8 *)ioremap_nocache(res->start, shdev->reg.size);

	if (!shdev->reg.base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -EIO;
		goto err_free;
	}
	if (pdev->id < 0)
		snprintf(shdev->name, sizeof(shdev->name) - 1,
			"%s", pdev->name);
	else
		snprintf(shdev->name, sizeof(shdev->name) - 1,
			"%s%d", pdev->name, pdev->id);

	init_waitqueue_head(&shdev->queue);
	shdev->irq = platform_get_irq(pdev, 0);
	ret = request_irq(shdev->irq, sh_du_interrupt, 0, shdev->name, shdev);
	if (ret) {
		dev_err(&pdev->dev, "request_irq failed\n");
		goto err_unmap;
	}

	shdev->misc.name = shdev->name;
	shdev->misc.minor = MISC_DYNAMIC_MINOR;
	shdev->misc.fops = &sh_du_fops;

	ret = misc_register(&shdev->misc);
	if (ret) {
		dev_err(&pdev->dev, "misc_register failed\n");
		goto err_irq;
	}

        shdev->clk = clk_get(&pdev->dev, "view");
	if (IS_ERR(shdev->clk)) {
		dev_err(&pdev->dev, "clk_get failed\n");
		ret = PTR_ERR(shdev->clk);
		goto err_misc;
	}
	ret = clk_enable(shdev->clk);
	if (ret) {
		dev_err(&pdev->dev, "clk_enable failed\n");
		goto err_clk;
	}

	platform_set_drvdata(pdev, shdev);
	dev_info(&pdev->dev, "misc device registered\n");

	return ret;

err_clk:
	clk_put(shdev->clk);
err_misc:
	misc_deregister(&shdev->misc);
err_irq:
	free_irq(shdev->irq, shdev);
err_unmap:
	iounmap(shdev->reg.base);
err_free:
	sh_du_delete(shdev);
	return ret;
}

static int sh_du_remove(struct platform_device *pdev)
{
	struct sh_du_miscdevice *shdev;

	shdev = (struct sh_du_miscdevice *)platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	clk_disable(shdev->clk);
	clk_put(shdev->clk);
	misc_deregister(&shdev->misc);
	free_irq(shdev->irq, shdev);
	iounmap(shdev->reg.base);
	sh_du_delete(shdev);
	return 0;
}

static struct platform_driver sh_du_driver = {
	.probe	= sh_du_probe,
	.remove	= sh_du_remove,
	.driver	= {
		.name	= "sh_du",
	},
};

static struct resource sh_du_resources[] = {
	[0] = {
		.start	= 0xFFF80000,
		.end	= 0xFFF9304C - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= evt2irq(0x3E0),
		.flags	= IORESOURCE_IRQ,
	},
};

static void sh_du_device_release(struct device *dev)
{
}

static struct platform_device sh_du_device = {
	.name		= "sh_du",
	.id		= -1,
	.resource	= sh_du_resources,
	.num_resources	= ARRAY_SIZE(sh_du_resources),
	.dev = {
		.release = sh_du_device_release,
	},
};

static int __init sh_du_driver_init(void)
{
	int ret;

	ret = platform_device_register(&sh_du_device);
	if (ret)
		return ret;

	ret = platform_driver_register(&sh_du_driver);
	if (ret)
		platform_device_unregister(&sh_du_device);

	return ret;
}

module_init(sh_du_driver_init);

static void __exit sh_du_driver_exit(void)
{
	platform_driver_unregister(&sh_du_driver);
	platform_device_unregister(&sh_du_device);
}

module_exit(sh_du_driver_exit);

MODULE_DESCRIPTION("Renesas DU driver");
MODULE_AUTHOR("Sosuke Tokunaga");
MODULE_LICENSE("GPL");


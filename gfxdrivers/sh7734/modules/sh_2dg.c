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
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "sh_2dg.h"

#define SH_2DG_CMDBUF_SIZE	(512)

#define SH_2DG_SCLR		(0x0000)
#define SH_2DG_SR		(0x0004)
#define SH_2DG_SRCR		(0x0008)
#define SH_2DG_ICIDR		(0x0010)
#define SH_2DG_DLSAR		(0x0048)

#define SH_2DG_SCLR_SRES	(0x01 << 31)
#define SH_2DG_SCLR_HRES	(0x01 << 30)
#define SH_2DG_SCLR_RS		(0x01 << 0)
#define SH_2DG_SR_VER		(0x0f << 28)
#define SH_2DG_SR_CER		(0x01 << 2)
#define SH_2DG_SR_INT		(0x01 << 1)
#define SH_2DG_SR_TRA		(0x01 << 0)

#define SH_2DG_CMD_TRAP		(0x00000000)

struct sh_2dg_miscdevice {
	char			name[8];
	u32			status;
	u32			icount;
	int			irq;
	struct {
		u8		*base;
		resource_size_t	size;
	} reg;
	struct clk		*clk;
	struct {
		u32		*va[2]; /* virtual address */
		u32		pa[2];  /* physical address */
		unsigned int	index;  /* current index */
		size_t		size;   /* buffer size */
		size_t		filled; /* filled size */
		struct page	*page;
		unsigned int    order;
	} cb; /* command buffer */
	struct semaphore	sem;
	struct miscdevice	misc;
};

static struct sh_2dg_miscdevice *sh_2dg_create(void)
{
	struct sh_2dg_miscdevice *ret;
	size_t size;

	ret = kzalloc(sizeof(*ret), GFP_KERNEL);
	if (!ret)
		return ret;

	size = 2 * SH_2DG_CMDBUF_SIZE * sizeof(u32);
	ret->cb.order = get_order(size);
	ret->cb.page = alloc_pages(GFP_KERNEL, ret->cb.order);

	if (!ret->cb.page) {
		kfree(ret);
		return ret;
	}

	size = PAGE_SIZE << ret->cb.order;
	ret->cb.size = size / (2 * sizeof(u32));
	ret->cb.pa[0] = page_to_phys(ret->cb.page);
	ret->cb.va[0] = ioremap_nocache(ret->cb.pa[0], size);
	ret->cb.pa[1] = ret->cb.pa[0] + (size / 2);
	ret->cb.va[1] = ret->cb.va[0] + ret->cb.size;

	return ret;
}

static void sh_2dg_delete(struct sh_2dg_miscdevice *dev)
{
	iounmap(dev->cb.va[0]);
	__free_pages(dev->cb.page, dev->cb.order);
	kfree(dev);
}

static struct sh_2dg_miscdevice *sh_2dg_get(struct file *filp)
{
	struct miscdevice *d = filp->private_data;
	return (!d) ? NULL : container_of(d, struct sh_2dg_miscdevice, misc);
}

static int sh_2dg_exec(struct sh_2dg_miscdevice *dev)
{
	int ret;

	dev->cb.va[dev->cb.index][dev->cb.filled] = SH_2DG_CMD_TRAP;
	wmb();
	ret = down_interruptible(&dev->sem);
	if (ret)
		goto out;

	dev->status &= ~(SH_2DG_SR_INT | SH_2DG_SR_TRA);
	iowrite32(dev->cb.pa[dev->cb.index], dev->reg.base + SH_2DG_DLSAR);
	iowrite32(SH_2DG_SCLR_RS, dev->reg.base + SH_2DG_SCLR);
	dev->cb.index ^= 1;
	dev->cb.filled = 0;
out:
	return ret;
}

static long sh_2dg_reset(struct sh_2dg_miscdevice *dev)
{
	u8 *addr;
	u32 sclr;

	addr = dev->reg.base + SH_2DG_SCLR;
	sclr = ioread32(addr);
	if ((sclr & SH_2DG_SCLR_SRES) == 0) {
		iowrite32(SH_2DG_SCLR_SRES, addr);
		mdelay(17);
		iowrite32(SH_2DG_SCLR_SRES | SH_2DG_SCLR_HRES, addr);
		mdelay(1);
		iowrite32(SH_2DG_SCLR_SRES, addr);
		dev->status = 0;
		dev->icount = 0;
		dev->cb.filled = 0;
	}
	iowrite32(0, addr);
	return 0;
}

static long sh_2dg_access(struct sh_2dg_miscdevice *dev,
			unsigned int dir,
			unsigned long arg)
{
	struct sh_2dg_reg reg;

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

static irqreturn_t sh_2dg_interrupt(int irq, void *dev_id)
{
	struct sh_2dg_miscdevice *dev = dev_id;
	u32 sr;

	sr = ioread32(dev->reg.base + SH_2DG_SR);
	dev->status |= sr;
	if (sr & SH_2DG_SR_INT)
		dev->icount++;
	if (sr & (SH_2DG_SR_CER | SH_2DG_SR_TRA))
		up(&dev->sem);
	iowrite32(sr, dev->reg.base + SH_2DG_SRCR);

	return IRQ_HANDLED;
}

static int sh_2dg_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int sh_2dg_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t sh_2dg_write(struct file *filp,
				const char __user *buf,
				size_t count,
				loff_t *f_pos)
{
	struct sh_2dg_miscdevice *shdev;
	size_t n;

	shdev = sh_2dg_get(filp);
	if (!shdev)
		return -ENODEV;

	if (!IS_ALIGNED(count, sizeof(u32)))
		return -EINVAL;

	n = count / sizeof(u32);
	if (n >= shdev->cb.size)
		return -EINVAL;

	if ((shdev->cb.filled + n) >= shdev->cb.size) {
		int rc = sh_2dg_exec(shdev);
		if (rc)
			return (ssize_t)rc;
	}

	if (shdev->status & SH_2DG_SR_CER)
		return -EIO;

	copy_from_user(shdev->cb.va[shdev->cb.index] + shdev->cb.filled,
			buf, count);
	shdev->cb.filled += n;

	return (ssize_t)count;
}

static int sh_2dg_fsync(struct file *filp,
			loff_t start,
			loff_t end,
			int datasync)
{
	struct sh_2dg_miscdevice *shdev;
	int rc;

	shdev = sh_2dg_get(filp);
	if (!shdev)
		return -ENODEV;

	if (shdev->cb.filled > 0) {
		rc = sh_2dg_exec(shdev);
		if (rc)
			return rc;
	}

	if (!datasync) {
		rc = down_interruptible(&shdev->sem);
		if (rc)
			return rc;
		up(&shdev->sem);
	}

	return (shdev->status & SH_2DG_SR_CER) ? -EIO : 0;
}

static long sh_2dg_ioctl(struct file *filp,
			unsigned int cmd,
			unsigned long arg)
{
	struct sh_2dg_miscdevice *shdev;

	shdev = sh_2dg_get(filp);
	if (!shdev)
		return -ENODEV;

	switch (cmd) {
	case SH_2DG_IOC_RESET:
		return sh_2dg_reset(shdev);
	case SH_2DG_IOC_GETREG:
	case SH_2DG_IOC_SETREG:
		return sh_2dg_access(shdev, _IOC_DIR(cmd), arg);
	default:
		return -EINVAL;
	}
}

static struct file_operations sh_2dg_fops = {
	.open		= sh_2dg_open,
	.release	= sh_2dg_release,
	.write		= sh_2dg_write,
	.fsync		= sh_2dg_fsync,
	.unlocked_ioctl	= sh_2dg_ioctl,
};

static int sh_2dg_probe(struct platform_device *pdev)
{
	int ret;
	struct sh_2dg_miscdevice *shdev;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "invalid resource\n");
		return -EINVAL;
	}

	shdev = sh_2dg_create();
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

	sema_init(&shdev->sem, 1);
	shdev->irq = platform_get_irq(pdev, 0);
	ret = request_irq(shdev->irq, sh_2dg_interrupt, 0, shdev->name, shdev);
	if (ret) {
		dev_err(&pdev->dev, "reqest_irq failed\n");
		goto err_unmap;
	}

	shdev->misc.name = shdev->name;
	shdev->misc.minor = MISC_DYNAMIC_MINOR;
	shdev->misc.fops = &sh_2dg_fops;

	ret = misc_register(&shdev->misc);
	if (ret) {
		dev_err(&pdev->dev, "misc_register failed\n");
		goto err_irq;
	}

        shdev->clk = clk_get(&pdev->dev, "2dg");
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
	sh_2dg_delete(shdev);
	return ret;
}

static int sh_2dg_remove(struct platform_device *pdev)
{
	struct sh_2dg_miscdevice *shdev;

	shdev = (struct sh_2dg_miscdevice *)platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	misc_deregister(&shdev->misc);
	clk_disable(shdev->clk);
	clk_put(shdev->clk);
	free_irq(shdev->irq, shdev);
	iounmap(shdev->reg.base);
	sh_2dg_delete(shdev);
	return 0;
}

static struct platform_driver sh_2dg_driver = {
	.probe	= sh_2dg_probe,
	.remove	= sh_2dg_remove,
	.driver	= {
		.name	= "sh_2dg",
	},
};

static struct resource sh_2dg_resources[] = {
	[0] = {
		.start	= 0xFFE80000,
		.end	= 0xFFE800FC - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= evt2irq(0x780),
		.flags	= IORESOURCE_IRQ,
	},
};

static void sh_2dg_device_release(struct device *dev)
{
}

static struct platform_device sh_2dg_device = {
	.name		= "sh_2dg",
	.id		= -1,
	.resource	= sh_2dg_resources,
	.num_resources	= ARRAY_SIZE(sh_2dg_resources),
	.dev = {
		.release = sh_2dg_device_release,
	},
};

static int __init sh_2dg_driver_init(void)
{
	int ret;

	ret = platform_device_register(&sh_2dg_device);
	if (ret)
		return ret;

	ret = platform_driver_register(&sh_2dg_driver);
	if (ret)
		platform_device_unregister(&sh_2dg_device);

	return ret;
}

module_init(sh_2dg_driver_init);

static void __exit sh_2dg_driver_exit(void)
{
	platform_driver_unregister(&sh_2dg_driver);
	platform_device_unregister(&sh_2dg_device);
}

module_exit(sh_2dg_driver_exit);

MODULE_DESCRIPTION("Renesas 2DG driver");
MODULE_AUTHOR("Sosuke Tokunaga");
MODULE_LICENSE("GPL");


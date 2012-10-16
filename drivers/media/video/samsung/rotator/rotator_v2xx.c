/* linux/drivers/media/video/samsung/rotator/rotator_v2xx.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Core file for Interface of the Samsung Rotator driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/sched.h>
//#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/pm_runtime.h>

#include <asm/page.h>
#include <asm/irq.h>

#include <plat/pd.h>

#include <mach/hardware.h>
#include <mach/map.h>

#include "rotator_v2xx.h"
#include "regs-rotator.h"

struct rot_ctrl	s5p_rot;

void rotator_power_onoff(bool onoff)
{
	if (onoff) {
#ifdef CONFIG_PM_RUNTIME
		pm_runtime_get_sync(s5p_rot.dev);
#endif
		/* clock enable */
		clk_enable(s5p_rot.clock);
	} else {
		/* clock disable */
		clk_disable(s5p_rot.clock);
#ifdef CONFIG_PM_RUNTIME
		pm_runtime_put_sync(s5p_rot.dev);
#endif
	}
}
void rotator_set_src(struct rot_ctrl *ctrl, struct rot_param *params)
{
	/* src address */
	writel(params->src_base[0], ctrl->regs + S5P_ROT_SRCBASEADDR0);
	writel(params->src_base[1], ctrl->regs + S5P_ROT_SRCBASEADDR1);
	writel(params->src_base[2], ctrl->regs + S5P_ROT_SRCBASEADDR2);

	/* src full size */
	writel(S5P_ROT_HEIGHT(params->src_full.height) |
			S5P_ROT_WIDTH(params->src_full.width),
			ctrl->regs + S5P_ROT_SRCIMGSIZE);

	/* src crop size */
	writel(S5P_ROT_HEIGHT(params->src_crop.height) |
			S5P_ROT_WIDTH(params->src_crop.width),
			ctrl->regs + S5P_ROT_SRCROTSIZE);

	/* src crop left, top */
	writel(S5P_ROT_LEFT(params->src_crop.left) |
			S5P_ROT_TOP(params->src_crop.top),
			ctrl->regs + S5P_ROT_SRC_XY);
}

void rotator_set_dst(struct rot_ctrl *ctrl, struct rot_param *params)
{
	/* dst address */
	writel(params->dst_base[0], ctrl->regs + S5P_ROT_DSTBASEADDR0);
	writel(params->dst_base[1], ctrl->regs + S5P_ROT_DSTBASEADDR1);
	writel(params->dst_base[2], ctrl->regs + S5P_ROT_DSTBASEADDR2);

	/* dst base size */
	writel(S5P_ROT_HEIGHT(params->dst_full.height) |
			S5P_ROT_WIDTH(params->dst_full.width),
			ctrl->regs + S5P_ROT_DSTIMGSIZE);

	/* dst window offset */
	writel(S5P_ROT_LEFT(params->dst_crop.left) |
			S5P_ROT_TOP(params->dst_crop.top),
			ctrl->regs + S5P_ROT_DST_XY);
}

void rotator_set_fmt(struct rot_ctrl *ctrl, struct rot_param *params)
{
	u32 cfg;
    
	cfg = readl(ctrl->regs + S5P_ROT_CTRL);
	cfg &= ~S5P_ROT_CTRL_INPUT_FMT_MASK;
	cfg |= S5P_ROT_SRC_FMT(params->fmt);

	writel(cfg, ctrl->regs + S5P_ROT_CTRL);
}

void rotator_set_degree_flip(struct rot_ctrl *ctrl, struct rot_param *params)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S5P_ROT_CTRL);
	cfg &= ~S5P_ROT_CTRL_DEGREE_MASK;
	cfg |= (S5P_ROT_DEGREE(params->degree) | S5P_ROT_FLIP(params->flip));

	writel(cfg, ctrl->regs + S5P_ROT_CTRL);
}

void rotator_start(struct rot_ctrl *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S5P_ROT_CTRL);
	cfg |= S5P_ROT_CTRL_START_ROTATE;
	writel(cfg, ctrl->regs + S5P_ROT_CTRL);
}

int rotator_check_vars(struct rot_param	*params)
{
	u32 align, check_align = 0;
	u32 min_w, max_w;
	u32 min_h, max_h;

	switch (params->fmt) {
	case ROT_YUV420_3P:
		align =	16;
		min_w =	64;
		min_h =	32;
		max_w =	64 * SZ_1K;
		max_h =	64 * SZ_1K;
		break;
	case ROT_YUV420_2P:
		align =	8;
		min_w =	32;
		min_h =	32;
		max_w =	64 * SZ_1K;
		max_h =	64 * SZ_1K;
		break;
	case ROT_YUV422_1P:
		align =	4;
		min_w =	16;
		min_h =	16;
		max_w =	32 * SZ_1K;
		max_h =	32 * SZ_1K;
		break;
	case ROT_RGB565:
		align =	4;
		min_w =	16;
		min_h =	16;
		max_w =	32 * SZ_1K;
		max_h =	32 * SZ_1K;
		break;
	case ROT_RGB888:
		align =	2;
		min_w =	8;
		min_h =	8;
		max_w =	16 * SZ_1K;
		max_h =	16 * SZ_1K;
		break;
	default:
		printk(KERN_ERR	"[%s:%d] source	format error\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	check_align |= (params->src_full.width % align)	? (0x1 << 0) : 0;
	check_align |= (params->src_full.height	% align) ? (0x1	<< 1) :	0;
	check_align |= (params->src_crop.width % align)	? (0x1 << 2) : 0;
	check_align |= (params->src_crop.height	% align) ? (0x1	<< 3) :	0;
	check_align |= (params->src_crop.left %	align) ? (0x1 << 4) : 0;
	check_align |= (params->src_crop.top % align) ?	(0x1 <<	5) : 0;

	check_align |= (params->dst_full.width % align)	? (0x1 << 6) : 0;
	check_align |= (params->dst_full.height	% align) ? (0x1	<< 7) :	0;
	check_align |= (params->dst_crop.left %	align) ? (0x1 << 8) : 0;
	check_align |= (params->dst_crop.top % align) ?	(0x1 <<	9) : 0;

	if (check_align) {
		printk(KERN_ERR	"[%s:%d] align error 0x%08x\n",
				__func__, __LINE__, check_align);
		return -EINVAL;
	}

	if (params->src_full.width > max_w || params->src_full.width < min_w) {
		printk(KERN_ERR	"[%s:%d] src size error\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (params->dst_full.width > max_w || params->dst_full.width < min_w) {
		printk(KERN_ERR	"[%s:%d] source	formant	error\n",
				__func__, __LINE__);
		return -EINVAL;

	}

	return 0;
}

u32 rotator_get_status(struct rot_ctrl *ctrl)
{
	u32 cfg	= 0;

	cfg = readl(ctrl->regs + S5P_ROT_STATUS);
	cfg &= S5P_ROT_CONFIG_STATUS_MASK;

	return cfg;
}

void rotator_enable_int(struct rot_ctrl	*ctrl)
{
	u32 cfg;
	cfg = readl(ctrl->regs + S5P_ROT_CONFIG);
	cfg |= S5P_ROT_CONFIG_ENABLE_INT;

	writel(cfg, ctrl->regs + S5P_ROT_CONFIG);
}

void rotator_disable_int(struct	rot_ctrl *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S5P_ROT_CONFIG);
	cfg &= ~S5P_ROT_CONFIG_ENABLE_INT;

	writel(cfg, ctrl->regs + S5P_ROT_CONFIG);
}

irqreturn_t rotator_irq(int irq, void *dev_id)
{
	struct rot_ctrl	*ctrl =	&s5p_rot;
	u32 cfg;

	cfg = readl(ctrl->regs + S5P_ROT_STATUS);
	cfg |= S5P_ROT_STATREG_INT_PENDING;

	writel(cfg, ctrl->regs + S5P_ROT_STATUS);

	ctrl->status = ROT_IDLE;

	wake_up(&ctrl->wq);

	return IRQ_HANDLED;
}

int rotator_open(struct	inode *inode, struct file *file)
{
	struct rot_ctrl	*ctrl =	&s5p_rot;
	struct rot_param *params;

	/* allocating the rotator instance */
	params	= kmalloc(sizeof(struct rot_param), GFP_KERNEL);
	if (params == NULL) {
        printk("%s %s %d\n",__FILE__, __func__, __LINE__);
		printk(KERN_ERR	"Instance memory allocation was	failed\n");
		return -ENOMEM;
	}
	memset(params, 0, sizeof(struct	rot_param));
	file->private_data = (struct rot_param *)params;

	atomic_inc(&ctrl->in_use);

	if (atomic_read(&ctrl->in_use) == 1) {
		/*ret = s5pv210_pd_enable("rotator_pd");
		if (ret < 0) {
			printk(KERN_ERR "failed to enable rotator power domain\n");
			return -ENOMEM;
		}
		clk_enable(ctrl->clock);*/
		rotator_power_onoff(true);
		rotator_enable_int(ctrl);

	}
	return 0;
}

int rotator_release(struct inode *inode, struct	file *file)
{
	struct rot_ctrl	*ctrl =	&s5p_rot;
	struct rot_param *params;

	params	= (struct rot_param *)file->private_data;
	if (params == NULL) {
		printk(KERN_ERR	"Can't release rotator!!\n");
		return -1;
	}
	kfree(params);

	atomic_dec(&ctrl->in_use);
	if (atomic_read(&ctrl->in_use) == 0) {
		rotator_disable_int(ctrl);
		rotator_power_onoff(false);
		/*
		clk_disable(ctrl->clock);

		ret = s5pv210_pd_disable("rotator_pd");
		if (ret < 0) {
			printk(KERN_ERR	"failed to disable rotator power domain\n");
			return -1;
		}*/
	}

	return 0;
}

static long rotator_ioctl(struct file *file, u32 cmd, unsigned long arg)
{
	struct rot_ctrl	*ctrl =	&s5p_rot;
	struct rot_param *params;
	struct rot_param *parg;
	int ret;

	if (rotator_get_status(ctrl) !=	S5P_ROT_STATREG_STATUS_IDLE) {
		printk(KERN_ERR	"Rotator state : %x\n",
						rotator_get_status(ctrl));
		return -EBUSY;
	}

	mutex_lock(&ctrl->lock);

	params	 = (struct rot_param *)file->private_data;
	parg	 = (struct rot_param *)arg;
	ret = copy_from_user(params, parg, sizeof(struct rot_param));
	if (ret) {
		printk(KERN_ERR	"%s: error : copy_from_user\n",	__func__);
		mutex_unlock(&ctrl->lock);
		return -EINVAL;
	}

	ret = rotator_check_vars(params);
	if (ret) {
		printk(KERN_ERR	"%s: invalid parameters\n", __func__);
		mutex_unlock(&ctrl->lock);
		return -EINVAL;
	}

	/* set parameter to regs */
	rotator_set_src(ctrl, params);
	rotator_set_dst(ctrl, params);
	rotator_set_fmt(ctrl, params);
	rotator_set_degree_flip(ctrl, params);

	rotator_start(ctrl);
	ctrl->status = ROT_RUN;

	if (!(file->f_flags & O_NONBLOCK)) {
		ret = wait_event_timeout(ctrl->wq, (ctrl->status == ROT_IDLE),
				ROTATOR_TIMEOUT);
		if (ret	== 0) {
			ctrl->status = ROT_IDLE;
			printk(KERN_ERR	"%s: Interrupt timeout\n", __func__);
		}
	}

    	printk("%s %s %d\n",__FILE__, __func__, __LINE__);
	mutex_unlock(&ctrl->lock);

	return 0;
}

static u32 rotator_poll(struct file *file, poll_table *wait)
{
	struct rot_ctrl	*ctrl =	&s5p_rot;
	u32 mask = 0;

	poll_wait(file,	&ctrl->wq, wait);

	if (rotator_get_status(ctrl) ==	S5P_ROT_STATREG_STATUS_IDLE)
		mask = POLLOUT | POLLWRNORM;

	return mask;
}

const struct file_operations rotator_fops = {
	.owner 		= THIS_MODULE,
	.open 		= rotator_open,
	.release 	= rotator_release,
	.unlocked_ioctl = rotator_ioctl,
	.poll 		= rotator_poll,
};

static struct miscdevice rotator_dev = {
	.minor = ROTATOR_MINOR,
	.name =	"s5p-rotator",
	.fops =	&rotator_fops,
};

static int __devinit rotator_probe(struct platform_device *pdev)
{
	struct rot_ctrl	*ctrl =	&s5p_rot;
	struct resource	*res;
	int ret;

	printk(KERN_INFO "rotator_probe	called\n");
	sprintf(ctrl->name, "%s", ROTATOR_NAME);

	/* Clock setting */
	sprintf(ctrl->clk_name,	"%s", ROT_CLK_NAME);
	ctrl->clock = clk_get(&pdev->dev, ctrl->clk_name);
	if (IS_ERR(ctrl->clock)) {
		printk(KERN_ERR	"failed	to get rotator clock source\n");
		return -EPERM;
	}
	clk_enable(ctrl->clock);


	/* IRQ handling	*/
	ctrl->irq_num =	platform_get_irq(pdev, 0);
	if (ctrl->irq_num <= 0)	{
		printk(KERN_ERR	"failed	to get irq resource\n");
		return -ENOENT;
	}

	ret = request_irq(ctrl->irq_num, rotator_irq, IRQF_DISABLED,
		pdev->name, NULL);
	if (ret) {
		printk(KERN_ERR	"request_irq(Rotator) failed.\n");
		return ret;
	}

	/* get the memory region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res	== NULL) {
		printk(KERN_ERR	"failed	to get memory region resouce\n");
		return -ENOENT;
	}

	res = request_mem_region(res->start, res->end -	res->start + 1,
			pdev->name);
	if (res	== NULL) {
		printk(KERN_ERR	"failed	to reserved memory region\n");
		return -ENOENT;
	}

	ctrl->regs = ioremap(res->start, res->end - res->start + 1);
	if (ctrl->regs == NULL)	{
		printk(KERN_ERR	"failed	ioremap\n");
		return -ENOENT;
	}

	ret = misc_register(&rotator_dev);
	if (ret) {
		printk(KERN_ERR	"cannot	register miscdev on minor=%d (%d)\n",
			ROTATOR_MINOR, ret);
		return ret;
	}

	init_waitqueue_head(&ctrl->wq);
	mutex_init(&ctrl->lock);

	clk_disable(ctrl->clock);

	ctrl->dev = &pdev->dev;
	printk(KERN_INFO "rotator_probe	success\n");

	return 0;
}

static int __devexit rotator_remove(struct platform_device *pdev)
{
	struct rot_ctrl	*ctrl =	&s5p_rot;
	struct resource	*res;

	clk_disable(ctrl->clock);
	free_irq(ctrl->irq_num,	NULL);

	if (ctrl->regs != NULL)	{
		printk(KERN_INFO "Rotator Driver, releasing resource\n");
		iounmap(ctrl->regs);

		/* get the memory region */
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (res	== NULL) {
			printk(KERN_ERR	"failed	to platform_get_resource\n");
			return -ENOENT;
		}

		release_mem_region(res->start, res->end	- res->start);
	}

	misc_deregister(&rotator_dev);

	return 0;
}

static int rotator_suspend(struct platform_device *dev,	pm_message_t state)
{
	struct rot_ctrl	*ctrl =	&s5p_rot;
	u32 i =	0;

	if (ctrl->status != ROT_IDLE) {
		ctrl->status = ROT_READY_SLEEP;

		while (i++ > 1000) {
			if (ctrl->status == ROT_IDLE)
				break;

			printk(KERN_ERR	"Rotator is running.\n");
		}
	} else {
		ctrl->status = ROT_READY_SLEEP;
	}

	ctrl->status = ROT_SLEEP;
	/*clk_disable(ctrl->clock);*/
	rotator_power_onoff(false);

	return 0;
}

static int rotator_resume(struct platform_device *pdev)
{
	struct rot_ctrl	*ctrl =	&s5p_rot;

	/*ret = s5pv210_pd_enable("rotator_pd");
	if (ret < 0) {
		printk(KERN_ERR	"failed to enable rotator power domain\n");
		return -1;
	}

	clk_enable(ctrl->clock);*/
	rotator_power_onoff(true);
	ctrl->status = ROT_IDLE;

	rotator_enable_int(ctrl);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int rotator_runtime_suspend(struct device * pdev)
{
	return 0;
}

static int rotator_runtime_resume(struct device * pdev)
{
	return 0;
}

static struct dev_pm_ops rotator_pm_runtime = {
	.runtime_suspend = rotator_runtime_suspend,
	.runtime_resume = rotator_runtime_resume,
};
#endif

static struct platform_driver s5p_rotator_driver = {
	.probe		= rotator_probe,
	.remove		= __devexit_p(rotator_remove),
	.suspend	= rotator_suspend,
	.resume		= rotator_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s5p-rotator",
#ifdef CONFIG_PM_RUNTIME
		.pm = &rotator_pm_runtime,
#endif
	},
};

static char banner[] __initdata	= KERN_INFO \
			"S5P Rotator Driver, (c) 2008 Samsung Electronics\n";
int __init rotator_init(void)
{
	u32 ret;
	printk(banner);
    
	ret = platform_driver_register(&s5p_rotator_driver);
	if (ret	!= 0) {
		printk(KERN_ERR
		       "rotator_driver platform	device register	failed\n");
		return -1;
	}

	return 0;
}

void __exit rotator_exit(void)
{
	platform_driver_unregister(&s5p_rotator_driver);
	mutex_destroy(&s5p_rot.lock);

	printk("s5p_rotator_driver exit\n");
}

module_init(rotator_init);
module_exit(rotator_exit);

MODULE_AUTHOR("Jonghun Han <jonghun.han@samsung.com>");
MODULE_DESCRIPTION("S5P	Image Rotator Device Driver");
MODULE_LICENSE("GPL");


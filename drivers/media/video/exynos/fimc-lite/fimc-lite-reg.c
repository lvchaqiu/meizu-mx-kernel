/*
 * Register interface file for Samsung Camera Interface (FIMC-Lite) driver
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/io.h>
#include <media/exynos_flite.h>
#include <mach/map.h>

#include "fimc-lite-core.h"

static int reg_debug;
module_param(reg_debug, int, 0644);
MODULE_PARM_DESC(reg_debug, "Enable module debug trace. Set to 1 to enable.");

void flite_hw_set_cam_source_size(struct flite_dev *dev)
{
	struct flite_frame *f_frame =  &dev->source_frame;
	u32 cfg = 0;

	cfg = readl(dev->regs + FLITE_REG_CISRCSIZE);

	cfg |= FLITE_REG_CISRCSIZE_SIZE_H(f_frame->o_width);
	cfg |= FLITE_REG_CISRCSIZE_SIZE_V(f_frame->o_height);

	writel(cfg, dev->regs + FLITE_REG_CISRCSIZE);
}

void flite_hw_set_cam_channel(struct flite_dev *dev)
{
	u32 cfg = readl(dev->regs + FLITE_REG_CIGENERAL);

	if (dev->id == 0)
		cfg &= FLITE_REG_CIGENERAL_CAM_A;
	else
		cfg |= FLITE_REG_CIGENERAL_CAM_B;

	writel(cfg, dev->regs + FLITE_REG_CIGENERAL);
}

void flite_hw_reset(struct flite_dev *dev)
{
	u32 cfg = 0;
	unsigned long timeo = jiffies + FLITE_MAX_RESET_READY_TIME;

	cfg = readl(dev->regs + FLITE_REG_CIGCTRL);
	cfg |= FLITE_REG_CIGCTRL_SWRST_REQ;
	writel(cfg, dev->regs + FLITE_REG_CIGCTRL);

	do {
		if (cfg & FLITE_REG_CIGCTRL_SWRST_RDY)
			break;
		usleep_range(1000, 5000);
	} while(time_before(jiffies, timeo));

	flite_dbg("wait time : %d ms",
		jiffies_to_msecs(jiffies - timeo + FLITE_MAX_RESET_READY_TIME));

	cfg |= FLITE_REG_CIGCTRL_SWRST;
	writel(cfg, dev->regs + FLITE_REG_CIGCTRL);
}

/* Support only FreeRun mode
 * If output DMA is supported, I will implement one shot mode
 * with Cpt_FrCnt and Cpt_FrEn
 */

void flite_hw_set_capture_start(struct flite_dev *dev)
{
	u32 cfg = 0;

	cfg = readl(dev->regs + FLITE_REG_CIIMGCPT);
	cfg |= FLITE_REG_CIIMGCPT_IMGCPTEN;

	writel(cfg, dev->regs + FLITE_REG_CIIMGCPT);
}

void flite_hw_set_capture_stop(struct flite_dev *dev)
{
	u32 cfg = 0;

	cfg = readl(dev->regs + FLITE_REG_CIIMGCPT);
	cfg &= ~FLITE_REG_CIIMGCPT_IMGCPTEN;

	writel(cfg, dev->regs + FLITE_REG_CIIMGCPT);
}

int flite_hw_set_source_format(struct flite_dev *dev)
{
	struct v4l2_mbus_framefmt *mbus_fmt = &dev->mbus_fmt;
	struct flite_fmt const *f_fmt = find_flite_format(mbus_fmt);
	u32 cfg = 0;

	if (!f_fmt) {
		flite_err("f_fmt is null");
		return -EINVAL;
	}

	cfg = readl(dev->regs + FLITE_REG_CIGCTRL);
	cfg |= f_fmt->fmt_reg;
	writel(cfg, dev->regs + FLITE_REG_CIGCTRL);

	if (f_fmt->is_yuv) {
		cfg = readl(dev->regs + FLITE_REG_CISRCSIZE);
		switch(f_fmt->code) {
		case V4L2_MBUS_FMT_YUYV8_2X8:
			cfg |= FLITE_REG_CISRCSIZE_ORDER422_IN_YCBYCR;
			break;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			cfg |= FLITE_REG_CISRCSIZE_ORDER422_IN_YCRYCB;
			break;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			cfg |= FLITE_REG_CISRCSIZE_ORDER422_IN_CBYCRY;
			break;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			cfg |= FLITE_REG_CISRCSIZE_ORDER422_IN_CRYCBY;
			break;
		default:
			flite_err("not supported mbus code");
			return -EINVAL;
		}
		writel(cfg, dev->regs + FLITE_REG_CISRCSIZE);
	}
	return 0;
}

void flite_hw_set_shadow_mask(struct flite_dev *dev, bool enable)
{
	u32 cfg = 0;
	cfg = readl(dev->regs + FLITE_REG_CIGCTRL);

	if (enable)
		cfg &= ~FLITE_REG_CIGCTRL_SHADOWMASK_DISABLE;
	else
		cfg |= FLITE_REG_CIGCTRL_SHADOWMASK_DISABLE;

	writel(cfg, dev->regs + FLITE_REG_CIGCTRL);
}

void flite_hw_set_output_dma(struct flite_dev *dev, bool enable)
{
	u32 cfg = 0;
	cfg = readl(dev->regs + FLITE_REG_CIGCTRL);

	if (enable)
		cfg &= ~FLITE_REG_CIGCTRL_ODMA_DISABLE;
	else
		cfg |= FLITE_REG_CIGCTRL_ODMA_DISABLE;

	writel(cfg, dev->regs + FLITE_REG_CIGCTRL);
}

void flite_hw_set_test_pattern_enable(struct flite_dev *dev)
{
	u32 cfg = 0;
	cfg = readl(dev->regs + FLITE_REG_CIGCTRL);
	cfg |= FLITE_REG_CIGCTRL_TEST_PATTERN_COLORBAR;

	writel(cfg, dev->regs + FLITE_REG_CIGCTRL);
}

void flite_hw_set_config_irq(struct flite_dev *dev, struct
		s3c_platform_camera *cam)
{
	u32 cfg = 0;
	cfg = readl(dev->regs + FLITE_REG_CIGCTRL);
	cfg &= ~(FLITE_REG_CIGCTRL_INVPOLPCLK | FLITE_REG_CIGCTRL_INVPOLVSYNC
			| FLITE_REG_CIGCTRL_INVPOLHREF);

	if (cam->inv_pclk)
		cfg |= FLITE_REG_CIGCTRL_INVPOLPCLK;
	if (cam->inv_vsync)
		cfg |= FLITE_REG_CIGCTRL_INVPOLVSYNC;
	if (cam->inv_href)
		cfg |= FLITE_REG_CIGCTRL_INVPOLHREF;

	writel(cfg, dev->regs + FLITE_REG_CIGCTRL);
}

void flite_hw_set_interrupt_source(struct flite_dev *dev, u32 source)
{
	u32 cfg = 0;
	cfg = readl(dev->regs + FLITE_REG_CIGCTRL);
	cfg |= source;

	writel(cfg, dev->regs + FLITE_REG_CIGCTRL);
}

void flite_hw_set_camera_type(struct flite_dev *dev, struct
		s3c_platform_camera *cam)
{
	u32 cfg = 0;
	cfg = readl(dev->regs + FLITE_REG_CIGCTRL);

	if (cam->type == CAM_TYPE_ITU)
		cfg &= ~FLITE_REG_CIGCTRL_SELCAM_MIPI;
	else
		cfg |= FLITE_REG_CIGCTRL_SELCAM_MIPI;

	writel(cfg, dev->regs + FLITE_REG_CIGCTRL);
}

void flite_hw_set_window_offset(struct flite_dev *dev)
{
	u32 cfg = 0;
	u32 hoff2, voff2;
	struct flite_frame *f_frame = &dev->source_frame;

	cfg = readl(dev->regs + FLITE_REG_CIWDOFST);
	cfg &= ~(FLITE_REG_CIWDOFST_HOROFF_MASK |
		FLITE_REG_CIWDOFST_VEROFF_MASK);
	cfg |= FLITE_REG_CIWDOFST_WINOFSEN |
		FLITE_REG_CIWDOFST_WINHOROFST(f_frame->offs_h) |
		FLITE_REG_CIWDOFST_WINVEROFST(f_frame->offs_v);

	writel(cfg, dev->regs + FLITE_REG_CIWDOFST);

	hoff2 = f_frame->o_width - f_frame->width - f_frame->offs_h;
	voff2 = f_frame->o_height - f_frame->height - f_frame->offs_v;
	cfg = FLITE_REG_CIWDOFST2_WINHOROFST2(hoff2) |
		FLITE_REG_CIWDOFST2_WINVEROFST2(voff2);

	writel(cfg, dev->regs + FLITE_REG_CIWDOFST2);
}

void flite_hw_set_last_capture_end_clear(struct flite_dev *dev)
{
	u32 cfg = 0;

	cfg = readl(dev->regs + FLITE_REG_CISTATUS2);
	cfg &= ~FLITE_REG_CISTATUS2_LASTCAPEND;

	writel(cfg, dev->regs + FLITE_REG_CISTATUS2);
}

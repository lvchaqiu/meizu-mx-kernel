/*
 * Register interface file for Samsung Camera Interface (FIMC-Lite) driver
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#if defined(CONFIG_MEDIA_CONTROLLER) && defined(CONFIG_ARCH_EXYNOS5)
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#endif
#include "fimc-lite-core.h"

#define MODULE_NAME			"exynos-fimc-lite"
#define DEFAULT_FLITE_SINK_WIDTH	800
#define DEFAULT_FLITE_SINK_HEIGHT	480

static struct flite_fmt flite_formats[] = {
	{
		.name		= "YUV422 8-bit 1 plane(UYVY)",
		.code		= V4L2_MBUS_FMT_UYVY8_2X8,
		.fmt_reg	= FLITE_REG_CIGCTRL_YUV422_1P,
		.is_yuv		= 1,
	},{
		.name		= "YUV422 8-bit 1 plane(VYUY)",
		.code		= V4L2_MBUS_FMT_VYUY8_2X8,
		.fmt_reg	= FLITE_REG_CIGCTRL_YUV422_1P,
		.is_yuv		= 1,
	},{
		.name		= "YUV422 8-bit 1 plane(YUYV)",
		.code		= V4L2_MBUS_FMT_YUYV8_2X8,
		.fmt_reg	= FLITE_REG_CIGCTRL_YUV422_1P,
		.is_yuv		= 1,
	},{
		.name		= "YUV422 8-bit 1 plane(YVYU)",
		.code		= V4L2_MBUS_FMT_YVYU8_2X8,
		.fmt_reg	= FLITE_REG_CIGCTRL_YUV422_1P,
		.is_yuv		= 1,
	},{
		/* ISP supports only GRBG order in 4212 */
		.name		= "RAW8(GRBG)",
		.code		= V4L2_MBUS_FMT_SGRBG8_1X8,
		.fmt_reg	= FLITE_REG_CIGCTRL_RAW8,
		.is_yuv		= 0,
	},{
		/* ISP supports only GRBG order in 4212 */
		.name		= "RAW10(GRBG)",
		.code		= V4L2_MBUS_FMT_SGRBG10_1X10,
		.fmt_reg	= FLITE_REG_CIGCTRL_RAW10,
		.is_yuv		= 0,
	},{
		/* ISP supports only GRBG order in 4212 */
		.name		= "RAW12(GRBG)",
		.code		= V4L2_MBUS_FMT_SGRBG12_1X12,
		.fmt_reg	= FLITE_REG_CIGCTRL_RAW12,
		.is_yuv		= 0,
	},{
		.name		= "User Defined(JPEG)",
		.code		= V4L2_MBUS_FMT_JPEG_1X8,
		.fmt_reg	= FLITE_REG_CIGCTRL_USER(1),
		.is_yuv		= 0,
	},
};

static struct flite_dev *to_flite_dev(struct v4l2_subdev *sdev)
{
	return container_of(sdev, struct flite_dev, sd);
}

inline struct flite_fmt const *find_flite_format(struct
		v4l2_mbus_framefmt *mf)
{
	int num_fmt = ARRAY_SIZE(flite_formats);

	while (num_fmt--)
		if (mf->code == flite_formats[num_fmt].code)
			break;
	if (num_fmt < 0)
		return NULL;

	return &flite_formats[num_fmt];
}

static int flite_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct flite_dev *flite = to_flite_dev(sd);
	struct flite_fmt const *f_fmt = find_flite_format(mf);
	struct flite_frame *f_frame = &flite->source_frame;

	flite_dbg("w: %d, h: %d", mf->width, mf->height);

	if (unlikely(!f_fmt)) {
		flite_err("f_fmt is null");
		return -EINVAL;
	}

	flite->mbus_fmt = *mf;

	/*
	 * These are the datas from fimc
	 * If you want to crop the image, you can use s_crop
	 */
	f_frame->o_width = mf->width;
	f_frame->o_height = mf->height;
	f_frame->width = mf->width;
	f_frame->height = mf->height;
	f_frame->offs_h = 0;
	f_frame->offs_v = 0;

	return 0;
}

static int flite_g_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct flite_dev *flite = to_flite_dev(sd);

	mf = &flite->mbus_fmt;

	return 0;
}

static int flite_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *cc)
{
	struct flite_dev *flite = to_flite_dev(sd);
	struct flite_frame *f;

	f = &flite->source_frame;

	cc->bounds.left		= 0;
	cc->bounds.top		= 0;
	cc->bounds.width	= f->o_width;
	cc->bounds.height	= f->o_height;
	cc->defrect		= cc->bounds;

	return 0;
}

static int flite_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *crop)
{
	struct flite_dev *flite = to_flite_dev(sd);
	struct flite_frame *f;

	f = &flite->source_frame;

	crop->c.left	= f->offs_h;
	crop->c.top	= f->offs_v;
	crop->c.width	= f->width;
	crop->c.height	= f->height;

	return 0;
}

static int flite_s_crop(struct v4l2_subdev *sd, struct v4l2_crop *crop)
{
	struct flite_dev *flite = to_flite_dev(sd);
	struct flite_frame *f;

	f = &flite->source_frame;

	if (crop->c.left + crop->c.width > f->o_width)
		return -EINVAL;
	if (crop->c.top + crop->c.height > f->o_height)
		return -EINVAL;

	f->width = crop->c.width;
	f->height = crop->c.height;
	f->offs_h = crop->c.left;
	f->offs_v = crop->c.top;

	flite_dbg("width : %d, height : %d, offs_h : %d, off_v : %dn",
			f->width, f->height, f->offs_h, f->offs_v);

	return 0;
}

static int flite_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct flite_dev *flite = to_flite_dev(sd);
	u32 index = flite->pdata->active_cam_index;
	struct s3c_platform_camera *cam = flite->pdata->cam[index];
	u32 int_src = FLITE_REG_CIGCTRL_IRQ_LASTEN0_ENABLE;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&flite->slock, flags);

	if (test_bit(FLITE_ST_SUSPENDED, &flite->state))
		goto s_stream_unlock;

	if (enable) {
		flite_hw_set_cam_channel(flite);
		flite_hw_set_cam_source_size(flite);
		flite_hw_set_camera_type(flite, cam);
		ret = flite_hw_set_source_format(flite);
		if (unlikely(ret < 0))
			goto s_stream_unlock;

		if (cam->use_isp)
			flite_hw_set_output_dma(flite, false);

		flite_hw_set_interrupt_source(flite, int_src);
		flite_hw_set_config_irq(flite, cam);
		flite_hw_set_window_offset(flite);
		flite_hw_set_capture_start(flite);

		set_bit(FLITE_ST_STREAMING, &flite->state);
	} else {
		if (test_bit(FLITE_ST_STREAMING, &flite->state)) {
			flite_hw_set_capture_stop(flite);
			spin_unlock_irqrestore(&flite->slock, flags);
			ret = wait_event_timeout(flite->irq_queue,
			!test_bit(FLITE_ST_STREAMING, &flite->state), HZ/20); /* 50 ms */
			if (unlikely(!ret)) {
				v4l2_err(sd, "wait timeout\n");
				ret = -EBUSY;
			}
		}

		return ret;
	}
s_stream_unlock:
	spin_unlock_irqrestore(&flite->slock, flags);
	return ret;
}

static irqreturn_t flite_irq_handler(int irq, void *priv)
{
	struct flite_dev *flite = priv;
	u32 int_src = 0;

	flite_hw_get_int_src(flite, &int_src);
	flite_hw_clear_irq(flite);

	spin_lock(&flite->slock);

	switch (int_src & FLITE_REG_CISTATUS_IRQ_MASK) {
	case FLITE_REG_CISTATUS_IRQ_SRC_OVERFLOW:
		flite_dbg("overflow generated");
		break;
	case FLITE_REG_CISTATUS_IRQ_SRC_LASTCAPEND:
		flite_hw_set_last_capture_end_clear(flite);
		flite_dbg("last capture end");
		clear_bit(FLITE_ST_STREAMING, &flite->state);
		wake_up(&flite->irq_queue);
		break;
	case FLITE_REG_CISTATUS_IRQ_SRC_FRMSTART:
		flite_dbg("frame start");
		break;
	case FLITE_REG_CISTATUS_IRQ_SRC_FRMEND:
		flite_dbg("frame end");
		break;
	}

	spin_unlock(&flite->slock);

	return IRQ_HANDLED;
}

static int flite_s_power(struct v4l2_subdev *sd, int on)
{
	struct flite_dev *flite = to_flite_dev(sd);
	int ret = 0;

	if (on) {
		pm_runtime_get_sync(&flite->pdev->dev);
		set_bit(FLITE_ST_POWERED, &flite->state);
	} else {
		pm_runtime_put_sync(&flite->pdev->dev);
		clear_bit(FLITE_ST_POWERED, &flite->state);
	}

	return ret;
}

#if defined(CONFIG_MEDIA_CONTROLLER) && defined(CONFIG_ARCH_EXYNOS5)
static int flite_subdev_enum_mbus_code(struct v4l2_subdev *sd,
				       struct v4l2_subdev_fh *fh,
				       struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(flite_formats))
		return -EINVAL;

	code->code = flite_formats[code->index].code;

	return 0;
}

static struct v4l2_mbus_framefmt *__flite_get_format(
		struct flite_dev *flite, struct v4l2_subdev_fh *fh,
		u32 pad, enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return fh ? v4l2_subdev_get_try_format(fh, pad) : NULL;
	else
		return &flite->mbus_fmt;
}

static void flite_try_format(struct flite_dev *flite, struct v4l2_subdev_fh *fh,
			     struct v4l2_mbus_framefmt *fmt,
			     enum v4l2_subdev_format_whence which)
{
	struct flite_fmt const *ffmt;
	struct flite_frame *f = &flite->source_frame;

	ffmt = find_flite_format(fmt);
	if (ffmt == NULL)
		ffmt = &flite_formats[1];

	fmt->code = ffmt->code;
	fmt->width = clamp_t(u32, fmt->width, 1, FLITE_MAX_WIDTH_SIZE);
	fmt->height = clamp_t(u32, fmt->height, 1, FLITE_MAX_HEIGHT_SIZE);

	f->offs_h = f->offs_v = 0;
	f->width = f->o_width = fmt->width;
	f->height = f->o_height = fmt->height;

	fmt->colorspace = V4L2_COLORSPACE_JPEG;
	fmt->field = V4L2_FIELD_NONE;
}

static int flite_subdev_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct flite_dev *flite = to_flite_dev(sd);
	struct v4l2_mbus_framefmt *mf;

	mf = __flite_get_format(flite, fh, fmt->pad, fmt->which);
	if (mf == NULL)
		return -EINVAL;

	fmt->format = *mf;

	if (fmt->pad != FLITE_PAD_SINK) {
		struct flite_frame *f = &flite->source_frame;
		fmt->format.width = f->width;
		fmt->format.height = f->height;
	}

	return 0;
}

static int flite_subdev_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct flite_dev *flite = to_flite_dev(sd);
	struct v4l2_mbus_framefmt *mf;

	if (fmt->pad != FLITE_PAD_SINK)
		return -EPERM;

	mf = __flite_get_format(flite, fh, fmt->pad, fmt->which);
	if (mf == NULL)
		return -EINVAL;

	flite_try_format(flite, fh, &fmt->format, fmt->which);
	*mf = fmt->format;

	return 0;
}

static void flite_try_crop(struct flite_dev *flite, struct v4l2_rect *crop)
{
	struct flite_frame *f_frame = &flite->source_frame;

	u32 max_left = f_frame->o_width - crop->width;
	u32 max_top = f_frame->o_height - crop->height;
	u32 crop_max_w = f_frame->o_width - crop->left;
	u32 crop_max_h = f_frame->o_height - crop->top;

	crop->left = clamp_t(u32, crop->left, 0, max_left);
	crop->top = clamp_t(u32, crop->top, 0, max_top);
	crop->width = clamp_t(u32, crop->width, 2, crop_max_w);
	crop->height = clamp_t(u32, crop->width, 1, crop_max_h);
}

static int __flite_get_crop(struct flite_dev *flite, struct v4l2_subdev_fh *fh,
			    unsigned int pad, enum v4l2_subdev_format_whence which,
			    struct v4l2_rect *crop)
{
	struct flite_frame *frame = &flite->source_frame;

	if (which == V4L2_SUBDEV_FORMAT_TRY) {
		crop = v4l2_subdev_get_try_crop(fh, pad);
	} else {
		crop->left = frame->offs_h;
		crop->top = frame->offs_v;
		crop->width = frame->width;
		crop->height = frame->height;
	}

	return 0;
}

static int flite_subdev_get_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_crop *crop)
{
	struct flite_dev *flite = to_flite_dev(sd);
	struct v4l2_rect fcrop;

	fcrop.left = fcrop.top = fcrop.width = fcrop.height = 0;

	if (crop->pad != FLITE_PAD_SINK)
		return -EINVAL;

	__flite_get_crop(flite, fh, crop->pad, crop->which, &fcrop);
	crop->rect = fcrop;

	return 0;
}

static int flite_subdev_set_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_crop *crop)
{
	struct flite_dev *flite = to_flite_dev(sd);
	struct flite_frame *f_frame = &flite->source_frame;

	if (crop->pad != FLITE_PAD_SINK)
		return -EINVAL;

	flite_try_crop(flite, &crop->rect);

	if (crop->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		f_frame->offs_h = crop->rect.left;
		f_frame->offs_v = crop->rect.top;
		f_frame->width = crop->rect.width;
		f_frame->height = crop->rect.height;
	}

	return 0;
}

static int flite_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;

	memset(&format, 0, sizeof(format));
	format.pad = FLITE_PAD_SINK;
	format.which = fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	format.format.code = V4L2_MBUS_FMT_YUYV8_2X8;
	format.format.width = DEFAULT_FLITE_SINK_WIDTH;
	format.format.height = DEFAULT_FLITE_SINK_HEIGHT;

	flite_subdev_set_fmt(sd, fh, &format);

	return 0;
}

static int flite_subdev_close(struct v4l2_subdev *sd,
			      struct v4l2_subdev_fh *fh)
{
	flite_info("");
	return 0;
}

static int flite_subdev_registered(struct v4l2_subdev *sd)
{
	flite_dbg("");
	return 0;
}

static void flite_subdev_unregistered(struct v4l2_subdev *sd)
{
	flite_dbg("");
}

static const struct v4l2_subdev_internal_ops flite_v4l2_internal_ops = {
	.open = flite_init_formats,
	.close = flite_subdev_close,
	.registered = flite_subdev_registered,
	.unregistered = flite_subdev_unregistered,
};

static int flite_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct flite_dev *flite = to_flite_dev(sd);

	flite_info("");
	switch (local->index | media_entity_type(remote->entity)) {
	case FLITE_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (flite->input != FLITE_INPUT_NONE) {
				flite_err("link is busy");
				return -EBUSY;
			}
			if (remote->index == CSIS_PAD_SOURCE)
				flite->input |= FLITE_INPUT_CSIS;
			else
				flite->input |= FLITE_INPUT_SENSOR;
		} else {
			flite->input = FLITE_INPUT_NONE;
		}
		break;

	case FLITE_PAD_SOURCE_PREVIEW | MEDIA_ENT_T_V4L2_SUBDEV: /* fall through */
	case FLITE_PAD_SOURCE_CAMCORDING | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED)
			flite->output = FLITE_OUTPUT_GSC;
		else
			flite->output = FLITE_OUTPUT_NONE;
		break;

	default:
		flite_err("ERR link");
		return -EINVAL;
	}

	return 0;
}

static const struct media_entity_operations flite_media_ops = {
	.link_setup = flite_link_setup,
};

static int flite_get_md_callback(struct device *dev, void *p)
{
	struct exynos_md **md_list = p;
	struct exynos_md *md = NULL;

	md = dev_get_drvdata(dev);

	if (md)
		*(md_list + md->id) = md;

	return 0; /* non-zero value stops iteration */
}

static struct exynos_md *flite_get_capture_md(enum mdev_node node)
{
	struct device_driver *drv;
	struct exynos_md *md[MDEV_MAX_NUM] = {NULL,};
	int ret;

	drv = driver_find(MDEV_MODULE_NAME, &platform_bus_type);
	if (!drv)
		return ERR_PTR(-ENODEV);

	ret = driver_for_each_device(drv, NULL, &md[0],
				     flite_get_md_callback);
	put_driver(drv);

	return ret ? NULL : md[node];

}

static struct v4l2_subdev_pad_ops flite_pad_ops = {
	.enum_mbus_code = flite_subdev_enum_mbus_code,
	.get_fmt	= flite_subdev_get_fmt,
	.set_fmt	= flite_subdev_set_fmt,
	.get_crop	= flite_subdev_get_crop,
	.set_crop	= flite_subdev_set_crop,
};
#endif

static int flite_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	struct flite_dev *flite = to_flite_dev(sd);
	unsigned long flags;

	spin_lock_irqsave(&flite->slock, flags);

	if (test_bit(FLITE_ST_STREAMING, &flite->state))
		flite_s_stream(sd, false);
	if (test_bit(FLITE_ST_POWERED, &flite->state))
		flite_s_power(sd, false);

	set_bit(FLITE_ST_SUSPENDED, &flite->state);

	spin_unlock_irqrestore(&flite->slock, flags);

	return 0;
}

static int flite_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	struct flite_dev *flite = to_flite_dev(sd);
	unsigned long flags;

	spin_lock_irqsave(&flite->slock, flags);

	if (test_bit(FLITE_ST_POWERED, &flite->state))
		flite_s_power(sd, true);
	if (test_bit(FLITE_ST_STREAMING, &flite->state))
		flite_s_stream(sd, true);

	clear_bit(FLITE_ST_SUSPENDED, &flite->state);

	spin_unlock_irqrestore(&flite->slock, flags);

	return 0;
}

static int flite_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	struct flite_dev *flite = to_flite_dev(sd);
	unsigned long flags;

	spin_lock_irqsave(&flite->slock, flags);

	set_bit(FLITE_ST_SUSPENDED, &flite->state);

	spin_unlock_irqrestore(&flite->slock, flags);

	return 0;
}

static int flite_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	struct flite_dev *flite = to_flite_dev(sd);
	unsigned long flags;

	spin_lock_irqsave(&flite->slock, flags);

	clear_bit(FLITE_ST_SUSPENDED, &flite->state);

	spin_unlock_irqrestore(&flite->slock, flags);

	return 0;
}

static struct v4l2_subdev_core_ops flite_core_ops = {
	.s_power = flite_s_power,
};

static struct v4l2_subdev_video_ops flite_video_ops = {
	.g_mbus_fmt	= flite_g_mbus_fmt,
	.s_mbus_fmt	= flite_s_mbus_fmt,
	.s_stream	= flite_s_stream,
	.cropcap	= flite_cropcap,
	.g_crop		= flite_g_crop,
	.s_crop		= flite_s_crop,
};

static struct v4l2_subdev_ops flite_subdev_ops = {
	.core	= &flite_core_ops,
#if defined(CONFIG_MEDIA_CONTROLLER) && defined(CONFIG_ARCH_EXYNOS5)
	.pad	= &flite_pad_ops,
#endif
	.video	= &flite_video_ops,
};

static int flite_probe(struct platform_device *pdev)
{
	struct resource *mem_res;
	struct resource *regs_res;
	struct flite_dev *flite;
	int ret = -ENODEV;

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "platform data is NULL\n");
		return -EINVAL;
	}

	flite = kzalloc(sizeof(struct flite_dev), GFP_KERNEL);
	if (!flite)
		return -ENOMEM;

	flite->pdev = pdev;
	flite->pdata = pdev->dev.platform_data;

	flite->id = pdev->id;

	init_waitqueue_head(&flite->irq_queue);
	spin_lock_init(&flite->slock);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		goto p_err1;
	}

	regs_res = request_mem_region(mem_res->start, resource_size(mem_res),
				      pdev->name);
	if (!regs_res) {
		dev_err(&pdev->dev, "Failed to request io memory region\n");
		goto p_err1;
	}

	flite->regs_res = regs_res;
	flite->regs = ioremap(mem_res->start, resource_size(mem_res));
	if (!flite->regs) {
		dev_err(&pdev->dev, "Failed to remap io region\n");
		goto p_err2;
	}

	flite->irq = platform_get_irq(pdev, 0);
	if (flite->irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		goto p_err3;
	}

	ret = request_irq(flite->irq, flite_irq_handler, 0, dev_name(&pdev->dev), flite);
	if (ret) {
		dev_err(&pdev->dev, "request_irq failed\n");
		goto p_err3;
	}

	v4l2_subdev_init(&flite->sd, &flite_subdev_ops);
	flite->sd.owner = THIS_MODULE;
	snprintf(flite->sd.name, sizeof(flite->sd.name), "%s.%d\n",
					MODULE_NAME, flite->id);
#if defined(CONFIG_MEDIA_CONTROLLER) && defined(CONFIG_ARCH_EXYNOS5)
	flite->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;

	flite->pads[FLITE_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	flite->pads[FLITE_PAD_SOURCE_PREVIEW].flags = MEDIA_PAD_FL_SOURCE;
	flite->pads[FLITE_PAD_SOURCE_CAMCORDING].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&flite->sd.entity, FLITE_PADS_NUM,
				flite->pads, 0);
	if (ret < 0)
		goto p_err3;

	flite_init_formats(&flite->sd, NULL);

	flite->sd.internal_ops = &flite_v4l2_internal_ops;
	flite->sd.entity.ops = &flite_media_ops;

	flite->mdev = flite_get_capture_md(MDEV_CAPTURE);
	if (IS_ERR_OR_NULL(flite->mdev))
		goto p_err3;

	ret = v4l2_device_register_subdev(&flite->mdev->v4l2_dev, &flite->sd);
	if (ret)
		goto p_err3;
#endif
	/* This allows to retrieve the platform device id by the host driver */
	v4l2_set_subdevdata(&flite->sd, pdev);

	/* .. and a pointer to the subdev. */
	platform_set_drvdata(pdev, &flite->sd);

	pm_runtime_enable(&pdev->dev);

	flite_info("fimc-lite%d probe success", pdev->id);

	return 0;

p_err3:
	iounmap(flite->regs);
p_err2:
	release_mem_region(regs_res->start, resource_size(regs_res));
p_err1:
	kfree(flite);
	return ret;
}

static int flite_remove(struct platform_device *pdev)
{
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	struct flite_dev *flite = to_flite_dev(sd);
	struct resource *res = flite->regs_res;

	flite_s_power(&flite->sd, 0);
	pm_runtime_disable(&pdev->dev);
	free_irq(flite->irq, flite);
	iounmap(flite->regs);
	release_mem_region(res->start, resource_size(res));
	kfree(flite);

	return 0;
}

static const struct dev_pm_ops flite_pm_ops = {
	.suspend		= flite_suspend,
	.resume			= flite_resume,
	.runtime_suspend	= flite_runtime_suspend,
	.runtime_resume		= flite_runtime_resume,
};

static struct platform_driver flite_driver = {
	.probe		= flite_probe,
	.remove	= __devexit_p(flite_remove),
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &flite_pm_ops,
	}
};

static int __init flite_init(void)
{
	int ret = platform_driver_register(&flite_driver);
	if (ret)
		flite_err("platform_driver_register failed: %d", ret);
	return ret;
}

static void __exit flite_exit(void)
{
	platform_driver_unregister(&flite_driver);
}
module_init(flite_init);
module_exit(flite_exit);

MODULE_AUTHOR("Sky Kang<sungchun.kang@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC-Lite driver");
MODULE_LICENSE("GPL");

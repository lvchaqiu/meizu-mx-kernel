/*
 * Samsung HDMI interface driver
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *
 * Tomasz Stanislawski, <t.stanislaws@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundiation. either version 2 of the License,
 * or (at your option) any later version
 */
#include "hdmi.h"

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/bug.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/exynos_mc.h>

MODULE_AUTHOR("Tomasz Stanislawski, <t.stanislaws@samsung.com>");
MODULE_DESCRIPTION("Samsung HDMI");
MODULE_LICENSE("GPL");

/* default preset configured on probe */
#define HDMI_DEFAULT_PRESET V4L2_DV_1080P60

/* I2C module and id for HDMIPHY */
static struct i2c_board_info hdmiphy_info = {
	I2C_BOARD_INFO("hdmiphy", 0x38),
};

static struct hdmi_driver_data hdmi_driver_data[] = {
	{ .hdmiphy_bus = 3 },
	{ .hdmiphy_bus = 8 },
	{ .hdmiphy_bus = 8 },
};

static struct platform_device_id hdmi_driver_types[] = {
	{
		.name		= "s5pv210-hdmi",
		.driver_data	= (unsigned long)&hdmi_driver_data[0],
	}, {
		.name		= "exynos4-hdmi",
		.driver_data	= (unsigned long)&hdmi_driver_data[1],
	}, {
		.name		= "exynos5-hdmi",
		.driver_data	= (unsigned long)&hdmi_driver_data[2],
	}, {
		/* end node */
	}
};

static const struct v4l2_subdev_ops hdmi_sd_ops;

static struct hdmi_device *sd_to_hdmi_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct hdmi_device, sd);
}

static const struct hdmi_preset_conf *hdmi_preset2conf(u32 preset)
{
	int i;

	for (i = 0; i < hdmi_pre_cnt; ++i)
		if (hdmi_conf[i].preset == preset)
			return  hdmi_conf[i].conf;
	return NULL;
}

const struct hdmi_3d_info *hdmi_preset2info(u32 preset)
{
	int i;

	for (i = 0; i < hdmi_pre_cnt; ++i)
		if (hdmi_conf[i].preset == preset)
			return  hdmi_conf[i].info;
	return NULL;
}

static int hdmi_set_infoframe(struct hdmi_device *hdev)
{
	struct hdmi_infoframe infoframe;
	const struct hdmi_3d_info *info;

	info = hdmi_preset2info(hdev->cur_preset);

	if (info->is_3d == HDMI_VIDEO_FORMAT_3D) {
		infoframe.type = HDMI_PACKET_TYPE_VSI;
		infoframe.ver = HDMI_VSI_VERSION;
		infoframe.len = HDMI_VSI_LENGTH;
		hdmi_reg_infoframe(hdev, &infoframe);
	}

	infoframe.type = HDMI_PACKET_TYPE_AVI;
	infoframe.ver = HDMI_AVI_VERSION;
	infoframe.len = HDMI_AVI_LENGTH;
	hdmi_reg_infoframe(hdev, &infoframe);

	return 0;
}

static int hdmi_set_packets(struct hdmi_device *hdev)
{
	hdmi_reg_set_acr(hdev);
	return 0;
}

static int hdmi_streamon(struct hdmi_device *hdev)
{
	struct device *dev = hdev->dev;
	struct hdmi_resources *res = &hdev->res;
	int ret, tries;

	dev_dbg(dev, "%s\n", __func__);

	hdev->streaming = 1;
	ret = v4l2_subdev_call(hdev->phy_sd, video, s_stream, 1);
	if (ret)
		return ret;

	/* waiting for HDMIPHY's PLL to get to steady state */
	for (tries = 100; tries; --tries) {
		if (is_hdmiphy_ready(hdev))
			break;

		mdelay(1);
	}
	/* steady state not achieved */
	if (tries == 0) {
		dev_err(dev, "hdmiphy's pll could not reach steady state.\n");
		v4l2_subdev_call(hdev->phy_sd, video, s_stream, 0);
		hdmi_dumpregs(hdev, "s_stream");
		return -EIO;
	}

	/* hdmiphy clock is used for HDMI in streaming mode */
	clk_disable(res->sclk_hdmi);
	clk_set_parent(res->sclk_hdmi, res->sclk_hdmiphy);
	clk_enable(res->sclk_hdmi);

	/* 3D test */
	hdmi_set_infoframe(hdev);

	/* set packets for audio */
	hdmi_set_packets(hdev);

	/* init audio */
#if defined(CONFIG_VIDEO_EXYNOS_HDMI_AUDIO_I2S)
	hdmi_reg_i2s_audio_init(hdev);
#elif defined(CONFIG_VIDEO_EXYNOS_HDMI_AUDIO_SPDIF)
	hdmi_reg_spdif_audio_init(hdev);
#endif
	/* enbale HDMI audio */
	if (hdev->audio_enable)
		hdmi_audio_enable(hdev, 1);

	/* enable HDMI and timing generator */
	hdmi_enable(hdev, 1);
	hdmi_tg_enable(hdev, 1);

	/* start HDCP if enabled */
	if (hdev->hdcp_info.hdcp_enable) {
		ret = hdcp_start(hdev);
		if (ret)
			return ret;
	}

	hdmi_dumpregs(hdev, "streamon");
	return 0;
}

static int hdmi_streamoff(struct hdmi_device *hdev)
{
	struct device *dev = hdev->dev;
	struct hdmi_resources *res = &hdev->res;

	dev_dbg(dev, "%s\n", __func__);

	if (hdev->hdcp_info.hdcp_enable)
		hdcp_stop(hdev);

	hdmi_audio_enable(hdev, 0);
	hdmi_enable(hdev, 0);
	hdmi_tg_enable(hdev, 0);

	/* pixel(vpll) clock is used for HDMI in config mode */
	clk_disable(res->sclk_hdmi);
	clk_set_parent(res->sclk_hdmi, res->sclk_pixel);
	clk_enable(res->sclk_hdmi);

	v4l2_subdev_call(hdev->phy_sd, video, s_stream, 0);

	hdev->streaming = 0;
	hdmi_dumpregs(hdev, "streamoff");
	return 0;
}

static int hdmi_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;

	dev_dbg(dev, "%s(%d)\n", __func__, enable);
	if (enable)
		return hdmi_streamon(hdev);
	return hdmi_streamoff(hdev);
}

static void hdmi_resource_poweron(struct hdmi_resources *res)
{
	/* power-on hdmi physical interface */
	clk_enable(res->hdmiphy);
	/* use VPP as parent clock; HDMIPHY is not working yet */
	clk_set_parent(res->sclk_hdmi, res->sclk_pixel);
	/* turn clocks on */
	clk_enable(res->sclk_hdmi);
}

static void hdmi_resource_poweroff(struct hdmi_resources *res)
{
	/* turn clocks off */
	clk_disable(res->sclk_hdmi);
	/* power-off hdmiphy */
	clk_disable(res->hdmiphy);
}

static int hdmi_runtime_resume(struct device *dev);
static int hdmi_runtime_suspend(struct device *dev);

static int hdmi_s_power(struct v4l2_subdev *sd, int on)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);

	/* If runtime PM is not implemented, hdmi_runtime_resume
	 * and hdmi_runtime_suspend functions are directly called.
	 */
#ifdef CONFIG_PM_RUNTIME
	int ret;

	if (on)
		ret = pm_runtime_get_sync(hdev->dev);
	else
		ret = pm_runtime_put_sync(hdev->dev);
	/* only values < 0 indicate errors */
	return IS_ERR_VALUE(ret) ? ret : 0;
#else
	if (on)
		hdmi_runtime_resume(hdev->dev);
	else
		hdmi_runtime_suspend(hdev->dev);
	return 0;
#endif
}

int hdmi_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;

	ctrl->value = hdmi_hpd_status(hdev);
	dev_dbg(dev, "HDMI cable is %s\n", ctrl->value ?
			"connected" : "disconnected");

	return 0;
}

static int hdmi_s_dv_preset(struct v4l2_subdev *sd,
	struct v4l2_dv_preset *preset)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;
	const struct hdmi_preset_conf *conf;

	conf = hdmi_preset2conf(preset->preset);
	if (conf == NULL) {
		dev_err(dev, "preset (%u) not supported\n", preset->preset);
		return -EINVAL;
	}
	hdev->cur_conf = conf;
	hdev->cur_preset = preset->preset;
	return 0;
}

static int hdmi_g_dv_preset(struct v4l2_subdev *sd,
	struct v4l2_dv_preset *preset)
{
	memset(preset, 0, sizeof(*preset));
	preset->preset = sd_to_hdmi_dev(sd)->cur_preset;
	return 0;
}

static int hdmi_g_mbus_fmt(struct v4l2_subdev *sd,
	  struct v4l2_mbus_framefmt *fmt)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;

	dev_dbg(dev, "%s\n", __func__);
	if (!hdev->cur_conf)
		return -EINVAL;
	*fmt = hdev->cur_conf->mbus_fmt;
	return 0;
}

static int hdmi_enum_dv_presets(struct v4l2_subdev *sd,
	struct v4l2_dv_enum_preset *preset)
{
	if (preset->index >= hdmi_pre_cnt)
		return -EINVAL;
	return v4l_fill_dv_preset_info(hdmi_conf[preset->index].preset, preset);
}

static const struct v4l2_subdev_core_ops hdmi_sd_core_ops = {
	.s_power = hdmi_s_power,
	.s_ctrl = hdmi_s_ctrl,
};

static const struct v4l2_subdev_video_ops hdmi_sd_video_ops = {
	.s_dv_preset = hdmi_s_dv_preset,
	.g_dv_preset = hdmi_g_dv_preset,
	.enum_dv_presets = hdmi_enum_dv_presets,
	.g_mbus_fmt = hdmi_g_mbus_fmt,
	.s_stream = hdmi_s_stream,
};

static const struct v4l2_subdev_ops hdmi_sd_ops = {
	.core = &hdmi_sd_core_ops,
	.video = &hdmi_sd_video_ops,
};

static int hdmi_runtime_suspend(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);

	dev_dbg(dev, "%s\n", __func__);
	hdmi_resource_poweroff(&hdev->res);
	return 0;
}

static int hdmi_runtime_resume(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	int ret = 0;

	dev_dbg(dev, "%s\n", __func__);

	hdmi_resource_poweron(&hdev->res);

	ret = hdmi_conf_apply(hdev);
	if (ret)
		goto fail;

	dev_dbg(dev, "poweron succeed\n");

	return 0;

fail:
	hdmi_resource_poweroff(&hdev->res);
	dev_err(dev, "poweron failed\n");

	return ret;
}

static const struct dev_pm_ops hdmi_pm_ops = {
	.runtime_suspend = hdmi_runtime_suspend,
	.runtime_resume	 = hdmi_runtime_resume,
};

static void hdmi_resources_cleanup(struct hdmi_device *hdev)
{
	struct hdmi_resources *res = &hdev->res;

	dev_dbg(hdev->dev, "HDMI resource cleanup\n");
	/* put clocks */
	if (!IS_ERR_OR_NULL(res->hdmiphy))
		clk_put(res->hdmiphy);
	if (!IS_ERR_OR_NULL(res->sclk_hdmiphy))
		clk_put(res->sclk_hdmiphy);
	if (!IS_ERR_OR_NULL(res->sclk_pixel))
		clk_put(res->sclk_pixel);
	if (!IS_ERR_OR_NULL(res->sclk_hdmi))
		clk_put(res->sclk_hdmi);
	if (!IS_ERR_OR_NULL(res->hdmi))
		clk_put(res->hdmi);
	memset(res, 0, sizeof *res);
}

static int hdmi_resources_init(struct hdmi_device *hdev)
{
	struct device *dev = hdev->dev;
	struct hdmi_resources *res = &hdev->res;

	dev_dbg(dev, "HDMI resource init\n");

	memset(res, 0, sizeof *res);
	/* get clocks, power */

	res->hdmi = clk_get(dev, "hdmi");
	if (IS_ERR_OR_NULL(res->hdmi)) {
		dev_err(dev, "failed to get clock 'hdmi'\n");
		goto fail;
	}
	res->sclk_hdmi = clk_get(dev, "sclk_hdmi");
	if (IS_ERR_OR_NULL(res->sclk_hdmi)) {
		dev_err(dev, "failed to get clock 'sclk_hdmi'\n");
		goto fail;
	}
	res->sclk_pixel = clk_get(dev, "sclk_pixel");
	if (IS_ERR_OR_NULL(res->sclk_pixel)) {
		dev_err(dev, "failed to get clock 'sclk_pixel'\n");
		goto fail;
	}
	res->sclk_hdmiphy = clk_get(dev, "sclk_hdmiphy");
	if (IS_ERR_OR_NULL(res->sclk_hdmiphy)) {
		dev_err(dev, "failed to get clock 'sclk_hdmiphy'\n");
		goto fail;
	}
	res->hdmiphy = clk_get(dev, "hdmiphy");
	if (IS_ERR_OR_NULL(res->hdmiphy)) {
		dev_err(dev, "failed to get clock 'hdmiphy'\n");
		goto fail;
	}

	return 0;
fail:
	dev_err(dev, "HDMI resource init - failed\n");
	hdmi_resources_cleanup(hdev);
	return -ENODEV;
}

static int hdmi_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	return 0;
}

/* hdmi entity operations */
static const struct media_entity_operations hdmi_entity_ops = {
	.link_setup = hdmi_link_setup,
};

static int hdmi_register_entity(struct hdmi_device *hdev)
{
	struct v4l2_subdev *sd = &hdev->sd;
	struct v4l2_device *v4l2_dev;
	struct media_pad *pads = &hdev->pad;
	struct media_entity *me = &sd->entity;
	struct device *dev = hdev->dev;
	struct exynos_md *md;
	int ret;

	dev_dbg(dev, "HDMI entity init\n");

	/* init hdmi subdev */
	v4l2_subdev_init(sd, &hdmi_sd_ops);
	sd->owner = THIS_MODULE;
	strlcpy(sd->name, "s5p-hdmi", sizeof(sd->name));

	dev_set_drvdata(dev, sd);

	/* init hdmi sub-device as entity */
	pads[HDMI_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	me->ops = &hdmi_entity_ops;
	ret = media_entity_init(me, HDMI_PADS_NUM, pads, 0);
	if (ret) {
		dev_err(dev, "failed to initialize media entity\n");
		return ret;
	}

	/* get output media ptr for registering hdmi's sd */
	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		dev_err(dev, "failed to get output media device\n");
		return -ENODEV;
	}

	v4l2_dev = &md->v4l2_dev;

	/* regiser HDMI subdev as entity to v4l2_dev pointer of
	 * output media device
	 */
	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret) {
		dev_err(dev, "failed to register HDMI subdev\n");
		return ret;
	}

	return 0;
}

static void hdmi_entity_info_print(struct hdmi_device *hdev)
{
	struct v4l2_subdev *sd = &hdev->sd;
	struct media_entity *me = &sd->entity;

	dev_dbg(hdev->dev, "\n************* HDMI entity info **************\n");
	dev_dbg(hdev->dev, "[SUB DEVICE INFO]\n");
	entity_info_print(me, hdev->dev);
	dev_dbg(hdev->dev, "*********************************************\n\n");
}

static void s5p_hpd_kobject_uevent(struct work_struct *work)
{
	struct hdmi_device *hdev = container_of(work, struct hdmi_device,
						hpd_work);
	char *disconnected[2] = { "HDMI_STATE=offline", NULL };
	char *connected[2]    = { "HDMI_STATE=online", NULL };
	char **envp = NULL;
	int state = atomic_read(&hdev->hpd_state);

	if (state)
		envp = connected;
	else
		envp = disconnected;

	kobject_uevent_env(&hdev->dev->kobj, KOBJ_CHANGE, envp);
	pr_info("%s: sent uevent %s\n", __func__, envp[0]);
}

static int __devinit hdmi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct i2c_adapter *phy_adapter;
	struct hdmi_device *hdmi_dev = NULL;
	struct hdmi_driver_data *drv_data;
	int ret;

	dev_dbg(dev, "probe start\n");

	hdmi_dev = kzalloc(sizeof(*hdmi_dev), GFP_KERNEL);
	if (!hdmi_dev) {
		dev_err(dev, "out of memory\n");
		ret = -ENOMEM;
		goto fail;
	}

	hdmi_dev->dev = dev;

	ret = hdmi_resources_init(hdmi_dev);
	if (ret)
		goto fail_hdev;

	/* mapping HDMI registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "get memory resource failed.\n");
		ret = -ENXIO;
		goto fail_init;
	}

	hdmi_dev->regs = ioremap(res->start, resource_size(res));
	if (hdmi_dev->regs == NULL) {
		dev_err(dev, "register mapping failed.\n");
		ret = -ENXIO;
		goto fail_hdev;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(dev, "get interrupt resource failed.\n");
		ret = -ENXIO;
		goto fail_regs;
	}

	ret = request_irq(res->start, hdmi_irq_handler, 0, "hdmi", hdmi_dev);
	if (ret) {
		dev_err(dev, "request interrupt failed.\n");
		goto fail_regs;
	}
	hdmi_dev->irq = res->start;

	/* workqueue for HPD */
	hdmi_dev->hpd_wq = create_workqueue("hdmi-hpd");
	if (hdmi_dev->hpd_wq == NULL)
		ret = -ENXIO;
	INIT_WORK(&hdmi_dev->hpd_work, s5p_hpd_kobject_uevent);

	/* setting v4l2 name to prevent WARN_ON in v4l2_device_register */
	strlcpy(hdmi_dev->v4l2_dev.name, dev_name(dev),
		sizeof(hdmi_dev->v4l2_dev.name));
	/* passing NULL owner prevents driver from erasing drvdata */
	ret = v4l2_device_register(NULL, &hdmi_dev->v4l2_dev);
	if (ret) {
		dev_err(dev, "could not register v4l2 device.\n");
		goto fail_irq;
	}

	drv_data = (struct hdmi_driver_data *)
		platform_get_device_id(pdev)->driver_data;
	dev_info(dev, "hdmiphy i2c bus number = %d\n", drv_data->hdmiphy_bus);

	phy_adapter = i2c_get_adapter(drv_data->hdmiphy_bus);
	if (phy_adapter == NULL) {
		dev_err(dev, "adapter request failed\n");
		ret = -ENXIO;
		goto fail_vdev;
	}

	hdmi_dev->phy_sd = v4l2_i2c_new_subdev_board(&hdmi_dev->v4l2_dev,
		phy_adapter, &hdmiphy_info, NULL);
	/* on failure or not adapter is no longer useful */
	i2c_put_adapter(phy_adapter);
	if (hdmi_dev->phy_sd == NULL) {
		dev_err(dev, "missing subdev for hdmiphy\n");
		ret = -ENODEV;
		goto fail_vdev;
	}

	clk_enable(hdmi_dev->res.hdmi);

	pm_runtime_enable(dev);

	hdmi_dev->cur_preset = HDMI_DEFAULT_PRESET;
	/* FIXME: missing fail preset is not supported */
	hdmi_dev->cur_conf = hdmi_preset2conf(hdmi_dev->cur_preset);

	/* default audio configuration : enable audio */
	hdmi_dev->audio_enable = 1;
	hdmi_dev->sample_rate = DEFAULT_SAMPLE_RATE;
	hdmi_dev->bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
	hdmi_dev->audio_codec = DEFAULT_AUDIO_CODEC;

	/* register hdmi subdev as entity */
	ret = hdmi_register_entity(hdmi_dev);
	if (ret)
		goto fail_vdev;

	hdmi_entity_info_print(hdmi_dev);

#if defined(CONFIG_ARCH_EXYNOS4)
	ret = pm_runtime_get_sync(dev);
	if (ret)
		goto fail_vdev;
#endif

	/* initialize hdcp resource */
	ret = hdcp_prepare(hdmi_dev);
	if (ret)
		goto fail_vdev;

	hdmi_hpd_enable(hdmi_dev, 1);

	dev_info(dev, "probe sucessful\n");

	return 0;

fail_vdev:
	v4l2_device_unregister(&hdmi_dev->v4l2_dev);

fail_irq:
	free_irq(hdmi_dev->irq, hdmi_dev);

fail_regs:
	iounmap(hdmi_dev->regs);

fail_init:
	hdmi_resources_cleanup(hdmi_dev);

fail_hdev:
	kfree(hdmi_dev);

fail:
	dev_err(dev, "probe failed\n");
	return ret;
}

static int __devexit hdmi_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hdmi_device *hdmi_dev = sd_to_hdmi_dev(sd);

	pm_runtime_disable(dev);
	clk_disable(hdmi_dev->res.hdmi);
	v4l2_device_unregister(&hdmi_dev->v4l2_dev);
	disable_irq(hdmi_dev->irq);
	free_irq(hdmi_dev->irq, hdmi_dev);
	iounmap(hdmi_dev->regs);
	hdmi_resources_cleanup(hdmi_dev);
	flush_workqueue(hdmi_dev->hdcp_wq);
	destroy_workqueue(hdmi_dev->hdcp_wq);
	kfree(hdmi_dev);
	dev_info(dev, "remove sucessful\n");

	return 0;
}

static struct platform_driver hdmi_driver __refdata = {
	.probe = hdmi_probe,
	.remove = __devexit_p(hdmi_remove),
	.id_table = hdmi_driver_types,
	.driver = {
		.name = "s5p-hdmi",
		.owner = THIS_MODULE,
		.pm = &hdmi_pm_ops,
	}
};

/* D R I V E R   I N I T I A L I Z A T I O N */

static int __init hdmi_init(void)
{
	int ret;
	static const char banner[] __initdata = KERN_INFO \
		"Samsung HDMI output driver, "
		"(c) 2010-2011 Samsung Electronics Co., Ltd.\n";
	printk(banner);

	ret = platform_driver_register(&hdmi_driver);
	if (ret)
		printk(KERN_ERR "HDMI platform driver register failed\n");

	return ret;
}
module_init(hdmi_init);

static void __exit hdmi_exit(void)
{
	platform_driver_unregister(&hdmi_driver);
}
module_exit(hdmi_exit);



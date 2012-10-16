/*
 *  base on sound/soc/s3c24xx/smdk_wm8994mst.c
 *
 *  Copyright (c) 2011 MEIZU Technologies  Co. Ltd
 *
 *  This program is free software; you can redistribute  it and/or  modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/mx_audio_platform.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include <mach/regs-clock.h>
#include <asm/mach-types.h>

#include "../codecs/wm8994.h"
#include "dma.h"
#include "i2s.h"
#include "pcm.h"

#include "spdif.h"

#define WM8994_FREQ_12000000 12000000
#define WM8994_FREQ_24000000 24000000
#define WM8994_FREQ_2048000  2048000
#define WM8994_FREQ_11289600 11289600
#define WM8994_FREQ_12288000 12288000
#define WM8994_FREQ_BB (8000*(32+1))
#define WM8994_DAI_AIF1	0
#define WM8994_DAI_AIF2	1
#define WM8994_DAI_AIF3	2

static int mx_wm8958_aif1_hw_init(struct snd_soc_pcm_runtime *rtd)
{

	printk("%s\n", __func__);
	return 0;
}
static int mx_wm8958_aif2_hw_init(struct snd_soc_pcm_runtime *rtd)
{
	printk("%s\n", __func__);
	return 0;
}

static int mx_wm8958_aif1_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	unsigned long rclk;
	int bfs, rfs, ret;

	printk("++%s format = %d rate = %d\n", __func__,params_format(params),(params_rate(params)));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	/* Set the codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Set the cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1, rclk,
					SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	if (machine_is_m030()) {
		ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1, WM8994_FLL_SRC_MCLK1,
						WM8994_FREQ_12000000, rclk);
		if (ret < 0)
			return ret;
	} else {
		ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1, WM8994_FLL_SRC_MCLK1,
						WM8994_FREQ_24000000, rclk);
		if (ret < 0)
			return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					rclk, MOD_OPCLK_PCLK);//select audio bus clock
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_0,//SAMSUNG_I2S_RCLKSRC_0 = Using BUSCLK,SAMSUNG_I2S_RCLKSRC_1 = Using I2SCLK
					rclk, 0);
	if (ret < 0)
		return ret;
	
	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,//RCLK supply to codec, set RFS
					rfs, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;
	
	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);//set BFS
	if (ret < 0)
		return ret;

	printk("--%s\n", __func__);

	return 0;
}

#ifdef CONFIG_SND_SAMSUNG_PCM_USE_EPLL
static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	pr_info("%s: clk_get_rate(%lu)\n", __func__,clk_get_rate(fout_epll));
	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
out:
	clk_put(fout_epll);

	return 0;
}
#endif /* CONFIG_SND_SAMSUNG_PCM_USE_EPLL */

static int mx_wm8958_aif2_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
#ifdef CONFIG_SND_SAMSUNG_PCM_USE_EPLL
	unsigned long epll_out_rate;
#endif /* CONFIG_SND_SAMSUNG_PCM_USE_EPLL */
	int rfs, ret;
	printk("++%s format = %d rate = %d\n", __func__,params_format(params),(params_rate(params)));

#ifdef CONFIG_SND_SAMSUNG_PCM_USE_EPLL
	switch (params_rate(params)) {
	case 8000:
	case 12000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		epll_out_rate = 49152000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		epll_out_rate = 67737600;
		break;
	default:
		printk(KERN_ERR "%s:%d Sampling Rate %u not supported!\n",
			__func__, __LINE__, params_rate(params));
		return -EINVAL;
	}
#endif /* CONFIG_SND_SAMSUNG_PCM_USE_EPLL */

	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 22025:
	case 32000:
	case 44100:
	case 48000:
	case 96000:
	case 24000:
#ifdef CONFIG_SND_SAMSUNG_PCM_USE_EPLL
		rfs = 256;
#else /* CONFIG_SND_SAMSUNG_PCM_USE_EPLL */
		rfs = 384;
#endif /* CONFIG_SND_SAMSUNG_PCM_USE_EPLL */
		break;
	case 64000:
		rfs = 384;
		break;
	case 11025:
	case 12000:
		rfs = 512;
		break;
	case 8000:
	case 88200:
		rfs = 128;
		break;
	default:
		printk(KERN_ERR "%s:%d Sampling Rate %u not supported!\n",
			__func__, __LINE__, params_rate(params));
		return -EINVAL;
	}

	/* Set the codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A
				| SND_SOC_DAIFMT_IB_NF
				| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set the cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A
				| SND_SOC_DAIFMT_IB_NF
				| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

#ifdef CONFIG_SND_SAMSUNG_PCM_USE_EPLL
	/*
	 * Samsung SoCs PCM has no MCLK(rclk) output support, so codec
	 * should have to make its own MCLK with FLL(or PLL) from other
	 * clock source.
	 */
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL2,
				params_rate(params)*rfs,
				SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	if (machine_is_m030()) {
		ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2,
					WM8994_FLL_SRC_MCLK1,
					WM8994_FREQ_12000000,
					params_rate(params)*rfs);
	}
	else{
		ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2,
					WM8994_FLL_SRC_MCLK1,
					WM8994_FREQ_24000000,
					params_rate(params)*rfs);
	}
		
	if (ret < 0)
		return ret;
#else
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_MCLK2,
					params_rate(params)*rfs,
					SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;
#endif /* CONFIG_SND_SAMSUNG_PCM_USE_EPLL */

#ifdef CONFIG_SND_SAMSUNG_PCM_USE_EPLL
	/* Set EPLL clock rate */
	ret = set_epll_rate(epll_out_rate);
	if (ret < 0)
		return ret;
#endif /* CONFIG_SND_SAMSUNG_PCM_USE_EPLL */

	/* Set SCLK_DIV for making bclk */
	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_PCM_SCLK_PER_FS, rfs);
	if (ret < 0)
		return ret;

	printk("--%s\n", __func__);
	return 0;
}

static struct snd_soc_ops mx_wm8958_aif1_ops = {
	.hw_params = mx_wm8958_aif1_hw_params,
};
static struct snd_soc_ops mx_wm8958_aif2_ops = {
	.hw_params = mx_wm8958_aif2_hw_params,
};

static struct snd_soc_dai_link mx_dai[] = {
	{ /* I2S Tx/Rx DAI i/f */
		.name = "WM8958-AIF1",
		.stream_name = "I2S Tx/Rx",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "wm8994-codec",
		.ops = &mx_wm8958_aif1_ops,
		.init = mx_wm8958_aif1_hw_init,
	}, { /* PCM Tx/Rx DAI i/f */
		.name = "WM8958-AIF2",
		.stream_name = "PCM Tx/Rx",
		.cpu_dai_name = "samsung-pcm.1",
		.codec_dai_name = "wm8994-aif2",
		.platform_name = "samsung-audio",
		.codec_name = "wm8994-codec",
		.ops = &mx_wm8958_aif2_ops,
		.init = mx_wm8958_aif2_hw_init,		
	},	
};

static struct snd_soc_card meizu_mx = {
	.name = "MX-I2S/PCM",
	.dai_link = mx_dai,
	.num_links = ARRAY_SIZE(mx_dai),
};

struct platform_device *mx_snd_device;
static struct platform_device *mx_audio_device = NULL;
void audio_switch(int High)
{
	struct mx_audio_platform_data *audio_data;
	if(mx_audio_device) {
		audio_data = mx_audio_device->dev.platform_data;
		if(audio_data && audio_data->audio_switch) {
			audio_data->audio_switch(High);
		}
	}
}
EXPORT_SYMBOL(audio_switch);

static int __devinit mx_audio_probe(struct platform_device *pdev)
{
	int ret;

	printk("%s++\n", __func__);

	mx_audio_device = pdev;
	mx_snd_device = platform_device_alloc("soc-audio", 0);
	if (!mx_snd_device)
		return -EIO;

	platform_set_drvdata(mx_snd_device, &meizu_mx);
	ret = platform_device_add(mx_snd_device);
	if (ret)
	{
		WARN_ON(1);
		platform_device_put(mx_snd_device);
	}

	
	printk("%s--\n", __func__);
	return ret;
}

static int __devexit mx_audio_remove(struct platform_device *pdev)
{
	platform_device_unregister(mx_snd_device);
	return 0;
}

static struct platform_driver mx_audio_driver={
	.probe = mx_audio_probe,
	.remove = mx_audio_remove,	
	.driver		= {
		.name	= "mx-audio",
		.owner	= THIS_MODULE,
	},
};

static int __init mx_audio_init(void)
{
	int ret;
	
	printk("%s++\n", __func__);
	
	ret = platform_driver_register(&mx_audio_driver);
	if (ret)
		return -ENOMEM;
	printk("%s--\n", __func__);
	return ret;
}

static void __exit mx_audio_exit(void)
{
	platform_driver_unregister(&mx_audio_driver);
}

module_init(mx_audio_init);
module_exit(mx_audio_exit);

MODULE_AUTHOR("Wei Chen, <Chwei@meizu.com>");
MODULE_DESCRIPTION("ALSA SoC MX WM8958 Audio Driver");
MODULE_LICENSE("GPL");

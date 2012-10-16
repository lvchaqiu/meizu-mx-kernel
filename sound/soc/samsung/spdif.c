/* sound/soc/samsung/spdif.c
 *
 * ALSA SoC Audio Layer - Samsung S/PDIF Controller driver
 *
 * Copyright (c) 2010 Samsung Electronics Co. Ltd
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/io.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <plat/audio.h>
#include <mach/dma.h>

#include "dma.h"
#include "spdif.h"

/**
 * struct samsung_spdif_info - Samsung S/PDIF Controller information
 * @lock: Spin lock for S/PDIF.
 * @dev: The parent device passed to use from the probe.
 * @regs: The pointer to the device register block.
 * @clk_rate: Current clock rate for calcurate ratio.
 * @pclk: The peri-clock pointer for spdif master operation.
 * @sclk: The source clock pointer for making sync signals.
 * @save_clkcon: Backup clkcon reg. in suspend.
 * @save_con: Backup con reg. in suspend.
 * @save_cstas: Backup cstas reg. in suspend.
 * @dma_playback: DMA information for playback channel.
 */
struct samsung_spdif_info {
	spinlock_t	lock;
	struct device	*dev;
	void __iomem	*regs;
	unsigned long	clk_rate;
	int		use_int_clk;
	struct clk	*pclk;
	struct clk	*sclk;
	u32		saved_clkcon;
	u32		saved_con;
	u32		saved_cstas;
	struct s3c_dma_params	*dma_playback;
	struct spdif_con 	conf;
	struct spdif_bstat	bstat;
	struct spdif_cstat	cstat;
	unsigned int		rep_cnt;	
};

static struct s3c2410_dma_client spdif_dma_client_out = {
	.name		= "S/PDIF Stereo out",
};

static struct s3c_dma_params spdif_stereo_out;
static struct samsung_spdif_info spdif_info;

static inline struct samsung_spdif_info *to_info(struct snd_soc_dai *cpu_dai)
{
	return snd_soc_dai_get_drvdata(cpu_dai);
}

static void spdif_snd_txctrl(struct samsung_spdif_info *spdif, int on)
{
	void __iomem *regs = spdif->regs;
	u32 clkcon;
	
	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	clkcon = readl(regs + CLKCON) & CLKCTL_MASK;
	if (on)
		writel(clkcon | CLKCTL_PWR_ON, regs + CLKCON);
	else
		writel(clkcon & ~CLKCTL_PWR_ON, regs + CLKCON);
}

static int spdif_set_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct samsung_spdif_info *spdif = to_info(cpu_dai);


	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	if (dir == SND_SOC_CLOCK_IN)
		spdif->use_int_clk = 0;
	else
		spdif->use_int_clk = 1;
	
	spdif->clk_rate = freq;
	dev_info(spdif->dev, "Entered %s:%d\n", __func__, __LINE__);
	return 0;
}

static int spdif_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct samsung_spdif_info *spdif = to_info(rtd->cpu_dai);
	unsigned long flags;

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		spin_lock_irqsave(&spdif->lock, flags);
		spdif_snd_txctrl(spdif, 1);
		spin_unlock_irqrestore(&spdif->lock, flags);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		spin_lock_irqsave(&spdif->lock, flags);
		spdif_snd_txctrl(spdif, 0);
		spin_unlock_irqrestore(&spdif->lock, flags);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int spdif_sysclk_ratios[] = {
	512, 384, 256,
};

static int spdif_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *socdai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct samsung_spdif_info *spdif = to_info(rtd->cpu_dai);
	void __iomem *regs = spdif->regs;
	struct s3c_dma_params *dma_data;
	u32 clkcon = 0;     /* for SPDCLKCON register */
	u32 con = 0;        /* for SPDCON register */
	u32 bstats = 0;     /* for SPDBSTAS register */
	u32 cstas = 0;      /* for SPDCSTAS register */
	u32 bits_per_sample = 0;    /* for bits per sample */
	unsigned long flags;
	int i, ratio;

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = spdif->dma_playback;
	else {
		dev_err(spdif->dev, "Capture is not supported\n");
		return -EINVAL;
	}

	snd_soc_dai_set_dma_data(rtd->cpu_dai, substream, dma_data);

	ratio = (spdif->clk_rate) / (params_rate(params));

	spin_lock_irqsave(&spdif->lock, flags);

	clkcon = readl(regs + CLKCON) & CLKCTL_MASK;
	
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16:
		pr_info("%s SNDRV_PCM_FORMAT_S16\n",__func__);
		spdif->conf.stream_type = CON_PCM_DATA;
		spdif->bstat.compressed_type = 0;
		spdif->conf.endian_format = SPDIF_BIG_ENDIAN;
		spdif->conf.data_size = SPDIF_BIT_16;
		spdif->conf.fifo_trans_mode = SPDIF_TRANS_DMA;
		spdif->conf.fifo_thresh = SPDIF_FIFO_LV_6;
		spdif->conf.user_dat_attach = 1;
		spdif->bstat.error_flag = 0;
		spdif->bstat.bitstream_num = 0;
		spdif->bstat.burst_data_len = 0; 
		spdif->cstat.chan_stat_blk = 0;
		spdif->cstat.audio_samp_word = SPDIF_ASW_LINEAR_PCM;
		spdif->cstat.category_code = 1;
		spdif->cstat.copyright = 0;
		break;
    case SNDRV_PCM_FORMAT_S24:
		pr_info("%s SNDRV_PCM_FORMAT_S24\n",__func__);
		spdif->conf.stream_type = CON_PCM_DATA;
		spdif->bstat.compressed_type = 0;
		spdif->conf.endian_format = SPDIF_BIG_ENDIAN;
		spdif->conf.data_size = SPDIF_BIT_24;
		spdif->conf.fifo_trans_mode = SPDIF_TRANS_DMA;
		spdif->conf.fifo_thresh = SPDIF_FIFO_LV_6;
		spdif->conf.user_dat_attach = 1;
		spdif->bstat.error_flag = 0;
		spdif->bstat.bitstream_num = 0;
		spdif->bstat.burst_data_len = 0; 
		spdif->cstat.chan_stat_blk = 0;
		spdif->cstat.audio_samp_word = SPDIF_ASW_LINEAR_PCM;
		spdif->cstat.category_code = 1;
		spdif->cstat.copyright = 1;
		break;
    case SNDRV_PCM_FORMAT_IEC958_SUBFRAME:
		pr_info("%s iec958\n",__func__);
		spdif->conf.stream_type = CON_STREAM_DATA;
		spdif->conf.endian_format = SPDIF_2BYTES_SWAP;
		spdif->conf.data_size = SPDIF_BIT_16;
		bits_per_sample = 16;
		spdif->conf.int_strmend_en = 0;
		spdif->conf.int_bufempty_en = 0;
		spdif->conf.int_userdata_en = 0;
		spdif->conf.int_fifo_en = 0;
		spdif->conf.user_dat_attach = 0;
		spdif->conf.fifo_thresh = SPDIF_FIFO_LV_15;
		spdif->conf.fifo_trans_mode = SPDIF_TRANS_DMA;
		spdif->conf.fifo_level0 = 0;
		spdif->bstat.compressed_type = SPDIF_COMP_AC3;
		spdif->bstat.error_flag = 0;
		spdif->bstat.data_dep_info = 0;
		spdif->bstat.bitstream_num = 0;
		spdif->bstat.burst_data_len = ratio * bits_per_sample;
		spdif->cstat.chan_stat_blk = 0;
		spdif->cstat.audio_samp_word = SPDIF_ASW_NON_LINEAR_PCM;
		spdif->cstat.category_code = SPDIF_CC_CDP;
		spdif->cstat.copyright = 1;
		spdif->rep_cnt = 1536*2;//for AC3
        break;		
	default:
		dev_err(spdif->dev, "Unsupported data size.\n");
		goto err;
	}

	if (spdif->use_int_clk)
		clkcon &= ~CLKCTL_MCLK_EXT;
	else
		clkcon |= CLKCTL_MCLK_EXT;
    
	dev_dbg(spdif->dev,"clk_rate is %ld and ratio is %d\n",spdif->clk_rate,ratio);
	for (i = 0; i < ARRAY_SIZE(spdif_sysclk_ratios); i++)
		if (ratio == spdif_sysclk_ratios[i])
			break;
	if (i == ARRAY_SIZE(spdif_sysclk_ratios)) {
		dev_err(spdif->dev, "Invalid clock ratio %ld/%d\n",
				spdif->clk_rate, params_rate(params));
		goto err;
	}

	if (spdif->use_int_clk)
		clk_set_rate(spdif->sclk, spdif->clk_rate);

	switch (ratio) {
	case 256:
		spdif->conf.main_aclk = SPDIF_FREQ_256;
		break;
	case 384:
		spdif->conf.main_aclk = SPDIF_FREQ_384;
		break;
	case 512:
		spdif->conf.main_aclk = SPDIF_FREQ_512;
		break;
	}

	switch (params_rate(params)) {
	case 44100:
		spdif->cstat.sample_rate = SPDIF_SR_44100;
		break;
	case 48000:
		spdif->cstat.sample_rate = SPDIF_SR_48000;
		break;
	case 32000:
		spdif->cstat.sample_rate = SPDIF_SR_32000;
		break;
	case 96000:
		spdif->cstat.sample_rate = SPDIF_SR_96000;
		break;
	default:
		goto err;
	}

	con = *((unsigned int*)&(spdif->conf));
	writel(con, regs + CON);
	cstas = *((unsigned int*)&(spdif->cstat));
	writel(cstas, regs + CSTAS);
	writel(clkcon, regs + CLKCON);
	bstats = *((unsigned int*)&(spdif->bstat));
	writel(bstats, regs + BSTAS);
	writel((unsigned int)spdif->rep_cnt, regs + DCNT);
	spin_unlock_irqrestore(&spdif->lock, flags);
	return 0;
err:
	spin_unlock_irqrestore(&spdif->lock, flags);
	return -EINVAL;
}

static void spdif_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct samsung_spdif_info *spdif = to_info(rtd->cpu_dai);
	void __iomem *regs = spdif->regs;
	u32 con, clkcon;

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	con = readl(regs + CON) & CON_MASK;
	clkcon = readl(regs + CLKCON) & CLKCTL_MASK;

	writel(con | CON_SW_RESET, regs + CON);
	cpu_relax();

	writel(clkcon & ~CLKCTL_PWR_ON, regs + CLKCON);
}

#ifdef CONFIG_PM
static int spdif_suspend(struct snd_soc_dai *cpu_dai)
{
	struct samsung_spdif_info *spdif = to_info(cpu_dai);
	u32 con = spdif->saved_con;

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	spdif->saved_clkcon = readl(spdif->regs	+ CLKCON) & CLKCTL_MASK;
	spdif->saved_con = readl(spdif->regs + CON) & CON_MASK;
	spdif->saved_cstas = readl(spdif->regs + CSTAS) & CSTAS_MASK;

	writel(con | CON_SW_RESET, spdif->regs + CON);
	cpu_relax();

	return 0;
}

static int spdif_resume(struct snd_soc_dai *cpu_dai)
{
	struct samsung_spdif_info *spdif = to_info(cpu_dai);

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	writel(spdif->saved_clkcon, spdif->regs	+ CLKCON);
	writel(spdif->saved_con, spdif->regs + CON);
	writel(spdif->saved_cstas, spdif->regs + CSTAS);

	return 0;
}
#else
#define spdif_suspend NULL
#define spdif_resume NULL
#endif

static struct snd_soc_dai_ops spdif_dai_ops = {
	.set_sysclk	= spdif_set_sysclk,
	.trigger	= spdif_trigger,
	.hw_params	= spdif_hw_params,
	.shutdown	= spdif_shutdown,
};

#define S5P_SPDIF_RATES	(SNDRV_PCM_RATE_32000 | \
			SNDRV_PCM_RATE_44100 | \
			SNDRV_PCM_RATE_48000 | \
			SNDRV_PCM_RATE_96000)

#define S5P_SPDIF_FORMATS SNDRV_PCM_FMTBIT_S16_LE |\
				    SNDRV_PCM_FMTBIT_U16_LE |\
				    SNDRV_PCM_FMTBIT_U8 |\
				    SNDRV_PCM_FMTBIT_S8 |\
				    SNDRV_PCM_FMTBIT_IEC958_SUBFRAME |\
				    SNDRV_PCM_FMTBIT_S24 |\
				    SNDRV_PCM_FMTBIT_U24
				    
struct snd_soc_dai_driver samsung_spdif_dai = {
	.name = "samsung-spdif",
	.playback = {
		.stream_name = "S/PDIF Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = S5P_SPDIF_RATES,
		.formats = S5P_SPDIF_FORMATS, },
	.ops = &spdif_dai_ops,
	.suspend = spdif_suspend,
	.resume = spdif_resume,
};

static __devinit int spdif_probe(struct platform_device *pdev)
{
	struct s3c_audio_pdata *spdif_pdata;
	struct resource *mem_res, *dma_res;
	struct samsung_spdif_info *spdif;
	int ret;

	spdif_pdata = pdev->dev.platform_data;

	dev_dbg(&pdev->dev, "Entered %s\n", __func__);

	dma_res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!dma_res) {
		dev_err(&pdev->dev, "Unable to get dma resource.\n");
		return -ENXIO;
	}

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Unable to get register resource.\n");
		return -ENXIO;
	}

	if (spdif_pdata && spdif_pdata->cfg_gpio
			&& spdif_pdata->cfg_gpio(pdev)) {
		dev_err(&pdev->dev, "Unable to configure GPIO pins\n");
		return -EINVAL;
	}

	spdif = &spdif_info;
	spdif->dev = &pdev->dev;
	spdif->use_int_clk = 1;
	spdif->clk_rate = 0;
	spin_lock_init(&spdif->lock);

	spdif->pclk = clk_get(&pdev->dev, "spdif");
	if (IS_ERR(spdif->pclk)) {
		dev_err(&pdev->dev, "failed to get peri-clock\n");
		ret = PTR_ERR(spdif->pclk);
		goto err0;
	}
	clk_enable(spdif->pclk);

	spdif->sclk = clk_get(&pdev->dev, "sclk_spdif");
	if (IS_ERR(spdif->sclk)) {
		dev_err(&pdev->dev, "failed to get internal source clock\n");
		ret = PTR_ERR(spdif->sclk);
		goto err1;
	}
	clk_enable(spdif->sclk);

	/* Request S/PDIF Register's memory region */
	if (!request_mem_region(mem_res->start,
				resource_size(mem_res), "samsung-spdif")) {
		dev_err(&pdev->dev, "Unable to request register region\n");
		ret = -EBUSY;
		goto err2;
	}

	spdif->regs = ioremap(mem_res->start, 0x100);
	if (spdif->regs == NULL) {
		dev_err(&pdev->dev, "Cannot ioremap registers\n");
		ret = -ENXIO;
		goto err3;
	}

	dev_set_drvdata(&pdev->dev, spdif);

	ret = snd_soc_register_dai(&pdev->dev, &samsung_spdif_dai);
	if (ret != 0) {
		dev_err(&pdev->dev, "fail to register dai\n");
		goto err4;
	}

	spdif_stereo_out.dma_size = 2;
	spdif_stereo_out.client = &spdif_dma_client_out;
	spdif_stereo_out.dma_addr = mem_res->start + DATA_OUTBUF;
	spdif_stereo_out.channel = dma_res->start;

	spdif->dma_playback = &spdif_stereo_out;

	return 0;

err4:
	iounmap(spdif->regs);
err3:
	release_mem_region(mem_res->start, resource_size(mem_res));
err2:
	clk_disable(spdif->sclk);
	clk_put(spdif->sclk);
err1:
	clk_disable(spdif->pclk);
	clk_put(spdif->pclk);
err0:
	return ret;
}

static __devexit int spdif_remove(struct platform_device *pdev)
{
	struct samsung_spdif_info *spdif = &spdif_info;
	struct resource *mem_res;

	snd_soc_unregister_dai(&pdev->dev);

	iounmap(spdif->regs);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem_res)
		release_mem_region(mem_res->start, resource_size(mem_res));

	clk_disable(spdif->sclk);
	clk_put(spdif->sclk);
	clk_disable(spdif->pclk);
	clk_put(spdif->pclk);

	return 0;
}

static struct platform_driver samsung_spdif_driver = {
	.probe	= spdif_probe,
	.remove	= spdif_remove,
	.driver	= {
		.name	= "samsung-spdif",
		.owner	= THIS_MODULE,
	},
};

static int __init spdif_init(void)
{
	return platform_driver_register(&samsung_spdif_driver);
}
module_init(spdif_init);

static void __exit spdif_exit(void)
{
	platform_driver_unregister(&samsung_spdif_driver);
}
module_exit(spdif_exit);

MODULE_AUTHOR("Seungwhan Youn, <sw.youn@samsung.com>");
MODULE_DESCRIPTION("Samsung S/PDIF Controller Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:samsung-spdif");

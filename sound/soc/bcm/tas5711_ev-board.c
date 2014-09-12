/*
 * ASoC Driver for TI TAS5711 Evaluation Board
 *
 * Author: Felix Kramer <felixkramerroki@aol.com>
 * based on the HifiBerry DAC driver by Florian Meier <florian.meier@koalo.de>
 *		Copyright 2014
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

static int snd_rpi_tas5711_evb_init(struct snd_soc_pcm_runtime *rtd)
{
	printk("+++++++++++initialized tas5711 evaluation board driver++++++++++++\n");
	return 0;
}

static int snd_rpi_tas5711_evb_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	unsigned int sample_bits =
		snd_pcm_format_physical_width(params_format(params));

	return snd_soc_dai_set_bclk_ratio(cpu_dai, sample_bits * 2);
}

/* machine stream operations */
static struct snd_soc_ops snd_rpi_tas5711_evb_ops = {
	.hw_params = snd_rpi_tas5711_evb_hw_params,
};

static struct snd_soc_dai_link snd_rpi_tas5711_evb_dai[] = {
{
	.name		= "TAS5711 EVB",
	.stream_name	= "TI TAS5711 Evaluation Board",
	.cpu_dai_name	= "bcm2708-i2s.0",
	.codec_dai_name	= "tas5711-hifi", /* TODO what/where is this?? */
	.platform_name	= "bcm2708-i2s.0",
	.codec_name	= "tas5711-codec",
	.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
	.ops		= &snd_rpi_tas5711_evb_ops,
	.init		= snd_rpi_tas5711_evb_init,
},
};

/* audio machine driver */
static struct snd_soc_card snd_rpi_tas5711_evb = {
	.name         = "snd_rpi_tas5711_evb",
	.dai_link     = snd_rpi_tas5711_evb_dai,
	.num_links    = ARRAY_SIZE(snd_rpi_tas5711_evb_dai),
};

static int snd_rpi_tas5711_evb_probe(struct platform_device *pdev)
{
	int ret = 0;

	snd_rpi_tas5711_evb.dev = &pdev->dev;
	ret = snd_soc_register_card(&snd_rpi_tas5711_evb);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
		printk("snd_soc_register_card() failed with code %d\n", ret);
	}

	return ret;
}

static int snd_rpi_tas5711_evb_remove(struct platform_device *pdev)
{
	return snd_soc_unregister_card(&snd_rpi_tas5711_evb);
}

static struct platform_driver snd_rpi_tas5711_evb_driver = {
        .driver = {
                .name   = "snd-tas5711-evb",
                .owner  = THIS_MODULE,
        },
        .probe          = snd_rpi_tas5711_evb_probe,
        .remove         = snd_rpi_tas5711_evb_remove,
};

module_platform_driver(snd_rpi_tas5711_evb_driver);

MODULE_AUTHOR("Felix Kramer <felixkramerroki@aol.com>");
MODULE_DESCRIPTION("ASoC Driver for TAS5711 Evaluation Board");
MODULE_LICENSE("GPL v2");

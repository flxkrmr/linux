/*
 * TAS5711 ASoC codec driver
 *
 * Author: Felix Kramer <felixkramerroki@aol.com>
 * based on the TAS5086 ASoC codec driver by Daniel Mack <zonque@gmail.com>
 *		Copyright 2014
 *
 *	TODO:
 *		check really needed librarys 
 *		tas5711_register_size size 8 size 12??
 *		control parameters of TLV macros (line 512)
 *		DAPM Parameters
 *		CONFIG_OF, CONFIG_PM??
 *		"ti,start-stop-period" ????
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

//TODO check really needed librarys 
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
//#include <linux/spi/spi.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#define TAS5711_CLK_IDX_MCLK	0
#define TAS5711_CLK_IDX_SCLK	1

/*
 * TAS5711 registers
 */

#if 0 // TAS5711 has automatic clock detect so we don't have to write in these registers
#define TAS5711_CLOCK_RATE(val)		(val << 5)
#define TAS5711_CLOCK_RATE_MASK		(0x7 << 5)
#define TAS5711_CLOCK_RATIO(val)	(val << 2)
#define TAS5711_CLOCK_RATIO_MASK	(0x7 << 2)
#endif

#define TAS5711_OSC_TRIM_VAL(val) 	(val << 1)
#define TAS5711_OSC_TRIM_VAL_MASK	(0x1 << 1)

#define TAS5711_CLOCK_CONTROL		0x00			/* Clock control register  */
#define TAS5711_DEV_ID			0x01			/* Device ID register */
#define TAS5711_ERROR_STATUS		0x02			/* Error status register */
#define TAS5711_SYS_CONTROL_1		0x03			/* System control register 1 */
#define TAS5711_SERIAL_DATA_IF		0x04			/* Serial data interface register  */
#define TAS5711_SYS_CONTROL_2		0x05			/* System control register 2 */
#define TAS5711_SOFT_MUTE		0x06			/* Soft mute register */
#define TAS5711_MASTER_VOL		0x07			/* Master volume  */
#define TAS5711_CHANNEL_VOL(X)		(0x08 + (X))	/* Channel 1-3 volume */
#define TAS5711_VOLUME_CONFIG		0x0E			/* Volume configuration register */
#define TAS5711_MOD_LIMIT		0x10			/* Modulation limit register */
#define TAS5711_INTER_DLY_1		0x11			/* Interchannel Delay Registers, Channel 1 */
#define TAS5711_INTER_DLY_2		0x12			/* Channel 2 */
#define TAS5711_INTER_DLY_not1		0x13			/* Channel not 1 */
#define TAS5711_INTER_DLY_not2		0x14			/* Channel not 2 */
#define TAS5711_PWM_SHUTDOWN		0x19			/* PWM shutdown group register */
#define TAS5711_START_STOP_PERIOD	0x1A			/* Start/Stop period register */
#define TAS5711_OSC_TRIM		0x1B			/* Oscillator trim register */
#define TAS5711_BKNDERR 		0x1C			/* Back-end error register */

#define TAS5711_INPUT_MUX		0x20			/* Input multiplexer register */
#define TAS5711_CHNL_4_SRC_SELECT	0x21			/* Channel 4 source select register */
#define TAS5711_PWM_OUTPUT_MUX		0x25			/* PWM output mux register */
#define TAS5711_DRC_CONTROL		0x46			/* DRC control register */
#define TAS5711_BANK_SWITCH_EQ		0x50			/* Bank switch and eq control register */


#define TAS5711_MAX_REGISTER		TAS5711_BANK_SWITCH_EQ

/* Bit Masks */
#define TAS5711_DEEMPH_MASK		0x03
#define TAS5711_SOFT_MUTE_ALL		0x07

/* Format Parameters */
#define TAS5711_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE  |		\
			     SNDRV_PCM_FMTBIT_S20_3LE |		\
			     SNDRV_PCM_FMTBIT_S24_LE |		\
			     SNDRV_PCM_FMTBIT_S32_LE)
#if 0
/*
 *	make list with snc_pcm_constraint_list()
 */

#define TAS5711_PCM_RATES   (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025  | \
			     SNDRV_PCM_RATE_12000 | SNDRV_PCM_RATE_16000  | \
			     SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_24000 | \
			     SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)
#endif				 
#define TAS5711_PCM_RATES   (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025  | \
			     SNDRV_PCM_RATE_16000 | \
			     SNDRV_PCM_RATE_22050 | \
			     SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)

/*
 * Default TAS5711 power-up configuration
 */

static const struct reg_default tas5711_reg_defaults[] = {
	{ 0x00,	0x6c },
	{ 0x01,	0x70 },
	{ 0x02,	0x00 },
	{ 0x03,	0xA0 },
	{ 0x04,	0x05 },
	{ 0x05,	0x40 },
	{ 0x06,	0x00 },
	{ 0x07,	0xFF },
	{ 0x08,	0x30 },
	{ 0x09,	0x30 },
	{ 0x0a,	0x30 },
	{ 0x0e,	0x91 },
	{ 0x10,	0x02 },
	{ 0x11,	0xAC },
	{ 0x12,	0x54 },
	{ 0x13,	0xAC },
	{ 0x14,	0x54 },
	{ 0x19,	0x30 },
	{ 0x1A,	0x0F },
	{ 0x1B,	0x82 },
	{ 0x1C,	0x02 },
	{ 0x20,	0x00017772 },
	{ 0x21,	0x00004303 },
	{ 0x25,	0x01021345 },
	{ 0x46,	0x00000000 },
	{ 0x50,	0x0F708000 },
};

static int tas5711_register_size(struct device *dev, unsigned int reg)
{
	switch (reg) {
		case TAS5711_CLOCK_CONTROL ... TAS5711_CHANNEL_VOL(3):
		case TAS5711_VOLUME_CONFIG:
		case TAS5711_MOD_LIMIT ... TAS5711_BKNDERR:
			return 1;
		case TAS5711_INPUT_MUX ... TAS5711_CHNL_4_SRC_SELECT:
		case TAS5711_PWM_OUTPUT_MUX:
		case TAS5711_DRC_CONTROL:
		case TAS5711_BANK_SWITCH_EQ:
			return 4;
	}

	dev_err(dev, "Unsupported register address: %d\n", reg);
	return 0;
}

static bool tas5711_accessible_reg(struct device *dev, unsigned int reg)
{
	/* reserved register returns false */
	switch (reg) {
		case 0x0B ... 0x0D:
		case 0x0F:
		case 0x15 ... 0x18:
		case 0x1D ... 0x1F:
		case 0x22 ... 0x24:
		case 0x26 ... 0x28:
		case 0x37 ... 0x39:
		case 0x47 ... 0x4F:
		case 0x5F:
		case 0x63 ... 0xF7:
		case 0xFB ... 0xFF:
			return false;
		default:
			return true;
	}
}

static bool tas5711_volatile_reg(struct device *dev, unsigned int reg)
{
	/* any more?? */
	switch (reg) {
		case TAS5711_CLOCK_CONTROL:
		case TAS5711_DEV_ID:
		case TAS5711_ERROR_STATUS:
			return true;
	}
	return false;
}

static bool tas5711_writeable_reg(struct device *dev, unsigned int reg)
{
	return tas5711_accessible_reg(dev, reg) && (reg != TAS5711_DEV_ID);
}

static int tas5711_reg_write(void *context, unsigned int reg,
			      unsigned int value)
{
	struct i2c_client *client = context;
	unsigned int i, size;
	uint8_t buf[5];
	int ret;

	size = tas5711_register_size(&client->dev, reg);
	if (size == 0)
		return -EINVAL;

	buf[0] = reg;

	for (i = size; i >= 1; --i) {
		buf[i] = value;
		value >>= 8;
	}

	ret = i2c_master_send(client, buf, size + 1);
	if (ret == size + 1)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}


static int tas5711_reg_read(void *context, unsigned int reg,
			     unsigned int *value)
{
	/* write value of register "reg" to "value" */
	struct i2c_client *client = context;
	uint8_t send_buf, recv_buf[4];
	struct i2c_msg msgs[2];
	unsigned int size;
	unsigned int i;
	int ret;

	size = tas5711_register_size(&client->dev, reg);
	if (size == 0)
		return -EINVAL;

	send_buf = reg;

	msgs[0].addr = client->addr;
	msgs[0].len = sizeof(send_buf);
	msgs[0].buf = &send_buf;
	msgs[0].flags = 0;

	msgs[1].addr = client->addr;
	msgs[1].len = size;
	msgs[1].buf = recv_buf;
	msgs[1].flags = I2C_M_RD;

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		return ret;
	else if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*value = 0;

	for (i = 0; i < size; i++) {
		*value <<= 8;
		*value |= recv_buf[i];
	}

	return 0;
}

struct tas5711_private {
	struct regmap	*regmap;
	unsigned int	mclk, sclk;
	unsigned int	format;

	bool		deemph;

	/* Current sample rate for de-emphasis control */
	int		rate;
	/* GPIO driving Reset pin, if any */
	int		gpio_nreset;
};

static int tas5711_deemph[] = { 0, 32000, 44100, 48000 };

static int tas5711_set_deemph(struct snd_soc_codec *codec)
{
	struct tas5711_private *priv = snd_soc_codec_get_drvdata(codec);
	int i, val = 0;

	if (priv->deemph)
		for (i = 0; i < ARRAY_SIZE(tas5711_deemph); i++)
			if (tas5711_deemph[i] == priv->rate)
				val = i;

	return regmap_update_bits(priv->regmap, TAS5711_SYS_CONTROL_1,
				  TAS5711_DEEMPH_MASK, val);
}

static int tas5711_get_deemph(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tas5711_private *priv = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = priv->deemph;

	return 0;
}

static int tas5711_put_deemph(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tas5711_private *priv = snd_soc_codec_get_drvdata(codec);

	priv->deemph = ucontrol->value.enumerated.item[0];

	return tas5711_set_deemph(codec);
}



static int tas5711_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct tas5711_private *priv = snd_soc_codec_get_drvdata(codec);

	switch (clk_id) {
		case TAS5711_CLK_IDX_MCLK:
			priv->mclk = freq;
			break;
		case TAS5711_CLK_IDX_SCLK:
			priv->sclk = freq;
			break;
	}

	return 0;
}

static int tas5711_set_dai_fmt(struct snd_soc_dai *codec_dai,
			       unsigned int format)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct tas5711_private *priv = snd_soc_codec_get_drvdata(codec);

	/* The TAS5086 can only be slave to all clocks */
	/* same with TA5711 ?? */
	if ((format & SND_SOC_DAIFMT_MASTER_MASK) != SND_SOC_DAIFMT_CBS_CFS) {
		dev_err(codec->dev, "Invalid clocking mode\n");
		return -EINVAL;
	}

	/* we need to refer to the data format from hw_params() */
	priv->format = format;

	return 0;
}

#if 0 // not needed because of clock auto detect
static const int tas5711_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};



static const int tas5711_ratios[] = {
	64, 128, 192, 256, 384, 512
};
#endif


static int index_in_array(const int *array, int len, int needle)
{
	/* returns index of value "needle" in given array */
	int i;

	for (i = 0; i < len; i++)
		if (array[i] == needle)
			return i;

	return -ENOENT;
}

static int tas5711_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	/* set hardware parameters 
	 * propably when starting the driver 
	 */
	struct snd_soc_codec *codec = dai->codec;
	struct tas5711_private *priv = snd_soc_codec_get_drvdata(codec);
	int val;
	int ret;

	priv->rate = params_rate(params);
	
#if 0 //TAS5711 has clock auto detect so the following shouldn't be needed

	/* Look up the sample rate and refer to the offset in the list */
	val = index_in_array(tas5711_sample_rates,
			     ARRAY_SIZE(tas5711_sample_rates), priv->rate);

	if (val < 0) {
		dev_err(codec->dev, "Invalid sample rate\n");
		return -EINVAL;
	}
	

	ret = regmap_update_bits(priv->regmap, TAS5711_CLOCK_CONTROL,
				 TAS5711_CLOCK_RATE_MASK,
				 TAS5711_CLOCK_RATE(val));
	if (ret < 0)
		return ret;


	/* MCLK / Fs ratio */
	val = index_in_array(tas5711_ratios, ARRAY_SIZE(tas5086_ratios),
			     priv->mclk / priv->rate);
	if (val < 0) {
		dev_err(codec->dev, "Inavlid MCLK / Fs ratio\n");
		return -EINVAL;
	}

	ret = regmap_update_bits(priv->regmap, TAS5086_CLOCK_CONTROL,
				 TAS5086_CLOCK_RATIO_MASK,
				 TAS5086_CLOCK_RATIO(val));
	if (ret < 0)
		return ret;


	ret = regmap_update_bits(priv->regmap, TAS5711_CLOCK_CONTROL,
				 TAS5711_CLOCK_SCLK_RATIO_48,
				 (priv->sclk == 48 * priv->rate) ? 
					TAS5711_CLOCK_SCLK_RATIO_48 : 0);
	if (ret < 0)
		return ret;
		
#endif


	/*
	 * Same format mapping as the TAS5086
	 *
	 * Commend from Daniel Mack:
	 * The chip has a very unituitive register mapping and muxes information
	 * about data format and sample depth into the same register, but not on
	 * a logical bit-boundary. Hence, we have to refer to the format passed
	 * in the set_dai_fmt() callback and set up everything from here.
	 *
	 * First, determine the 'base' value, using the format ...
	 */
// XXX we use the default format for now
#if 0 
	switch (priv->format & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_RIGHT_J:
			val = 0x00;
			break;
		case SND_SOC_DAIFMT_I2S:
			val = 0x03;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			val = 0x06;
			break;
		default:
			dev_err(codec->dev, "Invalid DAI format\n");
			return -EINVAL;
	}

	/* ... then add the offset for the sample bit depth. */
	switch (params_format(params)) {
        case SNDRV_PCM_FORMAT_S16_LE:
			val += 0;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			val += 1;
			break;
		case SNDRV_PCM_FORMAT_S24_3LE:
			val += 2;
			break;
		default:
			dev_err(codec->dev, "Invalid bit width\n");
			return -EINVAL;
	};

	ret = regmap_write(priv->regmap, TAS5711_SERIAL_DATA_IF, val);
	if (ret < 0)
		return ret;
#endif
#if 0
	/* clock is considered valid now */
	ret = regmap_update_bits(priv->regmap, TAS5711_CLOCK_CONTROL,
				 TAS5711_CLOCK_VALID, TAS5711_CLOCK_VALID);
	if (ret < 0)
		return ret;
#endif

	return tas5711_set_deemph(codec);
	/*
	 * which returns:
	 *		regmap_update_bits(priv->regmap, TAS5711_SYS_CONTROL_1,
	 *			  TAS5711_DEEMPH_MASK, val);
	 */
}

static int tas5711_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5711_private *priv = snd_soc_codec_get_drvdata(codec);
	unsigned int val = 0;

	if (mute)
		val = TAS5711_SOFT_MUTE_ALL;

	return regmap_write(priv->regmap, TAS5711_SOFT_MUTE, val);
}


 
/* TAS5711 controls */
static const DECLARE_TLV_DB_SCALE(tas5711_dac_tlv, -10350, 50, 1);


static const struct snd_kcontrol_new tas5711_controls[] = {
	SOC_SINGLE_TLV("Master Playback Volume", TAS5711_MASTER_VOL,
		       0, 0xff, 1, tas5711_dac_tlv),
	SOC_SINGLE_TLV("Channel 1 Volume", TAS5711_CHANNEL_VOL(0),
		       0, 0xff, 1, tas5711_dac_tlv),
	SOC_SINGLE_TLV("Channel 2 Volume", TAS5711_CHANNEL_VOL(1),
		       0, 0xff, 1, tas5711_dac_tlv),
#if 0
	SOC_DOUBLE_R_TLV("Channel 1/2 Playback Volume",
			 TAS5711_CHANNEL_VOL(0), TAS5711_CHANNEL_VOL(1),
			 0, 0xff, 1, tas5711_dac_tlv),
	SOC_SINGLE_TLV("Channel 3 Playback Volume",
			 TAS5711_CHANNEL_VOL(2),
			 0, 0xff, 1, tas5711_dac_tlv),
	SOC_SINGLE_BOOL_EXT("De-emphasis Switch", 0,
			    tas5711_get_deemph, tas5711_put_deemph),
#endif
};

// not really needed
#if 0
/* Input mux controls */
static const char *tas5711_dapm_sdin_texts[] =
{
	"SDIN-L", "SDIN-R"
};

//TODO check this, SOC_ENUM_SINGLE()???
static const struct soc_enum tas5711_dapm_input_mux_enum[] = {
	SOC_ENUM_SINGLE(TAS5711_INPUT_MUX, 20, 8, tas5711_dapm_sdin_texts),
	SOC_ENUM_SINGLE(TAS5711_INPUT_MUX, 16, 8, tas5711_dapm_sdin_texts),
};

static const struct snd_kcontrol_new tas5711_dapm_input_mux_controls[] = {
	SOC_DAPM_ENUM("Channel 1 input", tas5711_dapm_input_mux_enum[0]),
	SOC_DAPM_ENUM("Channel 2 input", tas5711_dapm_input_mux_enum[1]),
};

/* Output mux controls */
static const char *tas5711_dapm_channel_texts[] =
	{ "Channel 1 Mux", "Channel 2 Mux", 
	  "Channel 3 Mux", "Channel 4 Mux"};

static const struct soc_enum tas5711_dapm_output_mux_enum[] = {
	SOC_ENUM_SINGLE(TAS5711_PWM_OUTPUT_MUX, 20, 6, tas5711_dapm_channel_texts),
	SOC_ENUM_SINGLE(TAS5711_PWM_OUTPUT_MUX, 16, 6, tas5711_dapm_channel_texts),
	SOC_ENUM_SINGLE(TAS5711_PWM_OUTPUT_MUX, 12, 6, tas5711_dapm_channel_texts),
	SOC_ENUM_SINGLE(TAS5711_PWM_OUTPUT_MUX, 8,  6, tas5711_dapm_channel_texts),
};

static const struct snd_kcontrol_new tas5711_dapm_output_mux_controls[] = {
	SOC_DAPM_ENUM("OUT_A", tas5711_dapm_output_mux_enum[0]),
	SOC_DAPM_ENUM("OUT_B", tas5711_dapm_output_mux_enum[1]),
	SOC_DAPM_ENUM("OUT_C", tas5711_dapm_output_mux_enum[2]),
	SOC_DAPM_ENUM("OUT_D", tas5711_dapm_output_mux_enum[3]),
};

static const struct snd_soc_dapm_widget tas5711_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("SDIN-L"), //TODO this should be "Channel 1 input"
	SND_SOC_DAPM_INPUT("SDIN-R"),

	SND_SOC_DAPM_OUTPUT("OUT_A"), // or this should be "Channel 1 Mux"
	SND_SOC_DAPM_OUTPUT("OUT_B"),
	SND_SOC_DAPM_OUTPUT("OUT_C"),
	SND_SOC_DAPM_OUTPUT("OUT_D"),


	SND_SOC_DAPM_MUX("Channel 1 Mux", SND_SOC_NOPM, 0, 0,
			 &tas5711_dapm_input_mux_controls[0]),
	SND_SOC_DAPM_MUX("Channel 2 Mux", SND_SOC_NOPM, 0, 0,
			 &tas5711_dapm_input_mux_controls[1]),
	SND_SOC_DAPM_MUX("Channel 3 Mux", SND_SOC_NOPM, 0, 0,
			 &tas5711_dapm_input_mux_controls[2]),
	SND_SOC_DAPM_MUX("Channel 4 Mux", SND_SOC_NOPM, 0, 0,
			 &tas5711_dapm_input_mux_controls[3]),

	SND_SOC_DAPM_MUX("PWM1 Mux", SND_SOC_NOPM, 0, 0,
			 &tas5711_dapm_output_mux_controls[0]),
	SND_SOC_DAPM_MUX("PWM2 Mux", SND_SOC_NOPM, 0, 0,
			 &tas5711_dapm_output_mux_controls[1]),
	SND_SOC_DAPM_MUX("PWM3 Mux", SND_SOC_NOPM, 0, 0,
			 &tas5711_dapm_output_mux_controls[2]),
	SND_SOC_DAPM_MUX("PWM4 Mux", SND_SOC_NOPM, 0, 0,
			 &tas5711_dapm_output_mux_controls[3]),
};

static const struct snd_soc_dapm_route tas5711_dapm_routes[] = {
	/* SDIN inputs -> channel muxes */
	{ "Channel 1 Mux", "SDIN-L", "SDIN-L" },
	{ "Channel 1 Mux", "SDIN-R", "SDIN-R" },

	{ "Channel 2 Mux", "SDIN-L", "SDIN-L" },
	{ "Channel 2 Mux", "SDIN-R", "SDIN-R" },

	{ "Channel 2 Mux", "SDIN-L", "SDIN-L" },
	{ "Channel 2 Mux", "SDIN-R", "SDIN-R" },

	{ "Channel 3 Mux", "SDIN-L", "SDIN-L" },
	{ "Channel 3 Mux", "SDIN-R", "SDIN-R" },

	{ "Channel 4 Mux", "SDIN-L", "SDIN-L" },
	{ "Channel 4 Mux", "SDIN-R", "SDIN-R" },

	{ "Channel 5 Mux", "SDIN-L", "SDIN-L" },
	{ "Channel 5 Mux", "SDIN-R", "SDIN-R" },

	{ "Channel 6 Mux", "SDIN-L", "SDIN-L" },
	{ "Channel 6 Mux", "SDIN-R", "SDIN-R" },

	/* Channel muxes -> PWM muxes */
	{ "PWM1 Mux", "Channel 1 Mux", "Channel 1 Mux" },
	{ "PWM2 Mux", "Channel 1 Mux", "Channel 1 Mux" },
	{ "PWM3 Mux", "Channel 1 Mux", "Channel 1 Mux" },
	{ "PWM4 Mux", "Channel 1 Mux", "Channel 1 Mux" },

	{ "PWM1 Mux", "Channel 2 Mux", "Channel 2 Mux" },
	{ "PWM2 Mux", "Channel 2 Mux", "Channel 2 Mux" },
	{ "PWM3 Mux", "Channel 2 Mux", "Channel 2 Mux" },
	{ "PWM4 Mux", "Channel 2 Mux", "Channel 2 Mux" },

	{ "PWM1 Mux", "Channel 3 Mux", "Channel 3 Mux" },
	{ "PWM2 Mux", "Channel 3 Mux", "Channel 3 Mux" },
	{ "PWM3 Mux", "Channel 3 Mux", "Channel 3 Mux" },
	{ "PWM4 Mux", "Channel 3 Mux", "Channel 3 Mux" },

	{ "PWM1 Mux", "Channel 4 Mux", "Channel 4 Mux" },
	{ "PWM2 Mux", "Channel 4 Mux", "Channel 4 Mux" },
	{ "PWM3 Mux", "Channel 4 Mux", "Channel 4 Mux" },
	{ "PWM4 Mux", "Channel 4 Mux", "Channel 4 Mux" },

	{ "PWM1 Mux", "Channel 5 Mux", "Channel 5 Mux" },
	{ "PWM2 Mux", "Channel 5 Mux", "Channel 5 Mux" },
	{ "PWM3 Mux", "Channel 5 Mux", "Channel 5 Mux" },
	{ "PWM4 Mux", "Channel 5 Mux", "Channel 5 Mux" },

	{ "PWM1 Mux", "Channel 6 Mux", "Channel 6 Mux" },
	{ "PWM2 Mux", "Channel 6 Mux", "Channel 6 Mux" },
	{ "PWM3 Mux", "Channel 6 Mux", "Channel 6 Mux" },
	{ "PWM4 Mux", "Channel 6 Mux", "Channel 6 Mux" },

};

#endif
static const struct snd_soc_dai_ops tas5711_dai_ops = {
	.hw_params	= tas5711_hw_params,
	.set_sysclk	= tas5711_set_dai_sysclk,
	.set_fmt	= tas5711_set_dai_fmt,
	.mute_stream	= tas5711_mute_stream,
};

static struct snd_soc_dai_driver tas5711_dai = {
	.name = "tas5711-hifi",
	.playback = {
		.stream_name	= "Playback",
		.channels_min	= 2,
		.channels_max	= 3,
		.rates		= TAS5711_PCM_RATES,
		.formats	= TAS5711_PCM_FORMATS,
	},
	.ops = &tas5711_dai_ops,
};

#ifdef CONFIG_PM
static int tas5711_soc_resume(struct snd_soc_codec *codec)
{
	struct tas5711_private *priv = snd_soc_codec_get_drvdata(codec);

	/* Restore codec state */
	return regcache_sync(priv->regmap);
}
#else
#define tas5711_soc_resume	NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static const struct of_device_id tas5711_dt_ids[] = {
	{ .compatible = "ti,tas5711", },
	{ }
};
MODULE_DEVICE_TABLE(of, tas5711_dt_ids);
#endif


/* start/stop period values in microseconds */
static const int tas5711_start_stop_period[] = {
	  16500,   23900,   31400,   40400,   53900,   70300,   94200,   125700,
	 164600,  239400,  314200,  403900,  538600,  703100,  942500,  1256600,
	1728100, 2513600, 3299100, 4241700, 5655600, 7383700, 9897300, 13196400,
};

static int tas5711_probe(struct snd_soc_codec *codec)
{
	printk("++++++++++++++++++int tas5711 probe()++++++++++++++++++++\n");
	struct tas5711_private *priv = snd_soc_codec_get_drvdata(codec);
	int start_stop_period = 125700; /* hardware default is 125,70 ms */
	u8 pwm_start_mid_z = 0;
	int i, ret;

	//TODO what does this???
	if (of_match_device(of_match_ptr(tas5711_dt_ids), codec->dev)) {
		struct device_node *of_node = codec->dev->of_node;
		//TODO "ti,start-stop-period" ????
		of_property_read_u32(of_node, "ti,start-stop-period", &start_stop_period);

#if 0
		for (i = 0; i < 6; i++) {
			char name[25];

			snprintf(name, sizeof(name),
				 "ti,mid-z-channel-%d", i + 1);

			if (of_get_property(of_node, name, NULL) != NULL)
				pwm_start_mid_z |= 1 << i;
		}
		
#endif
	}
	else
		printk("of_match_device() == false\n");
#if 0
	/*
	 * If any of the channels is configured to start in Mid-Z mode,
	 * configure 'part 1' of the PWM starts to use Mid-Z, and tell
	 * all configured mid-z channels to start start under 'part 1'.
	 */
	if (pwm_start_mid_z)
		regmap_write(priv->regmap, TAS5711_PWM_START,
			     TAS5711_PWM_START_MIDZ_FOR_START_1 |
				pwm_start_mid_z);

	/* lookup and set split-capacitor charge period */
	if (charge_period == 0) {
		regmap_write(priv->regmap, TAS5711_SPLIT_CAP_CHARGE, 0);
	} else {
		i = index_in_array(tas5711_charge_period,
				   ARRAY_SIZE(tas5711_charge_period),
				   charge_period);
		if (i >= 0)
			regmap_write(priv->regmap, TAS5711_SPLIT_CAP_CHARGE,
				     i + 0x08);
		else
			dev_warn(codec->dev,
				 "Invalid split-cap charge period of %d ns.\n",
				 charge_period);
	}
#endif

//XXX writing to reserved registers. this is ok said the datasheet
#if 1
	/* enable Oscillator trim */
	ret = regmap_write(priv->regmap, TAS5711_OSC_TRIM,
			   0x00);
	if (ret < 0) {
		printk("setting up oscillator trim failed\n");
		return ret;
	}
#else
	/* enable factory trim */
	ret = regmap_update_bits(priv->regmap, TAS5711_OSC_TRIM, 
						TAS5711_OSC_TRIM_VAL(0), 
						TAS5711_OSC_TRIM_VAL_MASK);
	if (ret < 0) {
		printk("oscillator trim failed\n");
		return ret;
	}
#endif
// TODO check what to do here
#if 0 
	/* start all channels */
	ret = regmap_write(priv->regmap, TAS5711_SYS_CONTROL_2, 0x20);
	if (ret < 0)
		return ret;

	/* set master volume to 0 dB */
	ret = regmap_write(priv->regmap, TAS5711_MASTER_VOL, 0x30);
	if (ret < 0)
		return ret;
#endif
	/* start all channel (disable hard mute) */
	ret = regmap_write(priv->regmap, TAS5711_SYS_CONTROL_2,
			   0x00);
	if (ret < 0) {
		printk("starting all channels failed\n");
		return ret;
	}


	/* unmute all channels for now */
	ret = regmap_write(priv->regmap, TAS5711_SOFT_MUTE,
			   /*TAS5711_SOFT_MUTE_ALL*/0x00);
	if (ret < 0) {
		printk("soft mute on all channels failed\n");
		return ret;
	}

	/* set error status register to zero */
	ret = regmap_write(priv->regmap, TAS5711_ERROR_STATUS,
			   0x00);
	if (ret < 0) {
		printk("resetting error status register failed\n");
		return ret;
	}

	/* set master volume to 0 dB */
	ret = regmap_write(priv->regmap, TAS5711_MASTER_VOL,
			   0x30);
	if (ret < 0) {
		printk("setting master volume to 0dB failed\n");
		return ret;
	}


	return 0;
}

static int tas5711_remove(struct snd_soc_codec *codec)
{
	struct tas5711_private *priv = snd_soc_codec_get_drvdata(codec);
	int ret;

	/* mute all channels */
	ret = regmap_write(priv->regmap, TAS5711_SOFT_MUTE,
			   TAS5711_SOFT_MUTE_ALL);
	if (ret < 0) {
		printk("soft mute all channels failed\n");
		return ret;
	}

	/* start all channel (disable hard mute) */
	// TODO use macros here!
	ret = regmap_update_bits(priv->regmap, TAS5711_SYS_CONTROL_2, 
						0x40, 
						0x40);
	if (ret < 0) {
		printk("stop all channels failed\n");
		return ret;
	}
#if 0
	ret = regmap_write(priv->regmap, TAS5711_SYS_CONTROL_2,
			   0x0);
	if (ret < 0) {
		printk("stop all channels failed\n");
		return ret;
	}
#endif
	if (gpio_is_valid(priv->gpio_nreset))
		/* Set codec to the reset state */
		gpio_set_value(priv->gpio_nreset, 0);
	printk("removing tas5711 successfull\n");
	return 0;
};

static struct snd_soc_codec_driver soc_codec_dev_tas5711 = {
	.probe			= tas5711_probe,
	.remove			= tas5711_remove,
	.resume			= tas5711_soc_resume,
	.controls		= tas5711_controls,
	.num_controls		= ARRAY_SIZE(tas5711_controls),
#if 0
	.dapm_widgets		= tas5711_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(tas5711_dapm_widgets),
	.dapm_routes		= tas5711_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(tas5711_dapm_routes),
#endif
};

static const struct i2c_device_id tas5711_i2c_id[] = {
	{ "tas5711", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas5711_i2c_id);

static const struct regmap_config tas5711_regmap = {
	.reg_bits		= 8,
	.val_bits		= 32,
	.max_register		= TAS5711_MAX_REGISTER,
	.reg_defaults		= tas5711_reg_defaults,
	.num_reg_defaults	= ARRAY_SIZE(tas5711_reg_defaults),
	.cache_type		= REGCACHE_RBTREE,
	.volatile_reg		= tas5711_volatile_reg,
	.writeable_reg		= tas5711_writeable_reg,
	.readable_reg		= tas5711_accessible_reg,
	.reg_read		= tas5711_reg_read,
	.reg_write		= tas5711_reg_write,
};

static int tas5711_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	printk("+++++++++++++++++++in i2c probe+++++++++++++++++++++\n");
	struct tas5711_private *priv;
	struct device *dev = &i2c->dev;
	int gpio_nreset = -EINVAL;
	int i, ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		printk("devm_kzalloc() failed");
		return -ENOMEM;
	}

	priv->regmap = devm_regmap_init(dev, NULL, i2c, &tas5711_regmap);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		dev_err(&i2c->dev, "Failed to create regmap: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(i2c, priv);

	if (of_match_device(of_match_ptr(tas5711_dt_ids), dev)) {
		struct device_node *of_node = dev->of_node;
		gpio_nreset = of_get_named_gpio(of_node, "reset-gpio", 0);
	}

	if (gpio_is_valid(gpio_nreset))
		if (devm_gpio_request(dev, gpio_nreset, "TAS5711 Reset")) {
			gpio_nreset = -EINVAL;
		}

	if (gpio_is_valid(gpio_nreset)) {
		/* Reset codec - minimum assertion time is 400ns */
		gpio_direction_output(gpio_nreset, 0);
		udelay(1);
		gpio_set_value(gpio_nreset, 1);

		/* Codec needs ~15ms to wake up */
		msleep(15);
	}

	priv->gpio_nreset = gpio_nreset;

#if 0
	/* The TAS5086 always returns 0x03 in its TAS5086_DEV_ID register */
	ret = regmap_read(priv->regmap, TAS5711_DEV_ID, &i);
	if (ret < 0) {
		printk ("regmap_read() failed\n");
		return ret;
	}

	if (i != 0x3) {
		dev_err(dev,
			"Failed to identify TAS5711 codec (got %02x)\n", i);
		return -ENODEV;
	}
#endif
#if 0 // not needed
	ret = regmap_read(priv->regmap, TAS5711_ERROR_STATUS, &i);
	if (ret < 0) {
		printk ("regmap_read() failed with code: %d\n", ret);
		return ret;
	}
	else
		printk ("Error status: %d\n", i);

#endif

	return snd_soc_register_codec(&i2c->dev, &soc_codec_dev_tas5711,
		&tas5711_dai, 1);
}

static int tas5711_i2c_remove(struct i2c_client *i2c)
{
	printk("++++++++++++++ in tas5711_i2c_remove ++++++++++++++++++++\n");
	snd_soc_unregister_codec(&i2c->dev);
	printk("removing tas5711_i2c successfull\n");
	return 0;
}

static struct i2c_driver tas5711_i2c_driver = {
	.driver = {
		.name	= "tas5711",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tas5711_dt_ids),
	},
	.id_table	= tas5711_i2c_id,
	.probe		= tas5711_i2c_probe,
	.remove		= tas5711_i2c_remove,
};

module_i2c_driver(tas5711_i2c_driver);

MODULE_AUTHOR("Felix Kramer <felixkramerroki@aol.com>");
MODULE_DESCRIPTION("Texas Instruments TAS5711 ALSA SoC Codec Driver");
MODULE_LICENSE("GPL");


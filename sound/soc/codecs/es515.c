/*
 * es515.c  --	Audience eS515 ALSA SoC Audio driver
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author:
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "es515.h"
#include "es515-access.h"
#include "es515_firm.h"

#include <linux/io.h>
#include <mach/regs-pmu.h>

#define ES515_DEBUG

#ifdef CONFIG_SND_SOC_ES515_I2S_MASTER
#define CONFIG_ES515_MASTER
#endif

#define FW_FILE		"/vendor/firmware/es515_fw.bin"
#define USE_BUILTIN_FW

/*******************************************************************************
 * Function declarations
 ******************************************************************************/
#if defined(CONFIG_SND_SOC_ES515_I2C)
static int es515_i2c_read(struct es515_priv *es515, char *buf, int len);
static int es515_i2c_write(struct es515_priv *es515, char *buf, int len);
#endif
static int es515_codec_probe(struct snd_soc_codec *codec);
static unsigned int es515_read(struct snd_soc_codec *codec,
		unsigned int reg);
static int es515_write(struct snd_soc_codec *codec, unsigned int reg,
		unsigned int value);
static int es515_opt_not_available(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_control_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_control_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_control_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_control_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_sync_mode_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_sync_mode_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_suppress_response_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_suppress_response_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_passthrough(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_mp3_mode_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_output_known_sig_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_output_known_sig_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_device_param_id(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_device_param_id(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_device_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_device_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_algorithm_param_id(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_algorithm_param_id(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_algorithm_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_algorithm_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_set_codec_addr(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_codec_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_setdigital_gain(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_setdigital_gain(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_getdigital_gain(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_getdigital_gain(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_setdata_path(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_setdata_path(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_getdata_path(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_getdata_path(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_internal_route_config(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_internal_route_config(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_dereverb_gain_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_dereverb_gain_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_bwe_high_band_gain_value(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_bwe_high_band_gain_value(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_wakeup(struct es515_priv *es515);
static int es515_get_power_state_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_set_switch_codec_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_set_delete_codec_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_set_copy_codec_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_power_state_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static void es515_switch_route(unsigned int route_index);
static irqreturn_t es515_threaded_isr(int irq, void *data);
#if defined(CONFIG_SND_SOC_ES515_SLIMBUS)
static int es515_get_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_get_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_put_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_ap_get_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
static int es515_ap_put_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
#endif

/*******************************************************************************
 * Global variable declarations
 ******************************************************************************/

int skip_debug;
int fw_download = 0;

/* Status of Suppress Response Bit */
bool ES515_SR_BIT;

struct es515_priv es515_priv_glb;

static int es515_dai_word_length[4];
static int es515_device_word_length(int dai_id, int bits_per_sample);

#if defined(CONFIG_SND_SOC_ES515_SLIMBUS)
static unsigned int es515_ap_tx1_ch_cnt = 1;
unsigned int es515_rx1_route_enable;
unsigned int es515_tx1_route_enable;
unsigned int es515_rx2_route_enable;
#endif

static unsigned int es515_internal_route_num;

static unsigned int es515_power_state = 3;
static unsigned int es515_current_output_known_sig;
static unsigned int es515_current_device_param_id;
static unsigned int es515_current_algorithm_param_id;
static unsigned int es515_current_sync_mode;
static unsigned int es515_current_path_gain_value;
static unsigned int es515_current_path_type;
static unsigned int es515_current_data_path_value;
static unsigned int es515_requested_path_type;
static unsigned int es515_current_out_codec_port = ES515_CODEC_PORT_SPEAKER;
static unsigned int es515_current_copied_codec_port = ES515_CODEC_PORT_SPEAKER;
static unsigned int es515_current_copied_ports_nr;

struct es515_route_configs {
	uint8_t macro[MAX_MACRO_LEN];
	uint8_t active_out_codec_port;
};
static struct es515_route_configs es515_def_route_configs[] = {

	/* [0]: Analog to Analog: I/P: Aux In (Left,right),
	   O/P: Line Out (Chan 0, 1) */
	/* Sinewave enabled */
	{
		.macro = {
#if defined(CONFIG_ES515_MASTER)
			/* Set param ID for  Port 0, clock control */
			0x80, 0x0C, 0x0A, 0x09,
			/* Port 0, Master mode, 8KHz, */
			0x80, 0x0D, 0x01, 0x08,
#endif
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* SetDataPath, 0x0E: AUX, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x19, 0xC0,
			/* SetDataPath, 0x0E: AUX, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1D, 0xC1,
			/* SetDataPath, 0x16: LO, 0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0xC0,
			/* SetDataPath, 0x16: LO, 0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x52, 0xC1,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0x90, 0x5C, 0x00, 0x04,
			/* set 1Kz signal output */
			0x90, 0x1E, 0x00, 0x05,
			0xff,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},

	/* [1]: Digital to Analog: I/P: Port A (Ch0 ,1),
	   O/P: Line Out (Chan 0, 1) */
	/* Sinewave enabled */
	{
		.macro = {
#if defined(CONFIG_ES515_MASTER)
			/* Set param ID for  Port 0, clock control */
			0x80, 0x0C, 0x0A, 0x09,
			/* Port 0, Master mode, 8KHz, */
			0x80, 0x0D, 0x01, 0x08,
#endif
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set 1Kz signal output */
			0x90, 0x1E, 0x00, 0x05,
			/* SetDataPath, 0x00: PCM0, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x01,
			/* SetDataPath, 0x16: LO,  0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0xC0,
			/* SetDataPath, 0x16: LO,  0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x52, 0xC1,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xff,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,

	},
	/* [2]: Analog to Analog: I/P: Aux In (Left,right),
	   O/P: Line Out (Chan 0, 1) */
	/* Sinewave disabled */
	{
		.macro = {
#if defined(CONFIG_ES515_MASTER)
			/* Set param ID for  Port 0, clock control */
			0x80, 0x0C, 0x0A, 0x09,
			/* Port 0, Master mode, 8KHz, */
			0x80, 0x0D, 0x01, 0x08,
#endif
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* SetDataPath, 0x0E: AUX, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x19, 0xC0,
			/* SetDataPath, 0x0E: AUX, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1D, 0xC1,
			/* SetDataPath, 0x16: LO,  0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0xC0,
			/* SetDataPath, 0x16: LO,  0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x52, 0xC1,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0x90, 0x5C, 0x00, 0x04,
			0xff,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,

	},

	/* [3]: Digital to Analog: I/P: Port A (Ch0 ,1),
	   O/P: Line Out (Chan 0, 1) */
	/* Sinewave disabled */
	{
		.macro = {
#if defined(CONFIG_ES515_MASTER)
			/* Set param ID for  Port 0, clock control */
			0x80, 0x0C, 0x0A, 0x09,
			/* Port 0, Master mode, 8KHz, */
			0x80, 0x0D, 0x01, 0x08,
#endif
			/* Set Smooth gain rate to 0 db */
			0x80, 0x4E, 0x00, 0x00,
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x0F,
			/* set device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x0F,
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x01,
			/* SetDataPath, 0x16: LO,   0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0xC0,
			/* SetDataPath, 0x16: LO,   0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x52, 0xC1,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			/* preset mode 74:
			   16bits 48KHz headphone playback + MIC1 capture */
			0x80, 0x31, 0x00, 0x4A,
#ifndef CONFIG_ES515_MASTER	/* Codec Slave */
			/* Set param ID for  Port 0, clock control */
			0x80, 0x0C, 0x0A, 0x09,
			/* Port 0, Slave mode, 48KHz, */
			0x80, 0x0D, 0x00, 0x30,
#endif
			/* set device param Id HP Left Gain */
			0x90, 0x0C, 0x1C, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x09,
			/* set device param Id HP Right Gain */
			0x90, 0x0C, 0x1C, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x09,
			/* set device param Id MIC1 Gain */
			0x90, 0x0C, 0x10, 0x00,
			/* 30 dB */
			0x90, 0x0D, 0x00, 0x14,
			/* set device param Id MIC2 Gain */
			0x90, 0x0C, 0x11, 0x00,
			/* 30 dB */
			0x90, 0x0D, 0x00, 0x14,
			/* set HP Ramped Gain switch */
			0x90, 0x0C, 0x1F, 0x61,
			/* turn off */
			0x90, 0x0D, 0x00, 0x00,
			0xff,
		},
/*		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,*/
		.active_out_codec_port = ES515_CODEC_PORT_HEADPHONE,

	},
	/* [4]  Route for Capture */
	{
		.macro = {
#if defined(CONFIG_ES515_MASTER)
			/* Set param ID for  Port 0, clock control */
			0x80, 0x0C, 0x0A, 0x09,
			/* Port 0, Master mode, 8KHz, */
			0x80, 0x0D, 0x01, 0x08,
#endif
			/* set device param Id Aux Left Gain */
			0x90, 0x0C, 0x14, 0x00,
			/* 3 dB */
			0x90, 0x0D, 0x00, 0x04,
			/* set device param Id Aux Right Gain */
			0x90, 0x0C, 0x14, 0x02,
			/* 3 dB */
			0x90, 0x0D, 0x00, 0x04,
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* AUX CH-0 to AUDIN 1 */
			0xB0, 0x5A, 0x19, 0xc0,
			/* AUX CH-1 to AUDIN 2*/
			0xB0, 0x5A, 0x1d, 0xc1,
			/* AUDOUT1 TO PCM CH-0 */
			0xB0, 0x5A, 0x4c, 0x00,
			/* AUDOUT2 TO PCM CH-1 */
			0xB0, 0x5A, 0x50, 0x01,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xff,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,

	},
	/* [5] Route for two capture */
	{
		.macro = {
#if defined(CONFIG_ES515_MASTER)
			/* Set param ID for  Port 0, clock control */
			0x80, 0x0C, 0x0A, 0x09,
			/* Port 0, Master mode, 8KHz, */
			0x80, 0x0D, 0x01, 0x08,
#endif
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* SetDeviceParmID, 0x14: AUX, 0x00: Left Gain */
			0x80, 0x0C, 0x14, 0x00,
			/* 6 dB */
			0x80, 0x0D, 0x00, 0x02,
			/* SetDeviceParmID, 0x14: AUX, 0x02: Right Gain */
			0x80, 0x0C, 0x14, 0x02,
			/* 6 dB */
			0x80, 0x0D, 0x00, 0x02,
			/*SetDeviceParmID, 0x10: MIC1, 0x00: Gain */
			0x80, 0x0C, 0x10, 0x00,
			/*  6.0 dB */
			0x80, 0x0D, 0x00, 0x04,
			/*SetAlgoType, 0x0004: Pass Through */
			0xA0, 0x5C, 0x00, 0x04,
			/*SetDataRate, 0x0001: 16kHz */
			0xA0, 0x4C, 0x00, 0x01,
			/*SetDataPath, 0x0E: AUX, 0x00: Ch0, 0x06: AUDIN1 */
			0xA0, 0x5A, 0x19, 0xC0,
			/*SetDataPath, 0x0E: AUX, 0x01: Ch1, 0x07: AUDIN2 */
			0xA0, 0x5A, 0x1D, 0xC1,
			/*SetDataPath, 0x0B: MIC1, 0x00: Ch0, 0x08: AUDIN3 */
			0xA0, 0x5A, 0x21, 0x60,
			/*SetDataPath, 0x00: PCM0, 0x00: Ch0, 0x13: AUDOUT1 */
			0xA0, 0x5A, 0x4C, 0x00,
			/*SetDataPath, 0x00: PCM0, 0x01: Ch1, 0x14: AUDOUT2 */
			0xA0, 0x5A, 0x50, 0x01,
			/*SetDataPath, 0x01: PCM1, 0x00: Ch0, 0x15: AUDOUT3 */
			0x90, 0x5A, 0x54, 0x20,
			0xFF,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},
	/* [6] Route for two Playback */
	{
		.macro = {
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Ear Phone Gain */
			0x90, 0x0C, 0x15, 0x00,
			/* 6 dB */
			0x90, 0x0D, 0x00, 0x0F,
			/* SetDataPath, 0x00: PCM0, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x01,
			/* SetDataPath, 0x16: LO,  0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0xC0,
			/* SetDataPath, 0x16: LO,  0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x52, 0xC1,
			/* SetDataPath, 0x01: PCM1, 0x00: Ch0, 0x08: AUDIN3 */
			0xB0, 0x5A, 0x20, 0x20,
			/* SetDataPath, 0x01: PCM1, 0x01: Ch1, 0x09: AUDIN4 */
			0xB0, 0x5A, 0x24, 0x21,
			/* SetDataPath, 0x16: EP,  0x00: Ch0, 0x13: AUDOUT3 */
			0xB0, 0x5A, 0x55, 0xE0,
			/* SetDataPath, 0x16: EP,  0x01: Ch1, 0x14: AUDOUT4 */
			0xB0, 0x5A, 0x59, 0xE1,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xFF,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},
	/* [7] Route for Playback + Capture */
	{
		.macro = {
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* Cet device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Aux Left Gain */
			0x90, 0x0C, 0x14, 0x00,
			/* 3 dB */
			0x90, 0x0D, 0x00, 0x01,
			/* set device param Id Aux Right Gain */
			0x90, 0x0C, 0x14, 0x02,
			/* 3 dB */
			0x90, 0x0D, 0x00, 0x01,
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x01,
			/* SetDataPath, 0x16: LO,  0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0xC0,
			/* SetDataPath, 0x16: LO,  0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x52, 0xC1,
			/* AUX CH-0 to AUDIN 3 */
			0xB0, 0x5A, 0x21, 0xC0,
			/* AUX CH-1 to AUDIN 4*/
			0xB0, 0x5A, 0x25, 0xC1,
			/* AUDOUT3 TO PCM1 CH-0 */
			0xB0, 0x5A, 0x54, 0x20,
			/* AUDOUT4 TO PCM1 CH-1 */
			0xB0, 0x5A, 0x58, 0x21,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xFF,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},
	/* [8] Route for Playback + Capture from PCM0 */
	{
		.macro = {
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Aux Left Gain */
			0x90, 0x0C, 0x14, 0x00,
			/* 3 dB */
			0x90, 0x0D, 0x00, 0x01,
			/* set device param Id Aux Right Gain */
			0x90, 0x0C, 0x14, 0x02,
			/* 3 dB */
			0x90, 0x0D, 0x00, 0x01,
			/* SetDataPath, 0x00: PCM0, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x01,
			/* SetDataPath, 0x16: LO,  0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0xC0,
			/* SetDataPath, 0x16: LO,  0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x52, 0xC1,
			/* AUX CH-0 to AUDIN 3 */
			0xB0, 0x5A, 0x21, 0xC0,
			/* AUX CH-1 to AUDIN 4*/
			0xB0, 0x5A, 0x25, 0xC1,
			/* SetDataPath, 0x00: PCM0, 0x00: Ch0, 0x15: AUDOUT3 */
			0xB0, 0x5A, 0x54, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x01: Ch1, 0x16: AUDOUT4 */
			0xB0, 0x5A, 0x58, 0x01,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xFF,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},
	/* [9] Route for primary-secondary in from LI (two channels),
	 * cs out on PCM1 in (single channel) with voice processing mode */
	{
		.macro = {
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id Aux Left Gain */
			0x90, 0x0C, 0x14, 0x00,
			/* 3 dB */
			0x90, 0x0D, 0x00, 0x01,
			/* set device param Id Aux Right Gain */
			0x90, 0x0C, 0x14, 0x02,
			/* 3 dB */
			0x90, 0x0D, 0x00, 0x01,
			/* AUX CH-0 to PRI */
			0xB0, 0x5A, 0x05, 0xC0,
			/* AUX CH-1 to SEC */
			0xB0, 0x5A, 0x09, 0xC1,
			/* SetDataPath, 0x01: PCM1, 0x00: Ch0, 0x10: CSOUT */
			0xB0, 0x5A, 0x40, 0x20,
			/* SetAlgoType, 0x0001: Voice Proc*/
			0xB0, 0x5C, 0x00, 0x01,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xFF,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},
	/* [10] fein from PCM1 playback from file (one channel),
	 * feout on LO with voice processing mode */
	{
		.macro = {
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* SetDataPath, 0x01: PCM1, 0x01: Ch1, 0x05: FEIN */
			0xB0, 0x5A, 0x14, 0x21,
			/* SetDataPath, 0x16: LO, 0x00: Ch0, 0x11: FEOUT1 */
			0xB0, 0x5A, 0x46, 0xC0,
			/* SetAlgoType, 0x0001: Voice Proc*/
			0xB0, 0x5C, 0x00, 0x01,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xFF,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},
	/* [11] =  [9] & [10] simultaneously*/
	{
		.macro = {
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id Aux Left Gain */
			0x90, 0x0C, 0x14, 0x00,
			/* 3 dB */
			0x90, 0x0D, 0x00, 0x01,
			/* set device param Id Aux Right Gain */
			0x90, 0x0C, 0x14, 0x02,
			/* 3 dB */
			0x90, 0x0D, 0x00, 0x01,
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* SetDataPath, 0x01: PCM1, 0x00: Ch0, 0x10: CSOUT */
			0xB0, 0x5A, 0x40, 0x20,
			/* AUX CH-0 to PRI */
			0xB0, 0x5A, 0x05, 0xC0,
			/* AUX CH-1 to SEC */
			0xB0, 0x5A, 0x09, 0xC1,
			/* SetDataPath, 0x01: PCM1, 0x01: Ch1, 0x05: FEIN */
			0xB0, 0x5A, 0x14, 0x21,
			/* SetDataPath, 0x16: LO, 0x00: Ch0, 0x11: FEOUT1 */
			0xB0, 0x5A, 0x46, 0xC0,
			/* SetAlgoType, 0x0001: Voice Proc*/
			0xB0, 0x5C, 0x00, 0x01,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xFF,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},
	/* [12]: Digital to Analog: I/P: Port A (Ch0,1), O/P: EP (Chan 0,1) */
	{
		.macro = {
			/* Set Smooth gain rate to 0 db */
			0x80, 0x4E, 0x00, 0x00,
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id EP Gain */
			0x90, 0x0C, 0x15, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* SetDataPath, 0x00: PCM0, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x01,
			/* SetDataPath, 0x0F: EP,  0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4D, 0xE0,
			/* SetDataPath, 0x0F: EP,  0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x51, 0xE1,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xff,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,

	},
	/* [13]: Digital to Analog: I/P: Port A (Ch0,1), O/P: HP (Chan 0,1) */
	{
		.macro = {
			/* Set Smooth gain rate to 0 db */
			0x80, 0x4E, 0x00, 0x00,
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id HP Left Gain */
			0x90, 0x0C, 0x1C, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id HP Right Gain */
			0x90, 0x0C, 0x1C, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* SetDataPath, 0x00: PCM0, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x01,
			/* SetDataPath, 0x14: HP,  0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0x80,
			/* SetDataPath, 0x14: HP,  0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x52, 0x81,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xff,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,

	},
	/* [14]  Route for Capture: I/P MIC, O/P:PCM1 */
	{
		.macro = {

			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id MIC1 Gain */
			0x90, 0x0C, 0x10, 0x00,
			/* 3 dB */
			0x90, 0x0D, 0x00, 0x01,
			/* AUDIN1 TO MIC1 CH-0 */
			0xB0, 0x5A, 0x19, 0x60,
			/* AUDOUT1 TO PCM1 CH-0 */
			0xB0, 0x5A, 0x04, 0x20,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xff,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},
	/* [15]: Digital to Analog: I/P: Port B (Ch0 ,1),
	   O/P: Line Out (Chan 0, 1) */
	/* Sinewave disabled */
	{
		.macro = {
#if defined(CONFIG_ES515_MASTER)
			/* Set param ID for  Port 0, clock control */
			0x80, 0x0C, 0x0A, 0x09,
			/* Port 0, Master mode, 8KHz, */
			0x80, 0x0D, 0x01, 0x08,
#endif
			/* Set Smooth gain rate to 0 db */
			0x80, 0x4E, 0x00, 0x00,
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x0F,
			/* set device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x0F,
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* SetDataPath, 0x00: PCM1, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x20,
			/* SetDataPath, 0x00: PCM1, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x21,
			/* SetDataPath, 0x16: LO,   0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0xC0,
			/* SetDataPath, 0x16: LO,   0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x52, 0xC1,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xff,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,

	},
	/* [16]: Digital to Analog: I/P: Port B (Ch 0),
	   O/P: Line Out (Chan 0)*/
	/* Sinewave disabled */
	{
		.macro = {
#if defined(CONFIG_ES515_MASTER)
			/* Set param ID for  Port 0, clock control */
			0x80, 0x0C, 0x0A, 0x09,
			/* Port 0, Master mode, 8KHz, */
			0x80, 0x0D, 0x01, 0x08,
#endif
			/* Set Smooth gain rate to 0 db */
			0x80, 0x4E, 0x00, 0x00,
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x0F,
			/* set device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x0F,
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* SetDataPath, 0x00: PCM1, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x20,
			/* SetDataPath, 0x16: LO,   0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0xC0,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xff,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,

	},
	/* [17]: Digital to Analog: I/P: Port B (Ch0,1), O/P: HP (Chan 0,1) */
	{
		.macro = {
			/* Set Smooth gain rate to 0 db */
			0x80, 0x4E, 0x00, 0x00,
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id HP Left Gain */
			0x90, 0x0C, 0x1C, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id HP Right Gain */
			0x90, 0x0C, 0x1C, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* SetDataPath, 0x00: PCM1, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x20,
			/* SetDataPath, 0x00: PCM1, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x21,
			/* SetDataPath, 0x14: HP,  0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0x80,
			/* SetDataPath, 0x14: HP,  0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x52, 0x81,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xff,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,

	},
	/* [18]: Digital to Analog: I/P: Port B (Ch0,1), O/P: EP (Chan 0,1) */
	{
		.macro = {
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id Ear Phone Gain */
			0x90, 0x0C, 0x15, 0x00,
			/* 6 dB */
			0x90, 0x0D, 0x00, 0x0F,
			/* SetDataPath, 0x01: PCM1, 0x00: Ch0, 0x08: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x20,
			/* SetDataPath, 0x01: PCM1, 0x01: Ch1, 0x09: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x21,
			/* SetDataPath, 0x16: EP,  0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4D, 0xE0,
			/* SetDataPath, 0x16: EP,  0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x51, 0xE1,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xFF,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},
	/* [19] Route for two Playback */
	/*  Digital to Analog: I/P: Port A (Ch0,1), O/P: LO (Chan 0,1) */
	/*  Digital to Analog: I/P: Port B (Ch0,1), O/P: HP (Chan 0,1) */
	{
		.macro = {
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id Line out Left Gain */
			0x90, 0x0C, 0x1E, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Line out Right Gain */
			0x90, 0x0C, 0x1E, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id HP Left Gain */
			0x90, 0x0C, 0x1C, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id HP Right Gain */
			0x90, 0x0C, 0x1C, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* SetDataPath, 0x00: PCM0, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x01,
			/* SetDataPath, 0x16: LO,  0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x4E, 0xC0,
			/* SetDataPath, 0x16: LO,  0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x52, 0xC1,
			/* SetDataPath, 0x01: PCM1, 0x00: Ch0, 0x08: AUDIN3 */
			0xB0, 0x5A, 0x20, 0x20,
			/* SetDataPath, 0x01: PCM1, 0x01: Ch1, 0x09: AUDIN4 */
			0xB0, 0x5A, 0x24, 0x21,
			/* SetDataPath, 0x16: HP,  0x00: Ch0, 0x13: AUDOUT3 */
			0xB0, 0x5A, 0x56, 0x80,
			/* SetDataPath, 0x16: HP,  0x01: Ch1, 0x14: AUDOUT4 */
			0xB0, 0x5A, 0x5A, 0x81,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xFF,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},
	/* [20] Route for two Playback */
	/*  Digital to Analog: I/P: Port A (Ch0,1), O/P: HP (Chan 0,1) */
	/*  Digital to Analog: I/P: Port B (Ch0,1), O/P: EP (Chan 0,1) */
	{
		.macro = {
			/* Disable sine wave */
			0x80, 0x1E, 0x00, 0x00,
			/* set device param Id HP Left Gain */
			0x90, 0x0C, 0x1C, 0x00,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id HP Right Gain */
			0x90, 0x0C, 0x1C, 0x03,
			/* -20 dB */
			0x90, 0x0D, 0x00, 0x02,
			/* set device param Id Ear Phone Gain */
			0x90, 0x0C, 0x15, 0x00,
			/* 6 dB */
			0x90, 0x0D, 0x00, 0x0F,
			/* SetDataPath, 0x00: PCM0, 0x00: Ch0, 0x06: AUDIN1 */
			0xB0, 0x5A, 0x18, 0x00,
			/* SetDataPath, 0x00: PCM0, 0x01: Ch1, 0x07: AUDIN2 */
			0xB0, 0x5A, 0x1C, 0x01,
			/* SetDataPath, 0x16: HP,  0x00: Ch0, 0x13: AUDOUT1 */
			0xB0, 0x5A, 0x56, 0x80,
			/* SetDataPath, 0x16: HP,  0x01: Ch1, 0x14: AUDOUT2 */
			0xB0, 0x5A, 0x5A, 0x81,
			/* SetDataPath, 0x01: PCM1, 0x00: Ch0, 0x08: AUDIN3 */
			0xB0, 0x5A, 0x20, 0x20,
			/* SetDataPath, 0x01: PCM1, 0x01: Ch1, 0x09: AUDIN4 */
			0xB0, 0x5A, 0x24, 0x21,
			/* SetDataPath, 0x16: EP,  0x00: Ch0, 0x13: AUDOUT3 */
			0xB0, 0x5A, 0x55, 0xE0,
			/* SetDataPath, 0x16: EP,  0x01: Ch1, 0x14: AUDOUT4 */
			0xB0, 0x5A, 0x59, 0xE1,
			/* SetAlgoType, 0x0004: 4Channel Pass*/
			0xB0, 0x5C, 0x00, 0x04,
			/* Algo rate = 16 kHz  */
			0x90, 0x4c, 0x00, 0x01,
			0xFF,
		},
		.active_out_codec_port = ES515_CODEC_PORT_SPEAKER,
	},
};


static const char * const es515_mic_config_texts[] = {
	"CT 2-mic", "FT 2-mic", "DV 1-mic", "EXT 1-mic", "BT 1-mic",
	"CT ASR 2-mic", "FT ASR 2-mic", "EXT ASR 1-mic", "FT ASR 1-mic",
};
static const struct soc_enum es515_mic_config_enum =
SOC_ENUM_SINGLE(ES515_MIC_CONFIG, 0,
		ARRAY_SIZE(es515_mic_config_texts),
		es515_mic_config_texts);

static const char * const es515_aec_mode_texts[] = {
	"Off", "On", "rsvrd2", "rsvrd3", "rsvrd4", "unknown", "On half-duplex"
};
static const struct soc_enum es515_aec_mode_enum =
SOC_ENUM_SINGLE(ES515_AEC_MODE, 0, ARRAY_SIZE(es515_aec_mode_texts),
		es515_aec_mode_texts);

static const char * const es515_algo_rates_text[] = {
	"fs=8khz", "fs=16khz", "fs=24khz", "fs=48khz", "fs=96khz", "fs=192khz"
};
static const struct soc_enum es515_algo_sample_rate_enum =
SOC_ENUM_SINGLE(ES515_ALGO_SAMPLE_RATE, 0,
		ARRAY_SIZE(es515_algo_rates_text),
		es515_algo_rates_text);

static const struct soc_enum es515_algo_mix_rate_enum =
SOC_ENUM_SINGLE(ES515_MIX_SAMPLE_RATE, 0,
		ARRAY_SIZE(es515_algo_rates_text),
		es515_algo_rates_text);

static const char * const es515_algorithms_text[] = {
	"None", "VP", "Two CHREC", "AUDIO", "Four CHPASS"
};
static const struct soc_enum es515_algorithms_enum =
SOC_ENUM_SINGLE(ES515_ALGORITHM, 0,
		ARRAY_SIZE(es515_algorithms_text),
		es515_algorithms_text);

static const struct soc_enum es515_algorithms_stage_enum =
SOC_ENUM_SINGLE(ES515_ALGORITHM | ES515_STAGED_CMD, 0,
		ARRAY_SIZE(es515_algorithms_text),
		es515_algorithms_text);

#if defined(CONFIG_SND_SOC_ES515_SLIMBUS)
static const char * const es515_ap_tx1_ch_cnt_texts[] = {
	"One", "Two"
};
static const struct soc_enum es515_ap_tx1_ch_cnt_enum =
SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(es515_ap_tx1_ch_cnt_texts),
		es515_ap_tx1_ch_cnt_texts);
#endif

static const char * const es515_off_on_texts[] = {
	"Off", "On"
};

static const struct soc_enum es515_suppress_response_enum =
SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(es515_off_on_texts),
		es515_off_on_texts);

static const struct soc_enum es515_veq_enable_enum =
SOC_ENUM_SINGLE(ES515_VEQ_ENABLE, 0, ARRAY_SIZE(es515_off_on_texts),
		es515_off_on_texts);

static const struct soc_enum es515_dereverb_enable_enum =
SOC_ENUM_SINGLE(ES515_DEREVERB_ENABLE, 0,
		ARRAY_SIZE(es515_off_on_texts),
		es515_off_on_texts);

static const struct soc_enum es515_bwe_enable_enum =
SOC_ENUM_SINGLE(ES515_BWE_ENABLE, 0, ARRAY_SIZE(es515_off_on_texts),
		es515_off_on_texts);

static const struct soc_enum es515_bwe_post_eq_enable_enum =
SOC_ENUM_SINGLE(ES515_BWE_POST_EQ_ENABLE, 0,
		ARRAY_SIZE(es515_off_on_texts),
		es515_off_on_texts);

static const struct soc_enum es515_algo_processing_enable_enum =
SOC_ENUM_SINGLE(ES515_ALGO_PROCESSING, 0,
		ARRAY_SIZE(es515_off_on_texts),
		es515_off_on_texts);

static const struct soc_enum es515_algo_processing_stage_enable_enum =
SOC_ENUM_SINGLE(ES515_ALGO_PROCESSING | ES515_STAGED_CMD, 0,
		ARRAY_SIZE(es515_off_on_texts),
		es515_off_on_texts);

static const struct soc_enum es515_beep_generation_stage_enum =
SOC_ENUM_SINGLE(ES515_BEEP_GENERATION | ES515_STAGED_CMD, 0,
		ARRAY_SIZE(es515_off_on_texts),
		es515_off_on_texts);

static const struct soc_enum es515_beep_generation_enum =
SOC_ENUM_SINGLE(ES515_BEEP_GENERATION, 0,
		ARRAY_SIZE(es515_off_on_texts),
		es515_off_on_texts);

/* Sync Control Modes */
static const char * const es515_sync_control_mode_texts[] = {
	"Polling", "Level Low", "Level High", "Falling Edge", "Rising Edge"
};
static const struct soc_enum es515_sync_control_mode_enum =
SOC_ENUM_SINGLE(ES515_SYNC_CONTROL_MODE, 0,
		ARRAY_SIZE(es515_sync_control_mode_texts),
		es515_sync_control_mode_texts);

static const struct soc_enum es515_sync_control_mode_stage_enum =
SOC_ENUM_SINGLE(ES515_SYNC_CONTROL_MODE | ES515_STAGED_CMD, 0,
		ARRAY_SIZE(es515_sync_control_mode_texts),
		es515_sync_control_mode_texts);

/* Software Reset */
static const char * const es515_soft_reset_texts[] = {
	"Immediate", "Delayed"
};
static const struct soc_enum es515_soft_reset_enum =
SOC_ENUM_SINGLE(ES515_SOFT_RESET, 0,
		ARRAY_SIZE(es515_soft_reset_texts),
		es515_soft_reset_texts);

static const struct soc_enum es515_soft_reset_enum_stage =
SOC_ENUM_SINGLE(ES515_SOFT_RESET | ES515_STAGED_CMD, 0,
		ARRAY_SIZE(es515_soft_reset_texts),
		es515_soft_reset_texts);

/* MP3 Mode */
static const char * const es515_mp3_mode_texts[] = {
	"Inactive", "EP", "HP", "HF", "LO"
};
static const struct soc_enum es515_mp3_mode_enum =
SOC_ENUM_SINGLE(ES515_MP3_MODE, 0,
		ARRAY_SIZE(es515_mp3_mode_texts),
		es515_mp3_mode_texts);

/* Route Change Status */
static const char * const es515_route_change_texts[] = {
	"Active", "Muting", "Switching", "Unmuting", "Inactive"
};
static const struct soc_enum es515_route_change_status_enum =
SOC_ENUM_SINGLE(ES515_CHANGE_STATUS, 0,
		ARRAY_SIZE(es515_route_change_texts),
		es515_route_change_texts);

static const struct soc_enum es515_accessory_det_cfg_enum =
SOC_ENUM_SINGLE(ES515_ACCESSORY_DET_CONFIG, 0,
		ARRAY_SIZE(es515_off_on_texts),
		es515_off_on_texts);

static const struct soc_enum es515_accessory_det_cfg_stage_enum =
SOC_ENUM_SINGLE(ES515_ACCESSORY_DET_CONFIG | ES515_STAGED_CMD, 0,
		ARRAY_SIZE(es515_off_on_texts),
		es515_off_on_texts);

/* Accessory Detect Status */
static const char * const es515_accessory_det_status_texts[] = {
	"Unknown", "American Headset with MIC",
	"American Headphone (LRG)", "OMTP Headset with MIC"
};
static const struct soc_enum es515_accessory_det_status_enum =
SOC_ENUM_SINGLE(ES515_ACCESSORY_DET_STATUS, 0,
		ARRAY_SIZE(es515_accessory_det_status_texts),
		es515_accessory_det_status_texts);

/* Output Known Signal */
static const char * const es515_output_known_sig_texts[] = {
	"No Signal", "1kHz Sinusoid, 0 dBFS peak level"
};
static const struct soc_enum es515_output_known_sig_enum =
SOC_ENUM_SINGLE(ES515_OUTPUT_KNOWN_SIG, 0,
		ARRAY_SIZE(es515_output_known_sig_texts),
		es515_output_known_sig_texts);

static const struct soc_enum es515_output_known_sig_stage_enum =
SOC_ENUM_SINGLE(ES515_OUTPUT_KNOWN_SIG | ES515_STAGED_CMD, 0,
		ARRAY_SIZE(es515_output_known_sig_texts),
		es515_output_known_sig_texts);

static const char * const es515_switch_codec_texts[] = {
	"Earpiece", "Headphone", "Speaker", "Line Out"
};
static const struct soc_enum es515_switch_codec_enum =
SOC_ENUM_SINGLE(ES515_SWITCH_CODEC, 0,
		ARRAY_SIZE(es515_switch_codec_texts),
		es515_switch_codec_texts);
static const struct soc_enum es515_copy_codec_enum =
SOC_ENUM_SINGLE(ES515_COPY_CODEC, 0,
		ARRAY_SIZE(es515_switch_codec_texts),
		es515_switch_codec_texts);
static const struct soc_enum es515_delete_codec_enum =
SOC_ENUM_SINGLE(ES515_DELETE_CODEC, 0,
		ARRAY_SIZE(es515_switch_codec_texts),
		es515_switch_codec_texts);



static const char * const es515_power_state_texts[] = {
	"Sleep", "MP Sleep", "Media Playback", "Active"
};
static const struct soc_enum es515_power_state_enum =
SOC_ENUM_SINGLE(ES515_POWER_STATE, 0,
		ARRAY_SIZE(es515_power_state_texts),
		es515_power_state_texts);


static struct snd_kcontrol_new es515_digital_ext_snd_controls[] = {
	/* Enable/Disable Suppress Response */
	SOC_ENUM_EXT("Suppress Response", es515_suppress_response_enum,
			es515_get_suppress_response_enum,
			es515_put_suppress_response_enum),

	/* Feature API Controls */
#if defined(CONFIG_SND_SOC_ES515_SLIMBUS)
	/* SLIMbus Specific controls */
	SOC_SINGLE_EXT("ES515 RX1 Enable", SND_SOC_NOPM, 0, 1, 0,
			es515_get_rx1_route_enable_value,
			es515_put_rx1_route_enable_value),
	SOC_SINGLE_EXT("ES515 TX1 Enable", SND_SOC_NOPM, 0, 1, 0,
			es515_get_tx1_route_enable_value,
			es515_put_tx1_route_enable_value),
	SOC_SINGLE_EXT("ES515 RX2 Enable", SND_SOC_NOPM, 0, 1, 0,
			es515_get_rx2_route_enable_value,
			es515_put_rx2_route_enable_value),
#endif
	SOC_ENUM_EXT("Mic Config", es515_mic_config_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_ENUM_EXT("AEC Mode", es515_aec_mode_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_ENUM_EXT("VEQ Enable", es515_veq_enable_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_ENUM_EXT("Dereverb Enable", es515_dereverb_enable_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_SINGLE_EXT("Dereverb Gain", ES515_DEREVERB_GAIN, 0, 12, 0,
			es515_get_dereverb_gain_value,
			es515_put_dereverb_gain_value),
	SOC_ENUM_EXT("BWE Enable", es515_bwe_enable_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_SINGLE_EXT("BWE High Band Gain", ES515_BWE_HIGH_BAND_GAIN, 0, 30, 0,
			es515_get_bwe_high_band_gain_value,
			es515_put_bwe_high_band_gain_value),
	SOC_SINGLE_EXT("BWE Max SNR", ES515_BWE_MAX_SNR, 0, 70, 0,
			es515_get_bwe_max_snr_value,
			es515_put_bwe_max_snr_value),
	SOC_ENUM_EXT("BWE Post EQ Enable", es515_bwe_post_eq_enable_enum,
			es515_get_control_enum,
			es515_put_control_enum),

#if defined(CONFIG_SND_SOC_ES515_SLIMBUS)
	/* SLIMbus Specific controls */
	SOC_SINGLE_EXT("SLIMbus Link Multi Channel",
			ES515_SLIMBUS_LINK_MULTI_CHANNEL, 0, 65535, 0,
			es515_get_control_value,
			es515_put_control_value),
#endif

	SOC_ENUM_EXT("Delete Codec Output Port", es515_delete_codec_enum,
			es515_opt_not_available,
			es515_set_delete_codec_enum),
	SOC_ENUM_EXT("Switch Codec Output Port", es515_switch_codec_enum,
			es515_opt_not_available,
			es515_set_switch_codec_enum),
	SOC_ENUM_EXT("Copy Codec Output Port", es515_copy_codec_enum,
			es515_opt_not_available,
			es515_set_copy_codec_enum),
	SOC_ENUM_EXT("Set Power State", es515_power_state_enum,
			es515_get_power_state_enum,
			es515_put_power_state_enum),
	SOC_ENUM_EXT("Algorithm Processing", es515_algo_processing_enable_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_ENUM_EXT("Algorithm Sample Rate", es515_algo_sample_rate_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_ENUM_EXT("Algorithm", es515_algorithms_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_ENUM_EXT("Mix Sample Rate", es515_algo_mix_rate_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_SINGLE_EXT("Internal Route Config",
			SND_SOC_NOPM, 0, ES515_INTERNAL_ROUTE_MAX-1, 0,
			es515_get_internal_route_config,
			es515_put_internal_route_config),

#if defined(CONFIG_SND_SOC_ES515_SLIMBUS)
	/* SLIMbus Specific controls */
	SOC_ENUM_EXT("ES515-AP Tx Channels", es515_ap_tx1_ch_cnt_enum,
			es515_ap_get_tx1_ch_cnt,
			es515_ap_put_tx1_ch_cnt),
#endif

	/* Set Preset Command */
	SOC_SINGLE_EXT("Preset Mode", ES515_PRESET, 0, ES515_PRESET_MODE_MAX, 0,
			es515_opt_not_available,
			es515_put_control_value),

	/* Audio Path Commands */
	SOC_ENUM_EXT("Beep Generation", es515_beep_generation_enum,
			es515_get_control_enum,
			es515_put_control_enum),

	/* System API Commands */
	SOC_ENUM_EXT("Sync Control Command", es515_sync_control_mode_enum,
			es515_get_sync_mode_enum,
			es515_put_sync_mode_enum),
	SOC_ENUM_EXT("Software Reset", es515_soft_reset_enum,
			es515_opt_not_available,
			es515_put_control_enum),
	SOC_SINGLE_EXT("Bootload Initiate", ES515_BOOTLOAD_INIT, 0, 0, 0,
			es515_opt_not_available,
			es515_put_control_value),
	SOC_ENUM_EXT("Get Route Change Status", es515_route_change_status_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_SINGLE_EXT("Set Digital Passthrough",
			ES515_DIGITAL_PASS_THROUGH, 0, 65535, 0,
			es515_opt_not_available,
			es515_put_passthrough),
	SOC_SINGLE_EXT("Set Analog Passthrough ",
			ES515_ANALOG_PASS_THROUGH, 0, 65535, 0,
			es515_opt_not_available,
			es515_put_passthrough),
	SOC_ENUM_EXT("Set MP3 Mode", es515_mp3_mode_enum,
			es515_opt_not_available,
			es515_put_mp3_mode_enum),
	SOC_ENUM_EXT("Accessory Detect Configuration",
			es515_accessory_det_cfg_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_ENUM_EXT("Accessory Detect Status",
			es515_accessory_det_status_enum,
			es515_get_control_enum,
			es515_put_control_enum),

	/* Diagnostic API Commands */
	SOC_ENUM_EXT("Output Known Signal", es515_output_known_sig_enum,
			es515_get_output_known_sig_enum,
			es515_put_output_known_sig_enum),

	SOC_SINGLE_EXT("Set Digital Gain", ES515_DIGITAL_GAIN, 0, 65535, 0,
			es515_get_setdigital_gain,
			es515_put_setdigital_gain),

	SOC_SINGLE_EXT("Get Digital Gain", ES515_DIGITAL_GAIN, 0, 22, 0,
			es515_get_getdigital_gain,
			es515_put_getdigital_gain),

	/* System API Commands */
	SOC_SINGLE_EXT("Device Parameter ID", ES515_DEVICE_PARAM_ID, 0, 65535, 0,
			es515_get_device_param_id,
			es515_put_device_param_id),
	SOC_SINGLE_EXT("Device Parameter", ES515_DEVICE_PARAM, 0, 65535, 0,
			es515_get_device_param,
			es515_put_device_param),
	SOC_SINGLE_EXT("Set Data Path", ES515_DATA_PATH, 0, 65535, 0,
			es515_get_setdata_path,
			es515_put_setdata_path),
	SOC_SINGLE_EXT("Get Data Path", ES515_DATA_PATH, 0, 65535, 0,
			es515_get_getdata_path,
			es515_put_getdata_path),
	SOC_SINGLE_EXT("Algorithm Parameter ID", ES515_ALGORITHM_PARAM_ID,
			0, 65535, 0,
			es515_get_algorithm_param_id,
			es515_put_algorithm_param_id),
	SOC_SINGLE_EXT("Algorithm Parameter", ES515_ALGORITHM_PARAM, 0, 65535, 0,
			es515_get_algorithm_param,
			es515_put_algorithm_param),

	/* Internal API Information */
	SOC_SINGLE_EXT("Set Codec Address", ES515_SET_CODEC_ADDR, 0, 255, 0,
			es515_opt_not_available,
			es515_put_set_codec_addr),
	SOC_SINGLE_EXT("Codec Value", ES515_CODEC_VAL, 0, 255, 0,
			es515_get_control_value,
			es515_put_codec_value),
};


static struct snd_kcontrol_new es515_digital_snd_controls[] = {
	/* Audio Path Commands */
	SOC_SINGLE("Get System Interrupt Status",
			ES515_GET_SYS_INTERRUPT_STATUS, 0, 16383, 0),
	SOC_SINGLE("System Interrupt Mask",
			ES515_SYS_INTERRUPT_MASK, 0, 16383, 0),
	SOC_SINGLE("Clear System Interrupt Status",
			ES515_CLEAR_SYS_INTERRUPT_STATUS, 0, 16383, 0),
};

/* Staged Controls */
static struct snd_kcontrol_new es515_digital_ext_snd_staged_controls[] = {
	/* Audio Path Commands */
	SOC_ENUM_EXT("Algorithm Processing (S)",
			es515_algo_processing_stage_enable_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_ENUM_EXT("Algorithm (S)",
			es515_algorithms_stage_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_ENUM_EXT("Beep Generation (S)", es515_beep_generation_stage_enum,
			es515_get_control_enum,
			es515_put_control_enum),

	/* System API Commands */
	SOC_ENUM_EXT("Sync Control Command (S)",
			es515_sync_control_mode_stage_enum,
			es515_opt_not_available,
			es515_put_control_enum),
	SOC_ENUM_EXT("Software Reset (S)", es515_soft_reset_enum_stage,
			es515_opt_not_available,
			es515_put_control_enum),
	SOC_SINGLE_EXT("Bootload Initiate (S)",
			ES515_BOOTLOAD_INIT | ES515_STAGED_CMD, 0, 0, 0,
			es515_opt_not_available,
			es515_put_control_value),
	SOC_ENUM_EXT("Accessory Detect Configuration (S)",
			es515_accessory_det_cfg_stage_enum,
			es515_get_control_enum,
			es515_put_control_enum),
	SOC_ENUM_EXT("Output Known Signal (S)",
			es515_output_known_sig_stage_enum,
			es515_get_output_known_sig_enum,
			es515_put_output_known_sig_enum),

	/* System API Commands */
	SOC_SINGLE_EXT("Device Parameter ID (S)",
			ES515_DEVICE_PARAM_ID | ES515_STAGED_CMD, 0, 65535, 0,
			es515_get_device_param_id,
			es515_put_device_param_id),
	SOC_SINGLE_EXT("Device Parameter (S)",
			ES515_DEVICE_PARAM | ES515_STAGED_CMD, 0, 65535, 0,
			es515_get_device_param,
			es515_put_device_param),
	SOC_SINGLE_EXT("Set Digital Gain (S)",
			ES515_DIGITAL_GAIN | ES515_STAGED_CMD, 0, 65535, 0,
			es515_get_setdigital_gain,
			es515_put_setdigital_gain),

	/* Audio Path Commands */
	SOC_SINGLE_EXT("Set Data Path (S)",
			ES515_DATA_PATH | ES515_STAGED_CMD, 0, 65535, 0,
			es515_get_setdata_path,
			es515_put_setdata_path),
	SOC_SINGLE_EXT("Algorithm Parameter ID (S)",
			ES515_ALGORITHM_PARAM_ID | ES515_STAGED_CMD,
			0, 65535, 0,
			es515_get_algorithm_param_id,
			es515_put_algorithm_param_id),
	SOC_SINGLE_EXT("Algorithm Parameter (S)",
			ES515_ALGORITHM_PARAM | ES515_STAGED_CMD,
			0, 65535, 0,
			es515_get_algorithm_param,
			es515_put_algorithm_param),

};

/*******************************************************************************
 * Function Definitions
 ******************************************************************************/

static int es515_build_algo_read_msg(char *msg, int *msg_len,
		unsigned int reg)
{
	unsigned int index = reg & ES515_ADDR_MASK;
	unsigned int paramid;

	if (index >= ARRAY_SIZE(es515_algo_paramid))
		return -EINVAL;

	paramid = es515_algo_paramid[index];

	/* ES515_GET_ALGO_PARAM */
	*msg++ = (ES515_GET_ALGO_PARAM >> 8) & 0x00ff;
	*msg++ = ES515_GET_ALGO_PARAM & 0x00ff;

	/* PARAM ID */
	*msg++ = (paramid >> 8) & 0x00ff;
	*msg++ = paramid & 0x00ff;
	*msg_len = 4;

	return 0;
}

static int es515_build_algo_write_msg(char *msg, int *msg_len,
		unsigned int reg, unsigned int value)
{
	unsigned int index = reg & ES515_ADDR_MASK;
	unsigned int cmd;
	unsigned int paramid;

	if (index >= ARRAY_SIZE(es515_algo_paramid))
		return -EINVAL;

	paramid = es515_algo_paramid[index];

	/* ES515_SET_ALGO_PARAMID */
	cmd = ES515_SET_ALGO_PARAMID;
	if (reg & ES515_STAGED_CMD)
		cmd |= ES515_STAGED_MSG_BIT;
	*msg++ = (cmd >> 8) & 0x00ff;
	*msg++ = cmd & 0x00ff;

	/* PARAM ID */
	*msg++ = (paramid >> 8) & 0x00ff;
	*msg++ = paramid & 0x00ff;

	/* ES515_SET_ALGO_PARAM */
	cmd = ES515_SET_ALGO_PARAM;
	if (reg & ES515_STAGED_CMD)
		cmd |= ES515_STAGED_MSG_BIT;
	*msg++ = (cmd >> 8) & 0x00ff;
	*msg++ = cmd & 0x00ff;

	/* value */
	*msg++ = (value >> 8) & 0x00ff;
	*msg++ = value & 0x00ff;
	*msg_len = 8;

	return 0;
}

static int es515_build_dev_read_msg(char *msg, int *msg_len,
		unsigned int reg)
{
	unsigned int index = reg & ES515_ADDR_MASK;
	unsigned int paramid;

	if (index >= ARRAY_SIZE(es515_dev_paramid))
		return -EINVAL;

	paramid = es515_dev_paramid[index];

	/* ES515_GET_DEV_PARAM */
	*msg++ = (ES515_GET_DEV_PARAM >> 8) & 0x00ff;
	*msg++ = ES515_GET_DEV_PARAM & 0x00ff;

	/* PARAM ID */
	*msg++ = (paramid >> 8) & 0x00ff;
	*msg++ = paramid & 0x00ff;
	*msg_len = 4;

	return 0;
}

static int es515_build_dev_write_msg(char *msg, int *msg_len,
		unsigned int reg, unsigned int value)
{
	unsigned int index = reg & ES515_ADDR_MASK;
	unsigned int cmd;
	unsigned int paramid;

	if (index >= ARRAY_SIZE(es515_dev_paramid))
		return -EINVAL;

	paramid = es515_dev_paramid[index];

	/* ES515_SET_DEV_PARAMID */
	cmd = ES515_SET_DEV_PARAMID;
	if (reg & ES515_STAGED_CMD)
		cmd |= ES515_STAGED_MSG_BIT;
	*msg++ = (cmd >> 8) & 0x00ff;
	*msg++ = cmd & 0x00ff;

	/* PARAM ID */
	*msg++ = (paramid >> 8) & 0x00ff;
	*msg++ = paramid & 0x00ff;

	/* ES515_SET_DEV_PARAM */
	cmd = ES515_SET_DEV_PARAM;
	if (reg & ES515_STAGED_CMD)
		cmd |= ES515_STAGED_MSG_BIT;
	*msg++ = (cmd >> 8) & 0x00ff;
	*msg++ = cmd & 0x00ff;

	/* value */
	*msg++ = (value >> 8) & 0x00ff;
	*msg++ = value & 0x00ff;
	*msg_len = 8;

	return 0;
}

static int es515_build_cmd_read_msg(char *msg, int *msg_len,
		unsigned int reg, unsigned int *max)
{
	unsigned int index = reg & ES515_ADDR_MASK;
	struct es515_cmd_access *cmd_access;

	if (index >= ARRAY_SIZE(es515_cmd_access))
		return -EINVAL;
	cmd_access = es515_cmd_access + index;

	*msg_len = cmd_access->read_msg_len;
	memcpy(msg, &cmd_access->read_msg, *msg_len);

	if (max != NULL)
		*max = cmd_access->val_max;

	return 0;
}

static int es515_build_cmd_write_msg(char *msg, int *msg_len,
		unsigned int reg, unsigned int value)
{
	unsigned int index = reg & ES515_ADDR_MASK;
	struct es515_cmd_access *cmd_access;

	if (index >= ARRAY_SIZE(es515_cmd_access))
		return -EINVAL;
	cmd_access = es515_cmd_access + index;

	if (value > cmd_access->val_max) {
		pr_err("[%s]:Value %d is out of range\n", __func__, value);
		return -EINVAL;
	}

	*msg_len = cmd_access->write_msg_len;
	memcpy(msg, &cmd_access->write_msg, *msg_len);
	if (reg & ES515_STAGED_CMD)
		*msg |= (1 << 5);

	/* value */
	msg[2] = (value >> 8) & 0xff;
	msg[3] = value & 0x00ff;

	return 0;
}
/* TODO: Optimzation - use write_and_read_response wherever possible */
static unsigned int es515_write_and_read_response(char *buf,
		int buf_len)
{
	struct es515_priv *es515 = &es515_priv_glb;
	char ack_msg[16];
	unsigned int value;
	int rc;

	rc = ES515_BUS_WRITE(es515, buf, buf_len, 1);
	if (rc < 0) {
		pr_err("%s(): es515_xxxx_write() failure", __func__);
		return rc;
	}
	msleep(20);

	/* Read Acknowledge */
	rc = ES515_BUS_READ(es515, ack_msg, 4, 1);
	if (rc < 0) {
		pr_err("%s(): es515_xxxx_read() failure", __func__);
		return rc;
	}
	value = ack_msg[2] << 8 | ack_msg[3];

	return value;
}

/* FIXME: Check for error response from ES515 (0xFFFF) * */
static unsigned int es515_read(struct snd_soc_codec *codec,
		unsigned int reg)
{
	struct es515_priv *es515;
	unsigned int access = reg & ES515_ACCESS_MASK;
	char req_msg[16] = {0};
	char ack_msg[16] = {0};
	unsigned int msg_len;
	unsigned int value;
	unsigned int max = 0xFFFF;
	int rc;

	if (codec)
		es515 = snd_soc_codec_get_drvdata(codec);
	else
		es515 = &es515_priv_glb;


	switch (access) {
	case ES515_ALGO_ACCESS:
		rc = es515_build_algo_read_msg(req_msg, &msg_len, reg);
		break;
	case ES515_DEV_ACCESS:
		rc = es515_build_dev_read_msg(req_msg, &msg_len, reg);
		break;
	case ES515_CMD_ACCESS:
		rc = es515_build_cmd_read_msg(req_msg, &msg_len,
					      reg, &max);
		break;
	case ES515_OTHER_ACCESS:
		return 0;
	default:
		rc = -EINVAL;
		break;
	}
	if (rc) {
		pr_err("%s(): failed to build read message for address"
				"= 0x%04x\n", __func__, reg);
		return rc;
	}

	if (es515_power_state != ES515_POWER_STATE_NORMAL) {
		/* Bring the es515 to NORMAL power state */
		rc = es515_wakeup(es515);
		if (rc < 0) {
			pr_err("%s(): es515 wakeup failed, rc = %d",
					__func__, rc);
		}
	}

	rc = ES515_BUS_WRITE(es515, req_msg, msg_len, 1);
	if (rc < 0) {
		pr_err("%s(): es515_xxxx_write()", __func__);
		return rc;
	}
	msleep(20);
	rc = ES515_BUS_READ(es515, ack_msg, 4, 1);
	if (rc < 0) {
		pr_err("%s(): es515_xxxx_read()", __func__);
		return rc;
	}
	value = ack_msg[2] << 8 | ack_msg[3];

	/* Mask ack value to Max value */
	value &= max;

	return value;
}

static int es515_write(struct snd_soc_codec *codec, unsigned int reg,
		unsigned int value)
{
	struct es515_priv *es515;
	unsigned int access = reg & ES515_ACCESS_MASK;
	char msg[16];
	char *msg_ptr;
	int msg_len = 0;
	int sr_bit = 0;
	int i;
	int rc;

	if (codec)
		es515 = snd_soc_codec_get_drvdata(codec);
	else
		es515 = &es515_priv_glb;


	switch (access) {
	case ES515_ALGO_ACCESS:
		rc = es515_build_algo_write_msg(msg, &msg_len,
						reg, value);
		break;
	case ES515_DEV_ACCESS:
		rc = es515_build_dev_write_msg(msg, &msg_len,
					       reg, value);
		break;
	case ES515_CMD_ACCESS:
		rc = es515_build_cmd_write_msg(msg, &msg_len,
					       reg, value);
		break;
	case ES515_OTHER_ACCESS:
		return 0;
	default:
		rc = -EINVAL;
		break;
	}
	if (rc) {
		pr_err("%s(): failed to build write message for address"
				"= 0x%04x\n", __func__, reg);
		return rc;
	}

	if (es515_power_state != ES515_POWER_STATE_NORMAL) {
		/* Bring the es515 to NORMAL power state */
		rc = es515_wakeup(es515);
		if (rc < 0) {
			pr_err("%s(): es515 wakeup failed, rc = %d",
					__func__, rc);
		}
	}

	msg_ptr = msg;
	for (i = msg_len; i > 0; i -= 4) {
		/* Add status of Suppress Response bit */
		msg_ptr[0] |= (ES515_SR_BIT << 4);

		rc = ES515_BUS_WRITE(es515, msg_ptr, 4, 1);
		if (rc < 0) {
			pr_err("%s(): es515_xxxx_write()", __func__);
			return rc;
		}
		/* Retrieve SR bit */
		sr_bit = msg_ptr[0] & (ES515_SR_BIT_MASK);
		if (!sr_bit) {
			msleep(20);
			rc = ES515_BUS_READ(es515, msg_ptr, 4, 1);
			if (rc < 0) {
				pr_err("%s(): es515_xxxx_read()", __func__);
				return rc;
			}
		}
		msg_ptr += 4;
	}

	return rc;
}

#ifdef ES515_DEBUG
static void es515_fixed_config(struct es515_priv *es515)
{
	/* Set the 0th route by default */
//	es515_switch_route(0);
	es515_switch_route(3);
}
#endif

static int es515_opt_not_available(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//pr_info("\n[%s]: Operation not available\n", __func__);
	return 0; /* No Such Process */
}

static int es515_get_control_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = es515_priv.codec; */
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es515_read(NULL, reg);
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int es515_put_control_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = es515_priv.codec; */
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];
	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_get_control_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

	value = es515_read(NULL, reg);
	ucontrol->value.enumerated.item[0] = value;

	return 0;
}

static int es515_put_control_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.enumerated.item[0];

	if (value >= e->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_get_suppress_response_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = ES515_SR_BIT;

	return 0;
}

static int es515_put_suppress_response_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.enumerated.item[0];
	if (value >= e->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}
	ES515_SR_BIT = value;

	return rc;
}

#if defined(CONFIG_SND_SOC_ES515_SLIMBUS)
static int es515_get_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es515_rx1_route_enable;
	pr_debug("%s(): es515_rx1_route_enable = %d\n", __func__,
			es515_rx1_route_enable);

	return 0;
}

static int es515_put_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es515_rx1_route_enable = ucontrol->value.integer.value[0];
	pr_debug("%s(): es515_rx1_route_enable = %d\n", __func__,
			es515_rx1_route_enable);

	return 0;
}

static int es515_get_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es515_tx1_route_enable;
	pr_debug("%s(): es515_tx1_route_enable = %d\n", __func__,
			es515_tx1_route_enable);

	return 0;
}

static int es515_put_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es515_tx1_route_enable = ucontrol->value.integer.value[0];
	pr_debug("%s(): es515_tx1_route_enable = %d\n", __func__,
			es515_tx1_route_enable);

	return 0;
}


static int es515_get_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es515_rx2_route_enable;
	pr_debug("%s(): es515_rx2_route_enable = %d\n", __func__,
			es515_rx2_route_enable);

	return 0;
}

static int es515_put_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es515_rx2_route_enable = ucontrol->value.integer.value[0];
	pr_debug("%s(): es515_rx2_route_enable = %d\n", __func__,
			es515_rx2_route_enable);

	return 0;
}
#endif

static void es515_switch_route(unsigned int route_index)
{
	struct es515_priv *es515 = &es515_priv_glb;
	u8 msg[4];
	u8 *msg_ptr;
	char ack_msg[4] = {0};
	int rc;
	int sr_bit = 0;

	if (route_index >= ES515_INTERNAL_ROUTE_MAX) {
		pr_err("%s(): new es515_internal_route = %u is "
				"out of range\n", __func__, route_index);
		return;
	}


	pr_info("%s():switch current es515_internal_route = %u to new route"
			"= %u\n", __func__, es515_internal_route_num, route_index);

	pr_err("Switching to route: %d\n", route_index);

	es515_internal_route_num = route_index;
	es515_current_out_codec_port =
		es515_def_route_configs[es515_internal_route_num].active_out_codec_port;
	msg_ptr = es515_def_route_configs[es515_internal_route_num].macro;
	while (*msg_ptr != 0xff) {
		memcpy(msg, msg_ptr, 4);
		rc = ES515_BUS_WRITE(es515, msg, 4, 1);
		if (rc < 0) {
			pr_err("%s(): es515_xxxx_write()", __func__);
			return;
		}
		msg_ptr += 4;
		usleep_range(5000, 5000);
		/* Retrieve SR bit */
		sr_bit = msg[0] & (ES515_SR_BIT_MASK);
		if (!sr_bit) {
			msleep(20);
			rc = ES515_BUS_READ(es515, ack_msg, 4, 1);
			if (rc < 0) {
				pr_err("%s(): es515_xxxx_read()", __func__);
				return;
			}
		}

	}
}

static int es515_put_passthrough(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	if (value >= mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	/* Set given value to Passthrough mode */
	rc = es515_write(NULL, reg, value);

	/* Set power state to Sleep */
	rc = es515_write(NULL, ES515_POWER_STATE, 1);


	return rc;
}

static int es515_put_mp3_mode_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;

	value =	ucontrol->value.enumerated.item[0];

	if (value >= e->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	/* Set a route from Port A to Port D */
	rc = es515_write(NULL, ES515_DIGITAL_PASS_THROUGH, 0x01CC);

	/* Set given value to MP3 mode */
	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_get_output_known_sig_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es515_current_output_known_sig;
	return 0;
}

static int es515_put_output_known_sig_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;

	value =	ucontrol->value.enumerated.item[0];

	if (value >= e->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	es515_current_output_known_sig = value;
	/* Set 1kHz Sinusoid, 0 dBFS peak level */
	if (value) {
		value = 0x0005;
	}

	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_get_device_param_id(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es515_current_device_param_id;
	return 0;
}

static int es515_put_device_param_id(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	es515_current_device_param_id = value;

	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_get_device_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	char msg[16];
	int msg_len;
	int rc;

	value = es515_current_device_param_id;

	/* Build Command write message */
	rc = es515_build_cmd_read_msg(msg, &msg_len, reg, NULL);
	if (rc) {
		pr_err("%s(): failed to build write message for address"
				"= 0x%04x\n", __func__, reg);
		return rc;
	}

	msg[2] = (value >> 8) & 0xff;
	msg[3] = value & 0x00ff;

	value = es515_write_and_read_response(msg, msg_len);
	ucontrol->value.integer.value[0] = value;

	return 0;
}


static int es515_put_device_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_get_algorithm_param_id(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es515_current_algorithm_param_id;
	return 0;
}

static int es515_put_algorithm_param_id(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	es515_current_algorithm_param_id = value;

	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_get_algorithm_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	char msg[16];
	int msg_len;
	int rc;

	value = es515_current_algorithm_param_id;

	/* Build Command write message */
	rc = es515_build_cmd_read_msg(msg, &msg_len, reg, NULL);
	if (rc) {
		pr_err("%s(): failed to build write message for address"
				"= 0x%04x\n", __func__, reg);
		return rc;
	}

	msg[2] = (value >> 8) & 0xff;
	msg[3] = value & 0x00ff;

	value = es515_write_and_read_response(msg, msg_len);
	ucontrol->value.integer.value[0] = value;

	return 0;
}


static int es515_put_algorithm_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_put_set_codec_addr(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];
	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	/* Add 0xB0 as byte 1[15:8] */
	printk("\n[%s:] value0 = %d", __func__, value);
	value |= (0xB000);
	printk("\n[%s:] value1 = %d\n", __func__, value);
	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_put_codec_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];
	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	/* Add 0xB0 as byte 1[15:8] */
	value |= (0xB000);
	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_index_to_gain(int min, int step, int index)

{
	return	min + (step * index);
}

static int es515_gain_to_index(int min, int step, int gain)
{
	return	((gain - min) & 0xFFFF) / step;
}

static int es515_get_setdigital_gain(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	unsigned int value;

	value = es515_current_path_gain_value;

	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int es515_put_setdigital_gain(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_get_getdigital_gain(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	char msg[16];
	int msg_len;
	int rc;

	value = es515_current_path_type;

	/* Build Command write message */
	rc = es515_build_cmd_read_msg(msg, &msg_len, reg, NULL);

	if (rc) {
		pr_err("%s(): failed to build write message for address"
				"= 0x%04x\n", __func__, reg);
		return rc;
	}

	msg[2] = (value >> 8) & 0xff;
	msg[3] = value & 0x00ff;

	value = es515_write_and_read_response(msg, msg_len);
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int es515_put_getdigital_gain(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int value;

	value = ucontrol->value.integer.value[0];

	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	es515_current_path_type = value;

	return 0;
}
static int es515_get_setdata_path(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	unsigned int value;

	value = es515_current_data_path_value;

	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int es515_put_setdata_path(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	es515_current_data_path_value = value;

	rc = es515_write(NULL, reg, value);

	return rc;
}

static int es515_get_getdata_path(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	char msg[16];
	int msg_len;
	int rc;

	value = es515_requested_path_type;

	/* Build Command write message */
	rc = es515_build_cmd_read_msg(msg, &msg_len, reg, NULL);

	if (rc) {
		pr_err("%s(): failed to build write message for address"
				"= 0x%04x\n", __func__, reg);
		return rc;
	}

	msg[2] = (value >> 8) & 0xff;
	msg[3] = value & 0x00ff;

	value = es515_write_and_read_response(msg, msg_len);
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int es515_put_getdata_path(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int value;

	value = ucontrol->value.integer.value[0];

	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	es515_requested_path_type = value;

	return 0;
}

static int es515_get_internal_route_config(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es515_internal_route_num;

	return 0;
}

static int es515_get_sync_mode_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es515_current_sync_mode;

	return 0;
}
static int es515_put_sync_mode_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;

	int rc = 0;
	unsigned int value;
	unsigned long irq_flag = IRQF_DISABLED;

	value = ucontrol->value.enumerated.item[0];

	if (value >= e->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	if (es515_priv_glb.intr_type) {
		/* Free IRQ */
		free_irq(gpio_to_irq(es515_priv_glb.pdata->int_gpio), NULL);
	}

	/* FIXME: Level Triggered IRQs are not
	 * handled properly as of now. If interrupt type
	 * is set as Level Triggered, interrupts keep
	 * on coming and hangs the system. There is no way
	 * from host to alter GPIO level on ES515 to disable
	 * the interrupts, hence not supported. */
	switch (value) {
	case 1:
		irq_flag |= IRQF_TRIGGER_LOW;
		break;
	case 2:
		irq_flag |= IRQF_TRIGGER_HIGH;
		break;
	case 3:
		irq_flag |= IRQF_TRIGGER_FALLING;
		break;
	case 4:
		irq_flag |= IRQF_TRIGGER_RISING;
		break;
	}

	if (value) {
		rc = request_threaded_irq(
				gpio_to_irq(es515_priv_glb.pdata->int_gpio),
				NULL, es515_threaded_isr, irq_flag,
				"es515_theaded_isr", NULL);
		if (rc < 0) {
			pr_err("Error in setting interrupt mode :%d int_gpio:%d\n", rc, es515_priv_glb.pdata->int_gpio);
			return rc;
		}
		pr_debug("Interrupt Mode set to %d pin:%d\n", value, es515_priv_glb.pdata->int_gpio);
	}

	es515_write(NULL, ES515_SYNC_CONTROL_MODE, value);
	es515_priv_glb.intr_type = value;
	es515_current_sync_mode = value;
	return 0;
}

static int es515_put_internal_route_config(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int value;

	value = ucontrol->value.integer.value[0];

	if (value > mc->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}
	es515_switch_route(value);
	return 0;
}

#if defined(CONFIG_SND_SOC_ES515_SLIMBUS)
static int es515_ap_get_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es515_internal_route_num - 1;

	return 0;
}
static int es515_ap_put_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es515_ap_tx1_ch_cnt = ucontrol->value.enumerated.item[0] + 1;
	return 0;
}
#endif


static int es515_get_dereverb_gain_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es515_read(NULL, reg);
	ucontrol->value.integer.value[0] = es515_gain_to_index(-12, 1, value);

	return 0;
}

static int es515_put_dereverb_gain_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 12) {
		value = es515_index_to_gain(-12, 1,
				ucontrol->value.integer.value[0]);
		rc = es515_write(NULL, reg, value);
	} else {
		pr_err("[%s]:Value out of range\n", __func__);
		rc = -EINVAL;
	}
	return rc;
}


static int es515_get_bwe_high_band_gain_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es515_read(NULL, reg);
	ucontrol->value.integer.value[0] = es515_gain_to_index(-10, 1, value);

	return 0;
}

static int es515_put_bwe_high_band_gain_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 30) {
		value = es515_index_to_gain(-10, 1,
				ucontrol->value.integer.value[0]);
		rc = es515_write(NULL, reg, value);
	} else {
		pr_err("[%s]:Value out of range\n", __func__);
		rc = -EINVAL;
	}

	return rc;
}


static int es515_get_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es515_read(NULL, reg);
	ucontrol->value.integer.value[0] = es515_gain_to_index(-20, 1, value);

	return 0;
}

static int es515_put_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 70) {
		value = es515_index_to_gain(-20, 1,
				ucontrol->value.integer.value[0]);
		rc = es515_write(NULL, reg, value);
	} else {
		pr_err("[%s]:Value out of range\n", __func__);
		rc = -EINVAL;
	}

	return rc;
}

/* State Machine with possible states from NORMAL
 *
 * NORMAL --> SLEEP --> NORMAL
 * NORMAL --> MP_CMD --> NORMAL
 * NORMAL --> MP_SLEEP --> MP_CMD --> NORMAL
 *
 */


static int es515_wakeup(struct es515_priv *es515)
{
	static char sync_ok[] = { 0x80, 0, 0, 0 };
	char msg[16] = {0};
	int rc;

	pr_debug("%s()\n", __func__);

	if (es515_power_state == ES515_POWER_STATE_SLEEP ||
			es515_power_state == ES515_POWER_STATE_MP_SLEEP) {
		gpio_set_value(es515->pdata->wakeup_gpio, 0);
		msleep(30);
		gpio_set_value(es515->pdata->wakeup_gpio, 1);
		msleep(30);
	}

	if (es515_power_state == ES515_POWER_STATE_MP_SLEEP) {
		/* MP_SLEEP --> MP_COMMAND */
		msg[0] = ES515_SET_POWER_STATE >> 8;
		msg[1] = ES515_SET_POWER_STATE & 0xFF;
		msg[2] = ES515_SET_POWER_STATE_MP_CMD >> 8;
		msg[3] = ES515_SET_POWER_STATE_MP_CMD & 0xFF;

		rc = ES515_BUS_WRITE(es515, msg, 4, 1);
		if (rc < 0) {
			pr_err("%s(): failed sync write\n", __func__);
			return rc;
		}
		msleep(30);

		memset(msg, 0, 16);
		rc = ES515_BUS_READ(es515, msg, 4, 1);
		if (rc < 0) {
			pr_err("%s(): error reading sync ack rc =%d\n",
					__func__, rc);
			return rc;
		}
		es515_power_state = ES515_POWER_STATE_MP_COMMAND;
	}

	if (es515_power_state == ES515_POWER_STATE_MP_COMMAND) {
		/* MP_COMMAND --> NORMAL */
		msg[0] = ES515_SET_POWER_STATE >> 8;
		msg[1] = ES515_SET_POWER_STATE & 0xFF;
		msg[2] = ES515_SET_POWER_STATE_NORMAL >> 8;
		msg[3] = ES515_SET_POWER_STATE_NORMAL & 0xFF;

		rc = ES515_BUS_WRITE(es515, msg, 4, 1);
		if (rc < 0) {
			pr_err("%s(): failed sync write\n", __func__);
			return rc;
		}
		msleep(30);

		memset(msg, 0, 16);
		rc = ES515_BUS_READ(es515, msg, 4, 1);
		if (rc < 0) {
			pr_err("%s(): error reading sync ack rc =%d\n",
					__func__, rc);
			return rc;
		}
	}
	es515_power_state = ES515_POWER_STATE_NORMAL;

	memset(msg, 0, 16);
	msg[0] = ES515_SYNC_CMD >> 8;
	msg[1] = ES515_SYNC_CMD & 0x00ff;
	msg[2] = ES515_SYNC_POLLING >> 8;
	msg[3] = ES515_SYNC_POLLING & 0x00ff;
	rc = ES515_BUS_WRITE(es515, msg, 4, 1);
	if (rc < 0) {
		pr_err("%s(): failed sync write\n", __func__);
		return rc;
	}
	msleep(30);
	memset(msg, 0, 16);
	rc = ES515_BUS_READ(es515, msg, 4, 1);
	if (rc < 0) {
		pr_err("%s(): error reading sync ack rc =%d\n",
				__func__, rc);
		return rc;
	}
	if (memcmp(msg, sync_ok, 4) == 0) {
		pr_debug("%s(): sync ack good =0x%02x%02x%02x%02x\n",
				__func__, msg[0], msg[1], msg[2], msg[3]);
	} else {
		pr_err("%s(): sync ack failed =0x%02x%02x%02x%02x\n",
				__func__, msg[0], msg[1], msg[2], msg[3]);
		return -EIO;
	}

	msleep(30);
	es515_fixed_config(es515);

	return rc;
}

#if defined(CONFIG_PM) && defined(CONFIG_SND_SOC_ES515_POWERSAVE)
static int es515_sleep(struct es515_priv *es515)
{
	int rc;
	int value = 1; /* Sleep */
	rc = es515_write(NULL, ES515_POWER_STATE, value);
	msleep(20);
	es515_power_state = ES515_POWER_STATE_SLEEP;

	/* clocks off */

	return rc;
}
#endif

static int es515_get_power_state_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es515_power_state;

	return 0;
}

static int es515_set_copy_codec_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	unsigned int new_out_port = 0;
	int rc = 0;

	if (ucontrol->value.enumerated.item[0] >= e->max) {
		pr_err("Value out of range\n");
		return -EINVAL;
	}
	if (es515_current_out_codec_port == ES515_CODEC_PORT_INVALID) {
		pr_err("Current output port is invalid, cannot copy port!\n");
		return -EINVAL;
	}

	value = ucontrol->value.enumerated.item[0];
	new_out_port |= ((es515_current_out_codec_port << 8) & 0xff00);
	new_out_port |= (value & 0xff);

	rc = es515_write(NULL, reg, new_out_port);
	es515_current_copied_codec_port = ucontrol->value.enumerated.item[0];
	es515_current_copied_ports_nr++;

	return 0;
}

static int es515_set_delete_codec_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	unsigned int del_port = 0;
	int rc = 0;

	if (ucontrol->value.enumerated.item[0] >= e->max) {
		pr_err("Value out of range\n");
		return -EINVAL;
	}

	if (es515_current_copied_ports_nr == 0) {
		pr_err("Only one active port present. It can't be deleted!\n");
		return -EINVAL;
	}


	value = ucontrol->value.enumerated.item[0];
	del_port |= ((es515_current_out_codec_port << 8) & 0xff00);
	del_port |= (value & 0xff);

	rc = es515_write(NULL, reg, del_port);

	/* If the current default port is deleted, update the default
	 * port with the copied port */
	es515_current_out_codec_port =
		(es515_current_out_codec_port != ucontrol->value.enumerated.item[0]) ?
		es515_current_out_codec_port :
		es515_current_copied_codec_port;
	es515_current_copied_ports_nr--;
	pr_info("New default output port = %x\n", es515_current_out_codec_port);
	return 0;
}


static int es515_set_switch_codec_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	unsigned int new_out_port = 0;
	int rc = 0;

	if (ucontrol->value.enumerated.item[0] >= e->max) {
		pr_err("Value out of range\n");
		return -EINVAL;
	}

	if (es515_current_out_codec_port == ES515_CODEC_PORT_INVALID) {
		pr_err("Current output port is invalid, cannot switch port!\n");
		return -EINVAL;
	}

	value = ucontrol->value.enumerated.item[0];
	new_out_port |= ((es515_current_out_codec_port << 8) & 0xff00);
	new_out_port |= (value & 0xff);

	rc = es515_write(NULL, reg, new_out_port);

	/* Update the current output port for codec with new port */
	es515_current_out_codec_port = ucontrol->value.enumerated.item[0];

	return 0;
}


static int es515_put_power_state_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.enumerated.item[0] >= e->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	if (es515_power_state == ucontrol->value.enumerated.item[0]) {
		pr_warn("%s():no power state change\n", __func__);
		return 0;
	}

	value = ucontrol->value.enumerated.item[0];
	rc = es515_write(NULL, reg, value+1); /* Value starts from 1 */
	es515_power_state = ucontrol->value.enumerated.item[0];

	return 0;
}

static int es515_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
	int ret = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		break;
	}
	codec->dapm.bias_level = level;

	return ret;
}

#if defined(CONFIG_SND_SOC_ES515_I2S)
static int es515_i2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	/* struct snd_soc_codec *codec = dai->codec; */
	int ret = 0;

	return ret;
}

static int es515_i2s_set_pll(struct snd_soc_dai *dai, int pll_id,
		int source, unsigned int freq_in,
		unsigned int freq_out)
{
	/* struct snd_soc_codec *codec = dai->codec; */
	int ret = 0;

	return ret;
}

static int es515_i2s_set_clkdiv(struct snd_soc_dai *dai, int div_id,
		int div)
{
	/* struct snd_soc_codec *codec = dai->codec; */
	int ret = 0;

	return ret;
}

static int es515_i2s_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	/* struct snd_soc_codec *codec = dai->codec; */
	int ret = 0;

	return ret;
}

static int es515_i2s_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
		unsigned int rx_mask, int slots,
		int slot_width)
{
	/* struct snd_soc_codec *codec = dai->codec; */
	int ret = 0;

	return ret;
}

static int es515_i2s_set_channel_map(struct snd_soc_dai *dai,
		unsigned int tx_num, unsigned int *tx_slot,
		unsigned int rx_num, unsigned int *rx_slot)
{
	/* struct snd_soc_codec *codec = dai->codec; */
	int ret = 0;

	return ret;
}

static int es515_i2s_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int paramid = 0;
	unsigned int val = 0;

	switch (dai->id) {
	case 0:
		break;
	case 1:
		break;
	default:
		return -EINVAL;
	}

	if (tristate)
		val = 0x0001;
	else
		val = 0x0000;

	return snd_soc_write(codec, paramid, val);
}

static int es515_i2s_port_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int paramid = 0;
	unsigned int val = 0;

	/* Is this valid since DACs are not statically mapped to DAIs? */
	switch (dai->id) {
	case 0:
		break;
	case 1:
		break;
	default:
		return -EINVAL;
	}

	if (mute)
		val = 0x0000;
	else
		val = 0x0001;

	return snd_soc_write(codec, paramid, val);
}

static int es515_i2s_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	/* struct snd_soc_codec *codec = dai->codec; */
	/* int ret; */

	pr_debug("%s(): dai->name = %s, dai->id = %d\n", __func__,
			dai->name, dai->id);

	return 0;
}

static void es515_i2s_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	/* struct snd_soc_codec *codec = dai->codec; */

	pr_debug("%s(): dai->name = %s, dai->id = %d\n", __func__,
			dai->name, dai->id);

	if (!dai->active)
		es515_device_word_length(dai->id, 0);
}

static int es515_device_word_length (int dai_id, int bits_per_sample)
{
	int rc = 0;

	pr_debug("%s(): id = %d\n", __func__, dai_id);

	/* Update, if changed */
	if (bits_per_sample == es515_dai_word_length[dai_id])
		return 0;

	es515_dai_word_length[dai_id] = bits_per_sample;
	if (!bits_per_sample)
		return 0;

	/* Set Device Param ID for wordlength */
	rc = es515_write(NULL, ES515_DEVICE_PARAM_ID, 0x0A00 + (dai_id << 8));
	if (rc < 0) {
		pr_err("%s(): es515_device_word_length failed\n", __func__);
		return rc;
	}

	/* Set word legth as per bits */
	rc = es515_write(NULL, ES515_DEVICE_PARAM, bits_per_sample - 1);
	if (rc < 0) {
		pr_err("%s(): es515_device_word_length failed\n", __func__);
		return rc;
	}

	return rc;
}
static int es515_i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	int bits_per_sample = 0;
	int ret = 0;
	pr_debug("%s(): dai->name = %s, dai->id = %d\n", __func__,
			dai->name, dai->id);
	switch (dai->id) {
	case 0:
		pr_debug("%s(): ES515_PORTA_PARAMID\n", __func__);
		break;
	case 1:
		pr_debug("%s(): ES515_PORTB_PARAMID\n", __func__);
		break;
	default:
		pr_err("%s(): unknown port dai->id:%d\n",
		       __func__, dai->id);
		return -EINVAL;
	}

	pr_debug("%s(): params_channels(params) = %d\n", __func__,
			params_channels(params));
	switch (params_channels(params)) {
	case 1:
		pr_debug("%s(): 1 channel\n", __func__);
		break;
	case 2:
		pr_debug("%s(): 2 channels\n", __func__);
		break;
	case 4:
		pr_debug("%s(): 4 channels\n", __func__);
		break;
	default:
		pr_err("%s(): unsupported number of channels\n",
		       __func__);
		return -EINVAL;
	}

	pr_debug("%s(): params_rate(params) = %d\n", __func__,
			params_rate(params));
	switch (params_rate(params)) {
	case 8000:
		pr_debug("%s(): 8000Hz\n", __func__);
		break;
	case 11025:
		pr_debug("%s(): 11025\n", __func__);
		break;
	case 16000:
		pr_debug("%s(): 16000\n", __func__);
		break;
	case 22050:
		pr_debug("%s(): 22050\n", __func__);
		break;
	case 24000:
		pr_debug("%s(): 24000\n", __func__);
		break;
	case 32000:
		pr_debug("%s(): 32000\n", __func__);
		break;
	case 44100:
		pr_debug("%s(): 44100\n", __func__);
		break;
	case 48000:
		pr_debug("%s(): 48000\n", __func__);
		break;
	case 96000:
		pr_debug("%s(): 96000\n", __func__);
		break;
	case 192000:
		pr_debug("%s(): 96000\n", __func__);
		break;
	default:
		pr_err("%s(): unsupported rate = %d\n", __func__,
		       params_rate(params));
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		pr_debug("%s(): S16_LE\n", __func__);
		bits_per_sample = 16;
		break;
	case SNDRV_PCM_FORMAT_S16_BE:
		pr_debug("%s(): S16_BE\n", __func__);
		bits_per_sample = 16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		pr_debug("%s(): S20_3LE\n", __func__);
		bits_per_sample = 20;
		break;
	case SNDRV_PCM_FORMAT_S20_3BE:
		pr_debug("%s(): S20_3BE\n", __func__);
		bits_per_sample = 20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		pr_debug("%s(): S24_LE\n", __func__);
		bits_per_sample = 24;
		break;
	case SNDRV_PCM_FORMAT_S24_BE:
		pr_debug("%s(): S24_BE\n", __func__);
		bits_per_sample = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		pr_debug("%s(): S32_LE\n", __func__);
		bits_per_sample = 32;
		break;
	case SNDRV_PCM_FORMAT_S32_BE:
		pr_debug("%s(): S32_BE\n", __func__);
		bits_per_sample = 32;
		break;
	default:
		pr_err("%s(): unknown format\n", __func__);
		return -EINVAL;
	}
	if (ret) {
		pr_err("%s(): snd_soc_update_bits() failed\n",
				__func__);
		return ret;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("%s(): PLAYBACK\n", __func__);
	else
		pr_debug("%s(): CAPTURE\n", __func__);

	ret = es515_device_word_length(dai->id, bits_per_sample);

	return ret;
}

static int es515_i2s_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret = 0;

	pr_debug("%s() dai->name = %s, dai->id = %d\n", __func__,
			dai->name, dai->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("%s(): PLAYBACK\n", __func__);
	else
		pr_debug("%s(): CAPTURE\n", __func__);

	return ret;
}

static int es515_i2s_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret = 0;

	pr_debug("%s() dai->name = %s, dai->id = %d\n", __func__,
			dai->name, dai->id);
	return ret;
}

static int es515_i2s_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	int ret = 0;

	pr_debug("%s() dai->name = %s, dai->id = %d\n", __func__,
			dai->name, dai->id);
	return ret;
}
#endif

static struct snd_soc_dai_ops es515_portx_dai_ops = {
	.set_sysclk	= es515_i2s_set_sysclk,
	.set_pll	= es515_i2s_set_pll,
	.set_clkdiv	= es515_i2s_set_clkdiv,
	.set_fmt	= es515_i2s_set_dai_fmt,
	.set_tdm_slot	= es515_i2s_set_tdm_slot,
	.set_channel_map	= es515_i2s_set_channel_map,
	.set_tristate	= es515_i2s_set_tristate,
	.digital_mute	= es515_i2s_port_mute,
	.startup	= es515_i2s_startup,
	.shutdown	= es515_i2s_shutdown,
	.hw_params	= es515_i2s_hw_params,
	.hw_free	= es515_i2s_hw_free,
	.prepare	= es515_i2s_prepare,
	.trigger	= es515_i2s_trigger,
};

#ifdef CONFIG_PM
static int es515_codec_suspend(struct snd_soc_codec *codec)
{
#ifdef CONFIG_SND_SOC_ES515_POWERSAVE
	struct es515_priv *es515 = snd_soc_codec_get_drvdata(codec);

	es515_set_bias_level(codec, SND_SOC_BIAS_OFF);
	es515_sleep(es515);
#else
	es515_set_bias_level(codec, SND_SOC_BIAS_OFF);
#endif
	return 0;
}

static int es515_codec_resume(struct snd_soc_codec *codec)
{
#ifdef CONFIG_SND_SOC_ES515_POWERSAVE
	struct es515_priv *es515 = snd_soc_codec_get_drvdata(codec);

	es515_wakeup(es515);
	es515_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
#else
	es515_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
#endif
	return 0;
}
#else
#define es515_codec_suspend NULL
#define es515_codec_resume NULL
#endif

#ifdef ES515_JACK_DETECTION
int es515_jack_assign(struct snd_soc_codec *codec, struct snd_soc_jack *jack)
{
	struct es515_priv *es515 = snd_soc_codec_get_drvdata(codec);
	es515->jack = jack;
	return 0;
}
EXPORT_SYMBOL_GPL(es515_jack_assign);
#endif

static int es515_codec_probe(struct snd_soc_codec *codec)
{
	struct es515_priv *es515 = snd_soc_codec_get_drvdata(codec);
	int rc;

	pr_debug("%s()\n", __func__);
	es515->codec = codec;

	codec->control_data = snd_soc_codec_get_drvdata(codec);

	es515_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* Register digital_ext_snd controls */
	rc = snd_soc_add_codec_controls(codec, es515_digital_ext_snd_controls,
			ARRAY_SIZE(es515_digital_ext_snd_controls));
	if (rc)
		dev_err(codec->dev, "%s(): es515_digital_ext_snd_controls "
				"failed\n", __func__);

	/* Register digital_snd controls */
	rc = snd_soc_add_codec_controls(codec, es515_digital_snd_controls,
			ARRAY_SIZE(es515_digital_snd_controls));
	if (rc)
		dev_err(codec->dev, "%s(): es515_digital_snd_controls "
				"failed\n", __func__);

	/* Register digital_ext_snd_staged controls */
	rc = snd_soc_add_codec_controls(codec,
			es515_digital_ext_snd_staged_controls,
			ARRAY_SIZE(es515_digital_ext_snd_staged_controls));
	if (rc)
		dev_err(codec->dev, "%s(): "
				"es515_digital_ext_snd_staged_controls "
				"failed\n", __func__);

	return rc;
}

static int  es515_codec_remove(struct snd_soc_codec *codec)
{
	struct es515_priv *es515 = snd_soc_codec_get_drvdata(codec);

	es515_set_bias_level(codec, SND_SOC_BIAS_OFF);

	kfree(es515);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_es515 = {
	.probe =	es515_codec_probe,
	.remove =	es515_codec_remove,
	.suspend =	es515_codec_suspend,
	.resume =	es515_codec_resume,
	.set_bias_level =	es515_set_bias_level,
	.read =		es515_read,
	.write =	es515_write,
};

static struct snd_soc_dai_driver es515_dai[] = {
	{
		.name = "es515-porta",
		.playback = {
			.stream_name = "PORTA Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES515_RATES,
			.formats = ES515_FORMATS,
		},
		.capture = {
			.stream_name = "PORTA Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES515_RATES,
			.formats = ES515_FORMATS,
		},
		.ops = &es515_portx_dai_ops,
	},
	{
		.name = "es515-portb",
		.playback = {
			.stream_name = "PORTB Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES515_RATES,
			.formats = ES515_FORMATS,
		},
		.capture = {
			.stream_name = "PORTB Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES515_RATES,
			.formats = ES515_FORMATS,
		},
		.ops = &es515_portx_dai_ops,
	},
	{
		.name = "es515-portc",
		.playback = {
			.stream_name = "PORTC Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES515_RATES,
			.formats = ES515_FORMATS,
		},
		.capture = {
			.stream_name = "PORTC Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES515_RATES,
			.formats = ES515_FORMATS,
		},
		.ops = &es515_portx_dai_ops,
	},

};

static int es515_bootup(struct es515_priv *es515)
{
	static char sync_ok[] = { 0x80, 0, 0, 0 };
	char msg[16];
	unsigned int buf_frames;
	char *buf_ptr;
	int rc;
	int fw_load_retry = 0;

retry:

	/* Reset Board */
	gpio_set_value(es515->pdata->reset_gpio, 0);
	mdelay(1);
	gpio_set_value(es515->pdata->reset_gpio, 1);
	mdelay(100);

	/* set wakeup pin as 1 */
	gpio_set_value(es515->pdata->wakeup_gpio, 1);

	memset(msg, 0, 16);
	msg[0] = ES515_BOOT_CMD >> 8;
	msg[1] = ES515_BOOT_CMD & 0x00ff;
	rc = ES515_BUS_WRITE(es515, msg, 2, 0);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot write\n", __func__);
		return	rc;
	}
	mdelay(20);

	pr_debug("%s(): read boot cmd ack\n", __func__);
	memset(msg, 0, 16);

	rc = ES515_BUS_READ(es515, msg, 2, 0);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot ack\n", __func__);
		return	rc;
	}

	if ((msg[0] != (ES515_BOOT_ACK >> 8))
			|| (msg[1] != (ES515_BOOT_ACK & 0x00ff))) {
		pr_err("%s(): firmware load failed boot ack", __func__);
		if (fw_load_retry < 5) {
			fw_load_retry++;
			goto retry;
		}
		return	-EIO;
	}

	pr_debug("%s(): write firmware image\n", __func__);
	skip_debug = 1;
	/* send image */
	buf_frames = es515->fw->size / ES515_FW_LOAD_BUF_SZ;
	buf_ptr = (char *)es515->fw->data;
	pr_info("ES515 FW BUFF SIZE = %d, FW size = %d \n", \
		ES515_FW_LOAD_BUF_SZ, es515->fw->size);
	pr_info("%s(): F/W downloading...\n", __func__);
	fw_download = 1;
	for (; buf_frames; --buf_frames, buf_ptr += ES515_FW_LOAD_BUF_SZ) {
		rc = ES515_BUS_WRITE(es515, buf_ptr,
				ES515_FW_LOAD_BUF_SZ, 0);
		if (rc < 0) {
			pr_err("%s(): firmware load failed\n", __func__);
			fw_download = 0;
			return -EIO;
		}
	}
	if (es515->fw->size % ES515_FW_LOAD_BUF_SZ) {
		rc = ES515_BUS_WRITE(es515, buf_ptr,
				es515->fw->size % ES515_FW_LOAD_BUF_SZ,
				0);
		if (rc < 0) {
			pr_err("%s(): firmware load failed\n", __func__);
			fw_download = 0;
			return -EIO;
		}
	}
	fw_download = 0;
	/* Give the chip some time to become ready after firmware
	 * download. */
	mdelay(100);
#if 0
	skip_debug = 0;
#else
	skip_debug = 1;
#endif
	pr_debug("%s(): write ES515_SYNC_CMD\n", __func__);
	fw_load_retry = 0;
retry_sync:
	memset(msg, 0, 16);
	msg[0] = ES515_SYNC_CMD >> 8;
	msg[1] = ES515_SYNC_CMD & 0x00ff;
	msg[2] = ES515_SYNC_POLLING >> 8;
	msg[3] = ES515_SYNC_POLLING & 0x00ff;
	rc = ES515_BUS_WRITE(es515, msg, 4, 1);
	if (rc < 0) {
		if (fw_load_retry < 5) {
			fw_load_retry++;
			goto retry_sync;
		}
		pr_err("%s(): firmware load failed sync write\n", __func__);
		return rc;
	}
	mdelay(20);
	fw_load_retry = 0;
retry_rx:
	pr_debug("%s(): read sync cmd ack\n", __func__);
	memset(msg, 0, 16);
	rc = ES515_BUS_READ(es515, msg, 4, 1);
	if (rc < 0) {
		if (fw_load_retry < 5) {
					fw_load_retry++;
					goto retry_rx;
		}
		pr_err("%s(): error reading firmware sync ack rc =%d\n",
				__func__, rc);
		return rc;
	}
	if (memcmp(msg, sync_ok, 4) == 0) {
		pr_info("%s(): firmware sync ack good =0x%02x%02x%02x%02x\n",
				__func__, msg[0], msg[1], msg[2], msg[3]);
	} else {
		pr_err("%s(): firmware sync ack failed =0x%02x%02x%02x%02x\n",
				__func__, msg[0], msg[1], msg[2], msg[3]);
		return -EIO;
	}
	pr_info("Firmware downloaded successfully.\n");
#if defined(CONFIG_ES515_MASTER)
	pr_info("ES515 in MASTER mode\n");
#else
	pr_info("ES515 in SLAVE mode\n");
#endif

#ifdef ES515_BRINGUP
	/* get sleep */
	memset(msg, 0, 16);
	msg[0] = 0x8010 >> 8;
	msg[1] = 0x8010 & 0x00ff;
	msg[2] = 0x0001 >> 8;
	msg[3] = 0x0001 & 0x00ff;
	rc = ES515_BUS_WRITE(es515, msg, 4, 0);
	if (rc < 0) {
		pr_err("%s(): getting sleep failed\n", __func__);
		return	rc;
	}
	mdelay(1000);

	gpio_set_value(es515->pdata->wakeup_gpio, 0);
	gpio_set_value(es515->pdata->wakeup_gpio, 1);

	mdelay(50);

	memset(msg, 0, 16);
	msg[0] = ES515_SYNC_CMD >> 8;
	msg[1] = ES515_SYNC_CMD & 0x00ff;
	msg[2] = ES515_SYNC_POLLING >> 8;
	msg[3] = ES515_SYNC_POLLING & 0x00ff;
	rc = ES515_BUS_WRITE(es515, msg, 4, 1);
	if (rc < 0) {
		pr_err("%s(): wakeup failed sync write\n", __func__);
		return rc;
	}
	mdelay(20);
	fw_load_retry = 0;

	pr_debug("%s(): read sync cmd ack\n", __func__);
	memset(msg, 0, 16);
	rc = ES515_BUS_READ(es515, msg, 4, 1);
	if (rc < 0) {
		pr_err("%s(): error reading wakeup sync ack rc =%d\n",
				__func__, rc);
		return rc;
	}
	if (memcmp(msg, sync_ok, 4) == 0) {
		pr_info("%s(): wakeup sync ack good =0x%02x%02x%02x%02x\n",
				__func__, msg[0], msg[1], msg[2], msg[3]);
	} else {
		pr_err("%s(): wakeup sync ack failed =0x%02x%02x%02x%02x\n",
				__func__, msg[0], msg[1], msg[2], msg[3]);
		return -EIO;
	}

	mdelay(100);
#endif

	/* set preset # to make ready state 0x8031 0005*/
	memset(msg, 0, 16);
	msg[0] = 0x8031 >> 8;
	msg[1] = 0x8031 & 0x00ff;
	msg[2] = 0x0005 >> 8;
	msg[3] = 0x0005 & 0x00ff;
	rc = ES515_BUS_WRITE(es515, msg, 4, 0);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot write\n", __func__);
		return	rc;
	}
	mdelay(50);

	memset(msg, 0, 16);
	rc = ES515_BUS_READ(es515, msg, 4, 1);
	if (rc < 0) {
		pr_err("%s(): error reading wakeup sync ack rc =%d\n",
				__func__, rc);
		return rc;
	}
	pr_info("%s(): preset ack good =0x%02x%02x%02x%02x\n",
				__func__, msg[0], msg[1], msg[2], msg[3]);

	return 0;
}

#if defined(CONFIG_SND_SOC_ES515_I2C)
static int es515_i2c_read(struct es515_priv *es515, char *buf, int len)
{
	/*int i = 0;*/
	int rc = 0;
	struct i2c_msg msg[] = {
		{
			.addr = es515->this_client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	rc = i2c_transfer(es515->this_client->adapter, msg, 1);
	if (rc < 0) {
		pr_err("%s(): i2c_transfer() failed, rc = %d", __func__, rc);
		return rc;
	}
	if (!skip_debug && !fw_download) {
		pr_debug("[%s]: 0x%02x, 0x%02x, 0x%02x, 0x%02x,",
				__func__, buf[0], buf[1], buf[2], buf[3]);
//		dump_stack();
	}

	return rc;
}

static int es515_i2c_write(struct es515_priv *es515, char *buf, int len)
{
	int rc = 0;
	struct i2c_msg msg[] = {
		{
			.addr = es515->this_client->addr,
			.flags = 0,
			.len = len,
			.buf = buf,
		},
	};

	if (!skip_debug && !fw_download)
		pr_debug("[%s]: 0x%02x, 0x%02x, 0x%02x, 0x%02x,",
				__func__, buf[0], buf[1], buf[2], buf[3]);


	rc = i2c_transfer(es515->this_client->adapter, msg, 1);
	if (rc < 0) {
		pr_err("%s(): i2c_transfer() failed, rc = %d",
				__func__, rc);
		return rc;
	}

	return rc;
}

static irqreturn_t es515_threaded_isr(int irq, void *data)
{
	int value = 0;
	int i = 0, rc = 0;
#ifdef ES515_JACK_DETECTION
	struct es515_priv *es515 = data;
#endif

	value = es515_read(NULL, ES515_GET_SYS_INTERRUPT_STATUS);
	pr_debug("%s:%d Reading System Interrupt Status value %d\n",
		__func__, __LINE__, value);

	if (value < 0) {
		pr_err("Error reading System Interrupt Status\n");
		return IRQ_HANDLED;
	} else {
		for (i = 0; i < ES515_MAX_INTR_STATUS_BITS; i++) {
			/* Clear the Interrupt status */
			rc = es515_write(NULL, ES515_CLEAR_SYS_INTERRUPT_STATUS, value);
			if (rc < 0) {
				pr_err("Error in clearing interrupt status\n");
			}
			if ((value >> i) & 0x1) {
				switch (i) {
				case 0:
					pr_debug("Short Button Prl Press\n");
					break;
				case 1:
					pr_debug("Long Button Prl Press\n");
					break;
				case 2:
					pr_debug("Short Button Srl Press\n");
					break;
				case 3:
					pr_debug("Long Button Srl Press\n");
					break;
				case 4:
					pr_debug("Acc Dev Detect\n");
					value = es515_read(NULL, ES515_ACCESSORY_DET_STATUS);
					if (value < 0) {
						pr_debug("Error reading System Interrupt Status\n");
					} else {
						if (value == 1) {
							pr_debug("HEADSET Detect\n");
#ifdef ES515_JACK_DETECTION
							snd_soc_jack_report(es515->jack, SND_JACK_HEADSET, SND_JACK_HEADSET);
#endif
						} else {
							pr_debug("HEADPHONE Detect\n");
#ifdef ES515_JACK_DETECTION
							snd_soc_jack_report(es515->jack, SND_JACK_HEADPHONE, SND_JACK_HEADSET);
#endif
						}
					}
					/* Select the Button Detection Configuration */
					rc = es515_write(NULL, ES515_DEVICE_PARAM_ID, 0x1F30);
					if (rc > 0) {
						/* Enable the button detection */
						rc = es515_write(NULL, ES515_DEVICE_PARAM, 0x1);
						if (rc > 0) {
							/* Select the Long Button Press configuration */
							rc = es515_write(NULL, ES515_DEVICE_PARAM_ID, 0x1F36);
							if (rc > 0) {
								/* Set the button press threshold to 4 (number of times button pressed */
								rc = es515_write(NULL, ES515_DEVICE_PARAM, 0x3);
								if (rc < 0)
									pr_err("Error enabling the Button detection\n");
							} else
								pr_err("Error in selecting Long Button Press configuration\n");
						} else
							pr_err("Error in enabling Button Detection\n");
					} else
						pr_err("Error setting the Button detection configuration\n");
					break;
				case 5:
					pr_debug("Unplug Detect\n");
					break;
				case 6:
					pr_debug("Plug Detect\n");
					break;
				case 7:
					pr_debug("A212 Ready\n");
					break;
				case 8:
					pr_debug("Thermal Shutdown Detect\n");
					break;
				case 9:
					pr_debug("LO Short Circuit\n");
					break;
				case 10:
					pr_debug("HFL Short Circuit\n");
					break;
				case 11:
					pr_debug("HFR Short Circuit\n");
					break;
				case 12:
					pr_debug("HP Short Circuit\n");
					break;
				case 13:
					pr_debug("EP Short Circuit\n");
					break;
/* dead code
				default:
					pr_err("Unsupported Event\n");
					break;
*/
				}
			}
		}
	}
	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static struct esxxx_platform_data *es515_i2c_parse_dt(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct device_node *np = dev->of_node;
	struct esxxx_platform_data *pdata;

	if (!np)
		return NULL;

	pdata = kzalloc(sizeof(struct esxxx_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate platform data\n");
		return NULL;
	}

	dev->platform_data = pdata;

	pdata->reset_gpio = of_get_gpio(np, 0);
	pdata->wakeup_gpio = of_get_gpio(np, 1);
	pdata->int_gpio = of_get_gpio(np, 2);

	return pdata;
}
#else
static struct esxxx_platform_data *es515_i2c_parse_dt(struct i2c_client *i2c)
{
	return NULL;
}
#endif

void lpass_init_clkout(void);
static int es515_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct esxxx_platform_data *pdata = i2c->dev.platform_data;
	struct es515_priv *es515;
	int rc;

	/* Intialize globals. */
	skip_debug = 0;
	ES515_SR_BIT = true;

	dev_dbg(&i2c->dev, "%s()\n", __func__);

	if (pdata == NULL)
		pdata = es515_i2c_parse_dt(i2c);

	if (pdata == NULL) {
		dev_err(&i2c->dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	es515 = kzalloc(sizeof(struct es515_priv), GFP_KERNEL);
	if (es515 == NULL) {
		dev_err(&i2c->dev, "%s(): kzalloc failed", __func__);
		return -ENOMEM;
	}
	i2c_set_clientdata(i2c, es515);
	es515->this_client = i2c;

	es515_priv_glb.this_client = i2c;
	rc = gpio_request(pdata->reset_gpio, "es515_reset");
	if (rc < 0) {
		dev_err(&i2c->dev, "%s(): es515_reset request failed",
				__func__);
		goto reset_gpio_request_error;
	}
	rc = gpio_direction_output(pdata->reset_gpio, 1);
	if (rc < 0) {
		dev_err(&i2c->dev, "%s(): es515_reset direction failed",
				__func__);
		goto reset_gpio_direction_error;
	}
	rc = gpio_request(pdata->int_gpio, "es515_interrupt");
	if (rc < 0) {
		dev_err(&i2c->dev, "%s(): es515_reset request failed",
				__func__);
		goto reset_gpio_request_error;
	}
#ifdef JANG_RHEA /* gpio setting for EINT has been set in mach */
	rc = gpio_direction_input(pdata->int_gpio);
	if (rc < 0) {
		dev_err(&i2c->dev, "%s(): es515_gpioa direction failed",
				__func__);
		goto int_gpio_direction_error;
	}
#endif
	rc = gpio_request(pdata->wakeup_gpio, "es515_wakeup_gpio");
	if (rc < 0) {
		dev_err(&i2c->dev, "%s(): es515_wakeup_gpio request failed",
				__func__);
		goto wakeup_gpio_request_error;
	}
	rc = gpio_direction_output(pdata->wakeup_gpio, 1);
	if (rc < 0) {
		dev_err(&i2c->dev, "%s(): es515_wakeup_gpio direction failed",
				__func__);
		goto wakeup_gpio_direction_error;
	}
	es515->pdata = pdata;
	es515_priv_glb.pdata = pdata;

#ifndef USE_BUILTIN_FW
	rc = request_firmware(&es515->fw, FW_FILE, &i2c->dev);
	if (rc) {
		dev_err(&i2c->dev, "%s(): request_firmware(%s) failed %d\n",
				__func__, FW_FILE, rc);
		goto request_firmware_error;
	}
#else
	es515->fw = kzalloc(sizeof(*es515->fw), GFP_KERNEL);
	es515->fw->data = es515_firmware_bin;
	es515->fw->size = es515_firmware_bin_len;
#endif
	rc = es515_bootup(es515);
	if (rc) {
		dev_err(&i2c->dev, "%s(): es515_bootup failed %d\n",
				__func__, rc);
		goto bootup_error;
	}

#ifndef USE_BUILTIN_FW
	release_firmware(es515->fw);
#endif
	rc = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_es515, es515_dai,
			ARRAY_SIZE(es515_dai));
#ifdef ES515_DEBUG
	es515_fixed_config(es515);
#endif
	dev_dbg(&i2c->dev, "%s(): rc = snd_soc_regsiter_codec() = %d\n",
			__func__, rc);

	rc = request_threaded_irq(
			gpio_to_irq(es515_priv_glb.pdata->int_gpio),
				NULL, es515_threaded_isr,
				IRQF_DISABLED | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"es515_theaded_isr", es515);
	if (rc < 0) {
		pr_err("Error in setting interrupt mode :%d int_gpio:%d\n",
				rc, es515_priv_glb.pdata->int_gpio);
		return rc;
	}

	es515_priv_glb.intr_type = ES515_POLLING;

	return rc;

bootup_error:
#ifndef USE_BUILTIN_FW
request_firmware_error:
#endif
wakeup_gpio_direction_error:
	gpio_free(pdata->wakeup_gpio);
wakeup_gpio_request_error:
#ifdef JANG_RHEA /* gpio setting for EINT has been set in mach */
int_gpio_direction_error:
#endif
	gpio_free(pdata->int_gpio);
reset_gpio_direction_error:
	gpio_free(pdata->reset_gpio);
reset_gpio_request_error:
pdata_error:
	dev_dbg(&i2c->dev, "%s(): exit with error\n", __func__);
	return rc;
}

static int es515_i2c_remove(struct i2c_client *i2c)
{
	struct esxxx_platform_data *pdata = i2c->dev.platform_data;

	gpio_free(pdata->reset_gpio);
	gpio_free(pdata->wakeup_gpio);
	gpio_free(pdata->int_gpio);

	gpio_free(pdata->gpioa_gpio);

	snd_soc_unregister_codec(&i2c->dev);

	kfree(i2c_get_clientdata(i2c));

	return 0;
}

static const struct i2c_device_id es515_i2c_id[] = {
	{ "es515", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, es515_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id es515_i2c_dt_ids[] = {
	{ .compatible = "audience,es515"},
	{ }
};
#endif

static struct i2c_driver es515_i2c_driver = {
	.driver = {
		.name = "es515-codec",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(es515_i2c_dt_ids),
#endif
	},
	.probe = es515_i2c_probe,
	.remove = es515_i2c_remove,
	.id_table = es515_i2c_id,
};
#endif

static __init int es515_init(void)
{
	int ret;
#if defined(CONFIG_SND_SOC_ES515_I2C)
	ret = i2c_add_driver(&es515_i2c_driver);
	if (ret)
		pr_err("Failed to register Audience eS515 I2C driver: %d\n",
				ret);
#endif
	return 0;
}
module_init(es515_init);

static __exit void es515_exit(void)
{
#if defined(CONFIG_SND_SOC_ES515_I2C)
	i2c_del_driver(&es515_i2c_driver);
#endif

}
module_exit(es515_exit);

MODULE_DESCRIPTION("ASoC ES515 driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:es515-codec");
MODULE_FIRMWARE("audience_fw_es515.bin");

/* tda998x private data */

struct tda998x_priv {
	struct i2c_client *cec;
	struct i2c_client *hdmi;
	struct mutex mutex;
	struct delayed_work dwork;
	uint16_t rev;
	uint8_t current_page;
	int dpms;
	bool is_hdmi_sink;
	u8 vip_cntrl_0;
	u8 vip_cntrl_1;
	u8 vip_cntrl_2;
	struct tda998x_encoder_params params;

	wait_queue_head_t wq_edid;
	volatile int wq_edid_wait;
	struct drm_encoder *encoder;

	u8 audio_ports[2];
	int audio_sample_format;
	int dai_id;			/* DAI ID when streaming active */

	u8 *eld;

	struct snd_pcm_hw_constraint_list rate_constraints;
};

int tda998x_codec_register(struct device *dev);
void tda998x_codec_unregister(struct device *dev);

void tda998x_audio_start(struct tda998x_priv *priv, int full);
void tda998x_audio_stop(struct tda998x_priv *priv);

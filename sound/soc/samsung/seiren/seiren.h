#ifndef __SEIREN_H
#define __SEIREN_H

/* Register offset */
#define CA5_BOOTADDR		(0x0020)
#define CA5_WAKEUP		(0x0028)
#define CA5_STATUS		(0x002C)
#define CA5_DBG			(0x0030)
#define SW_INTR_CA5		(0x0040)
#define INTR_CA5_STATUS		(0x0044)
#define INTR_CA5_MASK		(0x0048)
#define SW_INTR_CPU		(0x0050)
#define INTR_CPU_STATUS		(0x0054)
#define INTR_CPU_MASK		(0x0058)

#define CA5_STATUS_WFI		(1 << 2)
#define CA5_STATUS_WFE		(1 << 1)

/* Mailbox between driver and firmware */
#define VIRSION_ID		(0x0000)
#define CMD_CODE		(0x0004)
#define HANDLE_ID		(0x0008)
#define IP_TYPE			(0x000C)
#define PHY_ADDR_INBUF		(0x000C)
#define PORT_TYPE		(0x000C)
#define PARAM_VAL1		(0x000C)
#define PHY_ADDR_DYNAMIC_MEM	(0x0010)
#define SIZE_OF_INBUF		(0x0010)
#define PARAM_VAL2		(0x0010)
#define SIZE_DYNAMIC_MEM	(0x0014)
#define SIZE_OF_INDATA		(0x0014)
#define PHY_ADDR_OUTBUF		(0x0018)
#define SIZE_OF_OUTBUF		(0x001C)
#define RETURN_CMD		(0x0040)
#define IP_ID			(0x0044)
#define SIZE_OUT_DATA		(0x0048)
#define CONSUMED_BYTE_IN	(0x004C)
#define BIT_DEC_SIZE		(0x0050)
#define FRAME_NUM		(0x0054)
#define CH_NUM			(0x0058)
#define FREQ_SAMPLE		(0x005C)
#define PARAMS_CNT		(0x0060)
#define PARAMS_VAL1		(0x0064)
#define PARAMS_VAL2		(0x0068)
#define FW_LOG_VAL1		(0x0078)
#define FW_LOG_VAL2		(0x007C)

/* Interrupt type */
#define INTR_WAKEUP		(0x0)
#define INTR_READY		(0x1000)
#define INTR_FW_LOG		(0xFFFF)


/* Memory size */
#define FWMEM_SIZE		(0x200000)
#define BASEMEM_OFFSET		(0x300000)
#define FWAREA_SIZE		(0x400000)
#define FWAREA_NUM		(3)
#define FWAREA_IOVA		(0x50000000)

/* Buffer Information - decode */
#define DEC_IBUF_SIZE		(4096)
#define DEC_OBUF_SIZE		(36864)

#define DEC_AAC_IBUF_SIZE       (4096)
#define DEC_AAC_OBUF_SIZE       (73728)

#define DEC_FLAC_IBUF_SIZE	(0x5000)
#define DEC_FLAC_OBUF_SIZE	(0x20000)
#define DEC_IBUF_NUM		(0x2)
#define DEC_OBUF_NUM		(0x2)

/* Buffer Information - sound process */
#define SP_IBUF_SIZE		(0x20000)
#define SP_OBUF_SIZE		(0x20000)
#define SP_IBUF_NUM		(0x1)
#define SP_OBUF_NUM		(0x1)


#define INSTANCE_MAX		(20)
#define SRAM_FW_MAX		(0x3E000)
#define SRAM_IO_BUF		(0x2C000)
#define SRAM_IBUF_OFFSET	(0)
#define SRAM_OBUF_OFFSET	(DEC_IBUF_SIZE)
#define BUF_SIZE_MAX		(0x50000)

#define SAMPLE_RATE_MIN		(8000)
#define CH_NUM_MIN		(1)
#define BAND_NUM_MAX		(16)

#define FW_LOG_ADDR		(0x3D000)
#define FW_LOG_LINE		(30)
#define FW_LOG_MAX		(80)

#define FW_ZERO_SET_BASE	(0x14000)
#define FW_ZERO_SET_SIZE	(0x1F00)

#define FW_SRAM_NAME		"seiren_fw_sram.bin"
#define FW_DRAM_NAME		"seiren_fw_dram.bin"

/* For Debugging */
#define esa_info(x...)		pr_info("SEIREN: " x)
#define esa_err(x...)		pr_err("SEIREN: ERR: " x)

#ifdef CONFIG_SND_SAMSUNG_SEIREN_DEBUG
#define esa_debug(x...)		pr_debug("SEIREN: " x)
#else
#define esa_debug(x...)
#endif

enum SEIREN_CMDTYPE {
	CMD_CREATE = 0x01,
	CMD_DESTROY,
	CMD_SET_PARAMS,
	CMD_GET_PARAMS,
	CMD_RESET,
	CMD_FLUSH,
	CMD_EXE,
	SYS_RESET = 0x80,
	SYS_RESTAR,
	SYS_RESUME,
	SYS_SUSPEND,
	SYS_GET_STATUS,
};

enum SEIREN_IPTYPE {
	ADEC_MP3 = 0x0,
	ADEC_AAC,
	ADEC_FLAC,
	SOUND_EQ = 0x9,
	SOUND_BASS,
	AENC_AMR,
	AENC_AAC,
};

enum SEIREN_PORTTYPE {
	PORT_IN = 0x1,
	PORT_OUT,
};

enum SEIREN_EOSSTATE {
	EOS_NO = 0x0,
	EOS_YET,
	EOS_FINAL,
};

enum SEIREN_PARAMCMD {
	/* PCM parameters */
	PCM_PARAM_MAX_SAMPLE_RATE = 0x0,
	PCM_PARAM_MAX_NUM_OF_CH,
	PCM_PARAM_MAX_BIT_PER_SAMPLE,

	PCM_PARAM_SAMPLE_RATE,
	PCM_PARAM_NUM_OF_CH,
	PCM_PARAM_BIT_PER_SAMPLE,

	PCM_MAX_CONFIG_INFO,
	PCM_CONFIG_INFO,

	/* EQ parameters */
	EQ_PARAM_NUM_OF_PRESETS = 0x10,
	EQ_PARAM_MAX_NUM_OF_BANDS ,
	EQ_PARAM_RANGE_OF_BANDLEVEL,
	EQ_PARAM_RANGE_OF_FREQ,

	EQ_PARAM_PRESET_ID,
	EQ_PARAM_NUM_OF_BANDS,
	EQ_PARAM_CENTER_FREQ,
	EQ_PARAM_BANDLEVEL,
	EQ_PARAM_BANDWIDTH,

	EQ_MAX_CONFIG_INFO,
	EQ_CONFIG_INFO,
	EQ_BAND_INFO,

	/* BASS parameters */

	/* Codec Dec parameters */
	ADEC_PARAM_SET_EOS = 0x30,
	ADEC_PARAM_GET_OUTPUT_STATUS,

	/* MP3 Dec parameters */

	/* AAC Dec parameters */

	/* FLAC Dec parameters */

	/* Codec Enc parameters */

	/* AMR Enc parameters */

	/* AAC Enc parameters */

	/* Buffer info */
	GET_IBUF_POOL_INFO = 0xA0,
	GET_OBUF_POOL_INFO,
	SET_IBUF_POOL_INFO,
	SET_OBUF_POOL_INFO,
};

struct audio_mem_info_t {
	u32	virt_addr;
	u32	phy_addr;
	u32	mem_size;
	u32	data_size;
	u32	block_count;
};

struct audio_mem_pool_info_t {
	u32	virt_addr;
	u32	phy_addr;
	u32	block_size;
	u32	block_count;
};

struct audio_pcm_config_info_t {
	u32	direction;	/* 0: input, 1:output */
	u32	sample_rate;
	u32	bit_per_sample;
	u32	num_of_channel;
};

struct audio_eq_config_info_t {
	u32	preset_id;	/* SEIREN_PRESET_ID */
	u32	num_of_bands;
	u32	band_level[BAND_NUM_MAX];
	u32	center_freq[BAND_NUM_MAX];
	u32	band_width[BAND_NUM_MAX];
};

struct audio_eq_max_config_info_t {
	u32	max_num_of_presets;
	u32	max_num_of_bands;
	u32	range_of_freq;
	u32	range_of_band_level;
};

struct audio_eq_band_info_t {
	u32	band_id;
	u32	band_level;
	u32	center_freq;
	u32	band_width;
};

struct seiren_info {
	struct platform_device *pdev;
	spinlock_t	lock;
	void __iomem	*regs;
	void __iomem	*mailbox;
	void __iomem	*sram;
	struct clk	*clk_ca5;
	struct clk	*opclk_ca5;
	unsigned int	irq_ca5;
	struct proc_dir_entry	*proc_file;
#ifdef CONFIG_SND_SAMSUNG_IOMMU
	struct iommu_domain	*domain;
#endif
	unsigned char	*fwarea[FWAREA_NUM];
	phys_addr_t	fwarea_pa[FWAREA_NUM];
	unsigned char	*bufmem;
	unsigned int	bufmem_pa;
	unsigned char	*fwmem;
	unsigned int	fwmem_pa;
	unsigned char	*fwmem_sram_bak;
	volatile bool	isr_done;
	bool		fwmem_loaded;
	int		fw_sbin_size;
	int		fw_dbin_size;

	int		rtd_cnt;
	struct esa_rtd	*rtd_pool[INSTANCE_MAX];

	unsigned char	fw_log[FW_LOG_MAX];
	unsigned int	fw_log_pos;
	char		*fw_log_buf;
	bool		fw_ready;
	bool		fw_suspended;
	bool		fw_use_dram;
};

struct esa_rtd {
	/* BUF informaion */
	struct audio_mem_info_t	ibuf_info;
	struct audio_mem_info_t	obuf_info;
	struct audio_pcm_config_info_t	pcm_info;
	unsigned long	buf_maxsize;	/* IBUF + OBUF */
	bool		use_sram;

	/* IBUF informaion */
	unsigned char	*ibuf0;
	unsigned char	*ibuf1;
	unsigned long	ibuf_size;
	unsigned int	ibuf_count;
	unsigned int	ibuf0_offset;
	unsigned int	ibuf1_offset;
	unsigned int	select_ibuf;

	/* OBUF informaion */
	unsigned char	*obuf0;
	unsigned char	*obuf1;
	unsigned long	obuf_size;
	unsigned int	obuf_count;
	unsigned int	obuf0_offset;
	unsigned int	obuf1_offset;
	unsigned int	obuf0_filled_size;
	unsigned int	obuf1_filled_size;
	unsigned int	select_obuf;
	bool		obuf0_filled;
	bool		obuf1_filled;

	/* status information */
	unsigned int	ip_type;
	unsigned int	handle_id;
	unsigned int	get_eos;
	bool		need_config;

	/* multi-instance */
	unsigned int	idx;
};

#endif /* __SEIREN_H */

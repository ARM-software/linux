
#ifndef __FIMD_FB_H__
#define __FIMD_FB_H__

/* S3C_FB_MAX_WIN
 * Set to the maximum number of windows that any of the supported hardware
 * can use. Since the platform data uses this for an array size, having it
 * set to the maximum of any version of the hardware can do is safe.
 */
#define S3C_FB_MAX_WIN	(5)

#ifdef CONFIG_FB_EXYNOS_FIMD_MC
#define SYSREG_MIXER0_VALID	(1 << 7)
#define SYSREG_MIXER1_VALID	(1 << 4)
#define FIMD_PAD_SINK_FROM_GSCALER_SRC		0
#define FIMD_PADS_NUM				1

/* SYSREG for local path between Gscaler and Mixer */
#define SYSREG_DISP1BLK_CFG	(S3C_VA_SYS + 0x0214)
#endif

#ifdef CONFIG_FB_EXYNOS_FIMD_MC_WB
#define SYSREG_DISP1WB_DEST(_x)			((_x) << 10)
#define SYSREG_DISP1WB_DEST_MASK		(0x3 << 10)
#define FIMD_WB_PAD_SRC_TO_GSCALER_SINK		0
#define FIMD_WB_PADS_NUM			1

/* SYSREG for local path between Gscaler and Mixer */
#define SYSREG_GSCLBLK_CFG	(S3C_VA_SYS + 0x0224)
#endif

#define VALID_BPP(x) (1 << ((x) - 1))

#define OSD_BASE(win, variant) ((variant).osd + ((win) * (variant).osd_stride))
#define VIDOSD_A(win, variant) (OSD_BASE(win, variant) + 0x00)
#define VIDOSD_B(win, variant) (OSD_BASE(win, variant) + 0x04)
#define VIDOSD_C(win, variant) (OSD_BASE(win, variant) + 0x08)
#define VIDOSD_D(win, variant) (OSD_BASE(win, variant) + 0x0C)

enum s3c_fb_pm_status {
	POWER_ON = 0,
	POWER_DOWN = 1,
	POWER_HIBER_DOWN = 2,
};

#ifdef CONFIG_FB_I80_COMMAND_MODE
struct s3c_fb_i80mode {
	const char *name;
	u8 cs_setup_time;
	u8 wr_setup_time;
	u8 wr_act_time;
	u8 wr_hold_time;
	u8 auto_cmd_rate;
	u8 frame_skip:2;
	u8 rs_pol:1;
	u32 refresh;
	u32 left_margin;
	u32 right_margin;
	u32 upper_margin;
	u32 lower_margin;
	u32 hsync_len;
	u32 vsync_len;
	u32 xres;
	u32 yres;
	u32 pixclock;
};
#endif

/**
 * struct s3c_fb_pd_win - per window setup data
 * @win_mode: The display parameters to initialise (not for window 0)
 * @virtual_x: The virtual X size.
 * @virtual_y: The virtual Y size.
 * @width: The width of display in mm
 * @height: The height of display in mm
 */

struct s3c_fb_pd_win {
#ifdef CONFIG_FB_I80_COMMAND_MODE
	struct s3c_fb_i80mode	win_mode;
#else
	struct fb_videomode	win_mode;
#endif

	unsigned short		default_bpp;
	unsigned short		max_bpp;
	unsigned short		virtual_x;
	unsigned short		virtual_y;
	unsigned short		width;
	unsigned short		height;
};

/**
 * struct s3c_fb_platdata -  S3C driver platform specific information
 * @setup_gpio: Setup the external GPIO pins to the right state to transfer
 *		the data from the display system to the connected display
 *		device.
 * @default_win: default window layer number to be used for UI layer.
 * @vidcon0: The base vidcon0 values to control the panel data format.
 * @vidcon1: The base vidcon1 values to control the panel data output.
 * @win: The setup data for each hardware window, or NULL for unused.
 * @display_mode: The LCD output display mode.
 *
 * The platform data supplies the video driver with all the information
 * it requires to work with the display(s) attached to the machine. It
 * controls the initial mode, the number of display windows (0 is always
 * the base framebuffer) that are initialised etc.
 *
 */
struct s3c_fb_platdata {
	void	(*setup_gpio)(void);
	void	(*backlight_off)(void);
	void	(*lcd_off)(void);

	struct s3c_fb_pd_win	*win[S3C_FB_MAX_WIN];

	u32			 default_win;

	u32			 vidcon0;
	u32			 vidcon1;
	int			 ip_version;
};

/**
 * struct s3c_fb_variant - fb variant information
 * @is_2443: Set if S3C2443/S3C2416 style hardware.
 * @nr_windows: The number of windows.
 * @vidtcon: The base for the VIDTCONx registers
 * @wincon: The base for the WINxCON registers.
 * @winmap: The base for the WINxMAP registers.
 * @keycon: The abse for the WxKEYCON registers.
 * @buf_start: Offset of buffer start registers.
 * @buf_size: Offset of buffer size registers.
 * @buf_end: Offset of buffer end registers.
 * @osd: The base for the OSD registers.
 * @palette: Address of palette memory, or 0 if none.
 * @has_prtcon: Set if has PRTCON register.
 * @has_shadowcon: Set if has SHADOWCON register.
 * @has_blendcon: Set if has BLENDCON register.
 * @has_alphacon: Set if has VIDWALPHA register.
 * @has_clksel: Set if VIDCON0 register has CLKSEL bit.
 * @has_fixvclk: Set if VIDCON1 register has FIXVCLK bits.
 */
struct s3c_fb_variant {
	unsigned int	is_2443:1;
	unsigned short	nr_windows;
	unsigned int	vidcon1;
	unsigned int	vidtcon;
	unsigned short	wincon;
	unsigned short	winmap;
	unsigned short	keycon;
	unsigned short	buf_start;
	unsigned short	buf_end;
	unsigned short	buf_size;
	unsigned short	osd;
	unsigned short	osd_stride;
	unsigned short	palette[S3C_FB_MAX_WIN];

	unsigned int	has_prtcon:1;
	unsigned int	has_shadowcon:1;
	unsigned int	has_blendcon:1;
	unsigned int	has_alphacon:1;
	unsigned int	has_clksel:1;
	unsigned int	has_fixvclk:1;
};

/**
 * struct s3c_fb_win_variant
 * @has_osd_c: Set if has OSD C register.
 * @has_osd_d: Set if has OSD D register.
 * @has_osd_alpha: Set if can change alpha transparency for a window.
 * @palette_sz: Size of palette in entries.
 * @palette_16bpp: Set if palette is 16bits wide.
 * @osd_size_off: If != 0, supports setting up OSD for a window; the appropriate
 *                register is located at the given offset from OSD_BASE.
 * @valid_bpp: 1 bit per BPP setting to show valid bits-per-pixel.
 *
 * valid_bpp bit x is set if (x+1)BPP is supported.
 */
struct s3c_fb_win_variant {
	unsigned int	has_osd_c:1;
	unsigned int	has_osd_d:1;
	unsigned int	has_osd_alpha:1;
	unsigned int	palette_16bpp:1;
	unsigned short	osd_size_off;
	unsigned short	palette_sz;
	u32		valid_bpp;
};

/**
 * struct s3c_fb_driverdata - per-device type driver data for init time.
 * @variant: The variant information for this driver.
 * @win: The window information for each window.
 */
struct s3c_fb_driverdata {
	struct s3c_fb_variant	variant;
	struct s3c_fb_win_variant *win[S3C_FB_MAX_WIN];
};

/**
 * struct s3c_fb_palette - palette information
 * @r: Red bitfield.
 * @g: Green bitfield.
 * @b: Blue bitfield.
 * @a: Alpha bitfield.
 */
struct s3c_fb_palette {
	struct fb_bitfield	r;
	struct fb_bitfield	g;
	struct fb_bitfield	b;
	struct fb_bitfield	a;
};

#ifdef CONFIG_ION_EXYNOS
struct s3c_dma_buf_data {
	struct ion_handle *ion_handle;
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sg_table;
	dma_addr_t dma_addr;
	struct sync_fence *fence;
};

struct s3c_reg_data {
	struct list_head	list;
	u32			shadowcon;
	u32			wincon[S3C_FB_MAX_WIN];
	u32			win_rgborder[S3C_FB_MAX_WIN];
	u32			winmap[S3C_FB_MAX_WIN];
	u32			vidosd_a[S3C_FB_MAX_WIN];
	u32			vidosd_b[S3C_FB_MAX_WIN];
	u32			vidosd_c[S3C_FB_MAX_WIN];
	u32			vidosd_d[S3C_FB_MAX_WIN];
	u32			vidw_alpha0[S3C_FB_MAX_WIN];
	u32			vidw_alpha1[S3C_FB_MAX_WIN];
	u32			blendeq[S3C_FB_MAX_WIN - 1];
	u32			vidw_buf_start[S3C_FB_MAX_WIN];
	u32			vidw_buf_end[S3C_FB_MAX_WIN];
	u32			vidw_buf_size[S3C_FB_MAX_WIN];
	struct s3c_dma_buf_data	dma_buf_data[S3C_FB_MAX_WIN];
	unsigned int		bandwidth;
	u32			win_overlap_cnt;
};
#endif

/**
 * struct s3c_fb_win - per window private data for each framebuffer.
 * @windata: The platform data supplied for the window configuration.
 * @parent: The hardware that this window is part of.
 * @fbinfo: Pointer pack to the framebuffer info for this window.
 * @varint: The variant information for this window.
 * @palette_buffer: Buffer/cache to hold palette entries.
 * @pseudo_palette: For use in TRUECOLOUR modes for entries 0..15/
 * @index: The window number of this window.
 * @palette: The bitfields for changing r/g/b into a hardware palette entry.
 */
struct s3c_fb_win {
	struct s3c_fb_pd_win	*windata;
	struct s3c_fb		*parent;
	struct fb_info		*fbinfo;
	struct s3c_fb_palette	 palette;
	struct s3c_fb_win_variant variant;

	u32			*palette_buffer;
	u32			 pseudo_palette[16];
	unsigned int		 index;
#ifdef CONFIG_ION_EXYNOS
	struct s3c_dma_buf_data	dma_buf_data;
	struct fb_var_screeninfo prev_var;
	struct fb_fix_screeninfo prev_fix;
#endif

	int			fps;

#ifdef CONFIG_FB_EXYNOS_FIMD_MC
	int use; /* use of widnow subdev in fimd */
	struct media_pad pads[FIMD_PADS_NUM]; /* window's pad : 1 sink */
	struct v4l2_subdev sd; /* Take a window as a v4l2_subdevice */
#endif
	int local; /* use of local path gscaler to window in fimd */
};

/**
 * struct s3c_fb_vsync - vsync information
 * @wait:		a queue for processes waiting for vsync
 * @timestamp:		the time of the last vsync interrupt
 * @active:		whether userspace is requesting vsync notifications
 * @irq_refcount:	reference count for the underlying irq
 * @irq_lock:		mutex protecting the irq refcount and register
 * @thread:		notification-generating thread
 */
struct s3c_fb_vsync {
	wait_queue_head_t	wait;
	ktime_t			timestamp;
	bool			active;
	int			irq_refcount;
	struct mutex		irq_lock;
	struct task_struct	*thread;
};

#ifdef CONFIG_DEBUG_FS
#define S3C_FB_DEBUG_FIFO_TIMESTAMPS 32
#define S3C_FB_DEBUG_REGS_SIZE 0x0280

struct s3c_fb_debug {
	ktime_t		fifo_timestamps[S3C_FB_DEBUG_FIFO_TIMESTAMPS];
	unsigned int	num_timestamps;
	unsigned int	first_timestamp;
	u8		regs_at_underflow[S3C_FB_DEBUG_REGS_SIZE];
};
#endif

/**
 * struct s3c_fb - overall hardware state of the hardware
 * @slock: The spinlock protection for this data sturcture.
 * @dev: The device that we bound to, for printing, etc.
 * @bus_clk: The clk (hclk) feeding our interface and possibly pixclk.
 * @lcd_clk: The clk (sclk) feeding pixclk.
 * @regs: The mapped hardware registers.
 * @variant: Variant information for this hardware.
 * @enabled: A bitmask of enabled hardware windows.
 * @output_on: Flag if the physical output is enabled.
 * @pdata: The platform configuration data passed with the device.
 * @windows: The hardware windows that have been claimed.
 * @irq_no: IRQ line number
 * @vsync_info: VSYNC-related information (count, queues...)
 */
struct s3c_fb {
	spinlock_t		slock;
	struct device		*dev;
	struct clk              *bus_clk;
	struct clk              *lcd_clk;
	struct clk              *axi_disp1;
	void __iomem		*regs;
	struct s3c_fb_variant	 variant;

	bool			output_on;
	struct mutex		output_lock;

	struct s3c_fb_platdata	*pdata;
	struct s3c_fb_win	*windows[S3C_FB_MAX_WIN];

	int			 irq_no;
	struct s3c_fb_vsync	 vsync_info;
	enum s3c_fb_pm_status	 power_state;

#ifdef CONFIG_ION_EXYNOS
	struct ion_client	*fb_ion_client;

	struct list_head	update_regs_list;
	struct mutex		update_regs_list_lock;
	struct work_struct	update_regs_work;
	struct workqueue_struct *update_regs_wq;

	struct sw_sync_timeline *timeline;
	int			timeline_max;
#endif

#ifdef CONFIG_FB_EXYNOS_FIMD_MC
	struct exynos_md *md;
#endif
#ifdef CONFIG_FB_EXYNOS_FIMD_MC_WB
	struct exynos_md *md_wb;
	int use_wb;	/* use of fimd subdev for writeback */
	int local_wb;	/* use of writeback path to gscaler in fimd */
	struct media_pad pads_wb;	/* FIMD1's pad */
	struct v4l2_subdev sd_wb;	/* Take a FIMD1 as a v4l2_subdevice */
#endif

#ifdef CONFIG_DEBUG_FS
	struct dentry		*debug_dentry;
	struct s3c_fb_debug	debug_data;
#endif
	struct exynos5_bus_mif_handle *fb_mif_handle;
	struct exynos5_bus_int_handle *fb_int_handle;
};

struct s3c_fb_rect {
	int	left;
	int	top;
	int	right;
	int	bottom;
};

struct s3c_fb_user_window {
	int x;
	int y;
};

struct s3c_fb_user_plane_alpha {
	int		channel;
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
};

struct s3c_fb_user_chroma {
	int		enabled;
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
};

struct s3c_fb_user_ion_client {
	int	fd;
	int	offset;
};

enum s3c_fb_pixel_format {
	S3C_FB_PIXEL_FORMAT_RGBA_8888 = 0,
	S3C_FB_PIXEL_FORMAT_RGBX_8888 = 1,
	S3C_FB_PIXEL_FORMAT_RGBA_5551 = 2,
	S3C_FB_PIXEL_FORMAT_RGB_565 = 3,
	S3C_FB_PIXEL_FORMAT_BGRA_8888 = 4,
	S3C_FB_PIXEL_FORMAT_BGRX_8888 = 5,
	S3C_FB_PIXEL_FORMAT_MAX = 6,
};

enum s3c_fb_blending {
	S3C_FB_BLENDING_NONE = 0,
	S3C_FB_BLENDING_PREMULT = 1,
	S3C_FB_BLENDING_COVERAGE = 2,
	S3C_FB_BLENDING_MAX = 3,
};

struct s3c_fb_win_config {
	enum {
		S3C_FB_WIN_STATE_DISABLED = 0,
		S3C_FB_WIN_STATE_COLOR,
		S3C_FB_WIN_STATE_BUFFER,
	} state;

	union {
		__u32 color;
		struct {
			int				fd;
			__u32				offset;
			__u32				stride;
			enum s3c_fb_pixel_format	format;
			enum s3c_fb_blending		blending;
			int				fence_fd;
			int				plane_alpha;
		};
	};

	int	x;
	int	y;
	__u32	w;
	__u32	h;
};

struct s3c_fb_win_config_data {
	int	fence;
	struct s3c_fb_win_config config[S3C_FB_MAX_WIN];
};


int s3c_fb_runtime_suspend(struct device *dev);
int s3c_fb_runtime_resume(struct device *dev);
int s3c_fb_resume(struct device *dev);
int s3c_fb_suspend(struct device *dev);

#define VALID_BPP(x) (1 << ((x) - 1))
#define VALID_BPP124 (VALID_BPP(1) | VALID_BPP(2) | VALID_BPP(4))
#define VALID_BPP1248 (VALID_BPP124 | VALID_BPP(8))

/* IOCTL commands */
#define S3CFB_WIN_POSITION		_IOW('F', 203, \
						struct s3c_fb_user_window)
#define S3CFB_WIN_SET_PLANE_ALPHA	_IOW('F', 204, \
						struct s3c_fb_user_plane_alpha)
#define S3CFB_WIN_SET_CHROMA		_IOW('F', 205, \
						struct s3c_fb_user_chroma)
#define S3CFB_SET_VSYNC_INT		_IOW('F', 206, __u32)

#define S3CFB_GET_ION_USER_HANDLE	_IOWR('F', 208, \
						struct s3c_fb_user_ion_client)
#define S3CFB_WIN_CONFIG		_IOW('F', 209, \
						struct s3c_fb_win_config_data)
#define S3CFB_WIN_PSR_EXIT 		_IOW('F', 210, int)
#endif

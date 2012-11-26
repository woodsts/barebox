
u64 imx_uid(void);

enum imx_bootsource {
	bootsource_unknown,
	bootsource_nand,
	bootsource_nor,
	bootsource_mmc,
	bootsource_i2c,
	bootsource_spi,
	bootsource_serial,
	bootsource_onenand,
	bootsource_hd,
};

enum imx_bootsource imx_bootsource(void);
void imx_set_bootsource(enum imx_bootsource src);

int imx_25_35_boot_save_loc(unsigned int ctrl, unsigned int type);
void imx_27_boot_save_loc(void __iomem *sysctrl_base);
int imx51_boot_save_loc(void __iomem *src_base);
int imx53_boot_save_loc(void __iomem *src_base);

/* There's a off-by-one betweem the gpio bank number and the gpiochip */
/* range e.g. GPIO_1_5 is gpio 5 under linux */
#define IMX_GPIO_NR(bank, nr)		(((bank) - 1) * 32 + (nr))

int imx1_init(void);
int imx1_devices_init(void);
int imx21_init(void);
int imx22_devices_init(void);
int imx25_init(void);
int imx25_devices_init(void);
int imx27_init(void);
int imx27_devices_init(void);
int imx31_init(void);
int imx31_devices_init(void);
int imx35_init(void);
int imx35_devices_init(void);
int imx51_init(void);
int imx51_devices_init(void);
int imx53_init(void);
int imx53_devices_init(void);
int imx6_init(void);
int imx6_devices_init(void);

#ifdef CONFIG_ARCH_IMX1
#define cpu_is_mx1()	(1)
#else
#define cpu_is_mx1()	(0)
#endif

#ifdef CONFIG_ARCH_IMX21
#define cpu_is_mx21()	(1)
#else
#define cpu_is_mx21()	(0)
#endif

#ifdef CONFIG_ARCH_IMX25
#define cpu_is_mx25()	(1)
#else
#define cpu_is_mx25()	(0)
#endif

#ifdef CONFIG_ARCH_IMX27
#define cpu_is_mx27()	(1)
#else
#define cpu_is_mx27()	(0)
#endif

#ifdef CONFIG_ARCH_IMX31
#define cpu_is_mx31()	(1)
#else
#define cpu_is_mx31()	(0)
#endif

#ifdef CONFIG_ARCH_IMX35
#define cpu_is_mx35()	(1)
#else
#define cpu_is_mx35()	(0)
#endif

#ifdef CONFIG_ARCH_IMX51
#define cpu_is_mx51()	(1)
#else
#define cpu_is_mx51()	(0)
#endif

#ifdef CONFIG_ARCH_IMX53
#define cpu_is_mx53()	(1)
#else
#define cpu_is_mx53()	(0)
#endif

#ifdef CONFIG_ARCH_IMX6
#define cpu_is_mx6()	(1)
#else
#define cpu_is_mx6()	(0)
#endif

#define cpu_is_mx23()	(0)
#define cpu_is_mx28()	(0)

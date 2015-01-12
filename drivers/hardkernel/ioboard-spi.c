//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  ODROID IOBOARD Board : IOBOARD SST25WF020 SPI Flash driver (charles.park)
//  2013.08.28
// 
//
//[*]--------------------------------------------------------------------------------------------------[*]
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include <linux/gpio.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <mach/regs-pmu.h>
#include <plat/gpio-cfg.h>
#include <linux/spi/spi.h>

#if defined(CONFIG_OF)
    #include <linux/of.h>
    #include <linux/of_gpio.h>
#endif

#include "ioboard-spi.h"

//[*]--------------------------------------------------------------------------------------------------[*]
#define SST25WF020_NAME 	    "ioboard-spi"

//[*]--------------------------------------------------------------------------------------------------[*]
#define SPI_MAX_BUFFER_SIZE     32

#define SPI_MAX_RETRY_CNT       100

// Read memory
#define CMD_READ                0x03
#define CMD_HIGH_SPEED_READ     0x0B
#define CMD_READ_STATUS_REG     0x05

// Erase memory
#define CMD_ERASE_4KB           0x20
#define CMD_ERASE_32KB          0x52
#define CMD_ERASE_64KB          0xD8
//#define CMD_ERASE_ALL           0x60
#define CMD_ERASE_ALL           0xC7

// BYTE write command
#define CMD_BYTE_WRITE          0x02
// Auto address increment programming (AAI word programming)
#define CMD_WRITE_WORD          0xAD

#define CMD_WRITE_ENABLE        0x06
#define CMD_WRITE_DISABLE       0x04

#define CMD_WRITE_STATUS_REG    0x01
    // BUSY
    //  1   : internal write operation is in progress
    //  0   : no internal write operation is in progress
    #define STATUS_REG_BUSY     0x01    // (Read)
    // WEL
    //  1   : Device is memory write enabled
    //  0   : Device is not memory write enabled
    #define STATUS_REG_WEL      0x02    // (Read)
    // BP1 BP0
    //  0   0   : none protected memory
    //  0   1   : 030000H-03FFFFH protected
    //  1   0   : 020000H-03FFFFH protected
    //  1   1   : 000000H-03FFFFH protected (Power-up default value)
    #define STATUS_REG_BP0      0x04    // (R/W)
    #define STATUS_REG_BP1      0x08    // (R/W)
    // BPL
    //  1   : BP1 and BP0 are read-only bits
    //  0   : BP1 and BP0 are read/writable (Power-up default value)
    #define STATUS_REG_BPL      0x80    // (R/W)

//    struct ioboard_spi  *ioboard_spi = dev_get_drvdata(&spi->dev);
//[*]--------------------------------------------------------------------------------------------------[*]
int wait_until_ready         (struct spi_device *spi, unsigned int check_usec)
{
    unsigned char   tx, status, retry_cnt = 0;

    do  {
        tx = CMD_READ_STATUS_REG; // ReaD Status Register(RDSR)
        
        spi_write_then_read(spi, &tx, sizeof(tx), &status, sizeof(status));
        udelay(check_usec);
        
        if(retry_cnt++ >= SPI_MAX_RETRY_CNT)    {
            printk("%s : timeout error!!\n", __func__); return  -1;
        }
    }   while(status & STATUS_REG_BUSY);
    
    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int is_ready    (struct spi_device *spi)
{
    unsigned char   tx, status;

    tx = CMD_READ_STATUS_REG; // ReaD Status Register(RDSR)
        
    spi_write_then_read(spi, &tx, sizeof(tx), &status, sizeof(status));

    // 0 : not ready, 1 : ready
    return  (status & STATUS_REG_BUSY) ? 0 : 1;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int ioboard_spi_write_enable     (struct spi_device *spi, unsigned char enable)
{
    unsigned char   tx;
    
    tx = enable ? CMD_WRITE_ENABLE : CMD_WRITE_DISABLE;

    spi_write_then_read(spi, &tx, sizeof(tx), NULL, 0);

    if(wait_until_ready(spi, 100))  {
        printk("%s : error!!\n", __func__);     return  -1;
    }   
    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int ioboard_spi_erase        (struct spi_device *spi, unsigned int addr, unsigned char mode)
{
    unsigned char   tx[4], tx_size;

    switch(mode) {
        case CMD_ERASE_4KB :    case CMD_ERASE_32KB:    case CMD_ERASE_64KB:
            tx[0] = mode;  tx_size = 4;
            tx[1] = (addr >> 16) & 0xFF;   
            tx[2] = (addr >>  8) & 0xFF;
            tx[3] = (addr      ) & 0xFF;
            break;
        case CMD_ERASE_ALL :
            tx[0] = mode;  tx_size = 1;
            break;
        default :    
            printk("%s : Unknown command 0x%02X\n", __func__, mode);
            return  -1;
    }
    // write enable
    if(ioboard_spi_write_enable(spi, 1) != 0)   return  -1;     // write enable

    spi_write_then_read(spi, &tx[0], tx_size, NULL, 0);     

    if(mode == CMD_ERASE_ALL)   mdelay(200);
    else                        mdelay(100);

    if(wait_until_ready(spi, 100))              return  -1;

    // write disable
    if(ioboard_spi_write_enable(spi, 0) != 0)   return  -1;     // write disable

    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int ioboard_spi_byte_write   (struct spi_device *spi, unsigned int addr, unsigned char wdata)
{
    unsigned char   tx[5];

    // write enable
    if(ioboard_spi_write_enable(spi, 1) != 0)   return  -1;     // write enable

    tx[0] = CMD_BYTE_WRITE;
    tx[1] = (addr >> 16) & 0xFF;
    tx[2] = (addr >>  8) & 0xFF;
    tx[3] = (addr      ) & 0xFF;
    tx[4] = wdata;    
    
    spi_write_then_read(spi, &tx[0], 5, NULL, 0);

    if(wait_until_ready(spi, 100))     {
        printk("%s : error!!\n", __func__);     return  -1;
    }

    if(ioboard_spi_write_enable(spi, 0) != 0)   return  -1;     // write disable

    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int ioboard_spi_read_memory_highspeed    (struct spi_device *spi, unsigned int addr, unsigned char *rdata, unsigned int size)
{
    unsigned char   tx[5]; // read memory highspeed
    
    tx[0] = CMD_HIGH_SPEED_READ;    tx[1] = (addr >> 16) & 0xFF;
    tx[2] = (addr >>  8) & 0xFF;    tx[3] = (addr      ) & 0xFF;
    tx[4] = 0x00; // Dummy cycle

    spi_write_then_read(spi, &tx[0], sizeof(tx), &rdata[0], size);

    if(wait_until_ready(spi, 100))     {
        printk("%s : error!!\n", __func__);     return  -1;
    }

    if(size == 0)   {
        printk("%s : read size is 0. error!!\n", __func__); return -1;
    }

    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int ioboard_spi_read_memory              (struct spi_device *spi, unsigned int addr, unsigned char *rdata, unsigned int size)
{
    unsigned char   tx[4]; // read memory
    
    tx[0] = CMD_READ;               tx[1] = (addr >> 16) & 0xFF;
    tx[2] = (addr >>  8) & 0xFF;    tx[3] = (addr      ) & 0xFF;

    spi_write_then_read(spi, &tx[0], sizeof(tx), &rdata[0], size);

    if(wait_until_ready(spi, 100))     {
        printk("%s : error!!\n", __func__);     return  -1;
    }

    if(size == 0)   {
        printk("%s : read size is 0. error!!\n", __func__); return -1;
    }

    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int ioboard_spi_read    (struct spi_device *spi, unsigned int addr, unsigned char *rdata, unsigned int size)
{
    unsigned int    offset = 0, mok, na;
    
    mok     = size / SPI_MAX_BUFFER_SIZE;
    na      = size % SPI_MAX_BUFFER_SIZE;
    
    while(mok)  {
        ioboard_spi_read_memory_highspeed(spi, addr + offset, &rdata[offset], SPI_MAX_BUFFER_SIZE);
        offset += SPI_MAX_BUFFER_SIZE;      mok = mok - 1;
    }
    
    if(na)  {
        ioboard_spi_read_memory_highspeed(spi, addr + offset, &rdata[offset], na);
    }
    
    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int ioboard_spi_write   (struct spi_device *spi, unsigned int addr, unsigned char *wdata, unsigned int size)
{
    unsigned int    i;

    if(size == 0)   {
        printk("%s : write size is 0. error!!\n", __func__); return -1;
    }
    
    for(i = 0; i < size; i++)   ioboard_spi_byte_write(spi, addr + i, wdata[i]);

    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int ioboard_spi_id_read      (struct spi_device *spi)
{
    unsigned char   tx[] = { 0xAB, 0x00, 0x00, 0x00 };
    unsigned char   rx[2];
    
    spi_write_then_read(spi, &tx, sizeof(tx), &rx, sizeof(rx));
    
    if(wait_until_ready(spi, 100) != 0)     {
        printk("%s : ready error!\n", __func__);    return  -1;
    }
    
    printk("%s : id read ! [0x%02X][0x%02X]\n", __func__, rx[0], rx[1]);

    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int ioboard_spi_write_status_reg (struct spi_device *spi, unsigned char status)
{
    unsigned char   tx[2];
    
    // write enable
    if(ioboard_spi_write_enable(spi, 1) != 0)   return  -1;     // write enable
        
    tx[0] = CMD_WRITE_STATUS_REG;
    tx[1] = status;
    spi_write_then_read(spi, &tx, sizeof(tx), NULL, 0);
    
    if(ioboard_spi_write_enable(spi, 0) != 0)   return  -1;     // write disable
        
    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
#if defined(CONFIG_OF)
    int     wp_gpio = -1;
#else
    int 	wp_gpio = EXYNOS5410_GPX1(5);			// GPIO Number
#endif    

static void ioboard_spi_wp_disable   (unsigned char disable)
{
#if defined(CONFIG_OF)
    if(wp_gpio < 0) {
        printk("%s : write protect gpio not defined!!\n", __func__);
        return;
    }
#endif    
    // write protection port init
    if(gpio_request(wp_gpio, "ioboard-spi-wp"))  {
        printk("%s : %s gpio request error!\n", __func__, "ioboard-spi-wp");
    }
    else    {
        s3c_gpio_setpull(wp_gpio, S3C_GPIO_PULL_NONE);

        // write-protection disable 
        if(disable) gpio_direction_output(wp_gpio, 1);   // write enable
        else        gpio_direction_output(wp_gpio, 0);   // write disable
    }
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void ioboard_spi_test        (struct spi_device *spi)
{
    unsigned char   wdata[64], rdata[64];
    unsigned int    addr = 0, i;
    
    for(i = 0; i < sizeof(wdata); i++)  wdata[i] = i;
    
    // read before erase
    memset(rdata, 0x00, sizeof(rdata));
    ioboard_spi_read (spi, addr, &rdata[0], sizeof(rdata));

    printk("\nread before erase : addr = 0x%04X", addr);
    for(i = 0; i < sizeof(rdata); i++)  {
        if(!(i % 16))   printk("\n");
        printk("[0x%02X] ", rdata[i]);
    }
    
    ioboard_spi_erase(spi, addr, CMD_ERASE_ALL);
    
    // read after erase
    memset(rdata, 0x00, sizeof(rdata));
    ioboard_spi_read (spi, addr, &rdata[0], sizeof(rdata));

    printk("\nread after erase : addr = 0x%04X", addr);
    for(i = 0; i < sizeof(rdata); i++)  {
        if(!(i % 16))   printk("\n");
        printk("[0x%02X] ", rdata[i]);
    }
    
    ioboard_spi_write(spi, addr, &wdata[0], sizeof(wdata));

    // read after erase
    memset(rdata, 0x00, sizeof(rdata));
    ioboard_spi_read (spi, 0, &rdata[0], sizeof(rdata));

    printk("\nread after write : addr = 0x%04X", addr);
    for(i = 0; i < sizeof(rdata); i++)  {
        if(!(i % 16))   printk("\n");
        printk("[0x%02X] ", rdata[i]);
    }
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int ioboard_spi_probe        (struct spi_device *spi)
{
	int     ret;
	struct  ioboard_spi     *ioboard_spi;

#if defined(CONFIG_OF)
	struct  device_node *np = spi->dev.of_node;
	
    wp_gpio = of_get_named_gpio(np, "wp-gpio", 0);

	if (!gpio_is_valid(wp_gpio))    wp_gpio = -1;
#endif        

	if(!(ioboard_spi = kzalloc(sizeof(struct ioboard_spi), GFP_KERNEL)))	{
		printk("ioboard-spi struct malloc error!\n");
		return	-ENOMEM;
	}

    ioboard_spi->spi                = spi;
    ioboard_spi->spi->mode          = SPI_MODE_0;
    ioboard_spi->spi->bits_per_word = 8;

	dev_set_drvdata(&spi->dev, ioboard_spi);

    if((ret = spi_setup(spi)) < 0) {
        printk("%s(%s) fail!\n", __func__, SST25WF020_NAME);
        goto    err;
    }
    
    // spi write protection disable
    ioboard_spi_wp_disable(1);

    // read chip id
    if((ret = ioboard_spi_id_read(spi)) < 0)
        goto    err;

    // software protection disable
    if((ret = ioboard_spi_write_status_reg(spi, 0))< 0) 
        goto    err;
    
    if((ret = ioboard_spi_misc_probe(spi)) < 0) {
        printk("%s : misc driver added fail!\n", __func__);
        goto    err;
    }

#if defined(CONFIG_ODROID_EXYNOS5_IOBOARD_DEBUG)    
    ioboard_spi_test(spi);
#endif
    
    printk("\n=================== %s ===================\n\n", __func__);

    return  0;

err:
    printk("\n=================== %s FAIL! ===================\n\n", __func__);

    return  ret;
        
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int ioboard_spi_remove   (struct spi_device *spi)
{
    struct ioboard_spi  *ioboard_spi = dev_get_drvdata(&spi->dev);

    if(wp_gpio)     gpio_free(wp_gpio);

    kfree(ioboard_spi);

	return 0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
#if defined(CONFIG_OF)
static const struct of_device_id ioboard_spi_dt[] = {
	{ .compatible = "ioboard-spi" },
	{ },
};
MODULE_DEVICE_TABLE(of, ioboard_spi_dt);
#endif

static struct spi_driver ioboard_spi_driver = {
	.driver = {
		.name	= SST25WF020_NAME,
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(ioboard_spi_dt),
#endif
	},
	.probe	= ioboard_spi_probe,
	.remove	= ioboard_spi_remove,
};

//[*]--------------------------------------------------------------------------------------------------[*]
/*
 * Module init and exit
 */
//[*]--------------------------------------------------------------------------------------------------[*]
static int __init ioboard_spi_init(void)
{
	return spi_register_driver(&ioboard_spi_driver);
}
module_init(ioboard_spi_init);

//[*]--------------------------------------------------------------------------------------------------[*]
static void __exit ioboard_spi_exit(void)
{
	spi_unregister_driver(&ioboard_spi_driver);
}
module_exit(ioboard_spi_exit);

//[*]--------------------------------------------------------------------------------------------------[*]
MODULE_DESCRIPTION("IOBOARD driver for ODROIDXU-Dev board");
MODULE_AUTHOR("Hard-Kernel");
MODULE_LICENSE("GPL");

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]

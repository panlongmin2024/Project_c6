#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <i2c.h>

#include <device.h>
#include <gpio.h>
#include <soc_regs.h>
#include <pd_manager_supply.h>

#ifdef CONFIG_SYS_LOG
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "tps65992_driver"
#include <logging/sys_log.h>
#endif



#define PP5V_EN_PIN		45 //PD chip PP5V pin, high to enable.
//#define PD_VDD_EN		WIO0 // PD chip 3.3V LDO enable，high to enable.
#define PD_VDD_EN		44
#define CONF_WIO_PIN_PD_EN	1
extern const char tps25750x_lowRegion_i2c_array[];
extern int gSizeLowRegionArray;
extern int pd_tps65992_write_4cc(struct device * dev, char *str);

void pd_tps65992_poweron(void)
{
	
	struct device *gpio_dev;
	

#if HW_3NODS_VERSION_3			//PD_VDD_EN
	// gpio_wio_dev = device_get_binding(CONFIG_GPIO_WIO_ACTS_DEV_NAME);
	// if (gpio_wio_dev == NULL)
	// {
	// 	printk("[%s] gpio_wio device is null\n", __FUNCTION__);
	// 	return;
	// }

	// gpio_pin_configure(gpio_wio_dev, 0, GPIO_DIR_OUT);
	// gpio_pin_write(gpio_wio_dev, 0, 1);


	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (gpio_dev == NULL)
	{
		SYS_LOG_ERR("[%d] PD_VDD_EN device is null\n", __LINE__);
		return;
	}

    gpio_pin_configure(gpio_dev, PD_VDD_EN, GPIO_DIR_OUT | GPIO_POL_NORMAL);
    gpio_pin_write(gpio_dev, PD_VDD_EN, 1);

#else  
	
    u32_t gpio_reg = 0;
	gpio_reg = sys_read32(WIO0_CTL);
	sys_write32(0x13040, WIO0_CTL);

	printk("[%s,%d] 1 sys_read32(WIO0_CTL): 0x%X\n", __FUNCTION__, __LINE__, sys_read32(WIO0_CTL));
	/* wait 10ms */
	k_busy_wait(20000);
	printk("[%s,%d] 2 sys_read32(WIO0_CTL): 0x%X\n", __FUNCTION__, __LINE__, sys_read32(WIO0_CTL));
#endif

	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!gpio_dev) 
	{
		SYS_LOG_ERR("[%d] PD_VDD_EN device is null\n", __LINE__);
		return;
	}
	gpio_pin_configure(gpio_dev, PP5V_EN_PIN, GPIO_DIR_OUT | GPIO_POL_NORMAL);
	gpio_pin_write(gpio_dev, PP5V_EN_PIN, 1);

	/* wait 20ms */
	k_busy_wait(20000);

}

void pd_tps65992_unload(void)
{
	struct device *gpio_dev;

	SYS_LOG_INF("[%d]\n", __LINE__);

#if HW_3NODS_VERSION_3                                  //PD_VDD_EN
	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (gpio_dev == NULL)
	{
		SYS_LOG_ERR("[%d] PD_VDD_EN device is null\n", __LINE__);
		return;
	}

    //gpio_pin_configure(gpio_dev, PD_VDD_EN, GPIO_DIR_IN | GPIO_POL_NORMAL);
    gpio_pin_write(gpio_dev, PD_VDD_EN, 0);

#else
	
    u32_t gpio_reg = 0;

	gpio_reg = sys_read32(WIO0_CTL);
	// sys_write32(0x10140, WIO0_CTL);
	sys_write32(0x10000, WIO0_CTL);


	printk("[%s,%d] 1 sys_read32(WIO0_CTL): 0x%X\n", __FUNCTION__, __LINE__, sys_read32(WIO0_CTL));
	/* wait 10ms */
	k_busy_wait(20000);
	printk("[%s,%d] 2 sys_read32(WIO0_CTL): 0x%X\n", __FUNCTION__, __LINE__, sys_read32(WIO0_CTL));
#endif

	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!gpio_dev) 
	{
		printk("[%s,%d] gpio_dev not found\n", __FUNCTION__, __LINE__);
		return;
	}
	gpio_pin_configure(gpio_dev, PP5V_EN_PIN, GPIO_DIR_OUT | GPIO_POL_NORMAL);
	gpio_pin_write(gpio_dev, PP5V_EN_PIN, 0);

	
}



int pd_tps65992_write_config(void)
{

    struct device *dev;
	union dev_config config = {0};
	u16_t dev_addr = 0x21;
	u16_t num_bytes;
	u16_t reg_addr = 0;
	int i2c_res = 0;
	int timeout_cnt = 10;

	u8_t buf[20] = {0};

	dev = device_get_binding(CONFIG_I2C_0_NAME);
	if (dev == NULL)
	{
		printk("[%s] I2C device is null\n", __FUNCTION__);
		return -1;
	}

	config.bits.speed = I2C_SPEED_FAST;
	i2c_configure(dev, config.raw);

	//1.
	reg_addr = 0x03;
	memset(buf, 0x00, sizeof(buf));
	num_bytes = 5;	
	i2c_burst_read(dev, dev_addr, reg_addr, buf, num_bytes);

	printk("[%s,%d] buf:0x%X 0x%X 0x%X 0x%X 0x%X\n", __FUNCTION__, __LINE__, buf[0], buf[1], buf[2], buf[3], buf[4]);

	// 04 50 54 43 48
	//PTCH
	if (buf[0] == 0x04 && buf[1] == 0x50 && buf[2] == 0x54 && buf[3] == 0x43 && buf[4] == 0x48)
	{
		printk("[%s,%d] ok\n", __FUNCTION__, __LINE__);
	}
	else 
	{
		printk("[%s,%d] error\n", __FUNCTION__, __LINE__);
		
		reg_addr = 0x0f;
		memset(buf, 0x00, sizeof(buf));
		num_bytes = 5;
		
		i2c_res = i2c_burst_read(dev, dev_addr, reg_addr, buf, num_bytes);
		if (i2c_res != 0)
		{
			printk("[%s,%d] i2c res fail\n", __FUNCTION__, __LINE__);
			return -1;
		}
		printk("[%s,%d] buf:0x%X 0x%X 0x%X 0x%X 0x%X\n", __FUNCTION__, __LINE__, buf[0], buf[1], buf[2], buf[3], buf[4]);
		return -2;
	}

	//2.
	reg_addr = 0x09;
	//06 40 15 00 00 30 05
	num_bytes = 7;
	buf[0] = 0x06;
	//fw size
	buf[1] = gSizeLowRegionArray & 0xff;
	buf[2] = ((gSizeLowRegionArray >> 8) & 0xff);
	buf[3] = ((gSizeLowRegionArray >> 16) & 0xff);
	buf[4] = ((gSizeLowRegionArray >> 24) & 0xff);

	buf[5] = 0x30;
	buf[6] = 0x05;
	i2c_burst_write(dev, dev_addr, reg_addr, buf, num_bytes);
	
	//3. PBMs
	reg_addr = 0x08;
	//04 50 42 4D 73
	num_bytes = 5;	
	buf[0] = 0x04;
	buf[1] = 0x50;
	buf[2] = 0x42;
	buf[3] = 0x4d;
	buf[4] = 0x73;
	i2c_burst_write(dev, dev_addr, reg_addr, buf, num_bytes);

	//4.
	reg_addr = 0x08;//cmd
	memset(buf, 0x00, sizeof(buf));
	num_bytes = 5;
	
	timeout_cnt = 10;
	while(1)
	{
		k_sleep(10);
		i2c_burst_read(dev, dev_addr, reg_addr, buf, num_bytes);

		// 04 00 00 00 00

		if (buf[0] == 0x04 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x00 && buf[4] == 0x00)
		{
			printk("[%s,%d] ok\n", __FUNCTION__, __LINE__);
			break;
		}

		if (timeout_cnt-- == 0)
		{
			printk("[%s,%d] error\n", __FUNCTION__, __LINE__);
			return -1;
		}
	}

	//5.
	reg_addr = 0x09;
	memset(buf, 0x00, sizeof(buf));
	num_bytes = 5;
	
	i2c_burst_read(dev, dev_addr, reg_addr, buf, num_bytes);

	// 40 00 00 00 00
	if (buf[0] == 0x40 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x00 && buf[4] == 0x00)
	{
		printk("[%s,%d] ok\n", __FUNCTION__, __LINE__);
	}
	else 
	{
		printk("[%s,%d] error\n", __FUNCTION__, __LINE__);
		return -1;
	}

	//6. download fw
	printk("[%s,%d] download fw start\n", __FUNCTION__, __LINE__);

	dev_addr = 0x30;

	int left_len = gSizeLowRegionArray;
	char *p = (char *)tps25750x_lowRegion_i2c_array;
	int write_cnt = 0;

	while(p < (tps25750x_lowRegion_i2c_array+gSizeLowRegionArray))
	{
		int write_len = 64;
		if (left_len < write_len)
		{
			write_len = left_len;
		}
		left_len -= write_len;

		// i2c_burst_write(dev, dev_addr, reg_addr, p, write_len);
		i2c_res = i2c_write(dev, p, write_len, dev_addr);
		if (i2c_res != 0)
		{
			printk("[%s,%d] write_cnt:%d ; i2c fail\n", __FUNCTION__, __LINE__, write_cnt);
			goto _pd_tps65992_write_err;
		}
		p += write_len;
		write_cnt ++;
		k_busy_wait(250);
		// printk("write_cnt:%d\n", write_cnt);
	}
	printk("[%s,%d] download fw end.\n", __FUNCTION__, __LINE__);

	k_sleep(40);
	//7.
	dev_addr = 0x21;
	reg_addr = 0x08;
	memset(buf, 0x00, sizeof(buf));
	num_bytes = 5;
	//04 50 42 4D 63
	buf[0] = 0x04;
	buf[1] = 0x50;
	buf[2] = 0x42;
	buf[3] = 0x4d;
	buf[4] = 0x63;

	i2c_burst_write(dev, dev_addr, reg_addr, buf, num_bytes);

	//8.
	memset(buf, 0x00, sizeof(buf));
	num_bytes = 5;
	timeout_cnt = 10;

	while(1)
	{
		k_sleep(10);
		i2c_burst_read(dev, dev_addr, reg_addr, buf, num_bytes);

		//04 00 00 00 00
		if (buf[0] == 0x04 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x00 && buf[4] == 0x00)
		{
			printk("[%s,%d] ok\n", __FUNCTION__, __LINE__);
			break;
		}else if(buf[0] == 0x04 && buf[1] == 0x50 && buf[2] == 0x42 && buf[3] == 0x4d && buf[4] == 0x63)
		{
			printk("[%s,%d] wait writing...\n", __FUNCTION__, __LINE__);	
		}else if(buf[0] == 0x04 && buf[1] == 0x21 && buf[2] == 0x43 && buf[3] == 0x4d && buf[4] == 0x44)
		{
			printk("[%s,%d] !CMD\n", __FUNCTION__, __LINE__);	
			goto _pd_tps65992_write_err;
		}

		if (timeout_cnt-- == 0)
		{
			printk("[%s,%d] no response!\n", __FUNCTION__, __LINE__);	
			goto _pd_tps65992_write_err;
		}
		
	}

	k_sleep(20);

	//9.
	reg_addr = 0x09;
	memset(buf, 0x00, sizeof(buf));
	num_bytes = 5;
	
	i2c_burst_read(dev, dev_addr, reg_addr, buf, num_bytes);

	// 40 00 00 00 00
	if (buf[0] == 0x40 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x00 && buf[4] == 0x00)
	{
		printk("[%s,%d] ok\n", __FUNCTION__, __LINE__);
	}
	else 
	{
		printk("[%s,%d] error\n", __FUNCTION__, __LINE__);
		goto _pd_tps65992_write_err;
	}

	//10.
	reg_addr = 0x03;
	num_bytes = 5;

	timeout_cnt = 10;

	while(1)
	{
		k_sleep(10);
		
		i2c_burst_read(dev, dev_addr, reg_addr, buf, num_bytes);

		// 04 50 54 43 48 PTCH
		// 04 41 50 50 20 APP 
		if (buf[0] == 0x04 && buf[1] == 0x41 && buf[2] == 0x50 && buf[3] == 0x50 && buf[4] == 0x20)
		{
			printk("[%s,%d] ok, APP !\n", __FUNCTION__, __LINE__);
			break;
		}
		else 
		{
			if (buf[0] == 0x04 && buf[1] == 0x50 && buf[2] == 0x54 && buf[3] == 0x43 && buf[4] == 0x48)
			{
				printk("[%s,%d] waiting..., in ptch\n", __FUNCTION__, __LINE__);	
			}
			else
			{
				printk("[%s,%d] error, no ptch\n", __FUNCTION__, __LINE__);	
			}

			if (timeout_cnt-- == 0)
			{
				goto _pd_tps65992_write_err;
			}
		}
	}

	return 0;


_pd_tps65992_write_err:

	k_sleep(10);
	//7.
	dev_addr = 0x21;
	reg_addr = 0x08;
	memset(buf, 0x00, sizeof(buf));
	num_bytes = 5;
	//04 50 42 4D 65 " PBMe"
	buf[0] = 0x04;
	buf[1] = 0x50;
	buf[2] = 0x42;
	buf[3] = 0x4d;
	buf[4] = 0x65;

	i2c_burst_write(dev, dev_addr, reg_addr, buf, num_bytes);
	k_sleep(10);

	return -3;;
}



void pd_tps65992_force_enter_ptch(void)
{
	struct device *dev;
	union dev_config config = {0};
	u16_t dev_addr = 0x21;
	u16_t num_bytes;
	u16_t reg_addr = 0;
	u8_t buf[20] = {0};

	dev = device_get_binding(CONFIG_I2C_0_NAME);
	config.bits.speed = I2C_SPEED_FAST;
	i2c_configure(dev, config.raw);

	pd_tps65992_write_4cc(dev, "GAID");   // GO2P
	
	num_bytes = 5;
	dev_addr = 0x21;
	reg_addr = 0x08;
	memset(buf, 0x00, sizeof(buf));

	int timeout_cnt = 0;
	while(1)
	{
		k_sleep(10);
		i2c_burst_read(dev, dev_addr, reg_addr, buf, num_bytes);

		if (buf[0] == 0x04 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x00 && buf[4] == 0x00)
		{
			printk("[%s,%d] ok\n", __FUNCTION__, __LINE__);
			break;
		}
		if(timeout_cnt ++ > 10){
			printk("[%s,%d] Timer out\n", __FUNCTION__, __LINE__);
			break;
		}
	}	

}

#define PD_REBOOT_MAX_COUNT			5

void pd_tps65992_load(void)
{
	int i = 0;

	pd_tps65992_poweron();
	
	for(i=0; i<=PD_REBOOT_MAX_COUNT; i++)
	{
		if(!pd_tps65992_write_config())
		{
			break;
		}

		pd_tps65992_force_enter_ptch();
		k_sleep(2000);
		
		// while(1)
		// {
		// 	printk("[%s,%d] PD config error!\n", __FUNCTION__, __LINE__);
		// 	k_sleep(2000);
		// }
		
	}

	if(i < PD_REBOOT_MAX_COUNT)
    	SYS_LOG_INF("[%d] ok\n", __LINE__);
	else{
		SYS_LOG_INF("[%d] error\n", __LINE__);
	}	
}



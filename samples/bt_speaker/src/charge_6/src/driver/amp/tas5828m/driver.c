#include "tas5828m.h"
#include <zephyr.h>
#include <device.h>
#include <i2c.h>
#include <device.h>
#include <gpio.h>
#include <soc_gpio.h>
#include <soc_regs.h>
#include <os_common_api.h>
#include <thread_timer.h>
#include <wltmcu_manager_supply.h>

#define I2C_DEV_ADDR   0x63 // PCBA
#define PIN_AMP_PDN		(Read_hw_ver() == GGC_EV1_TONLI_EV3 ? 13 : 53)//13 // AMP_PDN, low to shutdown the amp
#define PIN_BOOST_EN	52 //BOOST EN, high to enable.
#define AMP_AVDD_PW_ON		45 //AMP AVDD, high to enable.
static int amp_tas5828m_enable_flag = 0;
static os_mutex amp_tas5828m_mutex;
static os_mutex *amp_tas5828m_mutex_ptr = NULL;

static void amp_tas5828m_i2c_register(struct device *dev, const cfg_reg *r, u32_t n, const char *print_str, u8_t i2c_dev_addr)
{
	u32_t i = 0;
	int ret;
    int err_index;
	int err_cnt = 0;
	int last_time_ms;
	int cur_time_ms;

    printk("[%s,%d] %s, num = %d\n", __FUNCTION__, __LINE__, print_str, n);
    
	last_time_ms = k_uptime_get_32();

	printk("[%s,%d] %s, start\n", __FUNCTION__, __LINE__, print_str);

#if 0
	//dump register data ?
	print_buffer(r, 1, n*sizeof(cfg_reg), 2, 0);
#endif

	while (i < n) {
		ret = 0;
		err_index = i;

		switch (r[i].command)
		{
			case CFG_META_SWITCH:
				// Used in legacy applications.  Ignored here.
				break;

			case CFG_META_DELAY:
				k_busy_wait(r[i].param * 1000);
				break;
			case CFG_META_BURST:
				ret = i2c_write(dev, (uint8_t *)&r[i+1], r[i].param+1, i2c_dev_addr);
				i +=  (r[i].param / 2) + 1;
				break;
			
			default:
				ret = i2c_write(dev, (uint8_t *)&r[i], 2, i2c_dev_addr);
				break;
		}

		if (ret != 0)
		{
			printk("[%s,%d] %s, [%d] error\n", __FUNCTION__, __LINE__, print_str, err_index);
			err_cnt ++;
		}
	
		i++;
	}
	cur_time_ms = k_uptime_get_32();
	printk("[%s,%d] %s, complete, err_cnt:%d, time:%d - %d = %d ms\n", __FUNCTION__, __LINE__, print_str, 
		err_cnt, cur_time_ms, last_time_ms, cur_time_ms - last_time_ms);
}


static bool amp_tas5828m_config_avdd(void)
{
	struct device *i2c_dev = NULL;
	union dev_config config = {0};

	uint8_t buf[10]={0};
	
	if(amp_tas5828m_mutex_ptr == NULL){
		amp_tas5828m_mutex_ptr = &amp_tas5828m_mutex;
		os_mutex_init(amp_tas5828m_mutex_ptr);
	}
	os_mutex_lock(amp_tas5828m_mutex_ptr, OS_FOREVER);
    i2c_dev = device_get_binding(CONFIG_I2C_GPIO_0_NAME);
	if (!i2c_dev) 
	{
		printk("[%s,%d] i2c_dev not found\n", __FUNCTION__, __LINE__);
		goto exit;
	}
	
	config.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(i2c_dev, config.raw);

	// back to page 0 
	buf[0] = 0x0;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);

	// back to book 0 
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7f , buf, 1);

	// enter book0 page0
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);
	
	// enter engineer mode
	buf[0] = 0x11;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7d , buf, 1);

	buf[0] = 0xff;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7e , buf, 1);

	// enter page2
	buf[0] = 0x02;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);	

	k_sleep(1);
	i2c_burst_read(i2c_dev, I2C_DEV_ADDR, 0x08 , buf, 1);	

	k_sleep(1);
	buf[0] = buf[0] | 0x80;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x08 , buf, 1);	

	// enter book0 page0
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);
	
	// back to book 0 
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7f , buf, 1);

	// quit engineer mode
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7d , buf, 1);

	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7e , buf, 1);

exit:
	os_mutex_unlock(amp_tas5828m_mutex_ptr);
	return 0;
}


int amp_tas5828m_registers_init(void)
{
	struct device *i2c_dev = NULL;
	union dev_config config = {0};
	struct device *gpio_dev = NULL;
	
	if(amp_tas5828m_mutex_ptr == NULL){
		amp_tas5828m_mutex_ptr = &amp_tas5828m_mutex;
		os_mutex_init(amp_tas5828m_mutex_ptr);
	}
	os_mutex_lock(amp_tas5828m_mutex_ptr, OS_FOREVER);

	if(amp_tas5828m_enable_flag == 1){
		printk("have enable amp_tas5828m_enable_flag == 1\n");
		goto exit;
	}

	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!gpio_dev) 
	{
		printk("[%s,%d] gpio_dev not found\n", __FUNCTION__, __LINE__);
		goto exit;
	}

	gpio_pin_configure(gpio_dev, PIN_BOOST_EN, GPIO_DIR_OUT | GPIO_POL_NORMAL);
	gpio_pin_write(gpio_dev, PIN_BOOST_EN, 1);

	

	k_busy_wait(100);
	gpio_pin_configure(gpio_dev, PIN_AMP_PDN, GPIO_DIR_OUT | GPIO_POL_NORMAL);
	gpio_pin_write(gpio_dev, PIN_AMP_PDN, 1);

	printk("[%s,%d] GPIO_CTL(PIN_BOOST_EN):0x%X\n", __FUNCTION__, __LINE__, sys_read32(GPIO_CTL(PIN_BOOST_EN)));

    i2c_dev = device_get_binding(CONFIG_I2C_GPIO_0_NAME);
	if (!i2c_dev) 
	{
		printk("[%s,%d] i2c_dev not found\n", __FUNCTION__, __LINE__);
		goto exit;
	}
	
	config.bits.speed = I2C_SPEED_FAST;

	i2c_configure(i2c_dev, config.raw);

    amp_tas5828m_i2c_register(i2c_dev, registers, registers_cnt, "registers", I2C_DEV_ADDR);
	amp_tas5828m_enable_flag = 1;

    printk("[%s,%d] ok\n", __FUNCTION__, __LINE__);
	amp_tas5828m_config_avdd();

	k_sleep(1);
	gpio_pin_configure(gpio_dev, AMP_AVDD_PW_ON, GPIO_DIR_OUT | GPIO_POL_NORMAL);
	gpio_pin_write(gpio_dev, AMP_AVDD_PW_ON, 1);	

	k_busy_wait(100);

	amp_tas5828m_i2c_register(i2c_dev, ti_registers, ti_registers_cnt, "ti_registers", I2C_DEV_ADDR);

	printk("[%s,%d] config avdd ok\n", __FUNCTION__, __LINE__);

exit:	
	os_mutex_unlock(amp_tas5828m_mutex_ptr);
    return 0;
}

/// @brief 
/// @param  block->0 page->0; if you want to change book address, must write page address to 0 first. 
/// @param	00 --> 7f -- 03h, write mute command to 03h first, then write deep sleep mode
/// @return 

int amp_tas5828m_registers_deinit(void)
{

	struct device *i2c_dev = NULL;
	union dev_config config = {0};
	struct device *gpio_dev = NULL;
	uint8_t buf[10]={0};
	
	

	if(amp_tas5828m_mutex_ptr == NULL){
		amp_tas5828m_mutex_ptr = &amp_tas5828m_mutex;
		os_mutex_init(amp_tas5828m_mutex_ptr);
	}
	os_mutex_lock(amp_tas5828m_mutex_ptr, OS_FOREVER);
    i2c_dev = device_get_binding(CONFIG_I2C_GPIO_0_NAME);
	if (!i2c_dev) 
	{
		printk("[%s,%d] i2c_dev not found\n", __FUNCTION__, __LINE__);
		goto exit;
	}
	
	config.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(i2c_dev, config.raw);

	// back to page 0 
	buf[0] = 0x0;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);

	// back to book 0 
	buf[0] = 0x0;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7f , buf, 1);

	// write 03h mute
	buf[0] = 0x0b;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x03 , buf, 1);

	k_sleep(20);
	// write 03h deep sleep mode
	buf[0] = 0x0a;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x03 , buf, 1);


	k_sleep(20);
	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!gpio_dev) 
	{
		printk("[%s,%d] gpio_dev not found\n", __FUNCTION__, __LINE__);
		goto exit;
	}


	gpio_pin_write(gpio_dev, AMP_AVDD_PW_ON, 0);
	k_busy_wait(100);

	gpio_pin_write(gpio_dev, PIN_AMP_PDN, 0);
	k_sleep(10);

	gpio_pin_write(gpio_dev, PIN_BOOST_EN, 0);
	amp_tas5828m_enable_flag = 0;
	printk("amp_tas5828m_registers_deinit ok!\n");
exit:	
	os_mutex_unlock(amp_tas5828m_mutex_ptr);
	return 0;

}

int amp_tas5828m_clear_fault(void)
{

	struct device *i2c_dev = NULL;
	union dev_config config = {0};
	uint8_t buf[10]={0};
	
	if(amp_tas5828m_enable_flag == 0){
		printk("amp_tas5828m_clear_fault not init\n");
		return 0;
	}
	os_mutex_lock(amp_tas5828m_mutex_ptr, OS_FOREVER);
    i2c_dev = device_get_binding(CONFIG_I2C_GPIO_0_NAME);
	if (!i2c_dev) 
	{
		printk("[%s,%d] i2c_dev not found\n", __FUNCTION__, __LINE__);
		goto exit;
	}

	config.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(i2c_dev, config.raw);

	// back to page 0 
	buf[0] = 0x0;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);

	// back to book 0 
	buf[0] = 0x0;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7f , buf, 1);

	// clear fault
	buf[0] = 0x80;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x78 , buf, 1);
exit:	
	os_mutex_unlock(amp_tas5828m_mutex_ptr);
	return 0;
}

int amp_tas5828m_pa_start()
{

	struct device *i2c_dev = NULL;
	union dev_config config = {0};
	uint8_t buf[10]={0};
	
	if(amp_tas5828m_mutex_ptr == NULL){
		amp_tas5828m_mutex_ptr = &amp_tas5828m_mutex;
		os_mutex_init(amp_tas5828m_mutex_ptr);
	}
	os_mutex_lock(amp_tas5828m_mutex_ptr, OS_FOREVER);
    i2c_dev = device_get_binding(CONFIG_I2C_GPIO_0_NAME);
	if (!i2c_dev) 
	{
		printk("[%s,%d] i2c_dev not found\n", __FUNCTION__, __LINE__);
		goto exit;
	}
	
	config.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(i2c_dev, config.raw);

	// back to page 0 
	buf[0] = 0x0;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);

	// back to book 0 
	buf[0] = 0x0;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7f , buf, 1);

	// write 03h play
	buf[0] = 0x03;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x03 , buf, 1);

exit:	
	os_mutex_unlock(amp_tas5828m_mutex_ptr);
	return 0;
}

int amp_tas5828m_pa_stop()
{

	struct device *i2c_dev = NULL;
	union dev_config config = {0};
	uint8_t buf[10]={0};
	
	if(amp_tas5828m_mutex_ptr == NULL){
		amp_tas5828m_mutex_ptr = &amp_tas5828m_mutex;
		os_mutex_init(amp_tas5828m_mutex_ptr);
	}
	os_mutex_lock(amp_tas5828m_mutex_ptr, OS_FOREVER);
    i2c_dev = device_get_binding(CONFIG_I2C_GPIO_0_NAME);
	if (!i2c_dev) 
	{
		printk("[%s,%d] i2c_dev not found\n", __FUNCTION__, __LINE__);
		goto exit;
	}
	
	config.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(i2c_dev, config.raw);

	// back to page 0 
	buf[0] = 0x0;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);

	// back to book 0 
	buf[0] = 0x0;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7f , buf, 1);

	// write 0bh mute
	buf[0] = 0x0b;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x03 , buf, 1);

	k_sleep(2);

	// write 0ah Hiz
	buf[0] = 0x0a;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x03 , buf, 1);

	// k_sleep(2);

exit:	
	os_mutex_unlock(amp_tas5828m_mutex_ptr);
	return 0;
}

/* for factory test */
int amp_tas5828m_pa_select_left_speaker(void)
{
	struct device *i2c_dev = NULL;
	union dev_config config = {0};

	uint8_t buf[10]={0};

	/* first start pa! */
	amp_tas5828m_pa_start();
	
	printk("------>%s  %d\n", __FUNCTION__, __LINE__);
	if(amp_tas5828m_mutex_ptr == NULL){
		amp_tas5828m_mutex_ptr = &amp_tas5828m_mutex;
		os_mutex_init(amp_tas5828m_mutex_ptr);
	}
	os_mutex_lock(amp_tas5828m_mutex_ptr, OS_FOREVER);
    i2c_dev = device_get_binding(CONFIG_I2C_GPIO_0_NAME);
	if (!i2c_dev) 
	{
		printk("[%s,%d] i2c_dev not found\n", __FUNCTION__, __LINE__);
		goto exit;
	}
	
	config.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(i2c_dev, config.raw);

	// back to page 0 
	buf[0] = 0x0;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);

	// enter book 0x8c
	buf[0] = 0x8c;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7f , buf, 1);
	
	// enter page 0x09
	buf[0] = 0x09;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);
	//input mixer left to left = 0 dB
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x74 , buf, 1);
	buf[0] = 0x80;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x75 , buf, 1);	
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x76 , buf, 1);
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x77 , buf, 1);	

	// enter page 0x0a
	buf[0] = 0x0a;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);
	//input mixer right to right = -110 dB
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x08 , buf, 1);
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x09 , buf, 1);	
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x0a , buf, 1);
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x0b , buf, 1);	

exit:
	os_mutex_unlock(amp_tas5828m_mutex_ptr);
	return 0;
}

/* for factory test */
int amp_tas5828m_pa_select_right_speaker(void)
{
	struct device *i2c_dev = NULL;
	union dev_config config = {0};

	uint8_t buf[10]={0};

	/* first start pa! */
	amp_tas5828m_pa_start();
	
	if(amp_tas5828m_mutex_ptr == NULL){
		amp_tas5828m_mutex_ptr = &amp_tas5828m_mutex;
		os_mutex_init(amp_tas5828m_mutex_ptr);
	}
	os_mutex_lock(amp_tas5828m_mutex_ptr, OS_FOREVER);
    i2c_dev = device_get_binding(CONFIG_I2C_GPIO_0_NAME);
	if (!i2c_dev) 
	{
		printk("[%s,%d] i2c_dev not found\n", __FUNCTION__, __LINE__);
		goto exit;
	}
	
	config.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(i2c_dev, config.raw);

	// back to peag 0 
	buf[0] = 0x0;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1); 

	// enter book 0x8c
	buf[0] = 0x8c;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x7f , buf, 1);
	
	// enter page 0x09
	buf[0] = 0x09;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);
	//input mixer left to left = -110 dB
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x74 , buf, 1);
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x75 , buf, 1);	
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x76 , buf, 1);
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x77 , buf, 1);	

	// enter page 0x0a
	buf[0] = 0x0a;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x00 , buf, 1);
	//input mixer right to right = 0 dB
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x08 , buf, 1);
	buf[0] = 0x80;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x89 , buf, 1);	
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x0a , buf, 1);
	buf[0] = 0x00;
	i2c_burst_write(i2c_dev, I2C_DEV_ADDR, 0x0b , buf, 1);	

exit:
	os_mutex_unlock(amp_tas5828m_mutex_ptr);
	return 0;
}



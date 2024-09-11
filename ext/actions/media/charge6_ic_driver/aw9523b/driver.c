
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <init.h>
#include <kernel.h>

#include <thread_timer.h>
#include <i2c.h>

#include <gpio.h>
#include <pinmux.h>
#include <sys_event.h>

#include "power_supply.h"
#include "power_manager.h"

#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#include <msg_manager.h>
#include <soc_pm.h>
#include <mcu_manager_supply.h>
#include "aw9523b_reg.h"
#include "logic_mcu/aw9523b/aw9523b_head.h"
#include <soc_regs.h>

#ifdef CONFIG_SYS_LOG
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "io_expend_aw9523b"
#include <logging/sys_log.h>
#endif
#ifdef CONFIG_LED_MANAGER
#include <led_manager.h>
#endif

#define AW9523B_I2C_ADDR   			0x5B
#define AW9523B_LED_MODE_DRIVER   			1

//#define PIN_IO_EXPEND_RST			6 // AW9523_RST, low to shutdown the amp
#define PIN_IO_EXPEND_RST			44 // AW9523_RST, low to shutdown the amp
//#define PIN_IO_EXPEND_INT			23 // AW9523_INT, input irq
//#define PIN_IO_EXPEND_INT_CFG		(GPIO_DIR_IN | GPIO_PUD_PULL_UP | GPIO_INT | GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW | GPIO_INT_DEBOUNCE)

static struct device *gpio_dev = NULL;
//static struct gpio_callback io_expend_cb_handle;
static struct device *i2c_dev = NULL;



u8_t aw8523b_pin_cfg[AW8523B_PORT_NR] = { 0 };		// 0-output; 1-input
u8_t aw8523b_pin_output_data[AW8523B_PORT_NR] = { 0 };
u8_t aw8523b_pin_input_data[AW8523B_PORT_NR] = { 0 };
u8_t aw8523b_pin_int_enable[AW8523B_PORT_NR] = { 0 };		// 0-enable; 1-disable
u8_t aw8523b_pin_mode_enable[AW8523B_PORT_NR] = { 0 };		// 0-led; 1-gpio


int io_expend_aw9523b_gpio_config(int i2c_transmit_en, int port, int pin, int mode)
{
	aw8523b_pin_cfg[port] &= ~(1 << pin);
	aw8523b_pin_cfg[port] |= (mode << pin);
	if (i2c_transmit_en)
		i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_CONFIG, aw8523b_pin_cfg, 2);
	
	return 0;
}

int io_expend_aw9523b_gpio_output_cache_reg(int port, int pin, int level)
{
	if (level)
		aw8523b_pin_output_data[port] |=  (1 << pin);
	else
		aw8523b_pin_output_data[port] &= ~(1 << pin);
	return 0;
}

int io_expend_aw9523b_gpio_output_update_reg(int port)
{
	i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_OUTPUT + port, aw8523b_pin_output_data, 1);
	return 0;
}

int io_expend_aw9523b_gpio_output(int i2c_transmit_en, int port, int pin, int level)
{
	if (level)
		aw8523b_pin_output_data[port] |=  (1 << pin);
	else
		aw8523b_pin_output_data[port] &= ~(1 << pin);
        
	/*if (level)
        aw8523b_pin_output_data[port] &= ~(1 << pin);
	else
		aw8523b_pin_output_data[port] |=  (1 << pin);        
    */
	if (i2c_transmit_en)
		i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_OUTPUT, aw8523b_pin_output_data, 2);

	return 0;
}

int io_expend_aw9523b_gpio_input(int i2c_transmit_en, int port, int pin)
{
	if (i2c_transmit_en)
		i2c_burst_read(i2c_dev, AW9523B_I2C_ADDR, P0_INPUT, aw8523b_pin_input_data, 2);
	return ((aw8523b_pin_input_data[port] & (1<<pin)) != 0);
}

int io_expend_aw9523b_set_int_enable(int i2c_transmit_en, int port, int pin, int enable)
{
	if (enable)
		aw8523b_pin_int_enable[port] &= ~(1 << pin);
	else
		aw8523b_pin_int_enable[port] |=  (1 << pin);

	if (i2c_transmit_en)
		i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0INT_MSK, aw8523b_pin_int_enable, 2);

	return 0;
}

int io_expend_aw9523b_set_gpio_or_led_mode_enable(int i2c_transmit_en, int port, int pin, int enable)
{
	if (enable)
        aw8523b_pin_mode_enable[port] |=  (1 << pin);
	else
		aw8523b_pin_mode_enable[port] &= ~(1 << pin);		

	if (i2c_transmit_en)
		i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0WKMD, aw8523b_pin_mode_enable, 2);

	return 0;
}


static void io_expend_aw9523b_config_gpio_or_led_mode(void)
{
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_0, AW8523B_MODE_LED);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_1, AW8523B_MODE_LED);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_2, AW8523B_MODE_LED);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_3, AW8523B_MODE_LED);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_4, AW8523B_MODE_LED);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_5, AW8523B_MODE_LED);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_6, AW8523B_MODE_LED);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_7, AW8523B_MODE_LED);

	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_0, AW8523B_MODE_GPIO);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_1, AW8523B_MODE_GPIO);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_2, AW8523B_MODE_GPIO);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_3, AW8523B_MODE_GPIO);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_4, AW8523B_MODE_GPIO);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_5, AW8523B_MODE_GPIO);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_6, AW8523B_MODE_GPIO);
	io_expend_aw9523b_set_gpio_or_led_mode_enable(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_1, AW8523B_PIN_7, AW8523B_MODE_GPIO);

}

void io_expend_aw9523b_set_port_0_led_driver_level(uint8 level)
{
    //uint8_t reg,data;
    printk("\n%s,level:0x%x\n",__func__,level);
    i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_0_DIM4, &level, 1);
    i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_1_DIM5, &level, 1);
    i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_2_DIM6, &level, 1);
    i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_3_DIM7, &level, 1);
    i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_4_DIM8, &level, 1);
    i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_5_DIM9, &level, 1);
    i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_6_DIM10, &level, 1);
    i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_7_DIM11, &level, 1);
    /*
    for(reg = P0_INPUT;reg <= P1INT_MSK;reg++)
    {
        i2c_burst_read(i2c_dev, AW9523B_I2C_ADDR, reg, &data, 1);
        printk("\naw9523b reg:0x%x,value:0x%x\n",reg,data);
    }

    for(reg = CHIP_ID;reg <= P1WKMD;reg++)
    {
        i2c_burst_read(i2c_dev, AW9523B_I2C_ADDR, reg, &data, 1);
        printk("\naw9523b reg:0x%x,value:0x%x\n",reg,data);
    }
    
    for(reg = P1_0_DIM0;reg <= P1_7_DIM15;reg++)
    {
        i2c_burst_read(i2c_dev, AW9523B_I2C_ADDR, reg, &data, 1);
        printk("\naw9523b reg:0x%x,value:0x%x\n",reg,data);
    }
    */
}

int io_expend_aw9523b_led_mode_output(int port, int pin, int level)
{
    uint8_t value;
	if (level)
		value = 0x0;
	else
		value = 0x18;//0x18;

    if(port == AW8523B_PORT_0)  
    {
        printk("pin_%x = 0x%x\n",pin,value);
        i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_0_DIM4 + pin, &value, 1);
    }  
    
	return 0;
}
/*
uint8 i2cWriteOneByte(uint8 SlaveAddress,uint8 reg,uint8 data)
{
	u8_t buf[2];
	buf[0] = reg;
	buf[1] = data;
	return i2c_write(i2c_dev, buf, 2, SlaveAddress);
}
*/
#if 0
static void io_expend_int_cb_func(struct device *port, struct gpio_callback *cb,	u32_t pins)
{
	printk("[%s,%d] io_expend gpio%d int!\n", __FUNCTION__, __LINE__, PIN_IO_EXPEND_INT);
}
#endif

void harman_flip7_aw9523b_test_light_all_led(int on)
{
	if (on)
		aw8523b_pin_output_data[0] = 0;
	else
		aw8523b_pin_output_data[0] = 0xFF;
	i2c_burst_write(i2c_dev, AW9523B_I2C_ADDR, P0_OUTPUT, aw8523b_pin_output_data, 1);
}

void harman_flip7_aw9523b_init(void)
{
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_0, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_1, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_2, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_3, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_4, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_5, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_6, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_7, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_0, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_1, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_2, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_3, AW8523B_MODE_OUTPUT);
	io_expend_aw9523b_gpio_config(AW8523B_I2C_TRANSMIT_ENABLE,  AW8523B_PORT_1, AW8523B_PIN_7, AW8523B_MODE_INPUT);

	io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_0, AW8523B_LEVEL_HIGH);
	io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_1, AW8523B_LEVEL_HIGH);
	io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_2, AW8523B_LEVEL_HIGH);
	io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_3, AW8523B_LEVEL_HIGH);
	io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_4, AW8523B_LEVEL_HIGH);
	io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_5, AW8523B_LEVEL_HIGH);
	io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_6, AW8523B_LEVEL_HIGH);
	io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_0, AW8523B_PIN_7, AW8523B_LEVEL_HIGH);
		
	//io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_DISABLE, AW8523B_PORT_1, AW8523B_PIN_1, AW8523B_LEVEL_LOW);
}

int io_expend_aw9523b_registers_init(void)
{
	u8_t chip_id = 0xff;
	//u32_t gpio_reg = 0;	
	//memset(&io_expend_cb_handle, 0, sizeof(struct gpio_callback));
	
	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!gpio_dev){
		SYS_LOG_ERR("line %d, get dev err\n", __LINE__);
		return -1;
	}
	
	gpio_pin_configure(gpio_dev, PIN_IO_EXPEND_RST, GPIO_DIR_OUT | GPIO_POL_NORMAL);
	gpio_pin_write(gpio_dev, PIN_IO_EXPEND_RST, 0);
	k_sleep(10);
	gpio_pin_write(gpio_dev, PIN_IO_EXPEND_RST, 1);

#if 0 //PD_VDD_EN
	gpio_reg = sys_read32(WIO0_CTL);
	sys_write32(0x13040, WIO0_CTL);

	printk("[%s,%d] 1 sys_read32(WIO0_CTL): 0x%X\n", __FUNCTION__, __LINE__, sys_read32(WIO0_CTL));
	/* wait 10ms */
	//k_busy_wait(20000);
	printk("[%s,%d] 2 sys_read32(WIO0_CTL): 0x%X\n", __FUNCTION__, __LINE__, sys_read32(WIO0_CTL));
#endif
	//gpio_pin_configure(gpio_dev, PIN_IO_EXPEND_INT, PIN_IO_EXPEND_INT_CFG);
	
	//gpio_init_callback(&io_expend_cb_handle, io_expend_int_cb_func, GPIO_BIT(PIN_IO_EXPEND_INT));
	//io_expend_cb_handle.handler = io_expend_int_cb_func;
	//io_expend_cb_handle.pin_group = PIN_IO_EXPEND_INT / 32;
	//io_expend_cb_handle.pin_mask = GPIO_BIT(PIN_IO_EXPEND_INT);
	
	//gpio_add_callback(gpio_dev, &io_expend_cb_handle);
	//gpio_pin_enable_callback(gpio_dev, PIN_IO_EXPEND_INT);

	//k_busy_wait(5000); // delay before i2c communication
	
    i2c_dev = device_get_binding(CONFIG_I2C_0_NAME);
	if (!i2c_dev){
		SYS_LOG_ERR("line %d, get dev err\n", __LINE__);
		return -1;
	}
	
	//ACM86xx_registers_init(16);

	i2c_burst_read(i2c_dev, AW9523B_I2C_ADDR, CHIP_ID, &chip_id, 1); // read aw8523b chip die id
	if(chip_id == 0x23){
		printk("AW8523B chip_id = %d\n", chip_id);
		printk("[%s,%d] ok\n", __FUNCTION__, __LINE__);
	}else{
		SYS_LOG_ERR("line %d, get aw8523b chip_id err\n", __LINE__);
	}

	harman_flip7_aw9523b_init();
    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_1, AW8523B_PIN_1, 0);
    
    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_1, AW8523B_PIN_2, 0);
    /*******************add just led mode driver 2024.8.19 zth************************** */
    #ifdef AW9523B_LED_MODE_DRIVER
    io_expend_aw9523b_config_gpio_or_led_mode();
    io_expend_aw9523b_set_port_0_led_driver_level(0x00);
    #endif
    /***************************************************************************** */

    return 0;
}

void io_expend_aw9523b_ctl_20v5_set(uint8_t onoff)
{
    if(i2c_dev == NULL){
        return;
    }


    SYS_LOG_INF("[%d] on0ff=%d\n", __LINE__, onoff);

    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_1, AW8523B_PIN_1, onoff);

}
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
	u8_t   status;
	u8_t   pre_status;
    bool   pin_state;
    u32_t   very_slow_flash_cnt;
    u32_t   slow_flash_cnt;
    u32_t   regular_flash_cnt;
    u32_t   quick_flash_cnt;  

} led_manage_info_t;


#define VERY_SLOW_FLASH_HIGH_LEVEL_TIME             5*10-8  
#define VERY_SLOW_FLASH_LOW_LEVEL_TIME              40*10-64   
#define SLOW_FLASH_HIGH_LEVEL_TIME                  10*10-15  
#define SLOW_FLASH_LOW_LEVEL_TIME                   20*10-30    
#define REGULAR_FLASH_HIGH_LEVEL_TIME               5*10-8  
#define REGULAR_FLASH_LOW_LEVEL_TIME                10*10-14   
#define GUICK_FLASH_HIGH_LEVEL_TIME                 (25-3)//3*10-4  2024.8.19 hm uis 250ms
#define GUICK_FLASH_LOW_LEVEL_TIME                  (25-3)//3*10-4   

#define MCU_UI_PROCESS_TIME_PERIOD				10//100
#define LED_NUMBLE          (8+1)
static led_manage_info_t led_dev[LED_NUMBLE];
static struct thread_timer aw9523b_ui_timer;

#if 0
void led_status_manger_handle(void)
{
    

	 for(uint8_t i = 0; i < LED_NUMBLE;i++){
        if(led_dev[i].status != led_dev[i].pre_status ){
          led_dev[i].pre_status = led_dev[i].status;
		  if((led_dev[i].pre_status != BT_LED_STATUS_OFF) && (led_dev[i].pre_status != BT_LED_STATUS_ON))
		    {
		       led_dev[i].very_slow_flash_cnt = sys_pm_get_rc_timestamp();
		       led_dev[i].quick_flash_cnt = sys_pm_get_rc_timestamp();
		       led_dev[i].very_slow_flash_cnt = sys_pm_get_rc_timestamp();
		       led_dev[i].regular_flash_cnt = sys_pm_get_rc_timestamp();
		  	}
         }
	  }
	 
    for(uint8_t i = 0; i < LED_NUMBLE;i++){
        if(led_dev[i].status == BT_LED_STATUS_OFF){
            if(led_dev[i].pin_state == true){
                led_dev[i].pin_state = false;
                //mcu_ui_send_led_code(i,false);
                io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, true);
				//printk("[%s/%d],%d off\n\n",__func__,__LINE__,i);
            }
        }
        else if(led_dev[i].status == BT_LED_STATUS_ON){
            if(led_dev[i].pin_state == false){
                led_dev[i].pin_state = true;
                //mcu_ui_send_led_code(i,true);
                io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, false);
				//printk("[%s/%d],%d on\n\n",__func__,__LINE__,i);
            }
        }
        else if(led_dev[i].status == BT_LED_STATUS_VERY_SLOW_FLASH){
            if((sys_pm_get_rc_timestamp()- led_dev[i].very_slow_flash_cnt) < VERY_SLOW_FLASH_HIGH_LEVEL_TIME){
                if(led_dev[i].pin_state == false){
                    led_dev[i].pin_state = true;
                    //mcu_ui_send_led_code(i,true);
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, false);
                }
            }
            else{
                if(led_dev[i].pin_state == true){
                    led_dev[i].pin_state = false;
                    //mcu_ui_send_led_code(i,false);
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, true);
                }               
            }
        }
        else if(led_dev[i].status == BT_LED_STATUS_SLOW_FLASH){
            if((sys_pm_get_rc_timestamp()- led_dev[i].slow_flash_cnt) < SLOW_FLASH_HIGH_LEVEL_TIME){
                if(led_dev[i].pin_state == false){
                    led_dev[i].pin_state = true;
                    //mcu_ui_send_led_code(i,true);
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, false);
                }
            }
            else{
                if(led_dev[i].pin_state == true){
                    led_dev[i].pin_state = false;
                    //mcu_ui_send_led_code(i,false);
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, true);
                }               
            }
        } 
        else if(led_dev[i].status == BT_LED_STATUS_FLASH){
            if((sys_pm_get_rc_timestamp()- led_dev[i].regular_flash_cnt )< REGULAR_FLASH_HIGH_LEVEL_TIME){
                if(led_dev[i].pin_state == false){
                    led_dev[i].pin_state = true;
                    //mcu_ui_send_led_code(i,true);
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, false);
                }
            }
            else{
                if(led_dev[i].pin_state == true){
                    led_dev[i].pin_state = false;
                    //mcu_ui_send_led_code(i,false);
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, true);
                }               
            }
        } 
        else if(led_dev[i].status == BT_LED_STATUS_QUICK_FLASH){
            if((sys_pm_get_rc_timestamp()-led_dev[i].quick_flash_cnt) < GUICK_FLASH_HIGH_LEVEL_TIME){
                if(led_dev[i].pin_state == false){
                    led_dev[i].pin_state = true;
                    //mcu_ui_send_led_code(i,true);
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, false);
				}
            }
            else{
                if(led_dev[i].pin_state == true){
                    led_dev[i].pin_state = false;
                    //mcu_ui_send_led_code(i,false);
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, true);
                }               
            }
        } 
		    //led_dev[i].very_slow_flash_cnt++;
		    if((sys_pm_get_rc_timestamp()-led_dev[i].very_slow_flash_cnt)>= VERY_SLOW_FLASH_HIGH_LEVEL_TIME + VERY_SLOW_FLASH_LOW_LEVEL_TIME){
		        led_dev[i].very_slow_flash_cnt = sys_pm_get_rc_timestamp();
		    }
		   // led_dev[i].slow_flash_cnt++;
		    if((sys_pm_get_rc_timestamp()-led_dev[i].slow_flash_cnt) >= SLOW_FLASH_HIGH_LEVEL_TIME + SLOW_FLASH_LOW_LEVEL_TIME){
		        led_dev[i].slow_flash_cnt = sys_pm_get_rc_timestamp();
		    }
		    //led_dev[i].regular_flash_cnt++;
		    if((sys_pm_get_rc_timestamp()-led_dev[i].regular_flash_cnt)>= REGULAR_FLASH_HIGH_LEVEL_TIME + REGULAR_FLASH_LOW_LEVEL_TIME){
		        led_dev[i].regular_flash_cnt = sys_pm_get_rc_timestamp();
		    }
		    //led_dev[i].quick_flash_cnt++;   
		    if((sys_pm_get_rc_timestamp()-led_dev[i].quick_flash_cnt)>= GUICK_FLASH_HIGH_LEVEL_TIME + GUICK_FLASH_LOW_LEVEL_TIME){
		        led_dev[i].quick_flash_cnt = sys_pm_get_rc_timestamp();
		    } 
    }       
}    
#else
void led_status_manger_handle(void)
{
    

	 for(uint8_t i = 0; i < LED_NUMBLE;i++){
        if(led_dev[i].status != led_dev[i].pre_status ){
          led_dev[i].pre_status = led_dev[i].status;
		  if((led_dev[i].pre_status != BT_LED_STATUS_OFF) && (led_dev[i].pre_status != BT_LED_STATUS_ON))
		    {
		       led_dev[i].very_slow_flash_cnt = 0;
		       led_dev[i].quick_flash_cnt = 0;
		       led_dev[i].very_slow_flash_cnt = 0;
		       led_dev[i].regular_flash_cnt = 0;
		  	}
         }
	  }
	 
    for(uint8_t i = 0; i < LED_NUMBLE;i++){
        if(led_dev[i].status == BT_LED_STATUS_OFF){
            if(led_dev[i].pin_state == true){
                led_dev[i].pin_state = false;
                //mcu_ui_send_led_code(i,false);
                #ifdef AW9523B_LED_MODE_DRIVER
                io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, i, true);
                #else
                io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, true);
                #endif
				//printk("[%s/%d],%d off\n\n",__func__,__LINE__,i);
            }
        }
        else if(led_dev[i].status == BT_LED_STATUS_ON){
            if(led_dev[i].pin_state == false){
                led_dev[i].pin_state = true;
                //mcu_ui_send_led_code(i,true);
                #ifdef AW9523B_LED_MODE_DRIVER
                io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, i, false);
                #else
                io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, false);
                #endif
				//printk("[%s/%d],%d on\n\n",__func__,__LINE__,i);
            }
        }
        else if(led_dev[i].status == BT_LED_STATUS_VERY_SLOW_FLASH){
            if(led_dev[i].very_slow_flash_cnt < VERY_SLOW_FLASH_HIGH_LEVEL_TIME){
                if(led_dev[i].pin_state == false){
                    led_dev[i].pin_state = true;
                    //mcu_ui_send_led_code(i,true);
                    #ifdef AW9523B_LED_MODE_DRIVER
                    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, i, false);
                    #else
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, false);
                    #endif
                }
            }
            else{
                if(led_dev[i].pin_state == true){
                    led_dev[i].pin_state = false;
                    //mcu_ui_send_led_code(i,false);
                    #ifdef AW9523B_LED_MODE_DRIVER
                    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, i, true);
                    #else
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, true);
                    #endif
                }               
            }
        }
        else if(led_dev[i].status == BT_LED_STATUS_SLOW_FLASH){
            if(led_dev[i].slow_flash_cnt < SLOW_FLASH_HIGH_LEVEL_TIME){
                if(led_dev[i].pin_state == false){
                    led_dev[i].pin_state = true;
                    //mcu_ui_send_led_code(i,true);
                    #ifdef AW9523B_LED_MODE_DRIVER
                    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, i, false);
                    #else
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, false);
                    #endif
                }
            }
            else{
                if(led_dev[i].pin_state == true){
                    led_dev[i].pin_state = false;
                    //mcu_ui_send_led_code(i,false);
                    #ifdef AW9523B_LED_MODE_DRIVER
                    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, i, true);
                    #else
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, true);
                    #endif
                }               
            }
        } 
        else if(led_dev[i].status == BT_LED_STATUS_FLASH){
            if(led_dev[i].regular_flash_cnt < REGULAR_FLASH_HIGH_LEVEL_TIME){
                if(led_dev[i].pin_state == false){
                    led_dev[i].pin_state = true;
                    //mcu_ui_send_led_code(i,true);
                    #ifdef AW9523B_LED_MODE_DRIVER
                    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, i, false);
                    #else
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, false);
                    #endif
                }
            }
            else{
                if(led_dev[i].pin_state == true){
                    led_dev[i].pin_state = false;
                    //mcu_ui_send_led_code(i,false);
                    #ifdef AW9523B_LED_MODE_DRIVER
                    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, i, true);
                    #else
                    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, true);
                    #endif
                }               
            }
        } 
        else if(led_dev[i].status == BT_LED_STATUS_QUICK_FLASH){
            if(led_dev[i].quick_flash_cnt < GUICK_FLASH_HIGH_LEVEL_TIME){
                if(led_dev[i].pin_state == false){
                    led_dev[i].pin_state = true;
                    //mcu_ui_send_led_code(i,true);
                    if(i == (LED_NUMBLE - 1))
                    {
                        #ifdef CONFIG_LED_MANAGER
                        led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);
                        #endif
                    }
                    else{
                        #ifdef AW9523B_LED_MODE_DRIVER
                        io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, i, false);
                        #else
                        io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, false);
                        #endif
                    }
				}
            }
            else{
                if(led_dev[i].pin_state == true){
                    led_dev[i].pin_state = false;
                    //mcu_ui_send_led_code(i,false);
                    if(i == (LED_NUMBLE - 1))
                    {
                        #ifdef CONFIG_LED_MANAGER
                        led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);  
                        #endif  
                    }
                    else{
                        #ifdef AW9523B_LED_MODE_DRIVER
                        io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, i, true);
                        #else
                        io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, i, true);
                        #endif
                    }
                }               
            }
        } 
        led_dev[i].very_slow_flash_cnt++;
        if(led_dev[i].very_slow_flash_cnt >= VERY_SLOW_FLASH_HIGH_LEVEL_TIME + VERY_SLOW_FLASH_LOW_LEVEL_TIME){
            led_dev[i].very_slow_flash_cnt = 0;
        }
        led_dev[i].slow_flash_cnt++;
        if(led_dev[i].slow_flash_cnt >= SLOW_FLASH_HIGH_LEVEL_TIME + SLOW_FLASH_LOW_LEVEL_TIME){
            led_dev[i].slow_flash_cnt = 0;
        }
        led_dev[i].regular_flash_cnt++;
        if(led_dev[i].regular_flash_cnt >= REGULAR_FLASH_HIGH_LEVEL_TIME + REGULAR_FLASH_LOW_LEVEL_TIME){
            led_dev[i].regular_flash_cnt = 0;
        }
        led_dev[i].quick_flash_cnt++;   
        if(led_dev[i].quick_flash_cnt >= GUICK_FLASH_HIGH_LEVEL_TIME + GUICK_FLASH_LOW_LEVEL_TIME){
            led_dev[i].quick_flash_cnt = 0;
            /*防止灯闪不一致，同步*/
            for(uint8_t j = 0;j < LED_NUMBLE;j++)
            {
                if(led_dev[j].status != BT_LED_STATUS_QUICK_FLASH)
                {
                    break;
                }
                if(j == (LED_NUMBLE - 1))
                {
                    for(uint8_t n = 0;n < LED_NUMBLE;n++)
                    {
                        led_dev[n].quick_flash_cnt = 0;
                    }
                }
            }
            
        } 

    }       
} 

#endif
static void aw9523b_ui_handle_timer_fn(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	led_status_manger_handle();
}

void aw9523b_set_battery_low_red_led_status(void)
{
    led_dev[0].status = BT_LED_STATUS_ON;
    led_dev[0].pin_state = true;
    #ifdef AW9523B_LED_MODE_DRIVER
    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, 0, false);
    #else
    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, 0, false);
    #endif

    led_dev[1].status = BT_LED_STATUS_OFF;
    led_dev[1].pin_state = false;
    #ifdef AW9523B_LED_MODE_DRIVER
    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, 1, true);
    #else
    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, 1, true);
    #endif

    led_dev[2].status = BT_LED_STATUS_OFF;
    led_dev[2].pin_state = false;
    #ifdef AW9523B_LED_MODE_DRIVER
    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, 2, true);
    #else
    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, 2, true);
    #endif

    led_dev[3].status = BT_LED_STATUS_OFF;
    led_dev[3].pin_state = false;
    #ifdef AW9523B_LED_MODE_DRIVER
    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, 3, true);
    #else
    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, 3, true);
    #endif

    led_dev[4].status = BT_LED_STATUS_OFF;
    led_dev[4].pin_state = false;
    #ifdef AW9523B_LED_MODE_DRIVER
    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, 4, true);
    #else
    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, 4, true);
    #endif

    led_dev[5].status = BT_LED_STATUS_OFF;
    led_dev[5].pin_state = false;
    #ifdef AW9523B_LED_MODE_DRIVER
    io_expend_aw9523b_led_mode_output(AW8523B_PORT_0, 5, true);
    #else
    io_expend_aw9523b_gpio_output(AW8523B_I2C_TRANSMIT_ENABLE, AW8523B_PORT_0, 5, true);
    #endif
}

void aw9523b_set_led_status(uint8_t num,uint8_t status)
{
    led_dev[num].status = status;
    if(status == BT_LED_STATUS_OFF){
        led_dev[num].pin_state = true;
    }
    else if(status == BT_LED_STATUS_ON){
        led_dev[num].pin_state = false;
    }
    if(num == MCU_SUPPLY_PROP_LED_BT)
    {
        SYS_LOG_INF("[%d] status:%d\n", __LINE__, status);
    }

}

uint8_t aw9523b_get_led_status(uint8_t num)
{   

    SYS_LOG_INF("[%d] status:%d\n", __LINE__, led_dev[num].status);
    return  led_dev[num].status;
}

bool  aw9523b_ui_process_init(void)
{
	for(uint8_t i = 0;i < LED_NUMBLE;i++){
		led_dev[i].status = 0;
		led_dev[i].pin_state = 0;

	}

     thread_timer_init(&aw9523b_ui_timer, aw9523b_ui_handle_timer_fn, NULL);
	// thread_timer_start(&aw9523b_ui_timer, MCU_UI_PROCESS_TIME_PERIOD,MCU_UI_PROCESS_TIME_PERIOD);

     return true;
} 

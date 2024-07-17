
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
#include "logic_mcu/ls8a10049t/ls8a10049t.h"
#include "logic_mcu/aw9523b/aw9523b_head.h"
#include "app_defines.h"
#include <app_manager.h>
#include <pd_manager_supply.h>

#include <logging/sys_log.h>
#include <wltmcu_manager_supply.h>
#include <property_manager.h>

#define NTC_STATUS_NORMAL_TEMPERATURE			0
#define NTC_STATUS_OVER_TEMPERATURE				1



#define LS8A10049T_I2C_ADDR    		0x8		// 7bit I2C Addr
//#define PIN_LOGIC_MCU_INT			4 // logic MCU, INT pin
//#define PIN_LOGIC_MCU_INT_CFG		(GPIO_DIR_IN | GPIO_PUD_PULL_UP | GPIO_INT | GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW | GPIO_INT_DEBOUNCE)

#define LS8A10049T_REG_7E			0x7E
#define LS8A10049T_REG_7D			0x7D
#define LS8A10049T_REG_7C			0x7C
#define LS8A10049T_REG_7B			0x7B

typedef struct {
	u8_t bit0 : 1;			// poweron hold
	u8_t bit1 : 1;			// gpio1 output
	u8_t bit2 : 1;			// int_pending 
	u8_t bit3 : 1;			// watchdog
	u8_t bit4 : 1;			// gpio2 output
	u8_t bit5 : 1;
	u8_t bit6 : 1;			// poweroff
	u8_t bit7 : 1;			// poweron_key
} ls8a10049t_reg_7b_t;

typedef struct {
	u8_t bit0 : 1;			// water1_leak
	u8_t bit1 : 1;			// over_temperature
	u8_t bit2 : 1;			// 
	u8_t bit3 : 1;			// 
	u8_t bit4 : 1;			//
	u8_t bit5 : 1;			// USB cable in
	u8_t bit6 : 1;			// power key pressed
	u8_t bit7 : 1;			// water0_leak
} ls8a10049t_reg_7c_t;

static ls8a10049t_reg_7b_t reg_7b = { 0 };
static ls8a10049t_reg_7c_t reg_7c = { 0 };

enum {
	LS8A10049T_INT_EVENT_WATER0_BIT = 0,
	LS8A10049T_INT_EVENT_WATER1_BIT,
	LS8A10049T_INT_EVENT_NTC_BIT,
};

typedef struct {
	uint8_t mcu_int_event;
	bool mcu_int_flag;
}mcu_int_info_t;

int ls8a10049t_int_flag = 0;
bool ls8a10049t_dc_in_flag = 0;
//struct gpio_callback logic_mcu_int_cb_handle;

//static struct wlt_mcu_ls8a10049t_info mcu_mspm0l_ddata;
static struct device g_mcu_ls8a10049t_dev;
static struct device *p_mcu_ls8a10049t_dev = NULL;
extern bool key_water_status_read(void);

static struct logic_mcu_ls8a10049t_device_data_t logic_mcu_ls8a10049t_device_data;
static mcu_int_info_t mcu_int_info = { 0 };
static struct thread_timer ls8a10049t_int_event_timer;
static struct thread_timer ls8a10049t_dc_in_timer;

inline struct logic_mcu_ls8a10049t_device_data_t * logic_mcu_ls8a10049t_get_device_data(void)
{
	return &logic_mcu_ls8a10049t_device_data;
}



/**
 * @brief 开机成功锁电
 * @return: None
 * @details:
 *     Reg Description : reg[POWER_ON_RDY]
 *     Reg Address     : 0x7B bit[0]
 *     Type            : Write
 *     Reg Function    :
 *         开机成功锁电操作寄存器：
 *         A、在按键开机时，POWER_ON 输出高，蓝牙上电后，需在2.1S 时间将该寄存器位先写0
 *             再写1 产生上升沿，以标志蓝牙开机成功，LS8A10049T 如果没有收到该寄存器写入上升沿，
 *             则会进行自动重启，拉低1.5s POWER ON 再拉高，直到检测到该寄存器的上升沿后才保持开机；
 *         B、在插入DC 模式，开机会强制开机，不受该寄存器的影响
 *     Remark          :
 *         需按位操作，建议蓝牙在每次开机时（不区分按键、插入DC 或按照到底座），都对改寄存器写入上升沿标志开机成功
 */
extern void _ls8a10049t_power_hold_test(bool onoff);
static void _ls8a10049t_poweron_hold(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);
    SYS_LOG_INF("i2c_dev = %p\n", dev->i2c_dev);
   
	reg_7b.bit0 = 0;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	
	k_busy_wait(1000);
	reg_7b.bit0 = 1;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
    SYS_LOG_INF("HOLD write done\n");
	_ls8a10049t_power_hold_test(1);

	k_busy_wait(1000);					// otg disable
	reg_7b.bit4 = 0;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	reg_7b.bit4 = 1;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);


}

#ifdef CONFIG_ACTIONS_IMG_LOAD
void ls8a10049t_poweron_hold(void)
{
    //u8_t buf[4] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);	
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
    SYS_LOG_INF("i2c_dev = %p\n", iic_dev);
   
	reg_7b.bit0 = 0;
	i2c_burst_write(iic_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	
	k_busy_wait(1000);
	reg_7b.bit0 = 1;
	i2c_burst_write(iic_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
    SYS_LOG_INF("HOLD write done\n");
}
#endif
/**
 * @brief 关机
 * @return: None
 * @details:
 *     Reg Description : reg[POWER_OFF_RDY]
 *     Reg Address     : 0x7B bit[6]
 *     Type            : Write
 *     Reg Function    :
 *         关机操作寄存器：
 *         在开机状态下，对该位写入下降沿（先写1 再写0），则会进行关机
 *     Remark          :
 *         需按位操作
 */
static void _ls8a10049t_poweroff(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);    
	reg_7b.bit6 = 1;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	reg_7b.bit6 = 0;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
}


/**
 * @brief 清除中断寄存器
 * @param pending_status:  0:清除中断状态;   1:进入中断等待状态
 * @return: None
 * @details:
 *     Reg Description : reg[INT_CLEAR]
 *     Reg Address     : 0x7B bit[2]
 *     Type            : Write
 *     Reg Function    :
 *         中断清除寄存器，写0 清除中断状态，写1 进入中断等待状态
 *     Remark          :
 *         需按位操作
 */
static void _ls8a10049t_clear_int_pending(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);    
	reg_7b.bit2 = 0;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	reg_7b.bit2 = 1;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
}


/**
 * @brief 关机看门狗清除寄存器
 * @return: None
 * @details:
 *     Reg Description : reg[STANDBY_DET]
 *     Reg Address     : 0x7B bit[3]
 *     Type            : Write
 *     Reg Function    :
 *         关机看门狗清除寄存器，在开机状态下，如果再有按下POWER KEY，
 *         此时主控需在10S 内将该寄存器写入一次上升沿（先写0 再写1），
 *         以清除LS8A10049T 内部开门狗定时器。如果LS8A10049T 在10s 内没有收到上升沿操作，
 *         则会在10S 后进行关机，拉低POWER_ON 。
 *         在蓝牙能正常喂狗的情况下，按键长按10s 的响应一致，会进行拉低POWER_ON 强制关机。
 *     Remark          :
 *         需按位操作
 *         可参照LS4V44057 的逻辑，原来Flip6 蓝牙在开机后，会一直给STANDBY_DET 发脉冲进行喂狗
 */
static void _ls8a10049t_watchdog_clear(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);

	reg_7b.bit3 = 0;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	reg_7b.bit3 = 1;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);

}

static void _ls8a10049t_otg_mobile_en(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);

	i2c_burst_read(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);


	reg_7b.bit5 = 1;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	k_sleep(1);
    reg_7b.bit4 = 0;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	k_sleep(1);
	reg_7b.bit4 = 1;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);

	printk("%s:%d; reg 0x7b: 0x%x \n", __func__, __LINE__, *(u8_t *)&reg_7b);

    return;
}



/**
 * @brief GPIO输出
 * @return: None
 * @details:
 *     Reg Description : reg[reg[GPIO_OUT1]]
 *     Reg Address     : 0x7B bit[1]
 *     Type            : Write
 *     Reg Function    :
 *         GPIO_OUT1 控制输出，写‘1’输出高电平，写‘0’输出低电平
 *     Remark          :
 *         需按位操作
 */
static void _ls8a10049t_gpio1_output(struct logic_mcu_ls8a10049t_device_data_t *dev, int output_level)
{
	return;
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);    
	reg_7b.bit1 = (output_level != 0);
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
}


/**
 * @brief GPIO输出
 * @return: None
 * @details:
 *     Reg Description : reg[reg[GPIO_OUT2]]
 *     Reg Address     : 0x7B bit[4]
 *     Type            : Write
 *     Reg Function    :
 *         GPIO_OUT2 控制输出，写‘1’输出高电平，写‘0’输出低电平
 *     Remark          :
 *         需按位操作
 */
static void _ls8a10049t_gpio2_output(struct logic_mcu_ls8a10049t_device_data_t *dev, int output_level)
{
	return;
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);    
	reg_7b.bit4 = (output_level != 0);
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
}








/**
 * @brief 获取 POWER 按键状态
 * @return: 0: Power按键 没有按下;    1: Power按键 有按下
 * @details:
 *     Reg Description : reg[POWER_KEY]
 *     Reg Address     : 0x7C
 *     Type            : Read
 *     Reg Function    :
 *         先将0x7E bit[5~4] 写入‘01’，再将0x7D bit[5~3] 写入‘100’
 *         然后读取0x7C 的bit6 的值，如果读取为1，则说明有按键按下，如果读取为0，则说明按键松开
 *         读取完后，需要对改寄存器进行清除，清除方法：
 *         写0x7B bit[7] 为0，清除后，再写0x7B bit[7] 为1，以进入下次存储准备状态
 *     Remark          :
 *         需按位操作
 */
static int _ls8a10049t_check_power_key_pressed(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    u8_t buffer[4];
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);
    // 先将0x7E bit[5~4] 写入‘01’
    buffer[0] = 1 << 4;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7E, buffer, 1);

    // 再将0x7D bit[5~3] 写入‘100’
    buffer[0] = 0b100 << 3;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7D, buffer, 1);

    // 然后读取0x7C 的bit6 的值，如果读取为1，则说明有按键按下，如果读取为0，则说明按键松开
	i2c_burst_read(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7C, (u8_t *)&reg_7c, 1);
    int power_key_press = reg_7c.bit6;

    // 读取完后，需要对改寄存器进行清除，清除方法：
    // 写0x7B bit[7] 为0，清除后，再写0x7B bit[7] 为1，以进入下次存储准备状态
	reg_7b.bit7 = 0;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);

	reg_7b.bit7 = 1;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);

	int property_read_reboot = property_get_int(CFG_AUTO_USER_ATS_REBOOT,0);
	printf("------> property_read_reboot %d\n",property_read_reboot);
	if(property_read_reboot == 6){
		u8_t buffer[1+1] = "8";
		property_set_factory(CFG_AUTO_USER_ATS_REBOOT,buffer,1);
		power_key_press = 1;
	}

	SYS_LOG_INF("power_key_press = %d\n", power_key_press);

    return power_key_press;
}



/**
 * @brief 获取 USB 插入 状态
 * @return: 0: USB 没有插入;    1: USB 插入
 * @details:
 *     Reg Description : reg[DC_IN]
 *     Reg Address     : 0x7C
 *     Type            : Read
 *     Reg Function    :
 *         先将0x7E bit[5~4] 写入‘01’，再将0x7D bit[5~3] 写入‘001’
 *         然后读取0x7C 的bit5 的值，如果读取为1，则说明有适配器插入，如果读取为0，则说明适配器拔出
 *     Remark          :
 *         需按位操作
 */
static int _ls8a10049t_check_usb_cable_in_status(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    u8_t buffer[4];
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);
    // 先将0x7E bit[5~4] 写入‘01’
    buffer[0] = 1 << 4;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7E, buffer, 1);

    // 再将0x7D bit[5~3] 写入‘001’
    buffer[0] = 0b001 << 3;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7D, buffer, 1);


    // 然后读取0x7C 的bit5 的值，如果读取为1，则说明有适配器插入，如果读取为0，则说明适配器拔出
	i2c_burst_read(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7C, (u8_t *)&reg_7c, 1);
    int usb_cable_in = reg_7c.bit5;
	SYS_LOG_INF("usb_cable_in = %d\n", usb_cable_in);

	return usb_cable_in;
}


/**
 * @brief 获取 Water0 状态
 * @return: 0: 没有漏水;    1: 漏水
 * @details:
 *     Reg Description : reg[WATER0]
 *     Reg Address     : 0x7C
 *     Type            : Read
 *     Reg Function    :
 *         先将0x7E bit[5~4] 写入‘00’，再将0x7D bit[2~0] 写入‘101’
 *         然后读取0x7C 的bit7 的值，如果读取为1，则说明有检测到漏水，如果读取为0，则说明没有漏水发生
 *     Remark          :
 *         需按位操作
 */
static int _ls8a10049t_check_water0_leak(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    u8_t buffer[4];
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);
    // 先将0x7E bit[5~4] 写入‘00’
    buffer[0] = 0 << 4;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7E, buffer, 1);

    // 再将0x7D bit[2~0] 写入‘101’
    //buffer[0] = 0b101 << 3;
	buffer[0] = 0b101;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7D, buffer, 1);


    // 然后读取0x7C 的bit7 的值，如果读取为1，则说明有检测到漏水，如果读取为0，则说明没有漏水发生
	i2c_burst_read(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7C, (u8_t *)&reg_7c, 1);

    int water_leak = reg_7c.bit7;
	SYS_LOG_INF("water0_leak = %d\n", water_leak);
	return water_leak;
}


/**
 * @brief 获取 Water1 状态
 * @return: 0: 没有漏水;    1: 漏水
 * @details:
 *     Reg Description : reg[WATER1]
 *     Reg Address     : 0x7C
 *     Type            : Read
 *     Reg Function    :
 *         先将0x7E bit[5~4] 写入‘00’，再将0x7D bit[2~0] 写入‘110’
 *         然后读取0x7C 的bit0 的值，如果读取为1，则说明有检测到漏水，如果读取为0，则说明没有漏水发生
 *     Remark          :
 *         需按位操作
 */
static int _ls8a10049t_check_water1_leak(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    u8_t buffer[4];
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);
    // 先将0x7E bit[5~4] 写入‘00’
    buffer[0] = 0 << 4;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7E, buffer, 1);

    // 再将0x7D bit[2~0] 写入‘110’
    //buffer[0] = 0b110 << 3;
	buffer[0] = 0b110;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7D, buffer, 1);


    // 然后读取0x7C 的bit0 的值，如果读取为1，则说明有检测到漏水，如果读取为0，则说明没有漏水发生
	i2c_burst_read(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7C, (u8_t *)&reg_7c, 1);

    int water_leak = reg_7c.bit0;
	SYS_LOG_INF("water1_leak = %d\n", water_leak);
	return water_leak;
}



/**
 * @brief 获取 NTC 状态
 * @details:
 *     Reg Description : reg[NTC]
 *     Reg Address     : 0x7C
 *     Type            : Read
 *     Reg Function    :
 *         先将0x7E bit[5~4] 写入‘00’，再将0x7D bit[2~0] 写入‘110’
 *         然后读取0x7C 的bit1 的值，如果读取为0，则说明有检测到过温，如果读取为1，则说明没有过温发生
 *     Remark          :
 *         需按位操作
 */
static int _ls8a10049t_check_ntc_status(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    u8_t buffer[4];
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);
    // 先将0x7E bit[5~4] 写入‘00’
    buffer[0] = 0 << 4;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7E, buffer, 1);

    // 再将0x7D bit[2~0] 写入‘110’
    //buffer[0] = 0b110 << 3;
	buffer[0] = 0b110;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7D, buffer, 1);


    // 然后读取0x7C 的bit1 的值，如果读取为0，则说明有检测到过温，如果读取为1，则说明没有过温发生
	i2c_burst_read(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7C, (u8_t *)&reg_7c, 1);

    int over_temp = reg_7c.bit1;
	SYS_LOG_INF("------> over_temp = %d\n", over_temp);
	if (over_temp)
		return NTC_STATUS_OVER_TEMPERATURE;

	return NTC_STATUS_NORMAL_TEMPERATURE;
}

/////////////////////////////////////new modify/////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief 清除OTG_CTR寄存器
 * @param pending_status:  0:退出OTG检测状态;   1:进入OTG检测等待状态
 * @return: None
 * @details:
 *     Reg Description : reg[OTG_CTR]
 *     Reg Address     : 0x7B bit[5]
 *     Type            : Write
 *     Reg Function    :
 *         中断清除寄存器，写0 OTG检测状态，写1 进入OTG检测等待状态
 *     Remark          :
 *         需按位操作
 */
#ifdef OTG_PHONE_POWER_NEED_SHUTDOWN
static void _ls8a10049t_otg_check_enter_wait(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);    
	reg_7b.bit5 = 0;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	reg_7b.bit5 = 1;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	SYS_LOG_INF("otg ctl\n");
}
#endif
/**
 * @brief 使能OTG寄存器
 * @param pending_status:  0:不使能OTG检测;   1:使能OTG检测
 * @return: None
 * @details:
 *     Reg Description : reg[OTG_EN]
 *     Reg Address     : 0x7B bit[4]
 *     Type            : Write
 *     Reg Function    :
 *         使能OTG寄存器，先写0再写1使能OTG检测
 *     Remark          :
 *         需按位操作
 */
static void _ls8a10049t_otg_check_enable(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);    
	reg_7b.bit4 = 0;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	reg_7b.bit4 = 1;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	SYS_LOG_INF("otg enable\n");
}


/**
 * @brief 获取 POWER 长按10s RESET按键状态
 * @return: 0: Power长按键 没有按下;    1: Power长按键 有按下
 * @details:
 *     Reg Description : reg[POWER_KEY_10S]
 *     Reg Address     : 0x7C
 *     Type            : Read
 *     Reg Function    :
 *         先将0x7E bit[5~4] 写入‘01’，再将0x7D bit[5~3] 写入‘101’
 *         然后读取0x7C 的bit4 的值，如果读取为1，则说明有按键按下，如果读取为0，则说明按键松开
 *         读取完后，需要对改寄存器进行清除，清除方法：
 *         写0x7B bit[1] 为0，清除后，再写0x7B bit[1] 为1，以进入下次存储准备状态
 *     Remark          :
 *         需按位操作
 */
static int _ls8a10049t_check_powerkey_10s_pressed(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
    u8_t buffer[4];
    union dev_config config = {0};
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(dev->i2c_dev, config.raw);
    // 先将0x7E bit[5~4] 写入‘01’
    buffer[0] = 1 << 4;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7E, buffer, 1);

    // 再将0x7D bit[5~3] 写入‘101’
    buffer[0] = 0b101 << 3;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7D, buffer, 1);

    // 然后读取0x7C 的bit4 的值，如果读取为1，则说明有按键按下，如果读取为0，则说明按键松开
	i2c_burst_read(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7C, (u8_t *)&reg_7c, 1);
    int powerkey_10s_press = reg_7c.bit4;

    // 读取完后，需要对改寄存器进行清除，清除方法：
    // 写0x7B bit[1] 为0，清除后，再写0x7B bit[1] 为1，以进入下次存储准备状态
	reg_7b.bit1 = 0;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);

	reg_7b.bit1 = 1;
	i2c_burst_write(dev->i2c_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);

	SYS_LOG_INF("powerkey_10s_press = %d\n", powerkey_10s_press);

    return powerkey_10s_press;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

static int _logic_mcu_ls8a10049t_int_handle(struct logic_mcu_ls8a10049t_device_data_t *dev)
{
	mcu_manager_charge_event_para_t para;

	SYS_LOG_INF("logic_mcu_int_handle\n");
	do {
		int power_key_pressed = _ls8a10049t_check_power_key_pressed(dev);
		if (power_key_pressed) {
			SYS_LOG_INF("power_key_pressed\n");
			//_ls8a10049t_poweroff(dev);
			//_ls8a10049t_poweroff(dev);
			//_ls8a10049t_poweroff(dev);
			//sys_pm_poweroff();
			if(pd_get_app_mode_state() == CHARGING_APP_MODE){
				para.mcu_event_val = MCU_INT_CMD_POWERON;	
			}
			else{
				para.mcu_event_val = MCU_INT_CMD_POWEROFF;	
			}
            if (dev->mcu_notify) {
                dev->mcu_notify(MCU_INT_TYPE_POWER_KEY, &para);
            }
            else{
                printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
            }			
		}
		else
		{
			int powerkey_10s_pressed = _ls8a10049t_check_powerkey_10s_pressed(dev);
			if(powerkey_10s_pressed)
			{
				SYS_LOG_INF("powerkey_10s_pressed\n");
				para.mcu_event_val = MCU_INT_CMD_POWERON;

				if (dev->mcu_notify) {
					dev->mcu_notify(MCU_INT_TYPE_HW_RESET, &para);
				}
				else{
					printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
				}
			}
		}
		//break;

		int usb_cable_in = _ls8a10049t_check_usb_cable_in_status(dev);
		if (usb_cable_in) {
			SYS_LOG_INF("usb_cable_in\n");
			ls8a10049t_dc_in_flag = 1;
            para.mcu_event_val = MCU_INT_CMD_DC_IN;

            if (dev->mcu_notify) {
                dev->mcu_notify(MCU_INT_TYPE_DC, &para);
            }
            else{
                printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
            } 			
			// thread_timer_start(&ls8a10049t_dc_in_timer,200,200);    // modify by Totti on 6/19/2024
		}

		int water0_leak = _ls8a10049t_check_water0_leak(dev);
		if (water0_leak) {
			SYS_LOG_INF("water0_leak\n");
			//battery_data.usb_port_flag.water0_leak = 1;
		}

		int water1_leak = _ls8a10049t_check_water1_leak(dev);
		if (water1_leak) {
			SYS_LOG_INF("water1_leak\n");
			//battery_data.usb_port_flag.water1_leak = 1;
		}

		int over_temperature = _ls8a10049t_check_ntc_status(dev);
		if (over_temperature) {
			SYS_LOG_INF("ntc over_temperature\n");
			//battery_data.usb_port_flag.over_temperature = 1;
		}
	} while (0);

	// clear int pending bit
	_ls8a10049t_clear_int_pending(dev);

	return 0;
}

/*

static void logic_mcu_int_cbk(struct device *port, struct gpio_callback *cb, u32_t pins)
{
	SYS_LOG_INF("logic_mcu_int_cbk\n");

#if 0
	struct app_msg msg = {0};
	msg.type = MSG_LOGIC_MCU_LS8A10049T_INT;
	msg.ptr = NULL;

	if (!send_async_msg("main", &msg))
	{
	}
#endif

	ls8a10049t_int_flag = 1;

}
*/


void logic_mcu_ls8a10049t_int_clear(void)
{
	SYS_LOG_INF("");
	os_sched_lock();

	struct logic_mcu_ls8a10049t_device_data_t *dev = NULL;
	dev = logic_mcu_ls8a10049t_get_device_data();

	// clear int pending bit
	_ls8a10049t_clear_int_pending(dev);

	os_sched_unlock();
}

void logic_mcu_ls8a10049t_power_off(void)
{
	SYS_LOG_INF("");
	
	struct logic_mcu_ls8a10049t_device_data_t *dev = NULL;
	dev = logic_mcu_ls8a10049t_get_device_data();
	_ls8a10049t_poweroff(dev);
}





static void clear_no_use_warrings(void)
{
	void * a = NULL;
	a = _ls8a10049t_poweroff;
	a = _ls8a10049t_watchdog_clear;
	a = _ls8a10049t_gpio1_output;
	a = _ls8a10049t_gpio2_output;
}

static void bt_pd_send_update_fw_code(u8_t value)
{
    struct logic_mcu_ls8a10049t_device_data_t *ls8a10049t = p_mcu_ls8a10049t_dev->driver_data;
   
    SYS_LOG_INF("[%d],value=%d\n", __LINE__, value);
    ls8a10049t->mcu_update_fw = value;
}

static void bt_mcu_enable_bt_led_code(bool value)
{

    struct logic_mcu_ls8a10049t_device_data_t *ls8a10049t = p_mcu_ls8a10049t_dev->driver_data;
    
    SYS_LOG_INF("[%d],value=%d\n", __LINE__, value);
    ls8a10049t->enable_bt_led = value;
}

static void bt_mcu_enable_pty_boost_led_code(bool value)
{

    struct logic_mcu_ls8a10049t_device_data_t *ls8a10049t = p_mcu_ls8a10049t_dev->driver_data;
    
    SYS_LOG_INF("[%d],value=%d\n", __LINE__, value);
    ls8a10049t->enable_pty_boost_led = value;
}

static void bt_mcu_enable_bat_led_code(bool value)
{

    struct logic_mcu_ls8a10049t_device_data_t *ls8a10049t = p_mcu_ls8a10049t_dev->driver_data;
    
    SYS_LOG_INF("[%d],value=%d\n", __LINE__, value);
    ls8a10049t->enable_bat_led = value;
}

static bool bt_mcu_get_enable_bt_led_code(void)
{

    struct logic_mcu_ls8a10049t_device_data_t *ls8a10049t = p_mcu_ls8a10049t_dev->driver_data;
   
    return ls8a10049t->enable_bt_led;
}

static bool bt_mcu_get_enable_pty_boost_led_code(void)
{

    struct logic_mcu_ls8a10049t_device_data_t *ls8a10049t = p_mcu_ls8a10049t_dev->driver_data;
    
    return ls8a10049t->enable_pty_boost_led;
}

static bool bt_mcu_get_enable_bat_led_code(void)
{

    struct logic_mcu_ls8a10049t_device_data_t *ls8a10049t = p_mcu_ls8a10049t_dev->driver_data;
	//printk("\n%s, enable_bat_led:%d \n",ls8a10049t->enable_bat_led);
    return ls8a10049t->enable_bat_led;
}

static int mcu_ls8a10049t_wlt_get_property(struct device *dev,enum mcu_manager_supply_property psp,     \
				       union mcu_manager_supply_propval *val)
{
    struct logic_mcu_ls8a10049t_device_data_t *ls8a10049t = p_mcu_ls8a10049t_dev->driver_data;
    switch(psp)
    {
        case MCU_SUPPLY_PROP_LED_0:
            break;

        case MCU_SUPPLY_PROP_LED_1:
			val->intval = aw9523b_get_led_status(MCU_SUPPLY_PROP_LED_1);
            break;
        case MCU_SUPPLY_PROP_LED_2:
            break; 

        case MCU_SUPPLY_PROP_LED_3:
            break; 

        case MCU_SUPPLY_PROP_LED_4:
            break;

        case MCU_SUPPLY_PROP_LED_5:
            break;

        case MCU_SUPPLY_PROP_LED_BT:
            break;

        case MCU_SUPPLY_PROP_LED_PTY_BOOST:
            break;

        case MCU_SUPPLY_PROP_UPDATE_FW:
            val->intval = ls8a10049t->mcu_update_fw;
            break;
        case MCU_SUPPLY_PROP_RESET_MCU:
            break;  

         case MCU_SUPPLY_PROP_ENABLE_LED_BT:
            val->intval = bt_mcu_get_enable_bt_led_code();
            break;

        case MCU_SUPPLY_PROP_ENABLE_LED_PTY_BOOST:
            val->intval = bt_mcu_get_enable_pty_boost_led_code();
            break; 	

        case MCU_SUPPLY_PROP_ENABLE_LED_BAT:
            val->intval = bt_mcu_get_enable_bat_led_code();
            break; 					  
        default:
            break;               
    }

    return 0;
}

static void mcu_ls8a10049t_wlt_set_property(struct device *dev,enum mcu_manager_supply_property psp,    \
				       union mcu_manager_supply_propval *val)
{
	struct logic_mcu_ls8a10049t_device_data_t *ls8a10049t = p_mcu_ls8a10049t_dev->driver_data;
    int value = val->intval;
    switch(psp)
    {
        case MCU_SUPPLY_PROP_LED_0:
			if(!bt_mcu_get_enable_bat_led_code())
			aw9523b_set_led_status(MCU_SUPPLY_PROP_LED_0,value);
            break;

        case MCU_SUPPLY_PROP_LED_1:	
			if(!bt_mcu_get_enable_bat_led_code())	
			aw9523b_set_led_status(MCU_SUPPLY_PROP_LED_1,value);			
            break;
        case MCU_SUPPLY_PROP_LED_2:	
			if(!bt_mcu_get_enable_bat_led_code())
			aw9523b_set_led_status(MCU_SUPPLY_PROP_LED_2,value);		
            break; 

        case MCU_SUPPLY_PROP_LED_3:	
			if(!bt_mcu_get_enable_bat_led_code())
			aw9523b_set_led_status(MCU_SUPPLY_PROP_LED_3,value);	
            break; 

        case MCU_SUPPLY_PROP_LED_4:	
			if(!bt_mcu_get_enable_bat_led_code())
			aw9523b_set_led_status(MCU_SUPPLY_PROP_LED_4,value);		
            break;

        case MCU_SUPPLY_PROP_LED_5:	
			if(!bt_mcu_get_enable_bat_led_code())
			aw9523b_set_led_status(MCU_SUPPLY_PROP_LED_5,value);		
            break;

        case MCU_SUPPLY_PROP_LED_BT:
			if(!bt_mcu_get_enable_bt_led_code())	
			aw9523b_set_led_status(MCU_SUPPLY_PROP_LED_BT,value);		
            break;

        case MCU_SUPPLY_PROP_LED_PTY_BOOST:	
			if(!bt_mcu_get_enable_pty_boost_led_code())
			aw9523b_set_led_status(MCU_SUPPLY_PROP_LED_PTY_BOOST,value);		
            break;

		case MCU_SUPPLY_PROP_LED_POWER:
			aw9523b_set_led_status(MCU_SUPPLY_PROP_LED_POWER,value);
			break;	
		case MCU_SUPPLY_PROP_DSP_DCDC:	
			io_expend_aw9523b_ctl_20v5_set(value);
		break;
        case MCU_SUPPLY_PROP_UPDATE_FW:
            bt_pd_send_update_fw_code(value);
            break;
        case MCU_SUPPLY_PROP_RESET_MCU:
            
            break; 
        case MCU_SUPPLY_PROP_ENABLE_LED_BT:
            bt_mcu_enable_bt_led_code((bool)value);
            break;

        case MCU_SUPPLY_PROP_ENABLE_LED_PTY_BOOST:
            bt_mcu_enable_pty_boost_led_code((bool)value);
            break;

        case MCU_SUPPLY_PROP_ENABLE_LED_BAT:
            bt_mcu_enable_bat_led_code((bool)value);
            break; 

		case MCU_SUPPLY_PROP_RED_LED_FLASH:
			aw9523b_set_battery_low_red_led_status();
			break;

		case MCU_SUPPLY_PROP_REAL_SHUTDOWN_DUT:
			_ls8a10049t_otg_check_enable(ls8a10049t);
            break;

        default:
            break;               
    }

    
}

static void mcu_ls8a10049t_wlt_register_notify(struct device *dev, mcu_callback_t cb)
{
    struct logic_mcu_ls8a10049t_device_data_t *ls8a10049t = p_mcu_ls8a10049t_dev->driver_data;
	
	if ((ls8a10049t->mcu_notify == NULL) && cb)
	{
		ls8a10049t->mcu_notify = cb;
	}else
	{
		SYS_LOG_ERR("mcu notify func already exist!\n");
	}
}

static void mcu_ls8a10049t_wlt_response(struct device *dev,enum mcu_manager_supply_property psp,    \
				       union mcu_manager_supply_propval *val)
{
   struct logic_mcu_ls8a10049t_device_data_t *ls8a10049t = p_mcu_ls8a10049t_dev->driver_data;
   int value = val->intval;
    switch(psp)
    {
        case MCU_INT_TYPE_POWER_KEY:
            //bt_mcu_send_iic_cmd_code(BT_MCU_ACK_IIC_ADDR, MCU_INT_TYPE_POWER_KEY, val->intval);
			switch(value)
			{
				case MCU_INT_CMD_POWERON:
				case MCU_INT_CMD_STANDBY:
				case MCU_INT_CMD_CHARGING:
					_ls8a10049t_watchdog_clear(ls8a10049t);
				break;
				case MCU_INT_CMD_POWEROFF:
					logic_mcu_ls8a10049t_power_off();
				break;

				case MCU_INT_CMD_OTG_MOBILE_ON:
					_ls8a10049t_otg_mobile_en(ls8a10049t);		
					break;

				default:
				break;
			}
            break;


        case MCU_INT_TYPE_DC:
            //bt_mcu_send_iic_cmd_code(BT_MCU_ACK_IIC_ADDR, MCU_INT_TYPE_DC, val->intval);
            break;
        
        default:

            break;
    }
}


static const struct mcu_manager_supply_driver_api mcu_ls8a10049t_wlt_driver_api = {
	.get_property = mcu_ls8a10049t_wlt_get_property,
	.set_property = mcu_ls8a10049t_wlt_set_property,
    .register_mcu_notify = mcu_ls8a10049t_wlt_register_notify,
    .mcu_response = mcu_ls8a10049t_wlt_response,
};

#if 1
uint8_t pd_get_app_mode_state(void);

static void logic_mcu_ls8a10049t_set_int_event(uint8_t event)
{
	mcu_int_info.mcu_int_event |= (0x01 << event);
}

static void logic_mcu_ls8a10049t_clear_int_event(uint8_t event)
{
	mcu_int_info.mcu_int_event &= (~(0x01 << event));
}

static uint8_t logic_mcu_ls8a10049t_get_int_event(void)
{
	return mcu_int_info.mcu_int_event;
}

static bool logic_mcu_ls8a10049t_is_int_event(uint8_t event)
{
	bool ret = false;
	if((mcu_int_info.mcu_int_event & (0x01 << event)) == (0x01 << event)){
		ret = true;
	}
	return ret;
}

static void logic_mcu_ls8a10049t_int_timer_fn(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	uint8_t event = 0;	
    struct logic_mcu_ls8a10049t_device_data_t *dev = NULL;
    mcu_manager_charge_event_para_t para;
	uint8_t mcu_water_flag = 0;
    dev = logic_mcu_ls8a10049t_get_device_data();

	 printk("ZTH DEBUD: %s:%d\n", __func__, __LINE__);
	 event = logic_mcu_ls8a10049t_get_int_event();
	 //if((event & (0x01 << LS8A10049T_INT_EVENT_WATER0_BIT)) == (0x01 << LS8A10049T_INT_EVENT_WATER0_BIT)){
	 if(logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_WATER0_BIT)
	 || logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_WATER1_BIT)){
		if(logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_WATER0_BIT)){
			int water0_leak = _ls8a10049t_check_water0_leak(dev);
			if (!water0_leak) {
				SYS_LOG_INF("water0_leak OUT\n");
				mcu_water_flag &= (~(0x01 << LS8A10049T_INT_EVENT_WATER0_BIT));			
				logic_mcu_ls8a10049t_clear_int_event(LS8A10049T_INT_EVENT_WATER0_BIT);

			}
			else{
				SYS_LOG_INF("water0_leak in\n");
				mcu_water_flag |= (0x01 << LS8A10049T_INT_EVENT_WATER0_BIT);	
			}
	
		}

		if(logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_WATER1_BIT)){	
			int water1_leak = _ls8a10049t_check_water1_leak(dev);
			if (!water1_leak) {
				SYS_LOG_INF("water1_leak OUT\n");
				mcu_water_flag &= (~(0x01 << LS8A10049T_INT_EVENT_WATER1_BIT));
				logic_mcu_ls8a10049t_clear_int_event(LS8A10049T_INT_EVENT_WATER1_BIT);
			}
			else{
				SYS_LOG_INF("water1_leak in\n");
				mcu_water_flag |= (0x01 << LS8A10049T_INT_EVENT_WATER1_BIT);
			}				 		
		}

		if(mcu_water_flag){
			para.mcu_event_val = MCU_INT_CMD_WATER_IN;
		}
		else{
			para.mcu_event_val = MCU_INT_CMD_WATER_OUT;
		}

		if (dev->mcu_notify) {
			dev->mcu_notify(MCU_INT_TYPE_WATER, &para);
		}
		else{
			printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
		}					
	 }

	 //if((event & (0x01 << LS8A10049T_INT_EVENT_NTC_BIT)) == (0x01 << LS8A10049T_INT_EVENT_NTC_BIT)){
	 if(logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_NTC_BIT)){	
		int over_temperature = _ls8a10049t_check_ntc_status(dev);
		if (over_temperature) {
			SYS_LOG_INF("normal_temperature\n");
            para.mcu_event_val = MCU_INT_CMD_TEMPERATURE_NORMAL;

			logic_mcu_ls8a10049t_clear_int_event(LS8A10049T_INT_EVENT_NTC_BIT);
		}
		else{
			SYS_LOG_INF("OVER_temperature\n");
            para.mcu_event_val = MCU_INT_CMD_TEMPERATURE_HIGH;
		}

		if (dev->mcu_notify) {
			dev->mcu_notify(MCU_INT_TYPE_NTC, &para);
		}
		else{
			printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
		}				
	 }

	 if(logic_mcu_ls8a10049t_get_int_event() == 0){
		mcu_int_info.mcu_int_flag = 0;
		thread_timer_stop(&ls8a10049t_int_event_timer);
	 }
     
}

void wlt_logic_mcu_ls8a10049t_int_fn(void)
{
	uint8_t event = 0;	
    struct logic_mcu_ls8a10049t_device_data_t *dev = NULL;
    mcu_manager_charge_event_para_t para;
	uint8_t mcu_water_flag = 0;

	if(logic_mcu_ls8a10049t_get_int_event() == 0){

		return;
	}
	k_sleep(1);
    dev = logic_mcu_ls8a10049t_get_device_data();

	printk("ZTH DEBUD: %s:%d\n", __func__, __LINE__);
	event = logic_mcu_ls8a10049t_get_int_event();
	 //if((event & (0x01 << LS8A10049T_INT_EVENT_WATER0_BIT)) == (0x01 << LS8A10049T_INT_EVENT_WATER0_BIT)){
	 if(logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_WATER0_BIT)
	 || logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_WATER1_BIT)){
		if(logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_WATER0_BIT)){
			int water0_leak = _ls8a10049t_check_water0_leak(dev);
			if (!water0_leak) {
				SYS_LOG_INF("water0_leak OUT\n");
				mcu_water_flag &= (~(0x01 << LS8A10049T_INT_EVENT_WATER0_BIT));			
				logic_mcu_ls8a10049t_clear_int_event(LS8A10049T_INT_EVENT_WATER0_BIT);

			}
			else{
				SYS_LOG_INF("water0_leak in\n");
				mcu_water_flag |= (0x01 << LS8A10049T_INT_EVENT_WATER0_BIT);	
			}
	
		}

		if(logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_WATER1_BIT)){	
			int water1_leak = _ls8a10049t_check_water1_leak(dev);
			if (!water1_leak) {
				SYS_LOG_INF("water1_leak OUT\n");
				mcu_water_flag &= (~(0x01 << LS8A10049T_INT_EVENT_WATER1_BIT));
				logic_mcu_ls8a10049t_clear_int_event(LS8A10049T_INT_EVENT_WATER1_BIT);
			}
			else{
				SYS_LOG_INF("water1_leak in\n");
				mcu_water_flag |= (0x01 << LS8A10049T_INT_EVENT_WATER1_BIT);
			}				 		
		}

		if(mcu_water_flag){
			para.mcu_event_val = MCU_INT_CMD_WATER_IN;
		}
		else{
			para.mcu_event_val = MCU_INT_CMD_WATER_OUT;
		}

		if (dev->mcu_notify) {
			dev->mcu_notify(MCU_INT_TYPE_WATER, &para);
		}
		else{
			printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
		}					
	 }

	 //if((event & (0x01 << LS8A10049T_INT_EVENT_NTC_BIT)) == (0x01 << LS8A10049T_INT_EVENT_NTC_BIT)){
	 if(logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_NTC_BIT)){	
		int over_temperature = _ls8a10049t_check_ntc_status(dev);
		if (over_temperature) {
			SYS_LOG_INF("normal_temperature\n");
            para.mcu_event_val = MCU_INT_CMD_TEMPERATURE_NORMAL;

			logic_mcu_ls8a10049t_clear_int_event(LS8A10049T_INT_EVENT_NTC_BIT);
		}
		else{
			SYS_LOG_INF("OVER_temperature\n");
            para.mcu_event_val = MCU_INT_CMD_TEMPERATURE_HIGH;
		}

		if (dev->mcu_notify) {
			dev->mcu_notify(MCU_INT_TYPE_NTC, &para);
		}
		else{
			printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
		}				
	 }


     
}


static int mcu_ls8a10049t_input_event_report(void)
{
    struct logic_mcu_ls8a10049t_device_data_t *dev = NULL;
    mcu_manager_charge_event_para_t para;
    dev = logic_mcu_ls8a10049t_get_device_data();
	SYS_LOG_INF("logic_mcu_int_handle\n");

	do {
		int power_key_pressed = _ls8a10049t_check_power_key_pressed(dev);
		if (power_key_pressed) {
			
			//void *fg_app = app_manager_get_current_app();
			printk("[%s:%d] current_app : %d!\n", __func__, __LINE__,pd_get_app_mode_state());
			//if (fg_app && !strcmp(fg_app, APP_ID_CHARGE_APP_NAME)) {
			if(pd_get_app_mode_state() == CHARGING_APP_MODE){
				para.mcu_event_val = MCU_INT_CMD_POWERON;	
			}
			else{
				para.mcu_event_val = MCU_INT_CMD_POWEROFF;	
			}
            SYS_LOG_INF("power_key_pressed\n");
            if (dev->mcu_notify) {
				/* plm add: exit wlt test mode! */
				////start
				int ats_module_test_mode_write(uint8_t *buf, int size);
				char buffer[2] = "8";
				ats_module_test_mode_write(buffer,sizeof(buffer)-1);
				////end
                dev->mcu_notify(MCU_INT_TYPE_POWER_KEY, &para);
            }
            else{
                printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
            }
		}

		int usb_cable_in = _ls8a10049t_check_usb_cable_in_status(dev);
		if (usb_cable_in) {
			SYS_LOG_INF("usb_cable_in\n");
            para.mcu_event_val = MCU_INT_CMD_DC_IN;

            if (dev->mcu_notify) {
                dev->mcu_notify(MCU_INT_TYPE_DC, &para);
            }
            else{
                printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
            }            
		}
		if(!logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_WATER0_BIT)){
			int water0_leak = _ls8a10049t_check_water0_leak(dev);
			if (water0_leak) {
				SYS_LOG_INF("water0_leak\n");
				para.mcu_event_val = MCU_INT_CMD_WATER_IN;

				if (dev->mcu_notify) {
					dev->mcu_notify(MCU_INT_TYPE_WATER, &para);
				}
				else{
					printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
				}
				logic_mcu_ls8a10049t_set_int_event(LS8A10049T_INT_EVENT_WATER0_BIT);
			}
		}

		if(!logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_WATER1_BIT)){
			int water1_leak = _ls8a10049t_check_water1_leak(dev);
			if (water1_leak) {
				SYS_LOG_INF("water1_leak\n");
				para.mcu_event_val = MCU_INT_CMD_WATER_IN;

				if (dev->mcu_notify) {
					dev->mcu_notify(MCU_INT_TYPE_WATER, &para);
				}
				else{
					printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
				}
				logic_mcu_ls8a10049t_set_int_event(LS8A10049T_INT_EVENT_WATER1_BIT);
			}
		}

		if(!logic_mcu_ls8a10049t_is_int_event(LS8A10049T_INT_EVENT_NTC_BIT)){	
			int over_temperature = _ls8a10049t_check_ntc_status(dev);
			if (!over_temperature) {
				SYS_LOG_INF("over_temperature\n");
				para.mcu_event_val = MCU_INT_CMD_TEMPERATURE_HIGH;

				if (dev->mcu_notify) {
					dev->mcu_notify(MCU_INT_TYPE_NTC, &para);
				}
				else{
					printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
				}
				logic_mcu_ls8a10049t_set_int_event(LS8A10049T_INT_EVENT_NTC_BIT);
			}
		}
		if(logic_mcu_ls8a10049t_get_int_event()){
			if(mcu_int_info.mcu_int_flag == 0){
				mcu_int_info.mcu_int_flag = 1;
				// thread_timer_start(&ls8a10049t_int_event_timer, 2000,2000);

			}
		}
	} while (0);

	// clear int pending bit
	_ls8a10049t_clear_int_pending(dev);

    //config.bits.speed = I2C_SPEED_FAST;
    //i2c_configure(dev->i2c_dev, config.raw);
    return 0;
}

int mcu_ls8a10049t_int_deal(void)
{
	int ret = 0;
	if(!key_water_status_read())
    {
 		os_sched_lock();

        mcu_ls8a10049t_input_event_report();

		os_sched_unlock();   
		ret = 1;    
    }
	return ret;
}
#endif
bool mcu_ls8a10049t_is_dc_in(void)
{
    return (ls8a10049t_dc_in_flag == 1);
}

static void logic_mcu_ls8a10049t_dc_intimer_fn(struct thread_timer *ttimer, void *expiry_fn_arg)
{
    struct logic_mcu_ls8a10049t_device_data_t *dev = NULL;
    mcu_manager_charge_event_para_t para;
	static uint8_t two_second_cnt = 0;
	static uint8_t first_charge = 0;

	two_second_cnt++;
    dev = logic_mcu_ls8a10049t_get_device_data();

	int power_key_pressed = _ls8a10049t_check_power_key_pressed(dev);
	if (power_key_pressed) {
		two_second_cnt = 0;
		ls8a10049t_dc_in_flag = 0;
		thread_timer_stop(&ls8a10049t_dc_in_timer);
		return;
	}

	int usb_cable_in = _ls8a10049t_check_usb_cable_in_status(dev);
	if (!usb_cable_in) {
		if(first_charge == 0)
	   	{
			SYS_LOG_INF("usb_cable_out\n");
			para.mcu_event_val = MCU_INT_CMD_DC_OUT;

			if (dev->mcu_notify) {
				dev->mcu_notify(MCU_INT_TYPE_DC, &para);
			}
			else{
				printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
			} 
		}
		first_charge = 1;
		two_second_cnt = 0; 
		ls8a10049t_dc_in_flag = 0;
		thread_timer_stop(&ls8a10049t_dc_in_timer);          
	}
	if(two_second_cnt > 10){
		two_second_cnt = 0;
		ls8a10049t_dc_in_flag = 0;
		thread_timer_stop(&ls8a10049t_dc_in_timer); 
	}
	
}

void logic_mcu_ls8a10049t_int_first_handle(void)
{
    struct logic_mcu_ls8a10049t_device_data_t *dev = NULL;

    dev = logic_mcu_ls8a10049t_get_device_data();

	_logic_mcu_ls8a10049t_int_handle(dev);

	_ls8a10049t_poweron_hold(dev);

	//bt_mcu_set_first_power_on_flag();
	#ifdef OTG_PHONE_POWER_NEED_SHUTDOWN
	_ls8a10049t_otg_check_enter_wait(dev);
	#endif

	clear_no_use_warrings();
}

//extern void bt_mcu_set_first_power_on_flag(void);
int logic_mcu_LS8A10049T_init(void)
{
	struct logic_mcu_ls8a10049t_device_data_t *dev = NULL;
	dev = logic_mcu_ls8a10049t_get_device_data();

    p_mcu_ls8a10049t_dev = &g_mcu_ls8a10049t_dev;
    
    p_mcu_ls8a10049t_dev->driver_api = &mcu_ls8a10049t_wlt_driver_api;
    p_mcu_ls8a10049t_dev->driver_data = logic_mcu_ls8a10049t_get_device_data();
	dev->mcu_update_fw = 0;
	ls8a10049t_dc_in_flag = 0;
	// u8_t chip_id = 0xff;
    //memset(&logic_mcu_int_cb_handle, 0, sizeof(struct gpio_callback));


/*
	dev->gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!dev->gpio_dev){
		SYS_LOG_ERR("line %d, get dev err\n", __LINE__);
		return -1;
	}
*/
	//gpio_pin_configure(dev->gpio_dev, PIN_LOGIC_MCU_INT, PIN_LOGIC_MCU_INT_CFG);

	//logic_mcu_int_cb_handle.handler = logic_mcu_int_cbk;
	//logic_mcu_int_cb_handle.pin_group = PIN_LOGIC_MCU_INT / 32;
	//logic_mcu_int_cb_handle.pin_mask = GPIO_BIT(PIN_LOGIC_MCU_INT);

	//gpio_add_callback(dev->gpio_dev, &logic_mcu_int_cb_handle);
	//gpio_pin_enable_callback(dev->gpio_dev, PIN_LOGIC_MCU_INT);

	//k_busy_wait(5000);

    dev->i2c_dev = device_get_binding(CONFIG_I2C_0_NAME);
	if (!dev->i2c_dev){
		SYS_LOG_ERR("line %d, get dev err\n", __LINE__);
		return -1;
	}
	
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	gpio_pin_configure(gpio_dev, 23, GPIO_DIR_IN | GPIO_PUD_PULL_UP);
	thread_timer_init(&ls8a10049t_dc_in_timer, logic_mcu_ls8a10049t_dc_intimer_fn, NULL);

	//_logic_mcu_ls8a10049t_int_handle(dev);

	//_ls8a10049t_poweron_hold(dev);

	//bt_mcu_set_first_power_on_flag();

	//clear_no_use_warrings();
	thread_timer_init(&ls8a10049t_int_event_timer, logic_mcu_ls8a10049t_int_timer_fn, NULL);
	
	io_expend_aw9523b_registers_init();
    return 0;
}

struct device *wlt_device_get_mcu_ls8a10049t(void)
{
     return p_mcu_ls8a10049t_dev;
}
#if 0
void _logic_mcu_ls8a10049t_init(void)
{

}



struct logic_mcu_ls8a10049t_driver_api_t logic_mcu_ls8a10049t_driver_api = {
    .int_handle = _logic_mcu_ls8a10049t_int_handle,
};


struct logic_mcu_ls8a10049t_driver_api_t * mcu_ls8a10049t_get_api(void)
{
    return &logic_mcu_ls8a10049t_driver_api;
}
#endif
/* plm add,for factory test! */
int logic_mcu_read_ntc_status(void)
{
	struct logic_mcu_ls8a10049t_device_data_t *dev = NULL;
    dev = logic_mcu_ls8a10049t_get_device_data();

	return _ls8a10049t_check_ntc_status(dev);
}

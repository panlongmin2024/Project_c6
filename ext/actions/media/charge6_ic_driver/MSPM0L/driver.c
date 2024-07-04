
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
#ifdef CONFIG_LED_MANAGER
#include <led_manager.h>
#endif
#include <app_manager.h>
#include <pd_manager_supply.h>
#include "tts_manager.h"

#define BATTERY_CHARGING_REMAIN_CAP_LEVEL0        15
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL1        23
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL2        45
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL3        60
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL4        75
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL5        100

enum mcu_int_iic_address_t{
    BT_MCU_ACK_IIC_ADDR,
    MCU_INT_CMD_IIC_ADDR,
    MCU_READ_INT_EVENT_IIC_ADDR = 0x04,
    BT_MCU_BAT_LED0_IIC_ADDR = 0x10,
    BT_MCU_BAT_LED1_IIC_ADDR,
    BT_MCU_BAT_LED2_IIC_ADDR,
    BT_MCU_BAT_LED3_IIC_ADDR,
    BT_MCU_BAT_LED4_IIC_ADDR,
    BT_MCU_BAT_LED5_IIC_ADDR,
    BT_MCU_TWS_LED_IIC_ADDR = 0x16,
    BT_MCU_PAIR_LED_IIC_ADDR = 0x17, 

    BT_MCU_WATER_ADC0_THRED_IIC_ADDR = 0x40,
    BT_MCU_WATER_ADC1_THRED_IIC_ADDR,     

    BT_MCU_NTC_HIGHTEMP_THRED_IIC_ADDR = 0x52,
    BT_MCU_NTC_HIGHTEREMP_THRED_IIC_ADDR,   
    BT_MCU_NTC_VERYHIGHTEREMP_THRED_IIC_ADDR,  

    BT_MCU_REAL_SHUTDOWN_DUT_IIC_ADDR = 0x70,
};

#define I2C_DEV_ADDR        0x48                    //TODO
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
#if 1
struct wlt_mcu_mspm0l_info {
    struct device *i2c_dev;
    mcu_callback_t  mcu_notify;
    uint8_t mcu_update_fw;
    bool enable_bt_led;
    bool enable_pty_boost_led;
    bool enable_bat_led;
};

static struct wlt_mcu_mspm0l_info mcu_mspm0l_ddata;
static struct device g_mcu_mspm0l_dev;
static struct device *p_mcu_mspm0l_dev = NULL;
extern bool key_water_status_read(void);
uint8_t pd_get_app_mode_state(void);
extern void user_mspm0l_ota_sucess_startapp(void);
//extern int user_mspm0l_ota_process(void);
#if 0
static int mcu_mspm0l_get_value(uint8_t reg,uint8_t len)
{

    u8_t buf[4] = {0};
    uint16_t val = 0;
    u8_t lenth = len;

    union dev_config config = {0};
    //struct device *iic_dev;
    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;

    if(mcu_mspm0l->i2c_dev == NULL)
    {
        printk("[%s,%d]iic_dev not found\n", __FUNCTION__, __LINE__);
        return -EIO;
    }
    if(lenth > 2){
        lenth = 2;
    }
    //iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(mcu_mspm0l->i2c_dev, config.raw);
    if(i2c_burst_read(mcu_mspm0l->i2c_dev, I2C_DEV_ADDR, MCU_INT_CMD_IIC_ADDR, buf, lenth)<0)
    {
        printk("[%s:%d] MCU --> IIC buf: Error!\n", __func__, __LINE__);
        return -EIO;
    }
    if(lenth == 1){
        val = (uint16_t)buf[0];
    }
    else if(lenth == 2){
        val = (uint16_t)(buf[0]<<8 | buf[1]);
    }
    printk("[%s:%d] reg[%x]:%d\n", __func__, __LINE__,reg,val);
    return val;

}

static void mcu_mspm0l_set_value(uint8_t reg,uint16_t value)
{

    //int len = 0;
    u8_t buf[4] = {0};
    union dev_config config = {0};
    //struct device *iic_dev;
    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;

    if(mcu_mspm0l->i2c_dev == NULL)
    {
        printk("[%s,%d]iic_dev not found\n", __FUNCTION__, __LINE__);
        return;
    }

    //iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(mcu_mspm0l->i2c_dev, config.raw);

    buf[0] = (u8_t)(value >> 8);
    buf[1] = (u8_t)(value & 0x00ff);

    i2c_burst_write(mcu_mspm0l->i2c_dev, I2C_DEV_ADDR, reg, buf, 2);
    //printk("zth debug:%s:%d; len= %d; buf: %d \n", __func__,__LINE__, len, buf[0]);


}
#endif
static void mcu_mspm0l_led_display(uint8_t reg,uint8_t status)
{

    //int len = 0;
    u8_t buf[4] = {0};
    union dev_config config = {0};
    //struct device *iic_dev;
    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;

    if(mcu_mspm0l->i2c_dev == NULL)
    {
        printk("[%s,%d]iic_dev not found\n", __FUNCTION__, __LINE__);
        return;
    }
   // return;

    //iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(mcu_mspm0l->i2c_dev, config.raw);

    buf[0] = BT_LED_STATUS_OFF;
    if(status)
        buf[0] = status;
    i2c_burst_write(mcu_mspm0l->i2c_dev, I2C_DEV_ADDR, reg, buf, 1);
    //printk("zth debug:%s:%d; len= %d; buf: %d \n", __func__,__LINE__, len, buf[0]);


}

static int bt_mcu_send_iic_cmd_code(u8_t addr, u8_t cmd, u8_t value)
{
    u8_t buf[4] = {0}; 
    union dev_config config = {0};
    //struct device *iic_dev;
    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;
    
    SYS_LOG_INF("[%d],addr=%d,cmd=%d,value=%d\n", __LINE__, addr, cmd, value);
    //iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    if(mcu_mspm0l->i2c_dev)
    {
        config.bits.speed = I2C_SPEED_STANDARD;
        i2c_configure(mcu_mspm0l->i2c_dev, config.raw);

        buf[0] = (cmd <<4 | value);
        i2c_burst_write(mcu_mspm0l->i2c_dev, I2C_DEV_ADDR, addr, buf, 1);
        return 0;
    }
    return -1;

}

static void bt_mcu_send_update_fw_code(u8_t value)
{

    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;
    
    SYS_LOG_INF("[%d],value=%d\n", __LINE__, value);
    mcu_mspm0l->mcu_update_fw = value;

}

static void bt_mcu_enable_bt_led_code(bool value)
{

    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;
    
    SYS_LOG_INF("[%d],value=%d\n", __LINE__, value);
    mcu_mspm0l->enable_bt_led = value;
}

static void bt_mcu_enable_pty_boost_led_code(bool value)
{

    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;
    
    SYS_LOG_INF("[%d],value=%d\n", __LINE__, value);
    mcu_mspm0l->enable_pty_boost_led = value;
}

static void bt_mcu_enable_bat_led_code(bool value)
{

    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;
    
    SYS_LOG_INF("[%d],value=%d\n", __LINE__, value);
    mcu_mspm0l->enable_bat_led = value;
}

static bool bt_mcu_get_enable_bt_led_code(void)
{

    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;
   
    return mcu_mspm0l->enable_bt_led;
}

static bool bt_mcu_get_enable_pty_boost_led_code(void)
{

    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;
    
    return mcu_mspm0l->enable_pty_boost_led;
}

static bool bt_mcu_get_enable_bat_led_code(void)
{

    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;
   
    return mcu_mspm0l->enable_bat_led;
}

static int mcu_mspm0l_wlt_get_property(struct device *dev,enum mcu_manager_supply_property psp,     \
				       union mcu_manager_supply_propval *val)
{
    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;
    switch(psp)
    {
        case MCU_SUPPLY_PROP_LED_0:
            break;

        case MCU_SUPPLY_PROP_LED_1:
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
            val->intval = mcu_mspm0l->mcu_update_fw;
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

static void mcu_mspm0l_wlt_set_property(struct device *dev,enum mcu_manager_supply_property psp,    \
				       union mcu_manager_supply_propval *val)
{
    int value = val->intval;
    switch(psp)
    {
        case MCU_SUPPLY_PROP_LED_0:
            if(!bt_mcu_get_enable_bat_led_code())
            pd_iic_push_queue(MCU_IIC_TYPE_PROP_LED_0, (u8_t)value);
            //mcu_mspm0l_led_display(BT_MCU_BAT_LED0_IIC_ADDR,value);
            break;

        case MCU_SUPPLY_PROP_LED_1:
            if(!bt_mcu_get_enable_bat_led_code())
            pd_iic_push_queue(MCU_IIC_TYPE_PROP_LED_1, (u8_t)value);
            //mcu_mspm0l_led_display(BT_MCU_BAT_LED1_IIC_ADDR,value);
            break;
        case MCU_SUPPLY_PROP_LED_2:
            if(!bt_mcu_get_enable_bat_led_code())
            pd_iic_push_queue(MCU_IIC_TYPE_PROP_LED_2, (u8_t)value);
            //mcu_mspm0l_led_display(BT_MCU_BAT_LED2_IIC_ADDR,value);
            break; 

        case MCU_SUPPLY_PROP_LED_3:
            if(!bt_mcu_get_enable_bat_led_code())
            pd_iic_push_queue(MCU_IIC_TYPE_PROP_LED_3, (u8_t)value);
            //mcu_mspm0l_led_display(BT_MCU_BAT_LED3_IIC_ADDR,value);
            break; 

        case MCU_SUPPLY_PROP_LED_4:
            if(!bt_mcu_get_enable_bat_led_code())
            pd_iic_push_queue(MCU_IIC_TYPE_PROP_LED_4, (u8_t)value);
            //mcu_mspm0l_led_display(BT_MCU_BAT_LED4_IIC_ADDR,value);
            break;

        case MCU_SUPPLY_PROP_LED_5:
            if(!bt_mcu_get_enable_bat_led_code())
            pd_iic_push_queue(MCU_IIC_TYPE_PROP_LED_5, (u8_t)value);
            //mcu_mspm0l_led_display(BT_MCU_BAT_LED5_IIC_ADDR,value);
            break;

        case MCU_SUPPLY_PROP_LED_BT:
            if(!bt_mcu_get_enable_bt_led_code())
            pd_iic_push_queue(MCU_IIC_TYPE_PROP_LED_BT, (u8_t)value);
            //mcu_mspm0l_led_display(BT_MCU_PAIR_LED_IIC_ADDR,value);
            break;

        case MCU_SUPPLY_PROP_LED_PTY_BOOST:
            if(!bt_mcu_get_enable_pty_boost_led_code())
            pd_iic_push_queue(MCU_IIC_TYPE_PROP_LED_AURACAST, (u8_t)value);
            //mcu_mspm0l_led_display(BT_MCU_TWS_LED_IIC_ADDR,value);
            break;


        case MCU_SUPPLY_PROP_RED_LED_FLASH:
            mcu_mspm0l_led_display(BT_MCU_BAT_LED0_IIC_ADDR,1);
            mcu_mspm0l_led_display(BT_MCU_BAT_LED1_IIC_ADDR,0);
            mcu_mspm0l_led_display(BT_MCU_BAT_LED2_IIC_ADDR,0);
            mcu_mspm0l_led_display(BT_MCU_BAT_LED3_IIC_ADDR,0);
            mcu_mspm0l_led_display(BT_MCU_BAT_LED4_IIC_ADDR,0);
            mcu_mspm0l_led_display(BT_MCU_BAT_LED5_IIC_ADDR,0);
            break;

        case MCU_SUPPLY_PROP_UPDATE_FW:
            bt_mcu_send_update_fw_code(value);
            break;
        case MCU_SUPPLY_PROP_RESET_MCU:
            user_mspm0l_ota_sucess_startapp();
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

        case MCU_SUPPLY_PROP_REAL_SHUTDOWN_DUT:
            mcu_mspm0l_led_display(BT_MCU_REAL_SHUTDOWN_DUT_IIC_ADDR,1);
            break;
        default:
            break;               
    }

    
}

static void mcu_mspm0l_wlt_register_notify(struct device *dev, mcu_callback_t cb)
{
    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;
	
	if ((mcu_mspm0l->mcu_notify == NULL) && cb)
	{
		mcu_mspm0l->mcu_notify = cb;
	}else
	{
		SYS_LOG_ERR("mcu notify func already exist!\n");
	}
}

static void mcu_mspm0l_wlt_response(struct device *dev,enum mcu_manager_supply_property psp,    \
				       union mcu_manager_supply_propval *val)
{
   
    switch(psp)
    {
        case MCU_INT_TYPE_POWER_KEY:
            bt_mcu_send_iic_cmd_code(BT_MCU_ACK_IIC_ADDR, MCU_INT_TYPE_POWER_KEY, val->intval);
            break;


        case MCU_INT_TYPE_DC:
            bt_mcu_send_iic_cmd_code(BT_MCU_ACK_IIC_ADDR, MCU_INT_TYPE_DC, val->intval);
            break;
        
        default:

            break;
    }
}

static const struct mcu_manager_supply_driver_api mcu_mspm0l_wlt_driver_api = {
	.get_property = mcu_mspm0l_wlt_get_property,
	.set_property = mcu_mspm0l_wlt_set_property,
    .register_mcu_notify = mcu_mspm0l_wlt_register_notify,
    .mcu_response = mcu_mspm0l_wlt_response,
};

static uint8_t is_dc_in_power = 0;
static uint8_t is_dc_in_cnt = 0;
#define DC_IN_TIME_VALUE        20
#if 1
static int mcu_mspm0l_input_event_report(void)
{

    u8_t buf[4] = {0};
    union dev_config config = {0};
    mcu_manager_charge_event_para_t para;
    //struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data;
    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;

    if(mcu_mspm0l->i2c_dev == NULL)
    {
        printk("[%s,%d]iic_dev not found\n", __FUNCTION__, __LINE__);
        return -EIO;
    }
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(mcu_mspm0l->i2c_dev, config.raw);

    if(i2c_burst_read(mcu_mspm0l->i2c_dev, I2C_DEV_ADDR, MCU_INT_CMD_IIC_ADDR, buf, 1)<0)
    {
        printk("[%s:%d] MCU --> IIC buf: Error!\n", __func__, __LINE__);
        return -EIO;
    }
    printk("MCU --> IIC buf: 0x%x\n", buf[0]);
    if(buf[0] == 0){
        k_sleep(10);
        if(i2c_burst_read(mcu_mspm0l->i2c_dev, I2C_DEV_ADDR, MCU_READ_INT_EVENT_IIC_ADDR, buf, 1)<0)
        {
            printk("[%s:%d] MCU --> IIC buf: Error!\n", __func__, __LINE__);
            return -EIO;
        }
        printk("MCU EVENT buf: 0x%x\n", buf[0]);

    }
    para.mcu_event_val = buf[0] & 0x0f;
    /*
    if(((buf[0]>>4) & 0x0f) == MCU_INT_TYPE_POWER_KEY){
        if(pd_get_app_mode_state() == CHARGING_APP_MODE){
            para.mcu_event_val = MCU_INT_CMD_POWERON;	
        }
        else{
            para.mcu_event_val = MCU_INT_CMD_POWEROFF;	
        }
    }
    */
   if(((buf[0]>>4) & 0x0f) == MCU_INT_TYPE_POWER_KEY){
             is_dc_in_power = 0xff;   
    }

    if(is_dc_in_power == 0){
        if(((buf[0]>>4) & 0x0f) == MCU_INT_TYPE_DC){
             is_dc_in_power = 1;   
        }
        else{
            is_dc_in_power = 0xff;
        }
    }
    else if(is_dc_in_power == 1){
        if(is_dc_in_cnt > DC_IN_TIME_VALUE){
            is_dc_in_power = 0xff;
        }
    }
    else if(is_dc_in_power == 0xff){
        if(((buf[0]>>4) & 0x0f) == MCU_INT_TYPE_DC
        && para.mcu_event_val == MCU_INT_CMD_DC_OUT){
            printk("[%s/%d] dc out !!!",__func__,__LINE__);
             goto exit_reprt;   
        }
    }

    if (mcu_mspm0l->mcu_notify) {
        mcu_mspm0l->mcu_notify((buf[0]>>4) & 0x0f, &para);
    }
    else{
        printk("[%s:%d] MCU notify function did not register!\n", __func__, __LINE__);
    }
exit_reprt:
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(mcu_mspm0l->i2c_dev, config.raw);
    return 0;
}

int mcu_mspm0l_int_deal(void)
{
    int ret = 0;
	if(!key_water_status_read())
    {
        mcu_mspm0l_input_event_report();
        ret = 1;
    }
    if(is_dc_in_power == 1){
        is_dc_in_cnt++;
        if(is_dc_in_cnt > DC_IN_TIME_VALUE)
        {
            is_dc_in_power = 0xff;
            ///is_dc_in_cnt = 0;
        }
    }
    return ret;
}

bool mcu_mspm0l_is_dc_in(void)
{
    return (is_dc_in_power == 1);
}
#endif

#if 0
int mcu_mspmol_ota_deal(void)
{
    int ret = 0;
    struct wlt_mcu_mspm0l_info *mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;

    if(mcu_mspm0l->mcu_update_fw == 1){

        ret = user_mspm0l_ota_process();
        printk("[%s,%d] ret %d !!!\n", __FUNCTION__, __LINE__,ret);
        if(ret == 1){
            sys_event_notify(SYS_EVENT_MCU_FW_SUCESS);
           // k_sleep(3000);
           // user_mspm0l_ota_sucess_startapp();
            mcu_mspm0l->mcu_update_fw = 0xff;
        }
        else{
            mcu_mspm0l->mcu_update_fw = 0;
            sys_event_notify(SYS_EVENT_MCU_FW_FAIL);
        }        

    }
    else if(mcu_mspm0l->mcu_update_fw == 0xff)
    {
        ret = 1;
    }
    return ret;
}
#endif
void mcu_iic_type_prop_handle_fn(uint8_t type, uint8_t data)
{
    //SYS_LOG_INF("[%d] type:%d, data:%d\n", __LINE__, type, data); 
    switch(type)
    {
        case MCU_IIC_TYPE_PROP_LED_0:
            mcu_mspm0l_led_display(BT_MCU_BAT_LED0_IIC_ADDR,data);
        break;
        case MCU_IIC_TYPE_PROP_LED_1:
            mcu_mspm0l_led_display(BT_MCU_BAT_LED1_IIC_ADDR,data);
        break;
        case MCU_IIC_TYPE_PROP_LED_2:
            mcu_mspm0l_led_display(BT_MCU_BAT_LED2_IIC_ADDR,data);
        break;
        case MCU_IIC_TYPE_PROP_LED_3:
            mcu_mspm0l_led_display(BT_MCU_BAT_LED3_IIC_ADDR,data);
        break;
        case MCU_IIC_TYPE_PROP_LED_4:
            mcu_mspm0l_led_display(BT_MCU_BAT_LED4_IIC_ADDR,data);
        break;
        case MCU_IIC_TYPE_PROP_LED_5:
            mcu_mspm0l_led_display(BT_MCU_BAT_LED5_IIC_ADDR,data);
        break;
        case MCU_IIC_TYPE_PROP_LED_BT:
            mcu_mspm0l_led_display(BT_MCU_PAIR_LED_IIC_ADDR,data);
        break;
        case MCU_IIC_TYPE_PROP_LED_AURACAST:
            mcu_mspm0l_led_display(BT_MCU_TWS_LED_IIC_ADDR,data);
        break;
        default:
            break;        
    }
}

#ifdef CONFIG_ACTIONS_IMG_LOAD
int mcu_mspm0l_poweron_hold(void)
{
    u8_t buf[4] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);

	if(i2c_burst_read(iic_dev, I2C_DEV_ADDR, MCU_INT_CMD_IIC_ADDR, buf, 1)<0)
    {
        printk("[%s:%d] MCU --> IIC buf: Error!\n", __func__, __LINE__);
        return -EIO;
    }	
	printk("%s,MCU --> IIC buf: 0x%x\n", __func__,buf[0]);
	buf[0] = 0x21;
	i2c_burst_write(iic_dev, I2C_DEV_ADDR, BT_MCU_ACK_IIC_ADDR, buf, 1);
	return 0;	
}
#endif

int mcu_mspm0l_init(void)
{
    struct wlt_mcu_mspm0l_info *mcu_mspm0l = NULL; 
    p_mcu_mspm0l_dev = &g_mcu_mspm0l_dev;
    union dev_config config = {0};
    
    is_dc_in_power = 0;
    is_dc_in_cnt = 0;
    p_mcu_mspm0l_dev->driver_api = &mcu_mspm0l_wlt_driver_api;
    p_mcu_mspm0l_dev->driver_data = &mcu_mspm0l_ddata;

    mcu_mspm0l = p_mcu_mspm0l_dev->driver_data;


    mcu_mspm0l->mcu_update_fw = 0;
    mcu_mspm0l->enable_bt_led = false;
    mcu_mspm0l->enable_pty_boost_led = false;
    mcu_mspm0l->i2c_dev = device_get_binding(CONFIG_I2C_0_NAME);

    if(!mcu_mspm0l->i2c_dev)
    {
        printk("[%s,%d] iic_dev not found\n", __FUNCTION__, __LINE__);
        return -EIO;
    }else{
        config.bits.speed = I2C_SPEED_STANDARD;
        i2c_configure(mcu_mspm0l->i2c_dev , config.raw);
    }

#if 0
    u8_t buf[4] = {0};

    if(i2c_burst_read(mcu_mspm0l->i2c_dev, I2C_DEV_ADDR, MCU_INT_CMD_IIC_ADDR, buf, 1)<0)
    {
        printk("[%s:%d] MCU --> IIC buf: Error!\n", __func__, __LINE__);
        return -EIO;
    }
    printk("[%s:%d] MCU --> IIC buf: 0x%x\n", __func__, __LINE__,buf[0]);
    bt_mcu_send_iic_cmd_code(BT_MCU_ACK_IIC_ADDR, MCU_INT_TYPE_POWER_KEY, MCU_INT_CMD_POWERON);
#endif    
/*
    mcu_mspm0l_set_value(BT_MCU_WATER_ADC0_THRED_IIC_ADDR,260);
    mcu_mspm0l_get_value(BT_MCU_WATER_ADC0_THRED_IIC_ADDR,2);

    mcu_mspm0l_set_value(BT_MCU_WATER_ADC1_THRED_IIC_ADDR,260);
    mcu_mspm0l_get_value(BT_MCU_WATER_ADC1_THRED_IIC_ADDR,2);

    mcu_mspm0l_set_value(BT_MCU_NTC_HIGHTEMP_THRED_IIC_ADDR,710);
    mcu_mspm0l_get_value(BT_MCU_NTC_HIGHTEMP_THRED_IIC_ADDR,2);

    mcu_mspm0l_set_value(BT_MCU_NTC_HIGHTEREMP_THRED_IIC_ADDR,710);
    mcu_mspm0l_get_value(BT_MCU_NTC_HIGHTEREMP_THRED_IIC_ADDR,2);

    mcu_mspm0l_set_value(BT_MCU_NTC_VERYHIGHTEREMP_THRED_IIC_ADDR,710);
    mcu_mspm0l_get_value(BT_MCU_NTC_VERYHIGHTEREMP_THRED_IIC_ADDR,2);
*/
    return 0;
}

struct device *wlt_device_get_mcu_mspm0l(void)
{
     return p_mcu_mspm0l_dev;
}
#endif
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
#include <pd_manager_supply.h>
#include <mem_manager.h>
#include "run_mode/run_mode.h"

#include <logging/sys_log.h>
/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @addtogroup t_pinmux_basic
 * @{
 * @defgroup t_pinmux_gpio test_pinmux_gpio
 * @brief TestPurpose: verify PINMUX works on GPIO port
 * @details
 * - Test Steps (Quark_se_c1000_devboard)
 *   -# Connect pin_53(GPIO_19) and pin_50(GPIO_16).
 *   -# Configure GPIO_19 as output pin and set init  LOW.
 *   -# Configure GPIO_16 as input pin and set LEVEL HIGH interrupt.
 *   -# Set pin_50 to PINMUX_FUNC_A and test GPIO functionality.
 *   -# Set pin_50 to PINMUX_FUNC_B and test GPIO functionality.
 * - Expected Results
 *   -# When set pin_50 to PINMUX_FUNC_A, pin_50 works as GPIO and gpio
 *	callback will be triggered.
 *   -# When set pin_50 to PINMUX_FUNC_B, pin_50 works as I2S and gpio
 *	callback will not be triggered.
 * @}
 */

#define I2C_DEV_ADDR        0x48                    //TODO
#define I2C_PD_DEV_ADDR     0x21
// #define DEF_MCU_WATER_INT_PIN 0

#define TI_PD_INT_PIN			 19


#define PD_STATUS_REG_SINK_SOURCE_SHIFT_BIT         5
#define PD_STATUS_REG_SINK_SOURCE_MASK              0x01

#define PD_STATUS_REG_PLUG_PRESENT_SHIFT_BIT         0

#define MAX_SINK_JUDGE_TIME                         4

#define PD_STATUS_REG_SINK_LEGACY_SHIFT_BIT            24
#define PD_STATUS_REG_SINK_LEGACY_MASK                 0x03

enum pd_status_sink_legacy_map_t{
    PD_SINK_STATUS_PD,
    PD_SINK_STATUS_LEGACY,
};


enum bt_runtime_status_map_t{
    BT_RUNTIME_STATUS_MUSIC_PLAY,
    BT_RUNTIME_STATUS_NORMAL,
    BT_RUNTIME_STATUS_POWEROFF,

};



typedef enum {
    PD_REG_INT_RECV_SOURCE_CAP_FLAG,
    PD_STATUS_SOURCE_OUT,
    PD_PLUG_IN_OUT_FLAG,
    PD_CLEAR_ALL_FLAG,
}pd_reg_int_event_map_t;


typedef struct {
    u16_t   value;
    u8_t    priority;
    u8_t    volt;
} pd_ldo_sink_priority_map_t;


typedef struct {
    /** pd register address */
	u8_t addr;

    /** pd register value length */
	u8_t lenght;

} pd_reg_addr_len_map_t;



#define MCU_RUN_TIME_PERIOD 100

#define MCU_ONE_SECOND_PERIOD           ((1000/MCU_RUN_TIME_PERIOD))
#define MCU_HALF_SECOUD_PERIOD          ((500/MCU_RUN_TIME_PERIOD))

#define MCU_FIVE_SECOND_PERIOD           5
#define MCU_CHARGE_TEN_MINITE_COUNT     600
#define MCU_CHARGE_30_SECOND_COUNT      30
#define WLT_OTG_DEBOUNCE_TIMEOUT        10

#define MAX_SOURCE_DISC_COUNT           23

enum ti_pd_reg_address_t{

    PD_REG_IIC_CMD_ADDR         = 0x08,
    PD_REG_IIC_DATA_ADDR        = 0x09,
    PD_REG_INT_EVENT_ADDR       = 0x14,
    PD_REG_CLEAR_EVENT_ADDR     = 0x18,
    PD_REG_STATUS_ADDR          = 0x1A,
    PD_REG_PORT_CONFIGURE_ADDR  = 0x28,
    PD_REG_REV_SOURCE_ADDR      = 0x30,
    PD_REG_REV_SINK_ADDR        = 0x31,
    PD_REG_TRANS_SOURCE_ADDR    = 0x32,
    PD_REG_TRANS_SINK_ADDR      = 0x33,
    PD_REG_AUTO_SINK_ADDR       = 0x37,
    PD_REG_TRANS_POWER_STATUS   = 0x3f,
    PD_REG_TYPE_C_STATUS        = 0x69
    
};

enum ti_pd_input_cur_limit_value{

    PD_INPUT_LIMIT_3000MA_CUR    = 0x00,
    PD_INPUT_LIMIT_1500MA_CUR,     
    PD_INPUT_LIMIT_500MA_CUR     

};

enum ti_pd_charge_cur_limit_value{

    PD_CHARGE_LIMIT_2400MA_CUR    = 0x00,
    PD_CHARGE_LIMIT_1500MA_CUR,     
    PD_CHARGE_LIMIT_500MA_CUR     

};



static const pd_reg_addr_len_map_t  pd_reg_address_len_map[] = {

    {PD_REG_STATUS_ADDR,            0x06 }, 
    {PD_REG_IIC_CMD_ADDR,           0x04 }, 
    {PD_REG_IIC_DATA_ADDR,          0x04 }, 
    {PD_REG_INT_EVENT_ADDR,         0x04 },
    {PD_REG_PORT_CONFIGURE_ADDR,    0x01 },
    {PD_REG_CLEAR_EVENT_ADDR,       0x0b },
    {PD_REG_REV_SINK_ADDR,          0x06 },
    {PD_REG_TRANS_POWER_STATUS,     0x03 },
    {PD_REG_TYPE_C_STATUS,          0x05 },
    {PD_REG_AUTO_SINK_ADDR,         0x18 },
    {PD_REG_REV_SOURCE_ADDR,        0xFF },
    {PD_REG_TRANS_SINK_ADDR,        0xFF },


};


// 0xb4=9v; 0x64=5v; 0xf0=12v; 0x12c=15v; 0x190=20v;
// 12V-->15V-->9V--->20V-->5V

static const  pd_ldo_sink_priority_map_t pd_ldo_sink_prior_map[] = {
    {0x64,                  1,          50},            //
    {0x190,                 2,          200},            // 
    {0xb4,                  3,          90},            // 
    {0x12c,                 4,          150},            
    {0xf0,                  5,          120},
};



#define GPIO_PIN_USB_D_LINE                 42
#define GPIO_PIN_PD_SOURCE                  4

/*

*/

/*!
 * \brief 电池及充电事件定义
 */
typedef enum
{
    PD_REG_STATUS_NO_CHANGE,
    PD_REG_STATUS_SOURCE_CHANGE,
    PD_REG_STATUS_SINK_CHANGE,
    PD_REG_STATUS_LEGACY_SINK_CHANGE,
    
} pd_ti65992_ret_status_t;


struct wlt_pd_ti65992_info {
	struct thread_timer timer;
	struct device *gpio_dev;
    struct device *i2c_dev;
    pd_manager_callback_t notify;
    pd_manager_battery_t  battery_notify;
    pd_manager_mcu_callback_t mcu_notify;
    
    u32_t   pd_65992_sink_flag:1;
    u32_t   pd_sink_legacy_flag:3;
    u32_t   pd_65992_source_flag:1;
    u32_t   sink_charging_flag:1;
    u32_t   plug_present_flag:1;
    u32_t   chk_current_flag:1;
    u32_t   charge_full_flag:1;
    u32_t   charge_sink_test_flag:1;
    u32_t   pre_charge_flag:1;
    u32_t   pd_65992_unload_flag:1;

    int16   volt_value;
    int16   cur_value;
    u8_t    sink_judge_count;
    u8_t    source_judge_count;
    u8_t    pd_bt_runtime_status;
    u8_t    pd_source_disc_debunce_cnt;

};

///

struct wlt_pd_ti65992_config {
	char *iic_name;
	char *gpio_name;
    u16_t poll_interval_ms;
    u8_t  one_second_count;
};


struct wlt_pd_ti65992_sink_ldo_data {
    bool sink_pd_flag;
	u8_t ldo_max;
    u8_t ldo_index;
    u8_t buf[100];
};


static struct wlt_pd_ti65992_sink_ldo_data  g_sink_ldo_data;

static struct wlt_pd_ti65992_info pd_ti65992_ddata;
static struct device g_pd_ti65992_dev;
static struct device *p_pd_ti65992_dev = NULL;

// static struct k_thread wlt_ti65992_thread_data;
// K_THREAD_STACK_DEFINE(wlt_ti65992_stack_area, 1024);

// static char *wlt_ti65992_stack;

extern bool key_water_status_read(void);
extern bool media_player_is_working(void);
extern bool dc_power_in_status_read(void);
extern void pd_tps65992_load(void);
extern void pd_tps65992_unload(void);

int pd_tps65992_write_4cc(struct device * dev, char *str);
void pd_bq25798_disable_full_charge(bool flag);
void pd_tps65992_ext_reg_input(bool flag);

static void pd_tps65992_charging_disable(bool flag)
{
   
    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;
   
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    buf[0] = 0x05;
    buf[1] = 0x6b;
    buf[2] = 0x02;
    buf[3] = 0x0f;                                                                  // 

    SYS_LOG_INF("[%d] flag:%d\n", __LINE__, flag);

    if(flag)
    {
        buf[4] =  0xA6;                                                 //                                        
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
        k_sleep(2);
        pd_tps65992_write_4cc(iic_dev, "I2Cw");
    }else{
        buf[4] = 0xA2;                                                              //                                        
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
        k_sleep(2);
        pd_tps65992_write_4cc(iic_dev, "I2Cw");  
    }

    
}

void pd_tps65992_init_and_disable_charge(bool flag)
{
    pd_tps65992_load();
    pd_tps65992_charging_disable(true);
}

bool pd_source_pin_read(void)
{
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
                                                 
	u32_t val=0;
 
    if(gpio_dev != NULL)
    {
        gpio_pin_read(gpio_dev, GPIO_PIN_PD_SOURCE, &val);
    }
	return (val == 1);
}

bool pd_int_pin_read(void)
{                                                
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
                                                 
	u32_t val=0;
 
    if(gpio_dev != NULL)
    {
        gpio_pin_configure(gpio_dev, TI_PD_INT_PIN, GPIO_DIR_IN);
        gpio_pin_read(gpio_dev, TI_PD_INT_PIN, &val);
    }
	return (val == 0);
}


/****************************************************************************************************************
 * Auther: Totti
 * Data: 2023/12/12
 * 
*****************************************************************************************************************/

static void pd_tps65992_status_value(void)
{
    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    k_sleep(10);
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x0F, buf, 5);
    printf("Totti debug PD version:0x%x 0x%x 0x%x 0x%x 0x%x\n",buf[0], buf[1], buf[2], buf[3], buf[4]);

    k_sleep(10);
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x40, buf, 5);
    printf("Totti debug PD status:0x%x 0x%x 0x%x 0x%x 0x%x\n",buf[0], buf[1], buf[2], buf[3], buf[4]);

    k_sleep(10);
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x3f, buf, 3);
    printf("Totti debug Power status:0x%x 0x%x 0x%x \n",buf[0], buf[1], buf[2]);

}

int pd_tps65992_write_4cc(struct device * dev, char *str)
{
    u8_t buf[10] = {0};
    u8_t num_bytes;
    num_bytes = 4;
    int ret;

    printf("Totti debug write 4cc: %s\n", str);

    buf[0] = num_bytes;
    memcpy(&buf[1], str, num_bytes);
    k_sleep(2);
    ret = i2c_burst_write(dev, I2C_PD_DEV_ADDR, PD_REG_IIC_CMD_ADDR, buf, num_bytes+1);
    if(ret)
    {
         printf("%s:%d: reg 0x%x value error\n", __func__, __LINE__, PD_REG_IIC_CMD_ADDR);           
    }
    k_sleep(2);
    // k_sleep(2);
    // i2c_burst_read(dev, I2C_PD_DEV_ADDR, PD_REG_IIC_CMD_ADDR+1, buf, 5);
    // SYS_LOG_INF("REG: 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n",PD_REG_IIC_CMD_ADDR, buf[0], buf[1], buf[2], buf[3], buf[4]);


    return 0;

}
/*
int bt_mcu_send_iic_cmd_code(u8_t addr, u8_t cmd, u8_t value)
{
    u8_t buf[4] = {0}; 
    union dev_config config = {0};
    struct device *iic_dev;

    SYS_LOG_INF("[%d],addr=%d,cmd=%d,value=%d\n", __LINE__, addr, cmd, value);
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    if(iic_dev != NULL)
    {
        config.bits.speed = I2C_SPEED_STANDARD;
        i2c_configure(iic_dev, config.raw);

        buf[0] = (cmd <<4 | value);
        i2c_burst_write(iic_dev, I2C_DEV_ADDR, addr, buf, 1);
        return 0;
    }
    return -1;

}
*/
static int pd_tps65992_read_reg_value(uint8_t addr, u8_t *buf)
{
    union dev_config config = {0};
    struct device *iic_dev;
    u8_t  num_bytes = 0;
    u8_t i=0;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);


    for(i=0; i<(sizeof(pd_reg_address_len_map) / sizeof(pd_reg_addr_len_map_t)); i++)
    {
        // printf("[%s %d]  pd address: 0x%x 0x%x\n", __func__, __LINE__, addr, pd_reg_address_len_map[i].addr);
        if(addr == pd_reg_address_len_map[i].addr)
        {
            num_bytes = pd_reg_address_len_map[i].lenght;
            break;
        }
    }

    if(i >= (sizeof(pd_reg_address_len_map) / sizeof(pd_reg_addr_len_map_t)))
    {
        printf("[%s %d] dont find the pd address: 0x%x\n", __func__, __LINE__, addr);
        return 0;
    }

    if(num_bytes != 0xFF)
    {
        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, addr, buf, num_bytes);
    }else{

        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, addr, buf, 2);
        num_bytes = buf[0];
        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, addr, buf, num_bytes);
    }

    
    printf("Totti debug reg 0x%x value:  ", addr);
    for(int i=0; i< num_bytes; i++)
    {
        printf("0x%x, ", buf[i]);
    }
    printf("\n");

    return num_bytes;

}


static void pd_clear_int_event_register(pd_reg_int_event_map_t flag)

{
    u8_t buf[20] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);
    buf[0] = 0x0b;

    if(flag == PD_PLUG_IN_OUT_FLAG)
    {
        buf[1] = 0x08;
    }else if(PD_REG_INT_RECV_SOURCE_CAP_FLAG){
        buf[2] = 0x40;
    }else{
        for(int i=1; i<12; i++)
        {
            buf[i]= 0xff;
        }
        SYS_LOG_INF("[%d] clear all pd int \n", __LINE__);
    }
    
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_REG_CLEAR_EVENT_ADDR, buf, 0x4);

}


static void pd_tps65992_input_current_limit(u8_t val)
{
    u8_t buf[10] = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);


    buf[0] = 0x06;
    buf[1] = 0x6b;
    buf[2] = 0x03;
    buf[3] = 0x06;                                              // IINDPM
   
    if(val == PD_INPUT_LIMIT_500MA_CUR)
    {
        buf[4] = 0x00;                                              // 0x0032 = 500ma current;
        buf[5] = 0x32;
    }else if(val == PD_INPUT_LIMIT_1500MA_CUR)
    {
        buf[4] = 0x00;                                              // 0x0096 = 1500ma current;
        buf[5] = 0x96;
    }else{
        buf[4] = 0x01;                                              // 0x012c = 3A current;
        buf[5] = 0x2c;
    }

    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 6);
    pd_tps65992_write_4cc(iic_dev, "I2Cw");

    SYS_LOG_INF("[%d] limit current:%d \n", __LINE__, val);

    // buf[0] = 0x04;
    // buf[1] = 0x6b;
    // buf[2] = 0x06;                                              // IINDPM
    // buf[3] = 0x02;                                              
  
    // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    // pd_tps65992_write_4cc(iic_dev, "I2Cr");

    // i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    // printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x06  ,buf[0], buf[1], buf[2], buf[3]);

}


static void pd_legacy_sink_current_proccess(void)
{
    u8_t buf[10] = {0};

    if(pd_tps65992_read_reg_value(PD_REG_TRANS_POWER_STATUS, buf)!=0)
    {
        if(((buf[1] & 0xf0) == 0x30))
        {
            pd_tps65992_input_current_limit(PD_INPUT_LIMIT_500MA_CUR);
 
        }else if((buf[1] & 0xf0) == 0x40){
            pd_tps65992_input_current_limit(PD_INPUT_LIMIT_1500MA_CUR);

        }else{
            pd_tps65992_input_current_limit(PD_INPUT_LIMIT_3000MA_CUR);
        }
    }
}


static void pd_bq25798_adc_off(void)
{

    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    buf[0] = 0x05;
    buf[1] = 0x6b;
    buf[2] = 0x02;
    buf[3] = 0x2E;                                                // IINDPM
    buf[4] = 0x00;                                                //  disable adc

    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
    pd_tps65992_write_4cc(iic_dev, "I2Cw");

    printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x2E  ,buf[0], buf[1], buf[2], buf[3], buf[4]);


    // buf[0] = 0x05;
    // buf[1] = 0x6b;
    // buf[2] = 0x02;
    // buf[3] = 0x12;                                              // IINDPM
    // buf[4] = 0xA0;                                              // disable OTG
    // k_sleep(2);
    // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
    // pd_tps65992_write_4cc(iic_dev, "I2Cw");
    // printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x11  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

    buf[0] = 0x01;                                                  // length
    buf[1] = 0x01;                                                  // delay value; one second/bit
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 2);
    pd_tps65992_write_4cc(iic_dev, "DISC");

    buf[0] = 0x05;
    buf[1] = 0x6b;
    buf[2] = 0x02;
    buf[3] = 0x11;                                              // IINDPM
    buf[4] = 0x04;                                              // ship mode
    k_sleep(2);
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
    pd_tps65992_write_4cc(iic_dev, "I2Cw");
    printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x11  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

    k_sleep(5);

    // buf[0] = 0x04;
    // buf[1] = 0x6b;
    // buf[2] = 0x2E;
    // buf[3] = 0x01;                                             // IINDPM
    
    // k_sleep(2);                                      
    // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    // pd_tps65992_write_4cc(iic_dev, "I2Cr");
    // k_sleep(20);
    // i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    // printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x2E  ,buf[0], buf[1], buf[2], buf[3]);


}



void pd_tps65992_pd_24A_proccess(int val)
{
    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    SYS_LOG_INF("[%d] val:%d\n", __LINE__, val);

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    buf[0] = 0x06;
    buf[1] = 0x6b;
    buf[2] = 0x03;
    buf[3] = 0x03;                                              // IINDPM
    buf[4] = 0x00;                                              
    if(val == PD_CHARGE_LIMIT_1500MA_CUR)
    {
        buf[5] = 0x96;                                           // 0x0F0 = 1.5A current;
    }else if(val == PD_CHARGE_LIMIT_500MA_CUR)
    {
        buf[5] = 0x32;                                           // 0x2c = 500 ma
    }
    else{
        buf[5] = 0xF0;                                           // 0x0F0 = 2.4A current;
    }
   
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 6);
    pd_tps65992_write_4cc(iic_dev, "I2Cw");
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x 0x%x\n", 0x03  ,buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

}


void pd_tps65992_sink_charging_disable(bool flag)
{

    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data;
   
    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;
   
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    buf[0] = 0x05;
    buf[1] = 0x6b;
    buf[2] = 0x02;
    buf[3] = 0x0f;                                                                  // 

    pd_ti65992->sink_charging_flag = flag;

    SYS_LOG_INF("[%d] flag:%d\n", __LINE__, flag);

    if(flag)
    {
        buf[4] =  0xA6;                                                 //                                        
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
        pd_tps65992_write_4cc(iic_dev, "I2Cw");
    }else{
        buf[4] = 0xA2;                                                              //                                        
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
        pd_tps65992_write_4cc(iic_dev, "I2Cw");  
    }

    pd_tps65992_ext_reg_input(flag);
    
}

void pd_tps65992_source_send_ssrc(void)
{
    union dev_config config = {0};
    struct device *iic_dev;
   // u8_t buf[10] = {0};

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    SYS_LOG_INF("[%d] \n", __LINE__);

    k_sleep(2);
   // pd_tps65992_sink_charging_disable(false);
    pd_bq25798_disable_full_charge(false);
    
    // k_sleep(5);
    // buf[0] = 0x01;                                                  // length
    // buf[1] = 0x03;                                                  // delay value; one second/bit
    // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 2);
    // pd_tps65992_write_4cc(iic_dev, "DISC");
  //  k_sleep(5);
  //  pd_tps65992_write_4cc(iic_dev, "SWSr");
}


void pd_tps65992_pd_sink_only_mode(bool flag)
{
    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;
    
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data;
   
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);


    SYS_LOG_INF("[%d] otg_off:%d\n\n", __LINE__, flag);

    pd_ti65992->chk_current_flag = flag;

    if(flag)
    {
        k_sleep(2);
        buf[0] = 0x01;  
        buf[1] = 0x00;                                                                           // port sink only 
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_REG_PORT_CONFIGURE_ADDR, buf, 2);
    
    }else{

        buf[0] = 0x05;
        buf[1] = 0x6b;
        buf[2] = 0x02;      
        buf[3] = 0x16;                              // IINDPM
        buf[4] = 0xc8;                                
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
        pd_tps65992_write_4cc(iic_dev, "I2Cw");

    
        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
        SYS_LOG_INF("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x16  ,buf[0], buf[1], buf[2], buf[3]);


        k_sleep(10);
        buf[0] = 0x01;  
        buf[1] = 0x02;                                                                          // pord DRP mode
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_REG_PORT_CONFIGURE_ADDR, buf, 2);

        k_sleep(500);

        buf[0] = 0x05;
        buf[1] = 0x6b;
        buf[2] = 0x02;                                                 // IINDPM
        buf[3] = 0x16;
        buf[4] = 0xc0;                                
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
        pd_tps65992_write_4cc(iic_dev, "I2Cw");

        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
        SYS_LOG_INF("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x16  ,buf[0], buf[1], buf[2], buf[3]);


    //     pd_tps65992_write_4cc(iic_dev, "DISC");


    //     u8_t temp_value;
    //     buf[0] = 0x04;
    //     buf[1] = 0x6b;
    //     buf[2] = 0x12;
    //     buf[3] = 0x01;                                             // IINDPM
                                            
    //     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    //     pd_tps65992_write_4cc(iic_dev, "I2Cr");

    //     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    //     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x09  ,buf[0], buf[1], buf[2], buf[3]);

    //     temp_value = buf[2] & 0xBF;

    //     buf[0] = 0x05;
    //     buf[1] = 0x6b;
    //     buf[2] = 0x02;
    //     buf[3] = 0x12;  
    //   //  buf[4] = temp_value; 
    //     buf[4] = 0;

    //     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
    //     pd_tps65992_write_4cc(iic_dev, "I2Cw");
    
        // buf[0] = 0x04;
        // buf[1] = 0x6b;
        // buf[2] = 0x02;
        // buf[3] = 0x0f;                                    // battery
        // buf[4] = 0xA6;                                    // limit current = 0;

        // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);	
        // pd_tps65992_write_4cc(iic_dev, "I2Cw");

        // k_sleep(2);
        // i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
        // printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3],buf[4]);


    }


}

int pd_bq25798_sink_check_charging(void)
{
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data;
    u8_t buf[10] = {0};

    if(!pd_ti65992->i2c_dev)
        return -1;

    if(!pd_ti65992->pd_65992_sink_flag)
        return 0;


    if(pd_ti65992-> sink_judge_count)
    {
        return 0;
    }


    buf[0] = 0x04;
    buf[1] = 0x6b;
    buf[2] = 0x0f;
    buf[3] = 0x01;                                             // IINDPM
    
    k_sleep(2);                                      
    i2c_burst_write(pd_ti65992->i2c_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    pd_tps65992_write_4cc(pd_ti65992->i2c_dev, "I2Cr");
    k_sleep(2);
    i2c_burst_read(pd_ti65992->i2c_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    printf("Totti debug charge reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x0f  ,buf[0], buf[1], buf[2], buf[3]);

    if((pd_ti65992->sink_charging_flag) && (!(buf[2] & 0x04)))                 // check 0f_reg = 0xa6;  high z
    {   
        pd_tps65992_sink_charging_disable(true);
    }
    return 0;
}

void pd_bq25798_source_check_current(void)
{
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data; 
    static  u8_t otg_debounce_cnt = 0x00; 
    pd_manager_charge_event_para_t para;   

    if(pd_ti65992->pd_65992_source_flag)
    {
        if(!pd_ti65992->chk_current_flag)
        {
            int16_t value = (pd_ti65992->cur_value>= 0 ? pd_ti65992->cur_value : -pd_ti65992->cur_value);
            if((value >= 0) && (value <= 63))
            {
                if(otg_debounce_cnt++ == WLT_OTG_DEBOUNCE_TIMEOUT) {
                    otg_debounce_cnt = 0;
                    para.pd_event_val = 1;
                    pd_ti65992->notify(PD_EVENT_SOURCE_OTG_CURRENT_LOW, &para);            
                }     
            }else{
                otg_debounce_cnt = 0;
                para.pd_event_val = 0;
                pd_ti65992->notify(PD_EVENT_SOURCE_OTG_CURRENT_LOW, &para); 
            }
        }
    }else{
        otg_debounce_cnt = 0;
    }
}


void pd_bq25798_source_volt_compensation(void)
{
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data; 
    static  u8_t cur_debounce_cnt = 0x00; 
    static bool have_volt_flag = 0;
    static u16_t old_volt = 0;
    u16_t volt_value = 0; 
    u8_t buf[10] = {0};
   
    struct device *iic_dev;
    

    if(pd_ti65992->pd_65992_source_flag)
    {
        if((pd_ti65992->pd_bt_runtime_status == BT_RUNTIME_STATUS_POWEROFF) && (pd_ti65992->volt_value < 5700))
        {
            iic_dev = device_get_binding(CONFIG_I2C_0_NAME);

            int16_t value = (pd_ti65992->cur_value>= 0 ? pd_ti65992->cur_value : -pd_ti65992->cur_value);
            volt_value = value * (0.075/10);

            SYS_LOG_INF("[%d] com_volt:%d; cnt:%d \n", __LINE__, volt_value, cur_debounce_cnt);
            
            if(have_volt_flag)
            {
                if((value <= 1800))
                {
                    if(cur_debounce_cnt++ >= 2) {
                        cur_debounce_cnt = 0;
                        have_volt_flag = false;

                        buf[0] = 0x06;
                        buf[1] = 0x6b;
                        buf[2] = 0x03;
                        buf[3] = 0x0b;                                              // battery
                        buf[5] = (old_volt & 0xff);                                   
                        buf[4] = ((old_volt>>8) & 0xff);
                                 
                        
                        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 6);
                        pd_tps65992_write_4cc(iic_dev, "I2Cw");

                        printf("[%d] old_volt:%d, 0x%x:%x\n", __LINE__, old_volt, buf[4],buf[5]);

                    }     
                }else{
                    cur_debounce_cnt = 0;
                }
            }else{
                if(value >= 2000)
                {
                    if(cur_debounce_cnt++ >= 2) {
                        cur_debounce_cnt = 0;
                        have_volt_flag = true;

                        volt_value = value * (0.075/10);

                        buf[0] = 0x04;
                        buf[1] = 0x6b;
                        buf[2] = 0x0b;                                              // IINDPM
                        buf[3] = 0x02;                                              
  
                        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
                        pd_tps65992_write_4cc(iic_dev, "I2Cr");
                        
                        i2c_burst_read(pd_ti65992->i2c_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
                        printf("Treg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x0b  ,buf[0], buf[1], buf[2], buf[3]);

                        old_volt = (buf[2]<<8) | buf[3];

                       // if(volt_value > 20)                                         // 
                        {
                            volt_value = 14;                                            // 140mv    
                        }
                        volt_value += old_volt;

                        buf[0] = 0x06;
                        buf[1] = 0x6b;
                        buf[2] = 0x03;
                        buf[3] = 0x0b;    
                        buf[5] = (volt_value & 0xff);                                            // battery                                                             
                        buf[4] = ((volt_value>>8) & 0xff);
                        
                        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 6);
                        pd_tps65992_write_4cc(iic_dev, "I2Cw");

                        printf("[%d] old_volt:%d, volt_volt:%d; 0x%x:%x\n", __LINE__, old_volt, volt_value, buf[4],buf[5]);

                    }     
                }else{
                    cur_debounce_cnt = 0;
                }
            }    
            

        }else{
            have_volt_flag = false;
            cur_debounce_cnt = 0;
        }
    }else{
        have_volt_flag = false;
        cur_debounce_cnt = 0;
    }
}




int pd_bq25798_read_current(int16 *volt_val, int16 *cur_val)
{

    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;
   
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);


    // buf[0] = 0x04;
    // buf[1] = 0x6b;
    // buf[2] = 0x0d;
    // buf[3] = 0x01;                                             // IINDPM
    // k_sleep(2);                                            
    // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    // k_sleep(2);
    // pd_tps65992_write_4cc(iic_dev, "I2Cr");

    // i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
 //   printf("Totti od reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x;\n", 0x09  ,buf[0], buf[1], buf[2], buf[3]);

    buf[0] = 0x04;
    buf[1] = 0x6b;
    buf[2] = 0x35;
    buf[3] = 0x02;                                             // IINDPM
    k_sleep(2);                                        
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    k_sleep(2);
    pd_tps65992_write_4cc(iic_dev, "I2Cr");
    k_sleep(5);
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
  //  printf("Totti volt reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x; %d\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], ((buf[2]<<8) | buf[3]));

    *volt_val = (buf[2]<<8) | buf[3];

    buf[0] = 0x04;
    buf[1] = 0x6b;
    buf[2] = 0x31;
    buf[3] = 0x02;                                             // IINDPM
    k_sleep(2);                                        
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    k_sleep(2);
    pd_tps65992_write_4cc(iic_dev, "I2Cr");
    k_sleep(5);
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
  //  printf("Totti debug current reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x09  ,buf[0], buf[1], buf[2], buf[3]);

    *cur_val = (buf[2]<<8) | buf[3];
   // cur_val = cur_val>=0 ? cur_val : -cur_val;
    
    if(*cur_val > 10000)
    {
	    memset(buf, 0x00, sizeof(buf));
	    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x03, buf, 5);
	    printk("[%s,%d] buf:0x%X 0x%X 0x%X 0x%X 0x%X\n", __FUNCTION__, __LINE__, buf[0], buf[1], buf[2], buf[3], buf[4]);
        if((buf[1]=='P') && (buf[2]=='T') && (buf[3]=='C') && (buf[4]=='H'))
        {
            pd_tps65992_load();
        }
    }

    // i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x5c, buf, 4);
    // printf("TPS56992 reg 0x%x value: 0x%x, 0x%x 0x%x; 0x%x\n", 0x5c  ,buf[0], buf[1], buf[2], buf[3]);

    return 0;

}




void pd_tps65992_source_current_proccess(u8_t state)
{

    u8_t    buf[100] = {0};
    u8_t    charge_current = 0;
    //   static  bool status_change_flag = false;
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data;
        
    union  dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    memset(buf, 0x00, sizeof(buf));

    printf("[%s %d] status:%d \n", __func__, __LINE__, state);

    pd_ti65992->pd_bt_runtime_status = state;

    // buf[0] = 0x01;
    // buf[1] = 0x01;
    // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x75, buf, 0x2);	
    // k_sleep(2);

    // pd_tps65992_write_4cc(iic_dev, "ALRT"); 
    // k_sleep(200);

    switch(state){
        case BT_RUNTIME_STATUS_MUSIC_PLAY:
            buf[0] = 0x35;
            buf[1] = 1;
            buf[2] = 0x02;
            buf[4] = 0x32;                                              // 5v/500mA ,must be write first   
            buf[5] = 0x90;
            buf[6] = 0x01;
            buf[7] = 0x10;
            charge_current = 0x0d;  

     //       status_change_flag = true;
            break;

        case BT_RUNTIME_STATUS_NORMAL:
            buf[0] = 0x35;
            buf[1] = 1;
            buf[2] = 0x02;
            buf[4] = 0x64;                                              // 5v/1000mA ,must be write first   
            buf[5] = 0x90;
            buf[6] = 0x01;
            buf[7] = 0x10;
            charge_current = 0x1b;
            break;

        case BT_RUNTIME_STATUS_POWEROFF:       

            buf[0] = 0x35;
            buf[1] = 3;
            buf[2] = 0x2A;
            buf[4] = 0x2c;                                              // 5v/3A ,must be write first   
            buf[5] = 0x91;
            buf[6] = 0x01;
            buf[7] = 0x04;

            buf[8] = 0xc8;                                              // 9v/2A 
            buf[9] = 0xd0;
            buf[10] = 0x02;
            buf[11] = 0x00;

            buf[12] = 0x28;                                              // pps5v~11v 
            buf[13] = 0x32;
            buf[14] = 0xdc;                             // dc = 11v ; b4= 9v
            buf[15] = 0xc0;
            charge_current = 0x4d;
            break;

        default:
            break;    
    }

    k_sleep(2);
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_REG_TRANS_SOURCE_ADDR, buf, 0x35);	
    pd_tps65992_write_4cc(iic_dev, "SSrC");


    buf[0] = 0x05;
    buf[1] = 0x6b;
    buf[2] = 0x02;
    buf[3] = 0x0d;                                              // battery
    buf[4] = charge_current;                                    // 0.5 mv; 1A; 3A

    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);	
    pd_tps65992_write_4cc(iic_dev, "I2Cw");

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x0d  ,buf[0], buf[1], buf[2], buf[3], buf[4]); 
    k_sleep(2);

    if((state == BT_RUNTIME_STATUS_POWEROFF) && (!pd_ti65992->pd_65992_sink_flag))
    {
        
        if(pd_ti65992->pd_65992_source_flag)
        {
            pd_ti65992->pd_source_disc_debunce_cnt = MAX_SOURCE_DISC_COUNT;
        }
        pd_ti65992->chk_current_flag = true;

        buf[0] = 0x01;                                                  // length
        buf[1] = 0x01;                                                  // delay value; one second/bit
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 2);
        pd_tps65992_write_4cc(iic_dev, "DISC");

    }

}


void   pd_sink_test_mode(bool flag)
{
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data; 

    pd_ti65992->charge_sink_test_flag = flag;

    SYS_LOG_INF("[%d] sink test flag: %d \n", __LINE__, pd_ti65992->charge_sink_test_flag);
    
}



void pd_switch_volt(void)
{

    u32_t   ldo_temp_value = 0;
    u8_t buf[20];

    SYS_LOG_INF("[%d] sink_pd_flag:%d; %d, %d \n", __LINE__, g_sink_ldo_data.sink_pd_flag, g_sink_ldo_data.ldo_index, g_sink_ldo_data.ldo_max);


    if(!g_sink_ldo_data.sink_pd_flag)
    {
        return;
    }

    if(++g_sink_ldo_data.ldo_index >= g_sink_ldo_data.ldo_max)
    {   
        g_sink_ldo_data.ldo_index = 0x00;
    }


    ldo_temp_value = g_sink_ldo_data.buf[(g_sink_ldo_data.ldo_index*4)+2] | (g_sink_ldo_data.buf[(g_sink_ldo_data.ldo_index*4)+3]<<8) |         \
            (g_sink_ldo_data.buf[(g_sink_ldo_data.ldo_index*4)+4]<<16) | (g_sink_ldo_data.buf[(g_sink_ldo_data.ldo_index*4)+5]<<24); 


    SYS_LOG_INF("[%d] ldo_index:%d, ldo volt:%x \n", __LINE__, g_sink_ldo_data.ldo_index, ldo_temp_value);


    memset(buf, 0x00, sizeof(buf));

    buf[0] = 0x1;
    buf[1] = g_sink_ldo_data.ldo_index + 1;


    
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_REG_TRANS_SINK_ADDR, buf, 2);	

    k_sleep(10);
    pd_tps65992_write_4cc(iic_dev, "ANeg");


}

void pd_tps65992_ext_reg_input(bool flag)
{
    u8_t buf[20];
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    SYS_LOG_INF("[%d] flag=%d \n", __LINE__, flag);

    buf[0] = 0x05;
    buf[1] = 0x6b;
    buf[2] = 0x02;
    buf[3] = 0x14;   
    buf[4] = 0x1c;
    if(flag)                                           // battery
    {  
        buf[4] = 0x1e;                                    // enable ext-reg 
    }
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);	
    pd_tps65992_write_4cc(iic_dev, "I2Cw");

    if(flag)
    {
        pd_tps65992_input_current_limit(PD_INPUT_LIMIT_3000MA_CUR);
    }


}



/********************************************************************************************
 * 0:9     ; max current
 * 10:19   ; voltage
 * 20:21   ; peak currtent
 * 24      ; unchunked extended message support
 * 26      ; usb suspend supported
 * 30:31   ; supply type

 * 0:9     ; max current
 * 10:19   ; voltage
 * 20:21   ; Peak current
 * 22      ; EPR mode capable
 * 30:31   ; supply type
 * 
 * 0xb4=9v; 0x64=5v; 0xf0=12v; 0x12c=15v; 0x190=20v;
 ********************************************************************************************/

static bool pd_bq25798_check_int(void)
{
    u8_t    buf[20] = {0};


    if(pd_tps65992_read_reg_value(PD_REG_INT_EVENT_ADDR, buf)!=0)
    {
        if(buf[2] & 0x40)                       // Source Capabalities Message Received 
        {
            k_sleep(2);
            //pd_clear_int_event_register(PD_REG_INT_RECV_SOURCE_CAP_FLAG);
            pd_clear_int_event_register(PD_CLEAR_ALL_FLAG);
            
            return true;
        }
    }
    return false;
}

static void pd_tps65992_pd_prio_process_init()
{
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data; 

    pd_ti65992->sink_judge_count = MAX_SINK_JUDGE_TIME;   
    SYS_LOG_INF("[%d] \n", __LINE__);
}


static void pd_tps65992_pd_priority_process(bool flag)
{

    u8_t    buf[100] = {0};
//    u32_t   reg_value = 0;
    u8_t    i,j;
    u32_t   ldo_value = 0;
    u32_t   ldo_temp_value = 0;
    u8_t    ldo_usb_com_value = 0;
    // check pd state for 500 msecond
    u32_t   ldo_index = 0x00;
    u8_t    ldo_nums = 0;
    u8_t    ldo_pps_flag = false;
    u8_t    pps_max_volt = 0;
    u8_t    pps_min_volt = 0;
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data; 
    union dev_config config = {0};
    struct device *iic_dev;


    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    // k_sleep(2);
    // for(i=0; i<5; i++)
    // {
    //     if(pd_tps65992_read_reg_value(PD_REG_INT_EVENT_ADDR, buf)!=0)
    //     {
    //         if(buf[2] & 0x40)                       // Source Capabalities Message Received 
    //         {
    //             k_sleep(10);
    //             pd_clear_int_event_register(PD_REG_INT_RECV_SOURCE_CAP_FLAG);
    //             break;
    //         }
    //     }
    //     k_sleep(500);
    // }

    // if(i >= 5){
    //     printf("[%s:%d] cannot detect pd sink\n", __func__, __LINE__);
    //    // return;                                         // not pd sink
    // }

    if((!pd_ti65992-> sink_judge_count) || (!pd_ti65992->pd_65992_sink_flag))
    {
        return;
    }


    if(pd_bq25798_check_int() == false)
    {
        
        if(--pd_ti65992->sink_judge_count != 0)
        {
            SYS_LOG_INF("[%d], pd dont check sink int flag:%d", __LINE__, pd_ti65992->sink_judge_count);
            return;
        }else{
            SYS_LOG_INF("[%d] cannot detect pd sink\n", __LINE__);
        }
    }

    pd_ti65992-> sink_judge_count = 0;
   
    k_sleep(2);    
    if(pd_tps65992_read_reg_value(PD_REG_REV_SOURCE_ADDR, buf)==0)
    {
        printf("[%s:%d] read recieve source reg error \n", __func__, __LINE__);
        return;
    }

     ldo_nums = buf[1];
     ldo_temp_value = buf[2] | (buf[3]<<8) | (buf[4]<<16) | (buf[5]<<24);
     ldo_usb_com_value = buf[6];

    if(((ldo_nums== 0)) || (ldo_temp_value==0) )         // ldo number no equal 0
    {

        printf("[%s:%d] detect pd recieve source num is zero\n", __func__, __LINE__);
        if(pd_tps65992_read_reg_value(PD_REG_TRANS_POWER_STATUS, buf)!=0)
        {
            if(((buf[1] & 0xf0) == 0x30) || ((buf[1] & 0xf0) == 0x40))
            {
 
                printf("[%s:%d] detect sdp & cdp protocol\n", __func__, __LINE__); 
                
                if((buf[1] & 0xf0) == 0x30)                 // 0x30 sdp, 0x40 dcp
                {
                    pd_tps65992_input_current_limit(PD_INPUT_LIMIT_500MA_CUR);
                }else{
                    pd_tps65992_input_current_limit(PD_INPUT_LIMIT_1500MA_CUR);
                }

                k_sleep(500);   
                if(pd_tps65992_read_reg_value(PD_REG_REV_SINK_ADDR, buf)!=0)
                {
                    ldo_temp_value = buf[2] | (buf[3]<<8) | (buf[4]<<16) | (buf[5]<<24);
                    if(((buf[1] & 0x7) != 0) && (ldo_temp_value != 0))
                    {
                        printf("[%s:%d] sdp & cdp = 1, It is a Mobile\n", __func__, __LINE__);   
                        pd_tps65992_sink_charging_disable(true);         // yes, is mobile
                        return;     
                    }       
                }
                
                pd_tps65992_sink_charging_disable(false);
                return;
            }  
                                                                          //check mobile
        }
        
        pd_tps65992_input_current_limit(PD_INPUT_LIMIT_3000MA_CUR);
        // pd_legacy_sink_current_proccess();
        pd_tps65992_sink_charging_disable(false);
        return;                  // goto __charge_current;
    }

    if((ldo_nums == 1) && ((ldo_temp_value & 0x3ff) <= 0x12c)                         \
                && ((((ldo_temp_value >> 10) & 0x3ff) == pd_ldo_sink_prior_map[0].value)))  // check mobile
    {

        k_sleep(1000);    
        if(pd_tps65992_read_reg_value(PD_REG_REV_SINK_ADDR, buf)!=0)
        {
            if((buf[1] & 0x7) != 0) 
            {
                if(ldo_temp_value & 0x4000000)
                {
                    pd_tps65992_sink_charging_disable(true);            // yes, is mobile
                    printf("[%s:%d] have sink and bit34=1, It is a Mobile\n", __func__, __LINE__);  
                    return;     
                }

            }else{
                if(!(ldo_temp_value & 0x8000000))                       // 30 reg, 35 bit =0;  0 means the power source have limited power
                {
                    printf("[%s:%d] 30 reg bit[35]=0, It is a Mobile\n", __func__, __LINE__); 
                    pd_tps65992_sink_charging_disable(true);         // yes, is mobile
                    return;   
                }
            }
        }
    }


    // 12V-->15V-->9V--->20V-->5V
    // 0xb4=9v; 0x64=5v; 0xf0=12v; 0x12c=15v; 0x190=20v; 
    ldo_index = 0x00;
    for(i=0; i< ldo_nums; i++)
    {
        ldo_temp_value = buf[(i*4)+2] | (buf[(i*4)+3]<<8) | (buf[(i*4)+4]<<16) | (buf[(i*4)+5]<<24); 
        
        for(j=0; j< sizeof(pd_ldo_sink_prior_map)/sizeof(pd_ldo_sink_priority_map_t); j++)
        {

            if(((ldo_temp_value >> 30) & 0x03) == 0)                                    
            {
                 SYS_LOG_INF("[%d], ldo volt: 0x%x;\n", __LINE__, ((ldo_temp_value >> 10) & 0x3ff));

                if( ((ldo_temp_value >> 10) & 0x3ff) == pd_ldo_sink_prior_map[j].value)
                {
                    if(ldo_index < pd_ldo_sink_prior_map[j].priority )
                    {
                        ldo_index = pd_ldo_sink_prior_map[j].priority;
                        ldo_value = ldo_temp_value;
                        ldo_pps_flag = false;
                        SYS_LOG_INF("[%d], ldo index: %d;\n", __LINE__, ldo_index);
                    }
                }
            }else if(((ldo_temp_value >> 30) & 0x03) == 3)                                  // if 1, pps flag
            {
                pps_max_volt = (ldo_temp_value >> 17) & 0xff;
                pps_min_volt = (ldo_temp_value >> 8) & 0xff;

                SYS_LOG_INF("[%d], pps max_volt: %d; min_volt:%d\n", __LINE__, pps_max_volt, pps_min_volt);

                if((pd_ldo_sink_prior_map[j].volt <= pps_max_volt ) && (pd_ldo_sink_prior_map[j].volt >= pps_min_volt))
                {
                    if(ldo_index < pd_ldo_sink_prior_map[j].priority )
                    {
                        ldo_index = pd_ldo_sink_prior_map[j].priority;
                        ldo_value = ldo_temp_value;
                        ldo_pps_flag = true;
                        SYS_LOG_INF("[%d], ldo index: %d;\n", __LINE__, ldo_index);
                    }
                }
  
            }
        }
    }

    memcpy(g_sink_ldo_data.buf, buf, sizeof(buf));
    g_sink_ldo_data.ldo_index = 0;
    g_sink_ldo_data.ldo_max = ldo_nums;
    g_sink_ldo_data.sink_pd_flag = true;

//__charge_current:

    if(!pd_ti65992->charge_sink_test_flag)
    {
        memset(buf, 0x00, sizeof(buf));
        buf[0] = 0x35;
        buf[1] = 1;
        buf[2] = 0x2c;                                              // 5v/3A ,must be write first   
        buf[3] = 0x91;
        buf[4] = 0x01;
        buf[5] = 0x10;

        if(ldo_index)
        {
            buf[1] = 2;
            buf[6] = ldo_value & 0xff;                              // 5v/3A ,must be write first   
            buf[7] = (ldo_value>>8) & 0xff;
            buf[8] = (ldo_value>>16) & 0xff;
            buf[9] = (ldo_value>>24) & 0xff;

        }
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_REG_TRANS_SINK_ADDR, buf, 0x35);	
    }else{
        buf[1] = g_sink_ldo_data.ldo_index + 1;
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_REG_TRANS_SINK_ADDR, buf, 0x2);	
    }

    if(pd_tps65992_read_reg_value(PD_REG_TRANS_SINK_ADDR, buf)==0)
    {
        printf("[%s:%d] read recieve source reg error \n", __func__, __LINE__);
        return;
    }

    if(ldo_pps_flag)
    {
        if(pd_tps65992_read_reg_value(PD_REG_AUTO_SINK_ADDR, buf)==0)
        {
            SYS_LOG_INF("[%d] read recieve auto sink reg error \n", __LINE__);
            return;
        }

        // setting current;
        buf[13] =  ldo_value & 0x3f;

        // setting volt;
        ldo_temp_value = pd_ldo_sink_prior_map[ldo_index-1].volt * 5;

        buf[14] = (ldo_temp_value << 1) & 0xfE;
        buf[15] = (ldo_temp_value >> 7) & 0x7;

        k_sleep(10);
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_REG_AUTO_SINK_ADDR, buf, 0x18);	
        k_sleep(10);

    }
    pd_tps65992_write_4cc(iic_dev, "GSrC");
    k_sleep(200);
    pd_tps65992_sink_charging_disable(false);

}

void pd_bq25798_disable_full_charge(bool flag)
{

    u8_t    buf[20] = {0};
    struct device *iic_dev;


    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);


        buf[0] = 0x05;
        buf[1] = 0x6b;
        buf[2] = 0x02;
        buf[3] = 0x00;
        buf[4] = 0x0E;       
        if(flag)
        {
             buf[4] = 0x1A;       
        }
                                                // IINDPM
        k_sleep(2);                                        
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
        k_sleep(2);
        pd_tps65992_write_4cc(iic_dev, "I2Cw");

        k_sleep(2);
        buf[0] = 0x01;                                                  // length
        buf[1] = 0x07;                                                  // delay value; one second/bit
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 2);
        if(flag)
        {
            pd_tps65992_write_4cc(iic_dev, "GPsl");                         //1 = disable
        }else
        {
            pd_tps65992_write_4cc(iic_dev, "GPsh");                         //0 = enable     
        }
           
}


void pd_bq25798_full_charge_check(void)
{

    u8_t    buf[20] = {0};
    union dev_config config = {0};
    u16_t   volt_value;
    struct device *iic_dev;
    static u8_t pre_charge_count = 0;
    
  
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data;
    static bool recharge_flag = 0x00;
    pd_manager_charge_event_para_t para;

    if(!pd_ti65992->pd_65992_sink_flag)
    {
        pre_charge_count = 0;
        return;
    }
      
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    if(!pd_ti65992->charge_full_flag)
    {

        buf[0] = 0x04;
        buf[1] = 0x6b;
        buf[2] = 0x1c;                                  // 1c reg
        buf[3] = 0x01;                                             // IINDPM
        k_sleep(2);                                        
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
        k_sleep(2);
        pd_tps65992_write_4cc(iic_dev, "I2Cr");
        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
        printf("Totti reg 0x%x value: 0x%x, 0x%x,0x%x, 0x%x \n", 0x1C  ,buf[0], buf[1], buf[2], buf[3]);

        if((buf[2] & 0xe0) == 0xe0)                         // == 7 charge termination Done
        {

            para.pd_event_val = 1;
            pd_ti65992->notify(PD_EVENT_SINK_FULL, &para);   

            pd_bq25798_disable_full_charge(true);    
            pd_ti65992->charge_full_flag = true;
            recharge_flag = false;

        }else if((buf[2] & 0xe0) == 0x40)                   // = 2
        {
            if(!pd_ti65992->pre_charge_flag)
            {
                SYS_LOG_INF("[%d] in pre charge...\n", __LINE__);

                if(pre_charge_count++ > 3)
                {
                    pd_ti65992->pre_charge_flag = true;
                }
            }
 
        } 
        else{
            pre_charge_count = 0;
        }


        if(pd_ti65992->pre_charge_flag)
        {
            SYS_LOG_INF("[%d] check pre charge time out! \n", __LINE__);
            
            buf[0] = 0x04;
            buf[1] = 0x6b;
            buf[2] = 0x24;                                  // 1c reg
            buf[3] = 0x01;                                             // IINDPM
            k_sleep(2);                                        
            i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
            k_sleep(2);
            pd_tps65992_write_4cc(iic_dev, "I2Cr");
      
            i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
            printf("Totti reg 0x%x value: 0x%x, 0x%x,0x%x, 0x%x \n", 0x24  ,buf[0], buf[1], buf[2], buf[3]);

            if(buf[2] & 0x2)
            {
                SYS_LOG_INF("[%d] pre charge timer done!", __LINE__);
                pd_ti65992->pre_charge_flag = false;

                buf[0] = 0x05;
                buf[1] = 0x6b;
                buf[2] = 0x02;
                buf[3] = 0x0f;                                                                  // 
                buf[4] = 0x82;                                                              //                                        
                i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
                pd_tps65992_write_4cc(iic_dev, "I2Cw");  
            }
        }

    }else
    {
        // check bat volt

        buf[0] = 0x04;
        buf[1] = 0x6b;
        buf[2] = 0x3b;
        buf[3] = 0x02;                                             // IINDPM
        k_sleep(2);                                        
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
        pd_tps65992_write_4cc(iic_dev, "I2Cr");
        k_sleep(2);
        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
        printf("Totti bat volt reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x; %d\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], ((buf[2]<<8) | buf[3]));

        volt_value = (buf[2]<<8) | buf[3];

        if( volt_value <= 8000)
        {
            para.pd_event_val = 0;
            pd_ti65992->notify(PD_EVENT_SINK_FULL, &para);   
            pd_bq25798_disable_full_charge(false); 
            pd_ti65992->charge_full_flag = false;
        }

    }
 
}

// static void pd_read_bq25789_value_test(void)
// {

//     u8_t buf[10] = {0};
//     union dev_config config = {0};
//     struct device *iic_dev;

//     iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
//     config.bits.speed = I2C_SPEED_FAST;
//     i2c_configure(iic_dev, config.raw);

//     buf[0] = 0x04;
//     buf[1] = 0x6b;
//     buf[2] = 0x03;
//     buf[3] = 0x02;

//     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
	
//     memset(buf, 0x00, sizeof(buf));
//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     pd_tps65992_write_4cc(iic_dev, "I2Cr");

//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     buf[0] = 0x04;
//     buf[1] = 0x6b;
//     buf[2] = 0x06;
//     buf[3] = 0x02;

//     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
	
//     memset(buf, 0x00, sizeof(buf));
//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     pd_tps65992_write_4cc(iic_dev, "I2Cr");

//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     buf[0] = 0x06;
//     buf[1] = 0x6b;
//     buf[2] = 0x03;
//     buf[3] = 0x06;
//     buf[4] = 0x00;
//     buf[5] = 0xF0;

//     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 6);
	
//     memset(buf, 0x00, sizeof(buf));
//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 6);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4],  buf[5]);

//     pd_tps65992_write_4cc(iic_dev, "I2Cw");

//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);


//     buf[0] = 0x04;
//     buf[1] = 0x6b;
//     buf[2] = 0x03;
//     buf[3] = 0x02;

//     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
	
//     memset(buf, 0x00, sizeof(buf));
//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     pd_tps65992_write_4cc(iic_dev, "I2Cr");

//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     buf[0] = 0x04;
//     buf[1] = 0x6b;
//     buf[2] = 0x06;
//     buf[3] = 0x02;

//     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
	
//     memset(buf, 0x00, sizeof(buf));
//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     pd_tps65992_write_4cc(iic_dev, "I2Cr");

//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);
// }

/****************************************************************************************************************
 * Auther: Totti
 * Data: 2023/12/12
 * 
*****************************************************************************************************************/


// static const struct wlt_pd_ti65992_config pd_ti65992_wlt_cdata = {
//     .gpio_name = CONFIG_GPIO_ACTS_DEV_NAME,
//     .iic_name = CONFIG_I2C_0_NAME,
//     .poll_interval_ms = MCU_RUN_TIME_PERIOD,
//     .one_second_count = MCU_ONE_SECOND_PERIOD,
// };

static uint8_t pd_tps65992_get_status_reg()
{
    u8_t    buf[10] = {0};

    u32_t   reg_value = 0;
    u8_t    result = 0;
  
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data;
    

    if(!pd_ti65992->pd_65992_sink_flag)
        return 0;


    if(pd_ti65992->pd_sink_legacy_flag == PD_SINK_STATUS_LEGACY)
    {
        return 0;
    }
    
    if(pd_tps65992_read_reg_value(PD_REG_STATUS_ADDR, buf)!=0)
    {
        reg_value = (buf[4] << 24) | (buf[3] << 16) | (buf[2] << 8) | buf[1];
        result = (reg_value >> PD_STATUS_REG_SINK_LEGACY_SHIFT_BIT) & PD_STATUS_REG_SINK_LEGACY_MASK; 


        if(result == PD_SINK_STATUS_LEGACY)
        {
            pd_ti65992->pd_sink_legacy_flag = result;
            pd_legacy_sink_current_proccess();
        }
    }

    return 0;
}


void pd_detect_event_report(void){

    pd_manager_charge_event_para_t para;
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data;
    static uint8_t source_debounce_cnt = 0;
    u8_t    buf[10] = {0};
   // u32_t   reg_value = 0;

    if (!pd_ti65992->notify) {
        printk("[%s:%d] pd notify function did not register!\n", __func__, __LINE__);
        return;
    }


    if(!pd_ti65992->pd_65992_sink_flag)
    {
        if(pd_ti65992->pd_source_disc_debunce_cnt > 0)
        {
                SYS_LOG_INF("[%d]; disc_debounce_cnt = %d\n", __LINE__, pd_ti65992->pd_source_disc_debunce_cnt);
                pd_ti65992->pd_source_disc_debunce_cnt--;
                if(pd_ti65992->pd_source_disc_debunce_cnt == 0)
                {
                    pd_ti65992->chk_current_flag = false;
                }
                
        }else
        {
            if(pd_source_pin_read() != pd_ti65992->pd_65992_source_flag)  
            {

                if(source_debounce_cnt > 0)
                {
                    source_debounce_cnt--;
                    SYS_LOG_INF("[%d]; source_debounce_cnt = %d\n", __LINE__, source_debounce_cnt);

                    pd_tps65992_read_reg_value(PD_REG_TYPE_C_STATUS, buf);    

                        
                }else{
                    source_debounce_cnt = 0;
                    pd_ti65992->pd_65992_source_flag = pd_source_pin_read();
                    printf("%s:%d; source_flag = %d\n", __func__, __LINE__, pd_ti65992->pd_65992_source_flag);

                    para.pd_event_val = pd_ti65992->pd_65992_source_flag;
                    if(pd_ti65992->pd_65992_source_flag)
                    {
                        source_debounce_cnt = 2;
                        k_sleep(20);
                   
                        pd_tps65992_read_reg_value(PD_REG_TRANS_POWER_STATUS,buf);
                        printf("Totti debug Power status:0x%x 0x%x 0x%x \n",buf[0], buf[1], buf[2]);
                        if((buf[1]&0x0f) != 0x3)
                        {
                            para.pd_event_val |= 0x10;
                        }

                    }
                    pd_ti65992->source_judge_count = 0; 
                    pd_ti65992->notify(PD_EVENT_SOURCE_STATUS_CHG, &para);   
                    
                }
            }else{
                if(pd_ti65992->pd_65992_source_flag)
                {
                    if(pd_ti65992->source_judge_count > 10)
                    {
                        pd_ti65992->source_judge_count = 100;


                    }else{
                        pd_ti65992->source_judge_count++; 

                        if(pd_tps65992_read_reg_value(PD_REG_STATUS_ADDR, buf)!=0)
                        {
                            
                            //reg_value = (buf[4] << 24) | (buf[3] << 16) | (buf[2] << 8) | buf[1];
                           
                            if((buf[5] & 0x03) == 2)                                                       // usb plug in/out int
                            {
                                pd_ti65992->source_judge_count = 100;
                                pd_ti65992->notify(PD_EVENT_SOURCE_LEGACY_DET, &para);
                            }
                        }
                    } 

                    source_debounce_cnt = 2;  

                }
            }  
        }
    }

    if(!pd_ti65992->pd_65992_source_flag)
    {
        if(dc_power_in_status_read() != pd_ti65992->pd_65992_sink_flag)
        {
            printf("%s:%d \n", __func__, __LINE__);
            pd_ti65992->pd_65992_sink_flag = dc_power_in_status_read();
            pd_ti65992->pd_sink_legacy_flag = 0x00;
            pd_ti65992->sink_charging_flag = pd_ti65992->pd_65992_sink_flag;
            
            printf("%s:%d; sink_flag = %d\n", __func__, __LINE__, pd_ti65992->pd_65992_sink_flag);
            
            if(!pd_ti65992->pd_65992_sink_flag)
            {
                pd_ti65992->charge_full_flag = false;
                pd_bq25798_disable_full_charge(false);  
                pd_ti65992->pre_charge_flag = false;
                pd_tps65992_ext_reg_input(true);
                para.pd_event_val = 0x00;
                pd_ti65992->notify(PD_EVENT_SINK_FULL, &para); 
                pd_clear_int_event_register(PD_REG_INT_RECV_SOURCE_CAP_FLAG);
            }

            g_sink_ldo_data.sink_pd_flag = false;
            pd_ti65992->sink_judge_count = 0; 
            
            para.pd_event_val = pd_ti65992->pd_65992_sink_flag;
            pd_ti65992->notify(PD_EVENT_SINK_STATUS_CHG, &para);


        }
    }



    if((pd_ti65992->pd_65992_sink_flag) && (!pd_ti65992->sink_charging_flag) && (pd_ti65992->volt_value>8000))
    {

        if(pd_int_pin_read())
        {
            struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

            printf("TI PD int2\n");
            if(pd_tps65992_read_reg_value(PD_REG_INT_EVENT_ADDR, buf)!=0)
            {
                if(buf[1] & 0x08)                                   // usb plug in/out int
                {
                    gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G2, 0);
                }
            }
            k_sleep(2);
            pd_clear_int_event_register(PD_CLEAR_ALL_FLAG);

        }
    }
    
}

extern void mcu_iic_type_prop_handle_fn(uint8_t type, uint8_t data);
void pd_tps65992_iic_send_data()
{
    u8_t type, data;
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data;    

    while(!pd_iid_pop_queue(&type, &data))
    {

       // SYS_LOG_INF("[%d] type:%d, data:%d\n", __LINE__, type, data);   

        switch(type)
        {
            case PD_IIC_TYPE_PROP_CURRENT_2400MA:
                pd_tps65992_pd_24A_proccess(data);
                break;
                

            case PD_IIC_TYPE_PROP_SOURCE_CURRENT_500MA:
                pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_MUSIC_PLAY);  
                // pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CURRENT_500MA, (u8_t)val->intval);
                break;
            case PD_IIC_TYPE_PROP_SOURCE_CURRENT_1000MA:
                pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_NORMAL);
                //pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CURRENT_1000MA, (u8_t)val->intval);
                break; 

            case PD_IIC_TYPE_PROP_SOURCE_CHARGING_OTG:
                pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_POWEROFF);
                //pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CHARGING_OTG, (u8_t)val->intval);
                break; 

            case PD_IIC_TYPE_PROP_VOLT_PROIRITY:
                // pd 12-15-20-9-5 priority
                // pd_tps65992_pd_priority_process(val->intval);

                pd_tps65992_pd_prio_process_init();
                // pd_iic_push_queue(PD_IIC_TYPE_PROP_VOLT_PROIRITY, (u8_t)val->intval);
                break;

            case PD_IIC_TYPE_PROP_STANDBY:
                pd_bq25798_adc_off();
        
            // pd_iic_push_queue(PD_IIC_TYPE_PROP_STANDBY, (u8_t)val->intval);
                break;  


            case PD_IIC_TYPE_PROP_POWERDOWN:
                pd_tps65992_unload();
                thread_timer_stop(&pd_ti65992->timer);
                pd_ti65992->pd_65992_unload_flag = true;
                
                // pd_iic_push_queue(PD_IIC_TYPE_PROP_POWERDOWN, (u8_t)val->intval);
                break;    

            case PD_IIC_TYPE_PROP_OTG_OFF:
                if(data)
                {
                    pd_tps65992_pd_sink_only_mode(true); 
                    k_sleep(2000);
                    // pd_manager_disable_charging(true);
                    pd_tps65992_sink_charging_disable(true);
                }else{
                    pd_tps65992_pd_sink_only_mode(false); 
                    // pd_manager_disable_charging(false);
                    pd_tps65992_sink_charging_disable(false);
                }
                // pd_iic_push_queue(PD_IIC_TYPE_PROP_OTG_OFF, (u8_t)val->intval);
                break;  

            case PD_IIC_TYPE_PROP_SINK_CHANGING_ON:
                pd_tps65992_sink_charging_disable(false);
                // pd_iic_push_queue(PD_IIC_TYPE_PROP_SINK_CHANGING_ON, (u8_t)val->intval);
                break;

            case PD_IIC_TYPE_PROP_SINK_CHANGING_OFF:
                pd_tps65992_sink_charging_disable(true);
                // pd_iic_push_queue(PD_IIC_TYPE_PROP_SINK_CHANGING_OFF, (u8_t)val->intval);
                break; 

            case PD_IIC_TYPE_PROP_SOURCE_SSRC:
                pd_tps65992_source_send_ssrc();
            // pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_SSRC, (u8_t)val->intval);
                break;

            case MCU_IIC_TYPE_PROP_LED_0:
            case MCU_IIC_TYPE_PROP_LED_1:
            case MCU_IIC_TYPE_PROP_LED_2:
            case MCU_IIC_TYPE_PROP_LED_3:
            case MCU_IIC_TYPE_PROP_LED_4:
            case MCU_IIC_TYPE_PROP_LED_5:
            case MCU_IIC_TYPE_PROP_LED_BT:
            case MCU_IIC_TYPE_PROP_LED_AURACAST:
                mcu_iic_type_prop_handle_fn(type,data);
            break;
            default:
                break;               
        }
    }

}


extern int mcu_mspm0l_int_deal(void);
extern int mcu_mspmol_ota_deal(void);
extern int mcu_ui_ota_deal(void);

static void mcu_pd_iic_time_hander(struct thread_timer *ttimer, void *expiry_fn_arg)
{
    // const struct wlt_pd_ti65992_config *cfg = p_pd_ti65992_dev->config->config_info;
    struct wlt_pd_ti65992_info *pd_ti65992 = p_pd_ti65992_dev->driver_data;
    static u8_t ti65992_one_secound_count = 0;
    static u8_t ti65992_five_secound_count = 0;


   // printf("%s %d\n", __func__, __LINE__);
    //if(mcu_mspmol_ota_deal()){
    if(mcu_ui_ota_deal()){    
        return;
    }

    //mcu_mspm0l_int_deal();
    if(pd_ti65992->mcu_notify)
    {
        pd_ti65992->mcu_notify();
    }
 
    pd_detect_event_report();

    pd_tps65992_iic_send_data();

    if(pd_ti65992->pd_65992_unload_flag)
    {
        return;
    }

    if(++ti65992_one_secound_count < (MCU_ONE_SECOND_PERIOD))
    {
        if(ti65992_one_secound_count == MCU_HALF_SECOUD_PERIOD)
        {
            pd_tps65992_pd_priority_process(false);
        }
        return;
    }
    ti65992_one_secound_count = 0;

    pd_tps65992_pd_priority_process(false);

    pd_tps65992_get_status_reg();
    pd_bq25798_read_current(&pd_ti65992->volt_value, &pd_ti65992->cur_value);
    SYS_LOG_INF("[%d], voltage: %d; current: %d\n", __LINE__, pd_ti65992->volt_value, pd_ti65992->cur_value);

    pd_bq25798_source_volt_compensation();

    pd_bq25798_sink_check_charging();
    pd_bq25798_source_check_current();

    
    if(pd_ti65992->battery_notify)
    {
         pd_ti65992->battery_notify();
    }

    if(ti65992_five_secound_count++ < MCU_FIVE_SECOND_PERIOD)
    {
        return;
    }
    ti65992_five_secound_count = 0x00;

    pd_bq25798_full_charge_check();

}

static int pd_ti65992_wlt_get_property(struct device *dev,enum pd_manager_supply_property psp,     \
				       union pd_manager_supply_propval *val)
{
    struct wlt_pd_ti65992_info *pd_ti65992 = dev->driver_data;

    switch(psp)
    {
        case PD_SUPPLY_PROP_SINK_VOLT_VAULE:
            val->intval = pd_ti65992->volt_value;
            break;

        case PD_SUPPLY_PROP_PLUG_PRESENT:
            // val->intval = pd_ti65992->plug_present_flag;
            // val->intval = (pd_ti65992->pd_65992_sink_flag);
            val->intval = dc_power_in_status_read();
            break;

        case PD_SUPPLY_PROP_SINK_HIGH_Z_STATE:

            val->intval = 0x00;                                    // no charging
            if(pd_ti65992->pd_65992_sink_flag && (!pd_ti65992->sink_charging_flag))
            {
                val->intval = 0x01;                         // charging 
            }

            break;

        case PD_SUPPLY_PROP_UNLOAD_FINISHED:
           
            val->intval = pd_ti65992->pd_65992_unload_flag;                      // unload is finished
            
            break;

        default:
            break;    
    }

    return 0;
}


static void pd_ti65992_wlt_set_property(struct device *dev,enum pd_manager_supply_property psp,    \
				       union pd_manager_supply_propval *val)
{
   // struct wlt_pd_ti65992_info *pd_ti65992 = dev->driver_data;

    switch(psp)
    {
        case PD_SUPPLY_PROP_CURRENT_2400MA:
            // pd_tps65992_pd_24A_proccess(val->intval);
            pd_iic_push_queue(PD_IIC_TYPE_PROP_CURRENT_2400MA, (u8_t)val->intval);
            break;
            

        case PD_SUPPLY_PROP_SOURCE_CURRENT_500MA:
           // pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_MUSIC_PLAY);  
            pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CURRENT_500MA, (u8_t)val->intval);
            break;
        case PD_SUPPLY_PROP_SOURCE_CURRENT_1000MA:
            //pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_NORMAL);
            pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CURRENT_1000MA, (u8_t)val->intval);
            break; 

        case PD_SUPPLY_PROP_SOURCE_CHARGING_OTG:
            // pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_POWEROFF);
            pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CHARGING_OTG, (u8_t)val->intval);
            break; 

        case PD_SUPPLY_PROP_VOLT_PROIRITY:
            // pd 12-15-20-9-5 priority
            // pd_tps65992_pd_priority_process(val->intval);

            //pd_tps65992_pd_prio_process_init();
            pd_iic_push_queue(PD_IIC_TYPE_PROP_VOLT_PROIRITY, (u8_t)val->intval);
            break;

        case PD_SUPPLY_PROP_STANDBY:
            // pd_bq25798_adc_off();
    
            pd_iic_push_queue(PD_IIC_TYPE_PROP_STANDBY, (u8_t)val->intval);
            break;  

        case PD_SUPPLY_PROP_POWERDOWN:
            // pd_tps65992_unload();
            pd_iic_push_queue(PD_IIC_TYPE_PROP_POWERDOWN, (u8_t)val->intval);
            break;    

        case PD_SUPPLY_PROP_OTG_OFF:
            // if(val->intval)
            // {
            //     pd_tps65992_pd_sink_only_mode(true); 
            //     k_sleep(2000);
            //     // pd_manager_disable_charging(true);
            //     pd_tps65992_sink_charging_disable(true);
            // }else{
            //     pd_tps65992_pd_sink_only_mode(false); 
            //     // pd_manager_disable_charging(false);
            //     pd_tps65992_sink_charging_disable(false);
            // }
            pd_iic_push_queue(PD_IIC_TYPE_PROP_OTG_OFF, (u8_t)val->intval);
            break;  

        case PD_SUPPLY_PROP_SINK_CHANGING_ON:
            // pd_tps65992_sink_charging_disable(false);
            pd_iic_push_queue(PD_IIC_TYPE_PROP_SINK_CHANGING_ON, (u8_t)val->intval);
            break;

        case PD_SUPPLY_PROP_SINK_CHANGING_OFF:
            // pd_tps65992_sink_charging_disable(true);
            pd_iic_push_queue(PD_IIC_TYPE_PROP_SINK_CHANGING_OFF, (u8_t)val->intval);
            break; 

        case PD_SUPPLY_PROP_SOURCE_SSRC:
            // pd_tps65992_source_send_ssrc();
            pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_SSRC, (u8_t)val->intval);
            break;

        default:
            break;               
    }

    
}

#if 0
static void mcu_wlt_response(struct device *dev,enum pd_manager_supply_property psp,    \
				       union pd_manager_supply_propval *val)
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
#endif

void pd_ti65992_wlt_register_notify(struct device *dev, pd_manager_callback_t cb)
{
	struct wlt_pd_ti65992_info *pd_ti65992 = dev->driver_data;
	int flag;

	flag = irq_lock();
	
	if ((pd_ti65992->notify == NULL) && cb)
	{
		pd_ti65992->notify = cb;
	}else
	{
		SYS_LOG_ERR("pd notify func already exist!\n");
	}
    irq_unlock(flag);
	
}

void pd_wlt_battery_register_notify(struct device *dev, pd_manager_battery_t cb)
{
	struct wlt_pd_ti65992_info *pd_ti65992 = dev->driver_data;
    int flag;

	flag = irq_lock();
	
	if ((pd_ti65992->battery_notify == NULL) && cb)
	{
		pd_ti65992->battery_notify = cb;
	}else
	{
		SYS_LOG_ERR("battery notify func already exist!\n");
	}
	irq_unlock(flag);
}

#if 1
static void pd_ti65992_mcu_register_notify(struct device *dev, pd_manager_mcu_callback_t cb)
{
	struct wlt_pd_ti65992_info *pd_ti65992 = dev->driver_data;
    int flag;

	flag = irq_lock();		
	if ((pd_ti65992->mcu_notify == NULL) && cb)
	{
		pd_ti65992->mcu_notify = cb;
	}else
	{
		SYS_LOG_ERR("mcu notify func already exist!\n");
	}
    irq_unlock(flag);
}
#endif

static void pd_ti65992_wlt_enable(struct device *dev)
{
	// struct wlt_pd_ti65992_info *pd_ti65992 = dev->driver_data;
	// const struct wlt_pd_ti65992_config *cfg = dev->config->config_info;

    printf("[%s:%d]\n", __func__, __LINE__);

    // thread_timer_start(&pd_ti65992->timer, 0, MCU_RUN_TIME_PERIOD);
    // if (!wlt_ti65992_stack)
    //     return;

    // app_mem_free(wlt_ti65992_stack);
    // wlt_ti65992_stack = NULL;
	
}

static void pd_ti65992_wlt_disable(struct device *dev)
{
	// struct wlt_pd_ti65992_info *pd_ti65992 = dev->driver_data;
	// const struct wlt_pd_ti65992_config *cfg = dev->config->config_info;

    // thread_timer_stop(&pd_ti65992->timer);
}


static const struct pd_manager_supply_driver_api pd_ti65992_wlt_driver_api = {
	.get_property = pd_ti65992_wlt_get_property,
	.set_property = pd_ti65992_wlt_set_property,
	.register_pd_notify = pd_ti65992_wlt_register_notify,
    .register_mcu_notify = pd_ti65992_mcu_register_notify,
    //.mcu_response = mcu_wlt_response,
    .register_bat_notify = pd_wlt_battery_register_notify,
    .enable     = pd_ti65992_wlt_enable,
	.disable    = pd_ti65992_wlt_disable,

};


// static void  wlt_ti65992_proceess_thread(void *p1, void *p2, void *p3)
// {
//     struct wlt_pd_ti65992_info *pd_ti65992 = NULL; 
//     union dev_config config = {0};
//     pd_manager_charge_event_para_t para;

//     pd_tps65992_load();
//     k_sleep(50);

//     pd_ti65992 = p_pd_ti65992_dev->driver_data;

//     if(!pd_ti65992->i2c_dev)
//     {
//         printf("[%s,%d] iic_dev not found\n", __FUNCTION__, __LINE__);
//     }else{
//         config.bits.speed = I2C_SPEED_FAST;
// 	    i2c_configure(pd_ti65992->i2c_dev, config.raw);
//     }

//     pd_tps65992_write_4cc(pd_ti65992->i2c_dev, "DBfg");
//     pd_tps65992_status_value();

//     if (!pd_ti65992->notify) {
//         printk("[%s:%d] pd notify function did not register!\n", __func__, __LINE__);
//         return;
//     }

//     para.pd_event_val = 0;
//     pd_ti65992->notify(PD_EVENT_INIT_OK, &para); 
// }

int pd_ti65992_init(void)
{

    struct wlt_pd_ti65992_info *pd_ti65992 = NULL; 

    if(p_pd_ti65992_dev != NULL)
    {

        // pd_ti65992 = p_pd_ti65992_dev->driver_data;
        // pd_tps65992_write_4cc(pd_ti65992->i2c_dev, "SWSr");

        SYS_LOG_INF("[%d] had inited yet \n", __LINE__);
        return -1;
    }

    p_pd_ti65992_dev = &g_pd_ti65992_dev;
  //  const struct wlt_pd_ti65992_config *cfg; 
    
    p_pd_ti65992_dev->driver_api = &pd_ti65992_wlt_driver_api;
    p_pd_ti65992_dev->driver_data = &pd_ti65992_ddata;
  //  p_pd_ti65992_dev->config = (const struct device_config *)&pd_ti65992_wlt_cdata;

    pd_ti65992 = p_pd_ti65992_dev->driver_data;
 //   cfg = p_pd_ti65992_dev->config->config_info;
    pd_ti65992->pd_65992_unload_flag = false;


	pd_ti65992->gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!pd_ti65992->gpio_dev) 
	{
		printk("[%s,%d] gpio_dev not found\n", __FUNCTION__, __LINE__);
		
	}else{
        if(!run_mode_is_demo()){
	        gpio_pin_configure(pd_ti65992->gpio_dev, GPIO_PIN_USB_D_LINE, GPIO_DIR_OUT | GPIO_POL_NORMAL | GPIO_PUD_PULL_UP);
	        gpio_pin_write(pd_ti65992->gpio_dev, GPIO_PIN_USB_D_LINE, 1);
        }
        gpio_pin_configure(pd_ti65992->gpio_dev, GPIO_PIN_PD_SOURCE, GPIO_DIR_IN | GPIO_PUD_PULL_DOWN);
        gpio_pin_configure(pd_ti65992->gpio_dev, TI_PD_INT_PIN, GPIO_DIR_IN);

    }
    pd_ti65992->i2c_dev = device_get_binding(CONFIG_I2C_0_NAME);
   
    thread_timer_init(&pd_ti65992->timer, mcu_pd_iic_time_hander, NULL);
    
    pd_tps65992_load();
    
    if(!pd_ti65992->i2c_dev)
    {
        printf("[%s,%d] iic_dev not found\n", __FUNCTION__, __LINE__);
    }else{
        union dev_config config = {0};
        config.bits.speed = I2C_SPEED_FAST;
	    i2c_configure(pd_ti65992->i2c_dev, config.raw);
    }

    pd_tps65992_status_value();

    k_sleep(5);
    pd_tps65992_write_4cc(pd_ti65992->i2c_dev, "DBfg");
    pd_bq25798_disable_full_charge(false);
    pd_clear_int_event_register(PD_CLEAR_ALL_FLAG);


extern void mspm0l_ota_init(void);
    mspm0l_ota_init();
    
    thread_timer_start(&pd_ti65992->timer, MCU_RUN_TIME_PERIOD, MCU_RUN_TIME_PERIOD);

    
    
    // if(!wlt_ti65992_stack)
    // {
    //     wlt_ti65992_stack = app_mem_malloc(1024);
    //     if (wlt_ti65992_stack) {
    //         os_thread_create(wlt_ti65992_stack, 1024, wlt_ti65992_proceess_thread,
	// 	             NULL, NULL, NULL, 8, 0, 0);
    //     }else{
    //         SYS_LOG_ERR("wlt_ti65992_stack mem_malloc failed");
    //     }
    // }

    return 0;
}

struct device *wlt_device_get_pd_ti65992(void)
{
     return p_pd_ti65992_dev;
}





       
// DEVICE_AND_API_INIT(pd_ti65992, "pd_ti65992", pd_ti65992_init,
// 	    &pd_ti65992_ddata, &pd_ti65992_wlt_cdata, POST_KERNEL,
// 	    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &pd_ti65992_wlt_driver_api);

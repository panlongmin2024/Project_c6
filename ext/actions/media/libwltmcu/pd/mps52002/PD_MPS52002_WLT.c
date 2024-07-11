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
#include "pd_mps52002_image.h"
#include <property_manager.h>
#include "run_mode/run_mode.h"
#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif

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

#define mps_pd_current_version 0x3b
#define I2C_DEV_ADDR        0x48                    //TODO
#define I2C_PD_DEV_ADDR     0x27
// #define DEF_MCU_WATER_INT_PIN 0





enum bt_runtime_status_map_t{
    BT_RUNTIME_STATUS_MUSIC_PLAY,
    BT_RUNTIME_STATUS_NORMAL,
    BT_RUNTIME_STATUS_POWEROFF,

};

enum pd_status_sink_legacy_map_t{
    PD_SINK_STATUS_PD,
    PD_SINK_STATUS_LEGACY,
};


enum pd_sink_source_map_t{
    PD_STATUS_SINK_IN,
    PD_STATUS_SOURCE_OUT,
};


typedef struct {
    u16_t    value;
    u8_t    priority;
} pd_ldo_sink_priority_map_t;


struct wlt_pd_mps52002_info {
	struct thread_timer timer;
	struct device *gpio_dev;
    struct device *i2c_dev;
    pd_manager_callback_t notify;
    pd_manager_battery_t  battery_notify;
	pd_manager_mcu_callback_t mcu_notify;
    
    u32_t   pd_52002_sink_flag:1;
    u32_t   pd_sink_legacy_flag:3;
    u32_t   pd_52002_source_flag:1;
	u32_t   pd_52002_HIZ_flag:1;
    u32_t   otg_mobile_flag:1;
    u32_t   chk_current_flag:1;
	u32_t   sink_charging_flag:1;
	u32_t   charge_sink_test_flag:1;
	u32_t   pd_source_otg_disable_flag:1;
	u32_t   charge_full_flag:1;
	u32_t   pd_65992_unload_flag:1;

   // u32_t   otg_debounce_cnt;
	int16   volt_value;
    int16   cur_value;
	int16   src_volt_value;
	int16   src_cur_value;
	u8_t   	SRC_5OMA_FLAG;
	u8_t   	pd_version;
	u8_t	pd_sink_debounce_time;
	u8_t    pd_source_disc_debunce_cnt;


};

typedef struct {
    /** pd register address */
	u8_t addr;

    /** pd register value length */
	u8_t lenght;

} pd_reg_addr_len_map_t;


typedef struct {
    /** pd_link_status */
	bool    dc_detect_in_flag;
    /** _PD status */
	u16_t   dc_sink_charge_count;
    u32_t   pd_sink_source_flag:1;
    u32_t   pd_sink_legacy_flag:3;
    u32_t   app_music_flag:1;
    u32_t   otg_flag:1;
	

     /** _app music current status */
    u8_t    bt_app_mode;
  
} pd_manage_info_t;
void wake_up_pd(void);
void MPS_SET_SOURCE_SSRC(bool OnOFF);
void pd_mps52002_iic_send_data(void);


static void mps_ota_get_status(u8_t *status);

#define MCU_RUN_TIME_PERIOD 10

#define MCU_ONE_SECOND_PERIOD           ((1000/MCU_RUN_TIME_PERIOD))
#define MAX_DC_POWER_IN_TIMER			20

#define WLT_OTG_DEBOUNCE_TIMEOUT        8
#define MAX_SOURCE_DISC_COUNT           13
#define MAX_SINK_CHECK_MOBILE_TIME		2

enum ti_pd_reg_address_t{

    PD_OTG_IOUT_STATUS       = 0x00, //Read only
    PD_FIRMWARE_ID           = 0x01,//Read only 15:8 MPF52002_CHIP_ID SAMPLE 02 :MSP52002       
    PD_PD_CONTROL            = 0x02, // bit2:1  default 00 ; SRC Setting 00:18w 01:5w(5v1a) 10:2.5w(5v0.5a);bit0 sink disable
                                    
    PD_PD_STATUS       		= 0x03, ////Read only bit1 :src mode bit0: sink mode

    PD_CHARGE_CURRENT      = 0x04,//bit7:0 default :(3a)
    PD_SYS_POWER_STATUS    = 0x05, //if bt speaker to bit :01 sink to src
    PD_MOSFET_CTRL     		= 0x06, //mosfet contro PB3 OR PB6
	PD_MPS_07_REG			= 0x7,
	PD_MPS_09_REG			= 0x9,
	PD_MPS_0A_REG			= 0x0a,
    PD_MP2760_STATUS_0  	= 0X16,//
    PD_MP2760_STATUS_1  	= 0X17,//

	PD_MPS_23_REG			= 0x23,
	PD_MPS_24_REG			= 0x24,

	PD_MPS_2C_REG			= 0x2c,


    PD_DISABLE_SOURCE  		 =0X40,//disable source 
	PD_MPS_41_REG			= 0x41,

	PD_MPS_AA_REG			= 0xaa,
	PD_MPS_AF_REG			= 0xaf,
	PD_MPS_BA_REG			= 0xba,
	PD_MPS_BB_REG			= 0xbb,
	PD_MPS_DD_REG			= 0xdd,
    
};
void MPS_PB2_LOW(bool OnOFF);
void MPS_ONOFF_BGATE(bool OnOFF);
static u8_t ota_flag = 0;

#define GPIO_PIN_USB_D_LINE                 42
#define GPIO_PIN_PD_RST                          4
#define GPIO_PIN_PA6                           19
#define GPIO_PIN_EXTEND_RST                 44
#define GPIO_PIN_PD_SOURCE                  15


static struct wlt_pd_mps52002_info pd_mps52002_ddata;
static struct device g_pd_mps52002_dev = {0};
static struct device *p_pd_mps52002_dev = NULL;                                                

/****************************************************************************************************************
 * Auther: Totti
 * Data: 2023/12/12
 * 
*****************************************************************************************************************/


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



static int pd_tps52002_read_reg_value(uint8_t addr, u8_t *buf, int len)
{
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
	
    k_sleep(1);  
	if(i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, addr, buf, len)<0)
	{
		k_sleep(50);
		SYS_LOG_ERR("[%d] IIC first read %d Error!\n", __LINE__, addr);
		if(i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, addr, buf, 2)<0)
		{
			SYS_LOG_ERR("[%d] IIC read %d Error!\n", __LINE__, addr);
			return -1;
		}
	}

	if(addr  == PD_PD_STATUS)
	{
		return 0;
	}

    printf("\nTotti debug reg 0x%x value:  ", addr);
    for(int i=0; i< len; i++)
    {
        printf("0x%x, ", buf[i]);
    }
    printf("\n");
	return 0;

}
static int pd_mps52002_write_reg_value(uint8_t addr, u8_t *buf, int len)
{
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    
	config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);


	printf("live wirte reg 0x%x \n", addr);

	if(i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, addr, buf, len) <0)
	{
		k_sleep(50);

		SYS_LOG_ERR("[%d] IIC first write %d Error!\n", __LINE__, addr);

		if(i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, addr, buf, len) <0)
		{
			SYS_LOG_ERR("[%d] IIC write %d Error!\n", __LINE__, addr);
			return -1;
		}
	}
	return 0;
}



void pd_mps52002_pd_24A_proccess(int val)
{
    u8_t buf[2] = {0};
    // union dev_config config = {0};
    // struct device *iic_dev;
	
    // iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    // config.bits.speed = I2C_SPEED_STANDARD;
    // i2c_configure(iic_dev, config.raw);JBLCH6-524

  	if(val)
    	buf[0] = 0x1e;
  	else
  		buf[0] = 0x30;
  
    buf[1] = 0x00;

	pd_mps52002_write_reg_value(PD_CHARGE_CURRENT, buf, 2);
	
	k_sleep(1);
	pd_tps52002_read_reg_value(PD_CHARGE_CURRENT, buf, 2);
    printf("%s reg 0x%x value: 0x%x, 0x%x \n",__func__, PD_CHARGE_CURRENT  ,buf[0], buf[1]);

}

void pd_mps52002_pd_otg_on(bool flag)
{
    u8_t buf[2] = {0};
    // union dev_config config = {0};
    // struct device *iic_dev;
	struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;

    // iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    // config.bits.speed = I2C_SPEED_STANDARD;
    // i2c_configure(iic_dev, config.raw);
	
	wake_up_pd();
		
   	if(flag)
    {
        buf[0] = 0X01;
        buf[1] = 0x00; 
		pd_mps52002->pd_source_otg_disable_flag = true; 

   	}
    else
	{
      	buf[0] = 0X00;
      	buf[1] = 0x00;   
		pd_mps52002->pd_source_otg_disable_flag = false; 
	}

	pd_mps52002_write_reg_value(PD_DISABLE_SOURCE, buf,2);
	printf(" pd_tps52002_pd_otg_on PD_DISABLE_SOURCE write = %d \n",buf[0]);

	k_sleep(10);
	pd_tps52002_read_reg_value(PD_DISABLE_SOURCE, buf, 2);


    
}


bool mps_ota_set_status(int status)
{
    int ret;

    SYS_LOG_INF("%d\n", status);
    ret = property_set_int(CFG_MPS_OTA_STATUS, status);
    if (ret != 0)
    {       
        SYS_LOG_ERR("ota_flag property set err\n");
        return false;
    }
	property_flush(CFG_MPS_OTA_STATUS);
    return true;
}

static void mps_ota_get_status(u8_t *status)
{
	char temp[8];

	memset(temp, 0, sizeof(temp));

	int ret = property_get(CFG_MPS_OTA_STATUS, temp, sizeof(temp));
	SYS_LOG_INF("  %x\n", ret);
	if (ret >= 0)
		*status = atoi(temp);
	else
		*status = 0;

	SYS_LOG_INF("  %x\n", *status);
}
int OTA_PD(void)
{
	// struct device *iic_dev;
	// union dev_config config = {0};

	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	
	printf("live  OTA_PD start \n");
	
	gpio_pin_write(gpio_dev, GPIO_PIN_PD_RST, 1);
	k_sleep(10); //delay 10ms
	gpio_pin_write(gpio_dev, GPIO_PIN_PD_RST, 0);

	k_sleep(60); //delay 10ms
	
	// task_wdt_add(TASK_WDT_CHANNEL_MAIN_APP, 50000, NULL, NULL);
	// iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
	// config.bits.speed = I2C_SPEED_STANDARD;
	// i2c_configure(iic_dev, config.raw);     

	u8_t request_cmd[] = { 0x4D, 0x50, 0x53, 0x53, 0x54, 0x41, 0x52, 0x54 };
	
	//i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0xaa, request_cmd, sizeof(request_cmd));

	pd_mps52002_write_reg_value(PD_MPS_AA_REG, request_cmd, sizeof(request_cmd));

	printf("live  OTA_PD 333 \n");
	k_sleep(850); //delay 850ms
		
	//Check on status regsiter 0xBA D[0] == 1
	int count = 5;
	u8_t readdata;
	
	while (count-- > 0)
	{
		if(pd_tps52002_read_reg_value(PD_MPS_BA_REG, &readdata, 1) >= 0x00)
		{	
			if (readdata == 0x01)
			{
				break;
			}
		}

		k_sleep(100); //delay 100ms
		printf("[%s %d]  count:%d \n", __func__, __LINE__, count);
	}

	mps_ota_set_status(0x55);

	if( count <= 0 )
	{
		printf("[%s %d]  enter firmware update mode fail! \n", __func__, __LINE__);
		return 0;
	}
		
	// mps_ota_set_status(0x55);
	//Data transferring
//	u8_t *fw_data;
	//fw_data = PD_buff;//{0x00,0x00};//read_fw_data();
	//memcpy(fw_data,PD_buff,sizeof(PD_buff));
	int data_len = sizeof(fw_data);//fw_data.Length;
	int res = data_len % 128;
	int frame_index = 0;
	printf("live  OTA_PD 444 data_len = %d \n",data_len);
	
	for(int i=0; i<(data_len-res); i+=128)
	{		
		//u8_t data[64];// = fw_data.Skip(i).Take(512).ToArray();
		//memcpy(data, &fw_data[i],64);
		//u8_t crc_value = crc8_calulate(data, 512);
		u8_t frame[129];
		frame[0]= frame_index/4;
		//printf("live  OTA_PD 444 frame_index = %02x\n ",frame_index/8);
		memcpy(&frame[1], &fw_data[i], 128);
		//for(int j =0 ;j < 65;j++)
		//	printf("0x%02x,",frame[j]);
			//frame[513]=crc_value;
		// i2c_write(iic_dev, frame, 65, 0xAF);
		// i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR,0xAF, frame, 129);

		pd_mps52002_write_reg_value(PD_MPS_AF_REG, frame, 129);

		frame_index++;
		if(frame_index % 4 == 0)
		{
			k_sleep(60); //delay 50ms before read index and crc status
		}
		
		#ifdef CONFIG_TASK_WDT
			task_wdt_feed_all();
		#endif
	}
	if (res > 0)
	{
		//u8_t data1[res];
		int flowDataLen = 0;
		flowDataLen = data_len-res;
		//printf("live  OTA_PD 888 flowDataLen = %02x\n",flowDataLen);
		//memcpy(data1,  &fw_data[flowDataLen],res);
		//u8_t crc_value1 = crc8_calulate(data1, sizeof(data1));
		u8_t frame1[res+1];
		frame1[0] = frame_index/4;
			//printf("[%s %d]  = %d\n", __func__, __LINE__,frame1[0]);
		// printf("live  OTA_PD 444 frame1 = %02x\n",frame_index/8);
		memcpy(&frame1[1], &fw_data[flowDataLen], res);
			//for(int jj =0 ;jj <(res+1);jj++)
			//printf("0x%02x,",frame1[jj]);
		// frame1[res+1]=crc_value1;
			// i2c_write(iic_dev, frame1, res+1, 0xAF);
		
		// i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR,0xAF, frame1, res+1);

		pd_mps52002_write_reg_value(PD_MPS_AF_REG, frame1, res+1);

		k_sleep(60); //delay 50ms before read index and crc status

	}
	

	k_sleep(30); //delay 50ms before read index and crc status
	//Terminate data transferring
	u8_t stop_cmd[] =  {0x4D, 0x50, 0x53, 0x45, 0x4E, 0x44, 0x53, 0x5A};
	// i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0xdd, stop_cmd  , sizeof(stop_cmd));
	pd_mps52002_write_reg_value(PD_MPS_DD_REG, stop_cmd, sizeof(stop_cmd));
	
	
	//Check on status regsiter 0xBA D[1] == 1
	count = 5;
	while (count-- > 0)
	{

		pd_tps52002_read_reg_value(PD_MPS_BA_REG, &readdata, 1);	

		printf("readdata = 0x%02x\n",(readdata & 0x02));
		if ((readdata & 0x02) > 0)
		{
			break;
		}
		k_sleep(100); //delay 100ms
	}

	if ( count <= 0) 
	{	 
		mps_ota_set_status(0x55);
		printf("[%s %d] OTA_PD FAIL!!!!!!!!!!!!!!!!!!\n", __func__, __LINE__);
		return 0;
	}
	else
	{
	    printf("[%s %d] OTA_PD SUCESS!!!!!!!!!!!!!!!!!!\n", __func__, __LINE__);
		mps_ota_set_status(0x88);
	}
	//Jump to execute new Firmware
	u8_t jump_cmd = 0xAC;
	
	// i2c_burst_write(iic_dev,I2C_PD_DEV_ADDR,0xbb,&jump_cmd ,1); 
	pd_mps52002_write_reg_value(PD_MPS_BB_REG, &jump_cmd, 1);
	
	k_sleep(10); //delay 10ms
	gpio_pin_write(gpio_dev, GPIO_PIN_PD_RST, 1);
	k_sleep(10); //delay 10ms
	gpio_pin_write(gpio_dev, GPIO_PIN_PD_RST, 0);
	printf("[%s,%d] OTA_PD end\n", __FUNCTION__, __LINE__);
	return 1;
}	

static void pd_mps52002_status_value(void)
{
    u8_t buf[4] = {0};
	u8_t buf1[2] = {0x01,0x00};
	u8_t buf2[2] = {0x02,0x00};
	struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
    // union dev_config config = {0};
    // struct device *iic_dev;
	mps_ota_get_status(&ota_flag);
    printf("[%s %d] ota_flag:%d \n", __func__, __LINE__, ota_flag);
   
    // iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    // config.bits.speed = I2C_SPEED_STANDARD;
    // i2c_configure(iic_dev, config.raw);

   
	pd_tps52002_read_reg_value(PD_FIRMWARE_ID, buf, 2);
	pd_mps52002->pd_version = buf[0];
	printf("plm debug PD version:0x%x 0x%x \n",buf[0], buf[1]);
    if(ota_flag == 0x55 ||(pd_mps52002->pd_version < mps_pd_current_version))
	{
	  	OTA_PD();
	 	k_sleep(100);
    }
	
    printf("live debug PD version:0x%x 0x%x \n",buf[0], buf[1]);
	k_sleep(10);
	//i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x06, buf2, 2);
	pd_mps52002_write_reg_value(PD_MOSFET_CTRL, buf2, 2);

	
	k_sleep(1);
	printf("live debug 0x07 status:0x%x 0x%x  \n",buf[0], buf[1]);
	// i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x07, buf1, 2);
	pd_mps52002_write_reg_value(PD_MPS_07_REG, buf1, 2);
    
	k_sleep(10);
	pd_tps52002_read_reg_value(PD_SYS_POWER_STATUS, buf, 2);
	printf("live debug Power status:0x%x 0x%x  \n",buf[0], buf[1]);
	
	k_sleep(10);
	pd_tps52002_read_reg_value(PD_PD_STATUS, buf, 2);
    printf("live debug PD status:0x%x 0x%x \n",buf[0], buf[1]);

	k_sleep(10);
   
}

void pd_mps52002_source_current_proccess(u8_t state)
{
	struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
    u8_t    buf[2] = {0};
        
    printf("[%s %d] status:%d \n", __func__, __LINE__, state);

    memset(buf, 0x00, sizeof(buf));
	wake_up_pd();

    switch(state){
        case BT_RUNTIME_STATUS_MUSIC_PLAY:
            buf[1] = 0x00;
            buf[0] = 0x04;//5v0.5a
            break;

        case BT_RUNTIME_STATUS_NORMAL:
            buf[1] = 0x00;
            buf[0] = 0x02; //5v1a
            break;

        case BT_RUNTIME_STATUS_POWEROFF:       
            buf[1] = 0x00;
            buf[0] = 0x00; //9a/2a
            break;

        default:
            break;    
    }

    // union  dev_config config = {0};
    // struct device *iic_dev;
    // iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    // config.bits.speed = I2C_SPEED_STANDARD;
    // i2c_configure(iic_dev, config.raw);

    // k_sleep(50);
  	// if(power_manager_get_battery_capacity() <= 15)
  	// {
    //         buf[1] = 0x00;
    //         buf[0] = 0x08; //disable src
  	// }

	printf("live write  0x02 value: 0x%x, 0x%x\n",buf[0], buf[1]); 
    // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_PD_CONTROL, buf, 2);	
	pd_mps52002_write_reg_value(PD_PD_CONTROL, buf, 2);

	// pd_tps52002_read_reg_value(PD_PD_CONTROL, buf, 2);

    printf("live debug reg 0x%x value: 0x%x, 0x%x\n", 0x02 ,buf[0], buf[1]); 

	pd_mps52002->pd_source_disc_debunce_cnt = MAX_SOURCE_DISC_COUNT;
	pd_mps52002->chk_current_flag = true;

}


/*

*/

/*!
 * \brief ç”µæ± åŠå……ç”µäº‹ä»¶å®šä�? */
typedef enum
{
    PD_REG_STATUS_NO_CHANGE,
    PD_REG_STATUS_SOURCE_CHANGE,
    PD_REG_STATUS_SINK_CHANGE,
    PD_REG_STATUS_LEGACY_SINK_CHANGE,
    
} pd_mps52002_ret_status_t;



///

struct wlt_pd_mps52002_config {
	char *iic_name;
	char *gpio_name;
    u16_t poll_interval_ms;
    u8_t  one_second_count;
};




extern bool key_water_status_read(void);
extern bool media_player_is_working(void);
extern bool dc_power_in_status_read(void);


#define POWER_SUPLAY_CONTORL_PIN_G2                 51

void pd_set_G2_Low(void)
{
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

        SYS_LOG_INF("[%d] pd_set_G2_Low = 0  \n", __LINE__);
    if(gpio_dev != NULL)
    {
         gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G2, 0);
    }

}
bool pd_mps_source_pin_read(void)
{
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
                                                 
	u32_t val=0;
 
    if(gpio_dev != NULL)
    {
        gpio_pin_read(gpio_dev, GPIO_PIN_PD_SOURCE, &val);
    }
	return (val == 1);
}

#if 0
/**/
void pd_detect_event_report_MPS52002(void){

    pd_manager_charge_event_para_t para;
    struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
    uint8_t ret = 0;
	static u8_t otg_debounce_cnt = 0;

    if (!pd_mps52002->notify) {
        printk("[%s:%d] pd notify function did not register!\n", __func__, __LINE__);
        return;
    }
    if(dc_power_in_status_read() != pd_mps52002->pd_52002_sink_flag)
    {
        pd_mps52002->pd_52002_sink_flag = dc_power_in_status_read();

        para.pd_event_val = pd_mps52002->pd_52002_sink_flag;
        pd_mps52002->notify(PD_EVENT_SINK_STATUS_CHG, &para);
    }

    if(!pd_mps52002->pd_52002_sink_flag)
    {
        ret = pd_mps52002_get_status_reg();
        if(ret == PD_REG_STATUS_SOURCE_CHANGE)
        {
            para.pd_event_val = pd_mps52002->pd_52002_source_flag;
            pd_mps52002->notify(PD_EVENT_SOURCE_STATUS_CHG, &para);
        }
    }else{
        ret = pd_mps52002_get_status_reg();
        if(ret == PD_REG_STATUS_LEGACY_SINK_CHANGE)
        {
            printk("[%s %d] PD_REG_STATUS_LEGACY_SINK_CHANGE\n", __func__, __LINE__);
        }
    }
	pd_mps52002->SRC_5OMA_FLAG = pd_mps2760_read_current(&pd_mps52002->volt_value, &pd_mps52002->cur_value,&pd_mps52002->src_cur_value);

    if(pd_mps52002->pd_52002_source_flag)
    {
        if(!pd_mps52002->chk_current_flag)
        {
            int16_t value = pd_mps52002->src_cur_value;//pd_mps2760_read_current();
            value = (value>= 0 ? value : -value);
            SYS_LOG_INF("[%d] current value:%d; count:%d\n", __LINE__, value, otg_debounce_cnt);

            if(((value >= 0) && (value < 60))||pd_mps52002->SRC_5OMA_FLAG)
            {
                if(otg_debounce_cnt++ >= WLT_OTG_DEBOUNCE_TIMEOUT) {
                    para.pd_event_val = 0;
					pd_mps52002_src_small_current_otg_off(); 
					SYS_LOG_INF("[%d] count:%d\n", __LINE__, otg_debounce_cnt);
                    pd_mps52002->notify(PD_EVENT_SOURCE_OTG_CURRENT_LOW, &para);  
					otg_debounce_cnt = 0;
                }     
            }else{
                 otg_debounce_cnt = 0;
            }
        }
    }else{
          otg_debounce_cnt = 0;
    }

}
#else
extern bool bt_mcu_get_bt_wake_up_flag(void);
extern uint8_t pd_get_app_mode_state(void);
static uint8_t Wake_up_times = 0;
void pd_detect_event_report_MPS52002(void){

    pd_manager_charge_event_para_t para;
    struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
	uint8_t buf[2];
    static uint8_t readresult = 0x00;
	static uint8_t fisrtreadtimes = 0;

	static uint8_t pin_dc_power_in_debunce_count = 0;
	static uint8_t source_sink_debunce = 0x00;

	static uint8_t p_dc_power_in_flag = 0x00;

    if (!pd_mps52002->notify) {
        printk("[%s:%d] pd notify function did not register!\n", __func__, __LINE__);
        return;
    }

	if(!pd_mps_source_pin_read())
	{
		fisrtreadtimes = 0;
		// pd_mps52002->pd_source_disc_debunce_cnt = 0x00;
		// pd_mps52002->pd_sink_debounce_time = 0x00;
		// pin_dc_power_in_debunce_count = 0x00;
		// p_dc_power_in_flag = 0x00;
	}


	if(dc_power_in_status_read() != p_dc_power_in_flag)							//only for debug; Totti 2024/6/26
	{
		p_dc_power_in_flag = dc_power_in_status_read();
		SYS_LOG_INF("[%d] DC_POWER_IN = %d \n", __LINE__, p_dc_power_in_flag);
	}


	if(bt_mcu_get_bt_wake_up_flag())
    {
       Wake_up_times = 1;
       SYS_LOG_INF("[%d] wake_up_flag = 1  \n", __LINE__);
       return;
	}

	if(fisrtreadtimes <= 1) 
	{
	   fisrtreadtimes++;
	   return;
	}

	if(dc_power_in_status_read())
	{
		pin_dc_power_in_debunce_count = MAX_DC_POWER_IN_TIMER;
	}else{
		
		if(pin_dc_power_in_debunce_count > 0)
		{
			SYS_LOG_INF("%d pin_dc_power_in_debunce_count:%d \n", __LINE__, pin_dc_power_in_debunce_count);
			pin_dc_power_in_debunce_count--;

			if(pin_dc_power_in_debunce_count == 0x00)
			{
				SYS_LOG_INF("%d \n", __LINE__);
			
				if(pd_mps52002->pd_52002_source_flag)
				{
					pd_mps52002->pd_52002_source_flag = 0;
		
					SYS_LOG_INF("[%d] source_flag=%d \n", __LINE__, pd_mps52002->pd_52002_source_flag  );
			
					pd_mps52002->sink_charging_flag = 1;
					pd_mps52002->pd_52002_HIZ_flag = 0;
					pd_mps52002->pd_52002_sink_flag = 0;
					para.pd_event_val = pd_mps52002->pd_52002_source_flag;
					pd_mps52002->notify(PD_EVENT_SOURCE_STATUS_CHG, &para);
				}
		 
				if(pd_mps52002->pd_52002_sink_flag)
				{
					pd_mps52002->pd_52002_sink_flag = 0;
					SYS_LOG_INF("[%d] sink_flag:%d \n", __LINE__, pd_mps52002->pd_52002_sink_flag);
					pd_mps52002->sink_charging_flag = 0;
					pd_mps52002->pd_52002_source_flag = 0;	
					pd_mps52002->pd_52002_HIZ_flag = 0;
					pd_mps52002->charge_full_flag = 0;

					if(pd_mps52002->pd_source_otg_disable_flag)
					{
						pd_mps52002_pd_otg_on(true); 
					}
					
					para.pd_event_val = pd_mps52002->pd_52002_sink_flag;
					pd_mps52002->notify(PD_EVENT_SINK_STATUS_CHG, &para);
				}
				pd_mps52002->pd_source_disc_debunce_cnt = 0x00;
				pd_mps52002->pd_sink_debounce_time = 0x00;
			}
		}
	}

    pd_tps52002_read_reg_value(PD_PD_STATUS, buf, 2);

	
	if(readresult != buf[0])
	{
		if((readresult & 0x3) != (buf[0] & 0x3))
		{
			source_sink_debunce = 0x00;
		}

		SYS_LOG_INF("[%d] PD_PD_STATUS(0x3):0%x \n", __LINE__, buf[0]);	
	}
	
	readresult = buf[0];

	if(readresult & 0x04)													// check OTG mobile
	{

		
		if(pd_mps52002->pd_sink_debounce_time)
		{
			pd_mps52002->pd_sink_debounce_time = 0;
		}

		if(!pd_mps52002->otg_mobile_flag )
		{
			para.pd_event_val = 1;
			pd_mps52002->otg_mobile_flag = true;
			pd_mps52002->notify(PD_EVENT_OTG_MOBILE_DET, &para);
		}
		

			// SYS_LOG_INF("[%d] otg mobile BTMODE_APP_MODE \n", __LINE__);

            //  	// buf[0] =0x01;
		    //  	// pd_mps52002_write_reg_value(PD_SYS_POWER_STATUS, buf);
			//  	// buf[0] =0x00;
			//  	// pd_mps52002_write_reg_value(PD_DISABLE_SOURCE, buf);
	        // pd_mps52002->pd_52002_source_flag = 0;
			// pd_mps52002->sink_charging_flag = 1;
			//  pd_mps52002->pd_52002_HIZ_flag = 0;
			//  pd_mps52002->pd_52002_sink_flag = 1;
			//  para.pd_event_val = pd_mps52002->pd_52002_sink_flag;
			//  pd_mps52002->notify(PD_EVENT_SOURCE_STATUS_CHG, &para);
			
	}else{
		pd_mps52002->otg_mobile_flag = false;
	}
	
	
	if(!pd_mps52002->pd_52002_source_flag)
	{
		
		if((readresult&0x01) != pd_mps52002->pd_52002_sink_flag)
		{

			if(source_sink_debunce < 2 )
			{
				source_sink_debunce++ ;
				SYS_LOG_INF("[%d] source_sink_debunce:%d \n", __LINE__, source_sink_debunce);
				return;
			}

			source_sink_debunce = 0x00;
			pd_mps52002->sink_charging_flag = 1;
			pd_mps52002->pd_52002_source_flag = 0;	
			pd_mps52002->pd_52002_HIZ_flag = 0;
			pd_mps52002->pd_52002_sink_flag ^= 1;
			pd_mps52002->pd_source_disc_debunce_cnt = 0x00;
			pd_mps52002->pd_sink_debounce_time = 0;
			pd_mps52002->charge_full_flag = 0;
			if(pd_mps52002->pd_52002_sink_flag)
			{
				pd_mps52002->pd_sink_debounce_time = MAX_SINK_CHECK_MOBILE_TIME;
			}else{
				if(pd_mps52002->pd_source_otg_disable_flag)
				{
					pd_mps52002_pd_otg_on(true); 
				}
			}
			SYS_LOG_INF("[%d] sink_flag:%d \n", __LINE__, pd_mps52002->pd_52002_sink_flag);
			para.pd_event_val = pd_mps52002->pd_52002_sink_flag;
			pd_mps52002->notify(PD_EVENT_SINK_STATUS_CHG, &para);

		}
	

		if(pd_mps52002->pd_sink_debounce_time)
		{
			pd_mps52002->pd_sink_debounce_time--;
			if(!pd_mps52002->pd_sink_debounce_time)
			{
				pd_mps52002->sink_charging_flag = 0;
			}	
		}
	}
		
	if(!pd_mps52002->pd_52002_sink_flag)
	{
			
		if(pd_mps52002->pd_source_disc_debunce_cnt > 0)
		{
			SYS_LOG_INF("[%d]; disc_debounce_cnt = %d\n", __LINE__, pd_mps52002->pd_source_disc_debunce_cnt);
			pd_mps52002->pd_source_disc_debunce_cnt--;
			if(pd_mps52002->pd_source_disc_debunce_cnt == 0)
			{
				pd_mps52002->chk_current_flag = false;
			}
			
		}else
		{


			if(((readresult>>1) & 0x1) != pd_mps52002->pd_52002_source_flag)
			{
			
				if(source_sink_debunce < 2 )
				{
					source_sink_debunce++ ;
					SYS_LOG_INF("[%d] source_sink_debunce:%d \n", __LINE__, source_sink_debunce);
					return;
				}
				
				source_sink_debunce = 0x00;


				pd_mps52002->pd_52002_source_flag ^= 1;
			//	if(pd_mps52002->pd_52002_source_flag && (!pd_mps52002->pd_source_otg_disable_flag))
			//	{
			//		buf[0] =0x01;
			//		pd_mps52002_write_reg_value(PD_SYS_POWER_STATUS, buf);
			//		buf[0] =0x00;
			//		pd_mps52002_write_reg_value(PD_DISABLE_SOURCE, buf);
			//	}
				SYS_LOG_INF("[%d] source_flag = %d\n", __LINE__,pd_mps52002->pd_52002_source_flag);
				pd_mps52002->sink_charging_flag = 1;
				pd_mps52002->pd_52002_HIZ_flag = 0;
				pd_mps52002->pd_52002_sink_flag = 0;
				para.pd_event_val = pd_mps52002->pd_52002_source_flag;
				pd_mps52002->notify(PD_EVENT_SOURCE_STATUS_CHG, &para);

			}
		}
	}
	
}


bool check_sink_full(void)
{
    bool charger_status;
	u8_t buf_status[2];
	u16_t   reg_value = 0;
	pd_tps52002_read_reg_value(PD_MP2760_STATUS_0, buf_status, 2); 
	reg_value =  (buf_status[1] << 8) | buf_status[0];
	if((reg_value&0x140) == 0x140)
		charger_status = 1;
	else
		charger_status = 0;
	return charger_status;

}

void	pd_src_sink_full_check(void)
{
	struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
	pd_manager_charge_event_para_t para;
  	
	if(!pd_mps_source_pin_read())
		return;
	
	if(pd_mps52002->pd_52002_sink_flag)
	{
	  	if(check_sink_full())
	    {
	     	if(!pd_mps52002->charge_full_flag)
	    	{	    
	       		pd_mps52002->charge_full_flag = true;
	       		para.pd_event_val = 1;
	       		pd_mps52002->notify(PD_EVENT_SINK_FULL, &para); 
		   		SYS_LOG_INF("[%d] charer full =  %d", __LINE__, pd_mps52002->charge_full_flag);
	     	}
	    }else{
			if(pd_mps52002->charge_full_flag)
			{
				pd_mps52002->charge_full_flag = false;	
				para.pd_event_val = 0;
	       		pd_mps52002->notify(PD_EVENT_SINK_FULL, &para); 
		   		SYS_LOG_INF("[%d] charer full =  %d", __LINE__, pd_mps52002->charge_full_flag);

			}

			
		}
    }

}	


void pd_read_volt_current_process1(void)
{

	struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
	// struct device *iic_dev;
	// union dev_config config = {0};
	u8_t  buf[10]={0};

	if(Wake_up_times > 0)
	{
		Wake_up_times--;
	   	SYS_LOG_INF("[%d] waiting Wake_up_times = %d", __LINE__, Wake_up_times);
	   	return;
	}

	if(!pd_mps_source_pin_read())
		return;

  	// iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    // config.bits.speed = I2C_SPEED_STANDARD;
    // i2c_configure(iic_dev, config.raw);
    k_sleep(1);                                                                       
  
	pd_tps52002_read_reg_value(PD_MPS_24_REG, buf, 2);
    pd_mps52002->cur_value=(((buf[1]<<8) | buf[0])& 0x03ff)*6; 
	k_sleep(1);
	//  printf("live  volt reg 0x%x value: 0x%x, 0x%x cur_val %d\n", 0x23  ,buf[0], buf[1],*cur_val);
	
	pd_tps52002_read_reg_value(PD_MPS_23_REG, buf, 2);
	pd_mps52002->volt_value =(((buf[1]<<8) | buf[0])& 0x03ff)*20; 
	SYS_LOG_INF("mps2761 sink volt:%d, curr:%d", pd_mps52002->volt_value, pd_mps52002->cur_value);
	
}


void pd_read_volt_current_process2(void)
{
	struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
	pd_manager_charge_event_para_t para;
	static u8_t otg_debounce_cnt = 0;
	// struct device *iic_dev;
	u8_t  buf[10]={0};

	// iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
  	if(!pd_mps_source_pin_read())
		return;


	k_sleep(1);
	pd_tps52002_read_reg_value(PD_MPS_2C_REG, buf, 2);
	pd_mps52002->src_volt_value =(((buf[1]<<8) | buf[0])& 0x03ff)*20;

	k_sleep(1);
	pd_tps52002_read_reg_value(PD_OTG_IOUT_STATUS, buf, 2);
	pd_mps52002->SRC_5OMA_FLAG = buf[1]&80;
	pd_mps52002->src_cur_value =(int16)((((((buf[1]<<8) | buf[0])& 0x03ff)*0.806)-100)*5); 

	SYS_LOG_INF("mps2761 src volt:%d, cur:%d", pd_mps52002->src_volt_value, pd_mps52002->src_cur_value);
	SYS_LOG_INF("SRC OTG flag 1=full, 0=charging: 0x%x \n", pd_mps52002->SRC_5OMA_FLAG);

    if(pd_mps52002->pd_52002_source_flag)
    {
        if(!pd_mps52002->chk_current_flag)
        {
            int16_t value = pd_mps52002->src_cur_value;//pd_mps2760_read_current();
            value = (value>= 0 ? value : -value);
            SYS_LOG_INF("[%d] cur:%d; count:%d\n", __LINE__, value, otg_debounce_cnt);

            if(((value >= 0) && (value < 60)) || pd_mps52002->SRC_5OMA_FLAG)
            {
                if(otg_debounce_cnt++ >= WLT_OTG_DEBOUNCE_TIMEOUT) {
                    para.pd_event_val = 1;
					// pd_mps52002_src_small_current_otg_off(); 
					SYS_LOG_INF("[%d] count:%d\n", __LINE__, otg_debounce_cnt);
                    pd_mps52002->notify(PD_EVENT_SOURCE_OTG_CURRENT_LOW, &para);  
					otg_debounce_cnt = 0;

					
                }     
            }else{
				para.pd_event_val = 0;
                pd_mps52002->notify(PD_EVENT_SOURCE_OTG_CURRENT_LOW, &para); 
                otg_debounce_cnt = 0;
            }
        }else{
			otg_debounce_cnt = 0;
		}
    }else{
          otg_debounce_cnt = 0;
    }
}

#endif

void   pd_mps52002_sink_test_mode(bool flag)
{
    struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data; 

    pd_mps52002->charge_sink_test_flag = flag;

    SYS_LOG_INF("[%d] sink test flag: %d \n", __LINE__, pd_mps52002->charge_sink_test_flag);
    
}

void set_mps_volt(u8_t index)
{
    // union dev_config config = {0};
	// struct device *iic_dev;
    // iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    // config.bits.speed = I2C_SPEED_STANDARD;
    // i2c_configure(iic_dev, config.raw);

	u8_t buf[2];
	buf[0]= index;
	buf[1] =0x80;
    printf("live set_mps_volt  reg 0x%02x\n", index);
    // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x0a, buf, 2);
	pd_mps52002_write_reg_value(PD_MPS_0A_REG, buf, 2);
	
}

void pd_mps52002_switch_volt(void)
{
    // union dev_config config = {0};
    // struct device *iic_dev;
	u8_t buf[2];
    static u8_t PDO_index = 1;;
    // iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    // config.bits.speed = I2C_SPEED_STANDARD;
    // i2c_configure(iic_dev, config.raw);
  
   
	pd_tps52002_read_reg_value(PD_MPS_09_REG, buf, 2);

    printf("live pd_mps52002_switch_volt  reg 0x%02x\n:  ", buf[1]);
 
	 PDO_index ++;
	if(PDO_index > (buf[1]&0x07))
		PDO_index = 1;

	set_mps_volt(PDO_index);
    printf("\n");
}

bool pd_mps52002_ats_switch_volt(u8_t PDO_index)
{
    //  union dev_config config = {0};
    // struct device *iic_dev;
	u8_t buf[2];
    // iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    // config.bits.speed = I2C_SPEED_STANDARD;
    // i2c_configure(iic_dev, config.raw);
   
	pd_tps52002_read_reg_value(PD_MPS_09_REG, buf, 2);
	if(PDO_index > (buf[1]&0x07))
	{
		 return false;
	}
	set_mps_volt(PDO_index);
    return true;
}
//extern void mcu_ls8a10049t_int_deal(void);
extern int 	mcu_ui_ota_deal(void);
extern void led_status_manger_handle(void);
extern void wlt_logic_mcu_ls8a10049t_int_fn(void);
extern void batt_led_display_timeout(void);

static void mcu_pd_iic_time_hander_mps(struct thread_timer *ttimer, void *expiry_fn_arg)
{
    // const struct wlt_pd_mps52002_config *cfg = p_pd_mps52002_dev->config->config_info;
   	struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
    static u16_t mcu_one_secound_count = 0;
	static u8_t set_fisrt_flag = 0;

	led_status_manger_handle();

	if((mcu_one_secound_count % 5) == 0)
	{
		if((mcu_one_secound_count % 10) != 0)
		{

			batt_led_display_timeout();

			if(mcu_ui_ota_deal()){	 
				return;
			}

			if(pd_mps52002->mcu_notify)
			{
				pd_mps52002->mcu_notify();
			}

			pd_mps52002_iic_send_data();
			if(pd_mps52002->pd_65992_unload_flag)
			{
				return;
			}
		}else{

			
    		pd_detect_event_report_MPS52002();
    		//mcu_ls8a10049t_int_deal();
		}
	}

    if(mcu_one_secound_count++ <= MCU_ONE_SECOND_PERIOD)
    {

		switch(mcu_one_secound_count)
		{
			case (MCU_ONE_SECOND_PERIOD-12):
				if(pd_mps52002->battery_notify)
				{
					pd_mps52002->battery_notify();
				}
				wlt_logic_mcu_ls8a10049t_int_fn();
				break;

			case (MCU_ONE_SECOND_PERIOD-22):
				pd_src_sink_full_check();
				break;

			case (MCU_ONE_SECOND_PERIOD-32):
				if(pd_mps52002->charge_sink_test_flag)
				{
					if(!set_fisrt_flag)
       				{
           				set_mps_volt(1);
		   				set_fisrt_flag =1;
					}
				}
			
				break;

			case (MCU_ONE_SECOND_PERIOD-47):
				pd_read_volt_current_process1();
				break;

			case (MCU_ONE_SECOND_PERIOD-42):
				pd_read_volt_current_process2();
				break;	

			default:
			break;
		}

    }else{
		 mcu_one_secound_count = 1; 
	}
   
//	printk("[%s,%d]\n", __FUNCTION__, __LINE__);


}





static int pd_mps52002_wlt_get_property(struct device *dev,enum pd_manager_supply_property psp,     \
				       union pd_manager_supply_propval *val)
{
	uint8_t buf[2];

	struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
	
    switch(psp)
    {
        case PD_SUPPLY_PROP_SINK_VOLT_VAULE:
            val->intval = pd_mps52002->volt_value;
            break;
		case PD_SUPPLY_PROP_SINK_CURRENT_VAULE:
			 val->intval = pd_mps52002->cur_value; 
            break;
        case PD_SUPPLY_PROP_PLUG_PRESENT:
            val->intval = pd_mps52002->pd_52002_sink_flag; // dc_power_in_status_read();
            break;
		case PD_SUPPLY_PROP_SINK_HIGH_Z_STATE:
            val->intval = 0x00;  
		  	if(pd_mps52002->pd_52002_sink_flag &&(!pd_mps52002->sink_charging_flag))
		     	val->intval = 0x01;
            break;
		case PD_SUPPLY_PROP_PD_VERSION:
			val->intval = pd_mps52002->pd_version;
			break;
		case PD_SUPPLY_PROP_OTG_MOBILE:		
			  pd_tps52002_read_reg_value(PD_PD_STATUS, buf, 2);
			  val->intval = (buf[0]>>2) & 0x01;
			  SYS_LOG_INF("PD mobile status:%d", buf[0]);

			break;	
		case PD_SUPPLY_PROP_SOURCE_CURRENT_VAULE:
			 val->intval = pd_mps52002->src_cur_value; 
            break;	
		case PD_SUPPLY_PROP_SOURCE_VOLT_VAULE:
			 val->intval = pd_mps52002->src_volt_value; 
            break;	
		
		case PD_SUPPLY_PROP_UNLOAD_FINISHED:

			val->intval = pd_mps52002->pd_65992_unload_flag;
			// SYS_LOG_INF("PPD_SUPPLY_PROP_UNLOAD_FINISHED:%d", val->intval);
			break;	

        default:
            break;	  
    }


    return 0;
}


void mps_set_source_disc(void)
{

    u8_t buf[2] = {0};
    // union dev_config config = {0};
    // struct device *iic_dev;
	

    // iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    // config.bits.speed = I2C_SPEED_STANDARD;
    // i2c_configure(iic_dev, config.raw);
	
	wake_up_pd();

	k_sleep(50);
		
    buf[0] = 0X01;
    buf[1] = 0x00; 

	pd_mps52002_write_reg_value(PD_SYS_POWER_STATUS, buf, 2);					// 0x05 change function to disconnect
	printf("PD_SYS_POWER_STATUS:disc = %d \n",buf[0]);
}

void pd_mps52002_sink_charging_disable(bool flag)
{

    struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
   
	u8_t buf[2] = {0};
 //   union dev_config config = {0};
	u8_t buf_status[2] = {0};
    // struct device *iic_dev;
   
    // iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    // config.bits.speed = I2C_SPEED_STANDARD;
    // i2c_configure(iic_dev, config.raw);


    pd_tps52002_read_reg_value(PD_MP2760_STATUS_1, buf_status, 2);                                                     // 
    if(buf_status[0]&0x20)
    {
      SYS_LOG_INF("[%d] >>>>>>> timer out >>>>>>>>>> !!!\n", __LINE__);
      return ;
    }
    pd_mps52002->sink_charging_flag = flag;

    SYS_LOG_INF("[%d] flag:%d\n", __LINE__, flag);

    if(flag)
    {
        buf[0] =  0x00;                                                 //                                        
        //i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_CHARGE_CURRENT, buf, 2);
		pd_mps52002_write_reg_value(PD_CHARGE_CURRENT, buf, 2);
        k_sleep(2);

    }else{
        buf[0] = 0x30;                                                              //                                        
        // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_CHARGE_CURRENT, buf, 2);
		pd_mps52002_write_reg_value(PD_CHARGE_CURRENT, buf, 2);
        k_sleep(2);
    }
    
}

extern void io_expend_aw9523b_ctl_20v5_set(uint8_t onoff);

void pd_mps52002_iic_send_data()
{
    u8_t type, data;
  

	while(!pd_iid_pop_queue(&type, &data))
	{

		SYS_LOG_INF("[%d] type:%d, data:%d\n", __LINE__, type, data);
		switch(type)
		{

			case PD_IIC_TYPE_PROP_CURRENT_2400MA:
				pd_mps52002_pd_24A_proccess(data);
				// pd_iic_push_queue(PD_IIC_TYPE_PROP_CURRENT_2400MA, (u8_t)val->intval);
				break;

			case PD_IIC_TYPE_PROP_SOURCE_CURRENT_500MA:
				pd_mps52002_source_current_proccess(BT_RUNTIME_STATUS_MUSIC_PLAY);
				// pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CURRENT_500MA, (u8_t)val->intval);
				break;
			case PD_IIC_TYPE_PROP_SOURCE_CURRENT_1000MA:
				pd_mps52002_source_current_proccess(BT_RUNTIME_STATUS_NORMAL);
				// pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CURRENT_1000MA, (u8_t)val->intval);
				break; 

			case PD_IIC_TYPE_PROP_SOURCE_CHARGING_OTG:
				pd_mps52002_source_current_proccess(BT_RUNTIME_STATUS_POWEROFF);
				// pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CHARGING_OTG, (u8_t)val->intval);
				break; 


			case PD_IIC_TYPE_PROP_STANDBY:
				break;

			case PD_IIC_TYPE_PROP_OTG_OFF:
				if(data)
				    pd_mps52002_pd_otg_on(true); 
				else
					pd_mps52002_pd_otg_on(false); 

				// pd_iic_push_queue(PD_IIC_TYPE_PROP_OTG_OFF, (u8_t)val->intval);		
				break;    
			case PD_IIC_TYPE_PROP_SINK_CHANGING_ON:
				pd_mps52002_sink_charging_disable(false);
				// pd_iic_push_queue(PD_IIC_TYPE_PROP_SINK_CHANGING_ON, (u8_t)val->intval);
				break;

			case PD_IIC_TYPE_PROP_SINK_CHANGING_OFF:
				pd_mps52002_sink_charging_disable(true);
				// pd_iic_push_queue(PD_IIC_TYPE_PROP_SINK_CHANGING_OFF, (u8_t)val->intval);
				break; 
			case PD_IIC_TYPE_PROP_ONOFF_AMP_PVDD:
				MPS_PB2_LOW(0);
				// pd_iic_push_queue(PD_IIC_TYPE_PROP_ONOFF_AMP_PVDD, (u8_t)val->intval);
				break;   
			case PD_IIC_TYPE_PROP_ONOFF_BGATE:
				if(data)
				    MPS_ONOFF_BGATE(1);
				else
					MPS_ONOFF_BGATE(0);  

				// pd_iic_push_queue(PD_IIC_TYPE_PROP_ONOFF_BGATE, (u8_t)val->intval);

				break; 
			case PD_IIC_TYPE_PROP_SOURCE_SSRC:
				// MPS_SET_SOURCE_SSRC(1);
				mps_set_source_disc();
				// pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_SSRC, (u8_t)val->intval);
				break;

			case PD_IIC_TYPE_PROP_POWERDOWN:
				// thread_timer_stop(&pd_mps52002->timer);
				// pd_mps52002->pd_65992_unload_flag = true;
				break;	

			case PD_IIC_TYPE_PROP_SOURCE_DISC:
				mps_set_source_disc();
				break;	


			case MCU_IIC_TYPE_PROP_DSP_DCDC:
				io_expend_aw9523b_ctl_20v5_set(data);	
				break;
				
			default:
				break; 
		}      
	}
}


static void pd_mps52002_wlt_set_property(struct device *dev,enum pd_manager_supply_property psp,    \
				       union pd_manager_supply_propval *val)
{
	  struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data; 
	//struct wlt_pd_mps52002_info *pd_mps52002 = dev->driver_data;
    
	switch(psp)
    {
        case PD_SUPPLY_PROP_CURRENT_2400MA:
            // pd_mps52002_pd_24A_proccess(val->intval);
			pd_iic_push_queue(PD_IIC_TYPE_PROP_CURRENT_2400MA, (u8_t)val->intval);
            break;

        case PD_SUPPLY_PROP_SOURCE_CURRENT_500MA:
            // pd_mps52002_source_current_proccess(BT_RUNTIME_STATUS_MUSIC_PLAY);
			pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CURRENT_500MA, (u8_t)val->intval);
            break;
        case PD_SUPPLY_PROP_SOURCE_CURRENT_1000MA:
            // pd_mps52002_source_current_proccess(BT_RUNTIME_STATUS_NORMAL);
			pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CURRENT_1000MA, (u8_t)val->intval);
			// pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CHARGING_OTG, (u8_t)val->intval);
            break; 

        case PD_SUPPLY_PROP_SOURCE_CHARGING_OTG:
            // pd_mps52002_source_current_proccess(BT_RUNTIME_STATUS_POWEROFF);
			pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_CHARGING_OTG, (u8_t)val->intval);
            break; 

		case PD_SUPPLY_PROP_SOURCE_DISC:
			pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_DISC, (u8_t)val->intval);
			break;

        case PD_SUPPLY_PROP_STANDBY:
            // MPS_PB6_LOW(0);
			pd_iic_push_queue(PD_IIC_TYPE_PROP_STANDBY, (u8_t)val->intval);
            break;  
		case PD_SUPPLY_PROP_POWERDOWN:
			// thread_timer_stop(&pd_mps52002->timer);
			// pd_mps52002->pd_65992_unload_flag = true;
			thread_timer_stop(&pd_mps52002->timer);
			
			if(!val->intval)
			{
				pd_mps52002_pd_otg_on(true); 	
			}

			k_sleep(20);
			MPS_ONOFF_BGATE(0);  
			pd_mps52002->pd_65992_unload_flag = true;

            break;  

        case PD_SUPPLY_PROP_OTG_OFF:
			// if(val->intval)
            //     pd_mps52002_pd_otg_on(true); 
			// else
			// 	pd_mps52002_pd_otg_on(false); 

			pd_iic_push_queue(PD_IIC_TYPE_PROP_OTG_OFF, (u8_t)val->intval);		
            break;    
        case PD_SUPPLY_PROP_SINK_CHANGING_ON:
            // pd_mps52002_sink_charging_disable(false);
			pd_iic_push_queue(PD_IIC_TYPE_PROP_SINK_CHANGING_ON, (u8_t)val->intval);
            break;

        case PD_SUPPLY_PROP_SINK_CHANGING_OFF:
            // pd_mps52002_sink_charging_disable(true);
			pd_iic_push_queue(PD_IIC_TYPE_PROP_SINK_CHANGING_OFF, (u8_t)val->intval);
            break; 
		case PD_SUPPLY_PROP_ONOFF_AMP_PVDD:
            // MPS_PB2_LOW(0);
			
			pd_iic_push_queue(PD_IIC_TYPE_PROP_ONOFF_AMP_PVDD, (u8_t)val->intval);
            break;   
		case PD_SUPPLY_PROP_ONOFF_BGATE:
			//  if(val->intval)
            //      MPS_ONOFF_BGATE(1);
			// else
			// 	MPS_ONOFF_BGATE(0);  

			pd_iic_push_queue(PD_IIC_TYPE_PROP_ONOFF_BGATE, (u8_t)val->intval);

            break; 
		case PD_SUPPLY_PROP_SOURCE_SSRC:
		    // MPS_SET_SOURCE_SSRC(1);
			 pd_iic_push_queue(PD_IIC_TYPE_PROP_SOURCE_SSRC, (u8_t)val->intval);
			 break;
        default:
            break;               
    }

}



void pd_mps52002_wlt_register_notify(struct device *dev, pd_manager_callback_t cb)
{
	struct wlt_pd_mps52002_info *pd_mps52002 = dev->driver_data;
	int flag;

	flag = irq_lock();
	
	if ((pd_mps52002->notify == NULL) && cb)
	{
		pd_mps52002->notify = cb;
	}else
	{
		SYS_LOG_ERR("pd notify func already exist!\n");
	}
	
}

void pd_mps52002_mcu_register_notify(struct device *dev, pd_manager_mcu_callback_t cb)
{
	struct wlt_pd_mps52002_info *pd_mps52002 = dev->driver_data;
	
	int flag;

	flag = irq_lock();
	
	if ((pd_mps52002->mcu_notify == NULL) && cb)
	{
		pd_mps52002->mcu_notify = cb;
	}else
	{
		SYS_LOG_ERR("mcu notify func already exist!\n");
	}
	irq_unlock(flag);
}


static void pd_mps52002_wlt_enable(struct device *dev)
{
	struct wlt_pd_mps52002_info *pd_mps52002 = dev->driver_data;
	// const struct wlt_pd_mps52002_config *cfg = dev->config->config_info;

	// thread_timer_start(&pd_mps52002->timer, cfg->poll_interval_ms, cfg->poll_interval_ms);
	thread_timer_start(&pd_mps52002->timer, MCU_RUN_TIME_PERIOD, MCU_RUN_TIME_PERIOD);
}

static void pd_mps52002_wlt_disable(struct device *dev)
{
	struct wlt_pd_mps52002_info *pd_mps52002 = dev->driver_data;
	// const struct wlt_pd_mps52002_config *cfg = dev->config->config_info;

    thread_timer_stop(&pd_mps52002->timer);
}

void pd_wlt_battery_register_notify_mps(struct device *dev, pd_manager_battery_t cb)
{
	struct wlt_pd_mps52002_info *pd_mps52002= dev->driver_data;
    int flag;

	flag = irq_lock();
	
	if ((pd_mps52002->battery_notify == NULL) && cb)
	{
		pd_mps52002->battery_notify = cb;
	}else
	{
		SYS_LOG_ERR("battery notify func already exist!\n");
	}
	irq_unlock(flag);
}


static const struct pd_manager_supply_driver_api pd_mps52002_wlt_driver_api = {
	.get_property = pd_mps52002_wlt_get_property,
	.set_property = pd_mps52002_wlt_set_property,
	.register_pd_notify = pd_mps52002_wlt_register_notify,
	.register_mcu_notify = pd_mps52002_mcu_register_notify,
	.register_bat_notify = pd_wlt_battery_register_notify_mps,
    .enable     = pd_mps52002_wlt_enable,
	.disable    = pd_mps52002_wlt_disable,

};


int pd_mps52002_init(void)
{
    struct wlt_pd_mps52002_info *pd_mps52002 = NULL; 
    p_pd_mps52002_dev = &g_pd_mps52002_dev;
  //  const struct wlt_pd_mps52002_config *cfg; 
    union dev_config config = {0};
    
    p_pd_mps52002_dev->driver_api = &pd_mps52002_wlt_driver_api;
    p_pd_mps52002_dev->driver_data = &pd_mps52002_ddata;
  //  p_pd_mps52002_dev->config = (const struct device_config *)&pd_mps52002_wlt_cdata;

    pd_mps52002 = p_pd_mps52002_dev->driver_data;
 //   cfg = p_pd_mps52002_dev->config->config_info;
	pd_mps52002->pd_65992_unload_flag = 0x00;

	pd_mps52002->gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!pd_mps52002->gpio_dev) 
	{
		printf("[%s,%d] gpio_dev not found\n", __FUNCTION__, __LINE__);
		
	}else{
		if(!run_mode_is_demo()){
			printf("[%s,%d] run_mode_is_demo", __FUNCTION__, __LINE__);
	        gpio_pin_configure(pd_mps52002->gpio_dev, GPIO_PIN_USB_D_LINE, GPIO_DIR_OUT | GPIO_POL_NORMAL | GPIO_PUD_PULL_UP);
	        gpio_pin_write(pd_mps52002->gpio_dev, GPIO_PIN_USB_D_LINE, 1);
		}
		gpio_pin_configure(pd_mps52002->gpio_dev, GPIO_PIN_EXTEND_RST, GPIO_DIR_OUT | GPIO_POL_NORMAL | GPIO_PUD_PULL_UP);
	    gpio_pin_write(pd_mps52002->gpio_dev, GPIO_PIN_EXTEND_RST, 1);
        gpio_pin_configure(pd_mps52002->gpio_dev, GPIO_PIN_PD_RST, GPIO_DIR_OUT | GPIO_PUD_PULL_UP);
		gpio_pin_configure(pd_mps52002->gpio_dev, GPIO_PIN_PA6, GPIO_DIR_OUT | GPIO_PUD_PULL_UP);
		gpio_pin_configure(pd_mps52002->gpio_dev, GPIO_PIN_PD_SOURCE, GPIO_DIR_IN | GPIO_PUD_PULL_UP);
	    k_sleep(10);
	    gpio_pin_write(pd_mps52002->gpio_dev, GPIO_PIN_PD_RST, 0);
		gpio_pin_write(pd_mps52002->gpio_dev, GPIO_PIN_PA6, 0);
        k_sleep(100);
    }


    pd_mps52002->i2c_dev = device_get_binding(CONFIG_I2C_0_NAME);

    if(!pd_mps52002->i2c_dev)
    {
        printf("[%s,%d] iic_dev not found\n", __FUNCTION__, __LINE__);
    }else{
        config.bits.speed = I2C_SPEED_STANDARD;
        i2c_configure(pd_mps52002->i2c_dev , config.raw);
    }


    pd_mps52002_status_value();

	printk("[%s,%d]\n", __FUNCTION__, __LINE__);

    thread_timer_init(&pd_mps52002->timer, mcu_pd_iic_time_hander_mps, NULL);    
 //   thread_timer_start(&pd_mps52002->timer, MCU_RUN_TIME_PERIOD, MCU_RUN_TIME_PERIOD);

    return 0;
}

void wake_up_pd(void)
{
	// struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	if(!pd_mps_source_pin_read())
	{
		gpio_pin_write(gpio_dev, GPIO_PIN_PA6, 1);
		k_sleep(10);
	  	gpio_pin_write(gpio_dev, GPIO_PIN_PA6, 0);
		k_sleep(80);
	}
}
struct device *wlt_device_get_pd_mps52002(void)
{
     return p_pd_mps52002_dev;
}
void MPS_PB2_LOW(bool OnOFF)
{
  	// union dev_config config = {0};
  	// struct device *iic_dev;
	// iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
	// config.bits.speed = I2C_SPEED_STANDARD;
	// i2c_configure(iic_dev, config.raw);
	 
      
	wake_up_pd();
   	u8_t buf[2]={0x00,0x00};

   	printf("[%s,%d] %02x\n", __FUNCTION__, __LINE__,buf[0]);
   	// i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x41, buf, 2);    
	pd_mps52002_write_reg_value(PD_MPS_41_REG, buf, 2);
}

void MPS_SET_SOURCE_SSRC(bool OnOFF)
{
//   union dev_config config = {0};
//   struct device *iic_dev;
// 	 iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
// 	 config.bits.speed = I2C_SPEED_STANDARD;
// 	 i2c_configure(iic_dev, config.raw);
	     
	wake_up_pd();
   u8_t buf[2]={0x00,0x00};

    if(OnOFF)
	{
	   buf[0] = 0x01;
    }	
   printf("[%s,%d] %02x\n", __FUNCTION__, __LINE__,buf[0]);
   // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x05, buf, 2); 
   pd_mps52002_write_reg_value(PD_SYS_POWER_STATUS, buf, 2);   
}

void MPS_ONOFF_BGATE(bool OnOFF)
{
//   union dev_config config = {0};
//   struct device *iic_dev;
    
// 	 iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
// 	 config.bits.speed = I2C_SPEED_STANDARD;
// 	 i2c_configure(iic_dev, config.raw);
	 
     wake_up_pd();
    u8_t buf1[2]={0x00,0x00};
    if(OnOFF)
	{
	   buf1[0] = 0x01;
    }		
     // i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x07, buf1, 2); 
	pd_mps52002_write_reg_value(PD_MPS_07_REG, buf1, 2);
    k_sleep(10);
	printf("[%s,%d] %02x\n", __FUNCTION__, __LINE__,buf1[0]); 
}
// DEVICE_AND_API_INIT(pd_mps52002, "pd_mps52002", pd_mps52002_init,
// 	    &pd_mps52002_ddata, &pd_mps52002_wlt_cdata, POST_KERNEL,
// 	    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &pd_mps52002_wlt_driver_api);


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
	int16   src_cur_value;
	u8_t   	SRC_5OMA_FLAG;
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

#define WLT_OTG_DEBOUNCE_TIMEOUT        10
#define MAX_SOURCE_DISC_COUNT           13

enum ti_pd_reg_address_t{

    PD_OTG_IOUT_STATUS       = 0x00, //Read only
    PD_FIRMWARE_ID           = 0x01,//Read only 15:8 MPF52002_CHIP_ID SAMPLE 02 :MSP52002       
    PD_PD_CONTROL            = 0x02, // bit2:1  default 00 ; SRC Setting 00:18w 01:5w(5v1a) 10:2.5w(5v0.5a);bit0 sink disable
                                    
    PD_PD_STATUS       = 0x03, ////Read only bit1 :src mode bit0: sink mode

    PD_CHARGE_CURRENT      = 0x04,//bit7:0 default :(3a)
    PD_SYS_POWER_STATUS    = 0x05, //if bt speaker to bit :01 sink to src
    PD_MOSFET_CTRL     = 0x06, //mosfet contro PB3 OR PB6
    PD_MP2760_STATUS_0  = 0X16,//
    PD_MP2760_STATUS_1  = 0X17,//
    PD_DISABLE_SOURCE   =0X40,//disable source 
    
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



static void pd_tps52002_read_reg_value(uint8_t addr, u8_t *buf)
{
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
     k_sleep(10);  
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, addr, buf, 2);

	if(addr  == PD_PD_STATUS)
	{
		return;
	}

    printf("live debug reg 0x%x value:  ", addr);
    for(int i=0; i< 2; i++)
    {
        printf("0x%x, ", buf[i]);
    }
    printf("\n");

}
static void pd_mps52002_write_reg_value(uint8_t addr, u8_t *buf)
{
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
	 printf("live wirte reg 0x%x \n", addr);
	i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, addr, buf, 2);
}



void pd_mps52002_pd_24A_proccess(int val)
{
    u8_t buf[2] = {0};
    union dev_config config = {0};
    struct device *iic_dev;
	
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
  if(val)
    buf[0] = 0x1e;
  else
  	buf[0] = 0x30;
  
    buf[1] = 0x00;
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_CHARGE_CURRENT, buf, 2);
	
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, PD_CHARGE_CURRENT, buf, 2);
    printf("pd_mps52002_pd_24A_proccess  debug reg 0x%x value: 0x%x, 0x%x \n", 0x09  ,buf[0], buf[1]);

}

void pd_mps52002_pd_otg_on(bool flag)
{
    u8_t buf[2] = {0};
    union dev_config config = {0};
    struct device *iic_dev;
	struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
	
	wake_up_pd();
		
	// i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, PD_PD_CONTROL, buf, 2);
	// printf(" pd_tps52002_pd_otg_on PD_PD_CONTROL read = %d \n",buf[0]);

 
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

	 pd_mps52002_write_reg_value(PD_DISABLE_SOURCE, buf);
	 printf(" pd_tps52002_pd_otg_on PD_DISABLE_SOURCE write = %d \n",buf[0]);
    
}


// void pd_mps52002_src_small_current_otg_off(void)
// {
//     u8_t buf[2] = {0};
//     union dev_config config = {0};
//     struct device *iic_dev;
   
//     iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
//     config.bits.speed = I2C_SPEED_STANDARD;
//     i2c_configure(iic_dev, config.raw);
	
// 	i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, PD_PD_CONTROL, buf, 2);
// 	printf(" pd_mps52002_src_smart_current_otg_off  read = %d \n",buf[0]);


//         buf[0] = 0x08|(buf[0]&0X06);
//         buf[1] = 0x00;     

 
// 	printf(" pd_mps52002_src_smart_current_otg_off write = %d \n",buf[0]);
//     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_PD_CONTROL, buf, 2);

// }

u8_t pd_mps2760_read_current(int16 *volt_val, int16 *cur_val,int16 *src_cur_val)
{

    u8_t buf[2] = {0},SRC_FLAG = 0;
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
    k_sleep(1);                                                                       
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x24, buf, 2);
    *cur_val=(((buf[1]<<8) | buf[0])& 0x03ff)*6; 
	k_sleep(1);
	//  printf("live  volt reg 0x%x value: 0x%x, 0x%x cur_val %d\n", 0x23  ,buf[0], buf[1],*cur_val);
	i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x23, buf, 2);
	*volt_val =(((buf[1]<<8) | buf[0])& 0x03ff)*20; 
	SYS_LOG_INF("PD mps2761 sink voltage:%d, current:%d", *volt_val, *cur_val);
	
	k_sleep(1);
  	i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x2c, buf, 2);
	*volt_val =(((buf[1]<<8) | buf[0])& 0x03ff)*20;
	k_sleep(1);
	SYS_LOG_INF("PD mps2761 source voltage:%d, current:%d", *volt_val, *src_cur_val);

	k_sleep(1);
	i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x00, buf, 2);
	SRC_FLAG = buf[1]&80;
	SYS_LOG_INF("source OTG current flag 1=full, 0=charging: 0x%x \n", SRC_FLAG);
	*src_cur_val =(int16)((((((buf[1]<<8) | buf[0])& 0x03ff)*0.806)-100)*5); 
	k_sleep(1);
    
    return SRC_FLAG;

}
#if 0
u8_t crc_table[] =
{
    0x00,0x31,0x62,0x53,0xc4,0xf5,0xa6,0x97,0xb9,0x88,0xdb,0xea,0x7d,0x4c,0x1f,0x2e,
    0x43,0x72,0x21,0x10,0x87,0xb6,0xe5,0xd4,0xfa,0xcb,0x98,0xa9,0x3e,0x0f,0x5c,0x6d,
    0x86,0xb7,0xe4,0xd5,0x42,0x73,0x20,0x11,0x3f,0x0e,0x5d,0x6c,0xfb,0xca,0x99,0xa8,
    0xc5,0xf4,0xa7,0x96,0x01,0x30,0x63,0x52,0x7c,0x4d,0x1e,0x2f,0xb8,0x89,0xda,0xeb,
    0x3d,0x0c,0x5f,0x6e,0xf9,0xc8,0x9b,0xaa,0x84,0xb5,0xe6,0xd7,0x40,0x71,0x22,0x13,
    0x7e,0x4f,0x1c,0x2d,0xba,0x8b,0xd8,0xe9,0xc7,0xf6,0xa5,0x94,0x03,0x32,0x61,0x50,
    0xbb,0x8a,0xd9,0xe8,0x7f,0x4e,0x1d,0x2c,0x02,0x33,0x60,0x51,0xc6,0xf7,0xa4,0x95,
    0xf8,0xc9,0x9a,0xab,0x3c,0x0d,0x5e,0x6f,0x41,0x70,0x23,0x12,0x85,0xb4,0xe7,0xd6,
    0x7a,0x4b,0x18,0x29,0xbe,0x8f,0xdc,0xed,0xc3,0xf2,0xa1,0x90,0x07,0x36,0x65,0x54,
    0x39,0x08,0x5b,0x6a,0xfd,0xcc,0x9f,0xae,0x80,0xb1,0xe2,0xd3,0x44,0x75,0x26,0x17,
    0xfc,0xcd,0x9e,0xaf,0x38,0x09,0x5a,0x6b,0x45,0x74,0x27,0x16,0x81,0xb0,0xe3,0xd2,
    0xbf,0x8e,0xdd,0xec,0x7b,0x4a,0x19,0x28,0x06,0x37,0x64,0x55,0xc2,0xf3,0xa0,0x91,
    0x47,0x76,0x25,0x14,0x83,0xb2,0xe1,0xd0,0xfe,0xcf,0x9c,0xad,0x3a,0x0b,0x58,0x69,
    0x04,0x35,0x66,0x57,0xc0,0xf1,0xa2,0x93,0xbd,0x8c,0xdf,0xee,0x79,0x48,0x1b,0x2a,
    0xc1,0xf0,0xa3,0x92,0x05,0x34,0x67,0x56,0x78,0x49,0x1a,0x2b,0xbc,0x8d,0xde,0xef,
    0x82,0xb3,0xe0,0xd1,0x46,0x77,0x24,0x15,0x3b,0x0a,0x59,0x68,0xff,0xce,0x9d,0xac
};
u8_t crc8_calulate (u8_t* buf, int length) 
{
	u8_t crc = 0x00;
	int i;
	for(i = 0; i< length; i++)
	{
	crc = crc_table[crc ^ (buf[i])];
	}
	return (crc);
}
#endif
#if 0
int OTA_PD(void)
{
	   struct device *iic_dev;
	   union dev_config config = {0};
	   int ret = 0;
	   struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	   printf("live  OTA_PD start \n");
	    
		printf("live  OTA_PD 222 \n");
	  gpio_pin_write(gpio_dev, GPIO_PIN_PD_RST, 1);
		  k_sleep(10); //delay 10ms
	  gpio_pin_write(gpio_dev, GPIO_PIN_PD_RST, 0);

        k_sleep(60); //delay 10ms
      iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
      config.bits.speed = I2C_SPEED_STANDARD;
      i2c_configure(iic_dev, config.raw);                                                                            
	  u8_t request_cmd[] = { 0x4D, 0x50, 0x53, 0x53, 0x54, 0x41, 0x52, 0x54 };
	  i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0xaa, request_cmd, sizeof(request_cmd));
		  printf("live  OTA_PD 333 \n");
		 k_sleep(850); //delay 850ms
		 
		//Check on status regsiter 0xBA D[0] == 1
		u8_t count = 5;
		u8_t readdata;
		u8_t flg = 0;
		while (count > 0)
		{
	      i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0xba,&readdata, 1); 
		if ((readdata & 0x01) > 0)
		{
		 flg = 1;
		  break;
		}
		count--;
		k_sleep(100); //delay 100ms
		}
		if ( flg == 0 )
	    	printf("[%s %d]  enter firmware update mode fail! \n", __func__, __LINE__);
		//Data transferring
	//	u8_t *fw_data;
		//fw_data = PD_buff;//{0x00,0x00};//read_fw_data();
		//memcpy(fw_data,PD_buff,sizeof(PD_buff));
		int data_len = sizeof(fw_data);//fw_data.Length;
		int res = data_len % 64;
		int frame_index = 0;
		  printf("live  OTA_PD 444 data_len = %d \n",data_len);
		for(int i = 0;i<(data_len - res);i+= 64)
		{		
			//u8_t data[64];// = fw_data.Skip(i).Take(512).ToArray();
			//memcpy(data, &fw_data[i],64);
			//u8_t crc_value = crc8_calulate(data, 512);
			u8_t frame[65];
			frame[0]= frame_index/8;
			//printf("live  OTA_PD 444 frame_index = %02x\n ",frame_index/8);
			memcpy(&frame[1], &fw_data[i], 64);
			//for(int j =0 ;j < 65;j++)
			//	printf("0x%02x,",frame[j]);
			 //frame[513]=crc_value;
			// i2c_write(iic_dev, frame, 65, 0xAF);
			i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR,0xAF, frame, 65);
			frame_index++;
			if(frame_index % 8 == 0)
			 {
			    k_sleep(150); //delay 50ms before read index and crc status
			   i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0xba,&readdata, 1); 
			  if ((readdata & 0x0C) > 0)
			   {
			       printf("[%s %d]  crc fail \n", __func__, __LINE__);
			   }
			   else
			    {
			      // k_sleep(100); //delay 50ms before read index and crc status
			        printf("[%s %d]  succes \n", __func__, __LINE__);
                
				}
			    k_sleep(100); //delay 50ms before read index and crc status
			
			  
			}
			else
			   k_sleep(50); //delay 50ms before next data frame
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
			frame1[0] = frame_index/8;
			 //printf("[%s %d]  = %d\n", __func__, __LINE__,frame1[0]);
			// printf("live  OTA_PD 444 frame1 = %02x\n",frame_index/8);
			memcpy(&frame1[1], &fw_data[flowDataLen], res);
			 //for(int jj =0 ;jj <(res+1);jj++)
				//printf("0x%02x,",frame1[jj]);
			// frame1[res+1]=crc_value1;
			 // i2c_write(iic_dev, frame1, res+1, 0xAF);
		    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR,0xAF, frame1, res+1);
			k_sleep(100); //delay 50ms before read index and crc status
			 //  i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0xba,&readdata, 1); 
			//if ((readdata & 0x0C) > 0)
			// printf("[%s %d]  crc or index error occurredl\n", __func__, __LINE__);
		}
		//Terminate data transferring
		   u8_t stop_cmd[] =  {0x4D, 0x50, 0x53, 0x45, 0x4E, 0x44, 0x53, 0x5A};
		   i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0xdd, stop_cmd  , sizeof(stop_cmd));
		   //Check on status regsiter 0xBA D[1] == 1
		   count = 5;
		   flg = 0;
		while (count > 0)
		{
		    printf("readdata = 0x%02x\n",(readdata & 0x02));
		         i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0xba,&readdata, 1); 
				if ((readdata & 0x02) > 0)
				{
				 flg = 1;
				 break;
				}
 		  count--;
 		  k_sleep(100); //delay 100ms
		}
		if ( flg == 0) 
		{	 
		     ret = 0;
		     printf("[%s %d] error occurs when data transferring or flash writing\n", __func__, __LINE__);
		}
		else
		 {
		    ret = 1;
		 }
		//Jump to execute new Firmware
		u8_t jump_cmd = 0xAC;
		i2c_burst_write(iic_dev,I2C_PD_DEV_ADDR,0xbb,&jump_cmd ,1); 
		printf("[%s,%d] OTA_PD end\n", __FUNCTION__, __LINE__);
		return ret;
}
#else

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
	struct device *iic_dev;
	union dev_config config = {0};
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	printf("live  OTA_PD start \n");
	
	gpio_pin_write(gpio_dev, GPIO_PIN_PD_RST, 1);
	k_sleep(10); //delay 10ms
	gpio_pin_write(gpio_dev, GPIO_PIN_PD_RST, 0);

	k_sleep(60); //delay 10ms
	
	// task_wdt_add(TASK_WDT_CHANNEL_MAIN_APP, 50000, NULL, NULL);
	iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
	config.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(iic_dev, config.raw);                                                                            
	u8_t request_cmd[] = { 0x4D, 0x50, 0x53, 0x53, 0x54, 0x41, 0x52, 0x54 };
	i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0xaa, request_cmd, sizeof(request_cmd));
	printf("live  OTA_PD 333 \n");
	k_sleep(850); //delay 850ms
		
	//Check on status regsiter 0xBA D[0] == 1
	int count = 5;
	u8_t readdata;
	
	while (count-- > 0)
	{
		if(i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0xba,&readdata, 1) >= 0x00)
		{
			// if ((readdata & 0x01) > 0)
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
		i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR,0xAF, frame, 129);
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
		i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR,0xAF, frame1, res+1);
		k_sleep(60); //delay 50ms before read index and crc status

	}
	

	k_sleep(30); //delay 50ms before read index and crc status
	//Terminate data transferring
	u8_t stop_cmd[] =  {0x4D, 0x50, 0x53, 0x45, 0x4E, 0x44, 0x53, 0x5A};
	i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0xdd, stop_cmd  , sizeof(stop_cmd));
	
	//Check on status regsiter 0xBA D[1] == 1
	count = 5;
	while (count-- > 0)
	{
		i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0xba,&readdata, 1);
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
		printf("[%s %d] error occurs when data transferring or flash writing\n", __func__, __LINE__);
		return 0;
	}
	else
	{
		mps_ota_set_status(0x88);
	}
	//Jump to execute new Firmware
	u8_t jump_cmd = 0xAC;
	i2c_burst_write(iic_dev,I2C_PD_DEV_ADDR,0xbb,&jump_cmd ,1); 
	k_sleep(10); //delay 10ms
	gpio_pin_write(gpio_dev, GPIO_PIN_PD_RST, 1);
	k_sleep(10); //delay 10ms
	gpio_pin_write(gpio_dev, GPIO_PIN_PD_RST, 0);
	printf("[%s,%d] OTA_PD end\n", __FUNCTION__, __LINE__);
	return 1;
}	
#endif 

u8_t PD_version;

static void pd_mps52002_status_value(void)
{
    u8_t buf[2] = {0};
	u8_t buf1[2] = {0x01,0x00};
	u8_t buf2[2] = {0x02,0x00};
    union dev_config config = {0};
    struct device *iic_dev;
	mps_ota_get_status(&ota_flag);
    printf("[%s %d] ota_flag:%d \n", __func__, __LINE__, ota_flag);
    if(ota_flag == 0x55)
	{
 
	  	OTA_PD();
	 	return;
    }
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x01, buf, 2);
	PD_version = buf[0];
	k_sleep(1);
    printf("live debug PD version:0x%x 0x%x \n",buf[0], buf[1]);
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x3, buf, 2);
	k_sleep(1);
    printf("live debug PD status:0x%x 0x%x \n",buf[0], buf[1]);
	k_sleep(1);
	i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x06, buf2, 2);
	k_sleep(1);
	printf("live debug 0x07 status:0x%x 0x%x  \n",buf[0], buf[1]);
	i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x07, buf1, 2);
  //   OTA_PD();
    k_sleep(1);
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x05, buf, 2);
	k_sleep(1);
    printf("live debug Power status:0x%x 0x%x  \n",buf[0], buf[1]);
	
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

    union  dev_config config = {0};
    struct device *iic_dev;
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);

    // k_sleep(50);
  	// if(power_manager_get_battery_capacity() <= 15)
  	// {
    //         buf[1] = 0x00;
    //         buf[0] = 0x08; //disable src
  	// }

	printf("live write  0x02 value: 0x%x, 0x%x\n",buf[0], buf[1]); 
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_PD_CONTROL, buf, 2);	

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, PD_PD_CONTROL, buf, 2);
    printf("live debug reg 0x%x value: 0x%x, 0x%x\n", 0x02 ,buf[0], buf[1]); 

	pd_mps52002->pd_source_disc_debunce_cnt = MAX_SOURCE_DISC_COUNT;

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

// static const struct wlt_pd_mps52002_config pd_mps52002_wlt_cdata = {
//     .gpio_name = CONFIG_GPIO_ACTS_DEV_NAME,
//     .iic_name = CONFIG_I2C_0_NAME,
//     .poll_interval_ms = MCU_RUN_TIME_PERIOD,
//     .one_second_count = MCU_ONE_SECOND_PERIOD,
// };
#if 0
static uint8_t pd_mps52002_get_status_reg()
{
    u8_t    buf[2] = {0};
    u16_t   reg_value = 0;
    u8_t    ret = PD_REG_STATUS_NO_CHANGE;
    u8_t    result = 0;
    static  u8_t source_debounce_cnt = 0;
    struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
   
    pd_tps52002_read_reg_value(PD_PD_STATUS, buf);
   
        reg_value =  (buf[1] << 8) | buf[0];
        result = (reg_value >> 1) & 0x01;
        printk("[%s,%d]\n", __FUNCTION__,result);
        if(result != pd_mps52002->pd_52002_source_flag)
        {
            if(source_debounce_cnt < 1)
            {
                source_debounce_cnt++; 
            }else{
                source_debounce_cnt = 0;
                pd_mps52002->pd_52002_source_flag = result;
                SYS_LOG_INF("[%d] source flag:%d", __LINE__,pd_mps52002->pd_52002_source_flag);
                ret = PD_REG_STATUS_SOURCE_CHANGE;
            }

        }else{
            source_debounce_cnt = 0;
        }

        result = (reg_value) & 0x01; 
        if(result != pd_mps52002->pd_sink_legacy_flag)
        {
            pd_mps52002->pd_sink_legacy_flag = result;
            ret = PD_REG_STATUS_LEGACY_SINK_CHANGE;
        } 
		 result = (reg_value) & 0x04; 
        if(result != pd_mps52002->pd_52002_HIZ_flag)
        {
         SYS_LOG_INF("[%d] otg flag:%d", __LINE__,pd_mps52002->pd_52002_HIZ_flag);
            pd_mps52002->pd_52002_HIZ_flag = result;
        } 

    return ret;

}
#endif
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


	// if(!pd_mps_source_pin_read())
	// {
	//     if(pd_mps52002->pd_52002_sink_flag || pd_mps52002->pd_52002_source_flag)
	//       {
	//         if(pd_mps52002->volt_value >= 6500)
	//          {
	//            pd_set_G2_Low();
	//          }

	// 		 pd_mps52002->volt_value = 0x00;
	//          pd_mps52002->pd_52002_source_flag = 0;
	// 		 pd_mps52002->sink_charging_flag = 1;
	// 		 if(dc_power_in_status_read())
	// 		 {
	// 			pd_mps52002->pd_52002_HIZ_flag = 1;
	// 		 }
	// 		 else
	// 		 {
	// 			pd_mps52002->pd_52002_HIZ_flag = 0;
	// 		 }
			 	
			 
	// 		  pd_mps52002->pd_52002_sink_flag = 0;
	// 		  fisrtreadtimes = 0;
	// 		  para.pd_event_val = pd_mps52002->pd_52002_source_flag;
	// 		  pd_mps52002->notify(PD_EVENT_SOURCE_STATUS_CHG, &para);
	//    	      pd_mps52002->charge_full_flag = false;
    //           para.pd_event_val = 0;
	//           pd_mps52002->notify(PD_EVENT_SINK_FULL, &para); 
	//     }
	// 	return;
	// }


	if(dc_power_in_status_read() != p_dc_power_in_flag)							//only for debug; Totti 2024/6/26
	{
		p_dc_power_in_flag = dc_power_in_status_read();
		SYS_LOG_INF("[%d] DC_POWER_IN = %d \n", __LINE__, p_dc_power_in_flag);
	}


	if(bt_mcu_get_bt_wake_up_flag())
    {
       Wake_up_times = 10;
       SYS_LOG_INF("[%d] wake_up_flag = 1  \n", __LINE__);
       return;
	}

	if(fisrtreadtimes <= 1) 
	{
	   fisrtreadtimes++;
	   return;
	}


	if((!pin_dc_power_in_debunce_count) && !dc_power_in_status_read())
	{
		// SYS_LOG_INF("%d \n", __LINE__);
	 
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

			if(pd_mps52002->pd_source_otg_disable_flag)
			{
				pd_mps52002_pd_otg_on(true); 
			}
			
			para.pd_event_val = pd_mps52002->pd_52002_sink_flag;
			pd_mps52002->notify(PD_EVENT_SINK_STATUS_CHG, &para);
		}
		pd_mps52002->pd_source_disc_debunce_cnt = 0x00;
		pd_mps52002->pd_sink_debounce_time = 0x00;
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
		}
	}

    pd_tps52002_read_reg_value(PD_PD_STATUS, buf);
	
	if(readresult != buf[0])
	{
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
				if(pd_mps52002->pd_52002_sink_flag)
				{
					pd_mps52002->pd_sink_debounce_time = 12;
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

		source_sink_debunce = 0x00;
	
}


bool check_sink_full(void)
{
    bool charger_status;
	u8_t buf_status[2];
	u16_t   reg_value = 0;
	pd_tps52002_read_reg_value(PD_MP2760_STATUS_0, buf_status); 
	reg_value =  (buf_status[1] << 8) | buf_status[0];
	if((reg_value&0x140) == 0x140)
		charger_status = 1;
	else
		charger_status = 0;
	return charger_status;

}
void pd_src_sink_check(void)
{
	struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
	pd_manager_charge_event_para_t para;
	  static u8_t otg_debounce_cnt = 0;

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
	    }
     }

	 if(Wake_up_times > 0)
	 {
	    Wake_up_times--;
	   SYS_LOG_INF("[%d] waiting Wake_up_times = %d", __LINE__, Wake_up_times);
	   return;
	 }
	pd_mps52002->SRC_5OMA_FLAG = pd_mps2760_read_current(&pd_mps52002->volt_value, &pd_mps52002->cur_value,&pd_mps52002->src_cur_value);
	SYS_LOG_INF("------> read vlot %d current %d\n",pd_mps52002->volt_value,pd_mps52002->cur_value);
    if(pd_mps52002->pd_52002_source_flag)
    {
        if(!pd_mps52002->chk_current_flag)
        {
            int16_t value = pd_mps52002->src_cur_value;//pd_mps2760_read_current();
            value = (value>= 0 ? value : -value);
            SYS_LOG_INF("[%d] current value:%d; count:%d\n", __LINE__, value, otg_debounce_cnt);

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
    union dev_config config = {0};
	struct device *iic_dev;
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
	u8_t buf[2];
	buf[0]= index;
	buf[1] =0x80;
    printf("live set_mps_volt  reg 0x%02x\n", index);
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x0a, buf, 2);

}

void pd_mps52002_switch_volt(void)
{
    union dev_config config = {0};
    struct device *iic_dev;
	u8_t buf[2];
    static u8_t PDO_index = 1;;
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
  
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 2);

    printf("live pd_mps52002_switch_volt  reg 0x%02x\n:  ", buf[1]);
 
	 PDO_index ++;
	if(PDO_index > (buf[1]&0x07))
		PDO_index = 1;

	set_mps_volt(PDO_index);
    printf("\n");
}

bool pd_mps52002_ats_switch_volt(u8_t PDO_index)
{
     union dev_config config = {0};
    struct device *iic_dev;
	u8_t buf[2];
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 2);
	if(PDO_index > (buf[1]&0x07))
	{
		 return false;
	}
	set_mps_volt(PDO_index);
    return true;
}
//extern void mcu_ls8a10049t_int_deal(void);
extern int mcu_ui_ota_deal(void);
extern void led_status_manger_handle(void);
extern void wlt_logic_mcu_ls8a10049t_int_fn(void);

static void mcu_pd_iic_time_hander_mps(struct thread_timer *ttimer, void *expiry_fn_arg)
{
    // const struct wlt_pd_mps52002_config *cfg = p_pd_mps52002_dev->config->config_info;
   	struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
    static u16_t mcu_one_secound_count = 0;
	static u8_t set_fisrt_flag = 0;


	led_status_manger_handle();




	if((mcu_one_secound_count % 10) == 0)
	{
	//	printk("%s:%d\n", __func__,__LINE__);
   		
		extern void batt_led_display_timeout(void);
		batt_led_display_timeout();
   		if(mcu_ui_ota_deal()){	 
		   return;
	   	}

    	pd_detect_event_report_MPS52002();
    	//mcu_ls8a10049t_int_deal();
      	if(pd_mps52002->mcu_notify)
    	{
        	pd_mps52002->mcu_notify();
    	}

		pd_mps52002_iic_send_data();
		if(pd_mps52002->pd_65992_unload_flag)
		{
			return;
		}
	}

    if(mcu_one_secound_count++ <= MCU_ONE_SECOND_PERIOD)
    {
        return;
    }
    mcu_one_secound_count = 1; 

//	printk("[%s,%d]\n", __FUNCTION__, __LINE__);

	if(pd_mps52002->charge_sink_test_flag)
	{
	if(!set_fisrt_flag)
       {
           set_mps_volt(1);
		   set_fisrt_flag =1;
		}
	}
  
	pd_src_sink_check();
	if(pd_mps52002->battery_notify)
	{
		 pd_mps52002->battery_notify();
	}
   
	wlt_logic_mcu_ls8a10049t_int_fn();

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
        case PD_SUPPLY_PROP_PLUG_PRESENT:
            val->intval = dc_power_in_status_read();
            break;
		case PD_SUPPLY_PROP_SINK_HIGH_Z_STATE:
            val->intval = 0x00;  
		  	if(pd_mps52002->pd_52002_sink_flag &&(!pd_mps52002->sink_charging_flag))
		     	val->intval = 0x01;
            break;

		case PD_SUPPLY_PROP_OTG_MOBILE:
			pd_tps52002_read_reg_value(PD_PD_STATUS, buf);
			val->intval = (buf[0]>>2) & 0x01;
			SYS_LOG_INF("PD mobile status:%d", buf[0]);
			break;

		case PD_SUPPLY_PROP_UNLOAD_FINISHED:

			val->intval = pd_mps52002->pd_65992_unload_flag;
			// SYS_LOG_INF("PPD_SUPPLY_PROP_UNLOAD_FINISHED:%d", val->intval);
			break;	

		case PD_SUPPLY_PROP_SINK_CURRENT_VAULE:
			val->intval = pd_mps52002->cur_value;
			break;
			
        default:
            break;	  
    }


    return 0;
}

void mps_set_source_disc(void)
{

    u8_t buf[2] = {0};
    union dev_config config = {0};
    struct device *iic_dev;
	

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
	
	wake_up_pd();
		
    buf[0] = 0X01;
    buf[1] = 0x00; 

	pd_mps52002_write_reg_value(PD_SYS_POWER_STATUS, buf);					// 0x05 change function to disconnect
	printf(" pd_tps52002_pd_otg_on PD_DISABLE_SOURCE write = %d \n",buf[0]);
}

void pd_mps52002_sink_charging_disable(bool flag)
{

    struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data;
   
	u8_t buf[2] = {0};
    union dev_config config = {0};
	u8_t buf_status[2] = {0};
    struct device *iic_dev;
   
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
    pd_tps52002_read_reg_value(PD_MP2760_STATUS_1, buf_status);                                                     // 
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
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_CHARGE_CURRENT, buf, 2);
        k_sleep(2);

    }else{
        buf[0] = 0x30;                                                              //                                        
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_CHARGE_CURRENT, buf, 2);
        k_sleep(2);
    }
    
}


void pd_mps52002_iic_send_data()
{
    u8_t type, data;
    struct wlt_pd_mps52002_info *pd_mps52002 = p_pd_mps52002_dev->driver_data; 

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
				thread_timer_stop(&pd_mps52002->timer);
				pd_mps52002->pd_65992_unload_flag = true;
				break;	

			case PD_IIC_TYPE_PROP_SOURCE_DISC:
				mps_set_source_disc();
				break;	

			default:
				break; 
		}      
	}
}


static void pd_mps52002_wlt_set_property(struct device *dev,enum pd_manager_supply_property psp,    \
				       union pd_manager_supply_propval *val)
{
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

			// pd_mps52002_pd_otg_on(true); 	
			MPS_ONOFF_BGATE(0);  

			pd_iic_push_queue(PD_IIC_TYPE_PROP_POWERDOWN, (u8_t)val->intval);

			

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
		k_sleep(50);
	}
}
struct device *wlt_device_get_pd_mps52002(void)
{
     return p_pd_mps52002_dev;
}
void MPS_PB2_LOW(bool OnOFF)
{
  	union dev_config config = {0};
  	struct device *iic_dev;
	iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
	config.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(iic_dev, config.raw);
	 
      
	wake_up_pd();
   	u8_t buf[2]={0x00,0x00};

   	printf("[%s,%d] %02x\n", __FUNCTION__, __LINE__,buf[0]);
   	i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x41, buf, 2);    
}

void MPS_SET_SOURCE_SSRC(bool OnOFF)
{
  union dev_config config = {0};
  struct device *iic_dev;
	 iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
	 config.bits.speed = I2C_SPEED_STANDARD;
	 i2c_configure(iic_dev, config.raw);
	     
	wake_up_pd();
   u8_t buf[2]={0x00,0x00};

    if(OnOFF)
	{
	   buf[0] = 0x01;
    }	
   printf("[%s,%d] %02x\n", __FUNCTION__, __LINE__,buf[0]);
   i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x05, buf, 2);    
}

void MPS_ONOFF_BGATE(bool OnOFF)
{
  union dev_config config = {0};
  struct device *iic_dev;
    
	 iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
	 config.bits.speed = I2C_SPEED_STANDARD;
	 i2c_configure(iic_dev, config.raw);
	 
     wake_up_pd();
    u8_t buf1[2]={0x00,0x00};
    if(OnOFF)
	{
	   buf1[0] = 0x01;
    }		
     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x07, buf1, 2); 
    k_sleep(10);
	printf("[%s,%d] %02x\n", __FUNCTION__, __LINE__,buf1[0]); 
}
// DEVICE_AND_API_INIT(pd_mps52002, "pd_mps52002", pd_mps52002_init,
// 	    &pd_mps52002_ddata, &pd_mps52002_wlt_cdata, POST_KERNEL,
// 	    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &pd_mps52002_wlt_driver_api);


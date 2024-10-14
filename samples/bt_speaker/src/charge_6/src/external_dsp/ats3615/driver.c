#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gpio.h>
#include <os_common_api.h>
#include "include/ats3615_inner.h"
#include "sys_event.h"
#include <gpio.h>
#include <soc.h>
#include "board.h"
#include <thread_timer.h>
#include "power_manager.h"
#include "include/comm_interface.h"
#include "include/dolphin_rw.h"
#include "include/dolphin_com.h"
#include "include/hmdsp_tuning_hdr.h"
#include "include/dolphin_device_ids.h"
#include "self_media_effect/self_media_effect.h"
#define SLC_DEBUG 1 //1 for SLC debug, 0 for actions demo

#if SLC_DEBUG
#include "include/dolphin_firmware.h"
#include "dsp_source_code/charge6_fw_all.h"
#else
#include "dsp_source_code/dram0_all.h"
#include "dsp_source_code/dram1_all.h"
#include "dsp_source_code/iram0_all.h"
#endif

#ifdef CONFIG_C_AMP_TAS5828M
#include "amp/tas5828m/tas5828m_head.h"
#endif

#ifdef CONFIG_SYS_LOG
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "ats3615_driver"
#include <logging/sys_log.h>
#endif

#define ATS3615_CHIP_VERSION_VALUE		0xAC280000

extern int dolphin_comm_init(void);
struct dsp_ats3615_data{
	const unsigned int *data_iram_ptr;
	const unsigned int data_iram_size;
	const unsigned int *data_dram0_ptr;
	const unsigned int data_dram0_szie;
};
const struct dsp_ats3615_data dsp_ats3615_ram_data[]={
#if (SLC_DEBUG == 0)	
	{
		.data_dram0_ptr = data_iram,
		.data_dram0_szie = sizeof(data_iram),
		.data_iram_ptr = data_dram0,
		.data_iram_size = sizeof(data_dram0)
	},
#endif	
};
extern const unsigned char * g_dsp_tuning_info;
extern int g_dsp_tuning_info_valid;
extern unsigned int g_tuning_data_address;
extern unsigned int g_dolphin_com_struct_address;
extern int dolphin_send_tuning_info(void);
extern int get_tuning_info_by_odm(void);

extern int dolphin_comm_init(void);
extern int dolphin_comm_send_wait_finish(dolphin_com_t * p_comm);
extern int dolphin_comm_send_ex(dolphin_com_t * p_comm);
extern int ats3615_re_comm_after_reset(void);
int ext_dsp_send_battery_volt(float battery_volt);
static int dsp_init_flag = 0;
static os_mutex dsp_3615_mutex;
os_mutex *dsp_3615_mutex_ptr = NULL;
static struct thread_timer dsp_3615_timer;
static void show_dsp_code_info(dolphin_dsp_code_hdr_t * dsp_header)
{
	printk("--- dsp_code_info --- \n");
	
	printk("  firmware_info              = %s\n", dsp_header->firmware_info);
	printk("  start_address              = 0x%x\n", dsp_header->start_address);
	printk("  code_version               = %d\n", dsp_header->code_version);
	printk("  signalflow_version         = %d\n", dsp_header->signalflow_version);
	printk("  dolphin_com_struct_address = 0x%08x\n", dsp_header->dolphin_com_struct_address);
	printk("  tuning_data_address        = 0x%08x\n", dsp_header->tuning_data_address);
}

static void show_tuning_info(hmdsp_tuning_hdr_t * tuning_header)
{
	printk("--- tuning_info --- \n");

    printk("  magic                 = 0x%08x\n", tuning_header->magic);
    printk("  version               = %d\n", tuning_header->version);
    printk("  tuning_name           = %s\n", &tuning_header->tuning_name[0]);
    printk("  tuning_file_version   = %d\n", tuning_header->tuning_file_version);
    printk("  signalflow_version    = %d\n", tuning_header->signalflow_version);
    printk("  device_id             = 0x%08x\n", tuning_header->device_id);
    printk("  default_sigflow_index = %d\n", tuning_header->default_sigflow_index);
    printk("  default_tuning_index  = %d\n", tuning_header->default_tuning_index);
}

static int check_dsp_and_tuning_info_valid(void)
{
	dolphin_dsp_code_hdr_t * dsp_header = (dolphin_dsp_code_hdr_t *)charge6_fw;
	hmdsp_tuning_hdr_t * tuning_header = (hmdsp_tuning_hdr_t *)g_dsp_tuning_info;

	show_dsp_code_info(dsp_header);
	show_tuning_info(tuning_header);

    if (tuning_header->magic != HMDSP_TUNING_HDR_MAGIC) {
        SYS_LOG_ERR("Tuning Magic Fail %08x %08x \n", tuning_header->magic, HMDSP_TUNING_HDR_MAGIC);
        return 0;
    }

    if (tuning_header->version != HMDSP_TUNING_HDR_TUNING_FORMAT_VERSION) {
        SYS_LOG_ERR("Tuning Magic version %08x %08x \n", tuning_header->version, HMDSP_TUNING_HDR_TUNING_FORMAT_VERSION);
        return 0;
    }

    if (tuning_header->signalflow_version != dsp_header->signalflow_version) {
        SYS_LOG_ERR("signalflow_version %08x %08x \n", dsp_header->signalflow_version, tuning_header->signalflow_version);
        return 0;
    }

	u32_t product_id = HMDSP_PRODUCT_ID(tuning_header->device_id);
	u32_t manufacturer_id = HMDSP_MANUFACTURER_ID(tuning_header->device_id);
	//u32_t hardware_id = HMDSP_HARDWARE_ID(tuning_header->device_id);

    u32_t odm_id = ReadODM();


	if (product_id != PRODUCT_ID_CHARGE6) {
        SYS_LOG_ERR("product_id %08x %08x \n", product_id, PRODUCT_ID_CHARGE6);
        return 0;
	}


	if (manufacturer_id != odm_id) {
        //SYS_LOG_ERR("manufacturer_id %08x %08x \n", manufacturer_id, odm_id);
        //return 0;
	}

	if((odm_id == HW_TONGLI_BOARD)&&(manufacturer_id == 1)){
		SYS_LOG_INF("manufacturer_id %08x %08x \n", manufacturer_id, odm_id);
        //return 0;
	}else if((odm_id == HW_3NOD_BOARD)&&(manufacturer_id == 5)){
		SYS_LOG_INF("manufacturer_id %08x %08x \n", manufacturer_id, odm_id);
        //return 0;
	}else if((odm_id == HW_GUOGUANG_BOARD)&&(manufacturer_id == 3)){
		SYS_LOG_INF("manufacturer_id %08x %08x \n", manufacturer_id, odm_id);
        //return 0;
	}else{
		SYS_LOG_ERR("manufacturer_id %08x %08x \n", manufacturer_id, odm_id);
        return 0;
	}


/* 	if (hardware_id != odm_id_2) {
        SYS_LOG_ERR("hardware_id %08x %08x \n", hardware_id, odm_id_2);
        return 0;
	} */

	SYS_LOG_INF(" PASS\n");

	// valid pass
	g_tuning_data_address = dsp_header->tuning_data_address;
	g_dolphin_com_struct_address = dsp_header->dolphin_com_struct_address;
    return 1;


}

int external_dsp_ats3615_tuning_init(void)
{
	get_tuning_info_by_odm();
	g_dsp_tuning_info_valid = check_dsp_and_tuning_info_valid();
	return g_dsp_tuning_info_valid;
}
// Temporary use __ram_dsp_start(__ram_dsp_size) memory, so need call it before call inner dsp.
// And ensure memory size <= __ram_dsp_size.
int external_dsp_ats3615_load(int effect)
{
	int ret = 0;
	uint32_t reg_val;
	int retry_cnt = 0;
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
#ifndef SLC_DEBUG
	int write_size,one_write_size;
	const struct dsp_ats3615_data *data_ptr = &dsp_ats3615_ram_data[0];
#endif
	
	if(dsp_3615_mutex_ptr == NULL){
		SYS_LOG_ERR("not have init?\n");
		return -1;
	}
	os_mutex_lock(dsp_3615_mutex_ptr, OS_FOREVER);
	
	if(dsp_init_flag == 1){
		SYS_LOG_ERR("dsp 3615 have init\n");
		goto __err_exit;
	}

	if(!external_dsp_ats3615_tuning_init())
	{
		SYS_LOG_ERR("DSP and tuning info not match ! \n");
		goto __err_exit;
	}

	self_music_effect_ctrl_set_enable(true);
	
	gpio_pin_configure(gpio_dev, DSP_EN_PIN, GPIO_DIR_OUT);
	gpio_pin_write(gpio_dev, DSP_EN_PIN, 0);
	k_sleep(50);

	extern_dsp_3615_io_enable(1);
	board_i2s_io_enable(1);
	k_sleep(10);

	gpio_pin_write(gpio_dev, DSP_EN_PIN, 1);

	// if read chip version ok, it mean dsp ok.
#if 1
	while(1)
	{
		MASTER_Read_ATS3615_Reg(ATS3615_CHIP_VERSION, &reg_val);
		SYS_LOG_INF("ATS3615_CHIP_VERSION:0x%X\n", reg_val);

		if ((reg_val & 0xFFFF0000) == (ATS3615_CHIP_VERSION_VALUE & 0xFFFF0000))
		{
			SYS_LOG_INF("read chip version ok\n");
			break;
		}

		retry_cnt ++;

		k_sleep(10);//10ms

		SYS_LOG_WRN("read chip version fail, retry cnt:%d\n", retry_cnt);

		if (retry_cnt >= 10)
		{
			SYS_LOG_ERR("read chip version fail, break\n");
	
			gpio_pin_configure(gpio_dev, DSP_EN_PIN, GPIO_DIR_OUT);
			gpio_pin_write(gpio_dev, DSP_EN_PIN, 0);
			k_sleep(50);
			gpio_pin_write(gpio_dev, DSP_EN_PIN, 1);

			if(retry_cnt > 20){
				ret = -2;
				goto __err_exit;
			}
		}
	}
#endif

#if SLC_DEBUG
	dolphin_dsp_code_hdr_t * dsp_code = (dolphin_dsp_code_hdr_t *)charge6_fw;
	ret = Dolphin_Firmware_Send(NULL, dsp_code->firmware_data);
	if(ret)
	{
		SYS_LOG_ERR("Dolphin_Firmware_Send fail\n");
		goto __err_exit;
	}
    dolphin_send_tuning_info();

	// k_busy_wait(20*1000);//1ms

	MASTER_Write_ATS3615_Reg(ATS3615_MRCR0, 0); // reset DSP
	MASTER_Write_ATS3615_Reg(ATS3615_WD_CTL, 0);
	MASTER_Write_ATS3615_Reg(ATS3615_SPISLV_SEED, 0); // Unstall DSP
	MASTER_Write_ATS3615_Reg(ATS3615_DSP_VCT_ADDR, 0x40000780);
	MASTER_Write_ATS3615_Reg(ATS3615_MRCR0, (1 << ATS3615_MRCR0_DSP_RESET)); // take DSP out of reset
	k_sleep(20);//20ms
	dolphin_comm_init();
#else //actions demo

	MASTER_Write_ATS3615_Reg(ATS3615_SPISLV_SEED, 0xac28aaaa);
	MASTER_Read_ATS3615_Reg(ATS3615_SPISLV_SEED, &reg_val);
    SYS_LOG_INF("ATS3615_SPISLV_SEED:0x%X\n", reg_val);
    
	/*download iram*/
	SYS_LOG_INF("data_iram size:%d\n", data_ptr->data_dram0_szie);

	for(write_size = 0;write_size < data_ptr->data_dram0_szie;){
		one_write_size = ((data_ptr->data_dram0_szie - write_size) >= (u32_t)&__ram_dsp_size)?(u32_t)&__ram_dsp_size:(data_ptr->data_dram0_szie - write_size);
		memcpy(&__ram_dsp_start, data_ptr->data_dram0_ptr + write_size/4, one_write_size);
		MASTER_Write_ATS3615_Mem(0x40000000 + write_size, (unsigned int*)&__ram_dsp_start, one_write_size);
		write_size += one_write_size;
	}
	/*download dram0*/
	SYS_LOG_INF("data_dram0 size:%d\n", data_ptr->data_iram_size);
	for(write_size = 0;write_size < data_ptr->data_iram_size;){
		one_write_size = ((data_ptr->data_iram_size - write_size) >= (u32_t)&__ram_dsp_size)?(u32_t)&__ram_dsp_size:(data_ptr->data_iram_size - write_size);
		memcpy(&__ram_dsp_start, data_ptr->data_iram_ptr + write_size/4, one_write_size);
		MASTER_Write_ATS3615_Mem(0x30000000 + write_size, (unsigned int*)&__ram_dsp_start, one_write_size);
		write_size += one_write_size;
	}
	/*download dram1*/   //dram1 size = 0
	
    MASTER_Write_ATS3615_Reg(ATS3615_MRCR0, 0);
	MASTER_Read_ATS3615_Reg(ATS3615_MRCR0, &reg_val);
    SYS_LOG_INF("ATS3615_MRCR0:0x%X\n", reg_val);

	MASTER_Write_ATS3615_Reg(ATS3615_SPISLV_SEED, 0); 
    MASTER_Read_ATS3615_Reg(ATS3615_SPISLV_SEED, &reg_val);
    SYS_LOG_INF("ATS3615_SPISLV_SEED:0x%X\n", reg_val);

    MASTER_Write_ATS3615_Reg(ATS3615_DSP_VCT_ADDR, 0x40000780);
    MASTER_Read_ATS3615_Reg(ATS3615_DSP_VCT_ADDR, &reg_val);
    SYS_LOG_INF("ATS3615_DSP_VCT_ADDR:0x%X\n", reg_val);
    
	MASTER_Write_ATS3615_Reg(ATS3615_MRCR0, 1);
#endif
    dsp_init_flag = 1;
    SYS_LOG_INF("dsp load ok\n");

	#ifdef CONFIG_BT_SELF_APP
		extern int spkeq_SetCus(void);
		extern bool selfapp_eq_is_default(void);
		extern u8_t selfapp_config_get_PB_state(void); 
		extern int ext_dsp_restore_default(void);
		ext_dsp_restore_default();
		if((!selfapp_eq_is_default())&& (selfapp_config_get_PB_state() == 0)){
			spkeq_SetCus();
		}
	#endif
	os_mutex_unlock(dsp_3615_mutex_ptr);
	return ret;

__err_exit:
	os_mutex_unlock(dsp_3615_mutex_ptr);
	return ret;
}

void external_dsp_ats3615_deinit(void)
{
	if(dsp_3615_mutex_ptr == NULL){
		SYS_LOG_ERR("not have init?\n");
		return;
	}
	os_mutex_lock(dsp_3615_mutex_ptr, OS_FOREVER);
	dolphin_comm_deinit();
	extern_dsp_3615_io_enable(0);
	board_i2s_io_enable(0);
	dsp_init_flag = 0;
	SYS_LOG_INF("ok\n");
	os_mutex_unlock(dsp_3615_mutex_ptr);
	return;
}

int external_dsp_ats3615_reset(void)
{
	SYS_LOG_WRN(" ");
	if(dsp_3615_mutex_ptr == NULL){
		SYS_LOG_ERR("not have init?\n");
		return -1;
	}
	os_mutex_lock(dsp_3615_mutex_ptr, OS_FOREVER);
	dolphin_comm_deinit();
	dsp_init_flag = 0;
	SYS_LOG_INF("ok\n");
	os_mutex_unlock(dsp_3615_mutex_ptr);
	external_dsp_ats3615_load(0);
	return 0;
}

void external_dsp_ats3615_pre_init(void)
{
	if(dsp_3615_mutex_ptr == NULL){
		dsp_3615_mutex_ptr = &dsp_3615_mutex;
		os_mutex_init(dsp_3615_mutex_ptr);
	}else{
		SYS_LOG_ERR("have init?\n");
	}
}

#ifdef CONFIG_SPI_SWITCH_BY_GPIO
#define DSP_INT_PIN			12

extern void board_spi1_disable(void);
extern void board_spi1_enable(void);

void spi_switch_polling(void)
{
	static u8_t dsp_gpio_status = 1;
	u32_t val, val2;
	//return;
	if(dsp_init_flag == 0)
		return;
		
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	gpio_pin_read(gpio_dev, DSP_INT_PIN, &val);
	printk("spi %d\n",val);
	u8_t gpio_status = (val != 0);
	if (dsp_gpio_status != gpio_status)
	{
		k_sleep(K_MSEC(5));
		gpio_pin_read(gpio_dev, DSP_INT_PIN, &val2);
		if (val != val2)
			return;

		k_sleep(K_MSEC(5));
		gpio_pin_read(gpio_dev, DSP_INT_PIN, &val2);
		if (val != val2)
			return;

		dsp_gpio_status = gpio_status;
		printk("spi_switch_polling : %d \n", gpio_status);
		if (gpio_status)
		{
			SYS_LOG_INF("SPI1 enable\n");
			board_spi1_enable();
			k_sleep(20);
			dolphin_comm_init();
		}
		else
		{
			SYS_LOG_INF("SPI1 disable\n");
			dolphin_comm_deinit();
			board_spi1_disable();
		}
	}

}
#endif

static void external_dsp_ats3615_timer_handle(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	static u32_t timer_cnt = 0;
	os_mutex_lock(dsp_3615_mutex_ptr, OS_FOREVER);
	if(dsp_init_flag == 0){

		os_mutex_unlock(dsp_3615_mutex_ptr);
		return ;
	}

#if SLC_DEBUG
	timer_cnt++;
	if((timer_cnt%5) == 0)
		ats3615_comm_send_battery_volt(((float)power_manager_get_battery_vol())/1000000);
#endif

#ifdef CONFIG_SPI_SWITCH_BY_GPIO
	spi_switch_polling();
#endif	

	os_mutex_unlock(dsp_3615_mutex_ptr);
}

void external_dsp_ats3615_timer_start(void)
{
    spi_switch_polling();
	thread_timer_init(&dsp_3615_timer, external_dsp_ats3615_timer_handle, NULL);
    thread_timer_start(&dsp_3615_timer, 1000, 1000);
}











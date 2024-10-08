#include <zephyr.h>
#include <device.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ats_cmd/ats.h"
#include "common/sys_reboot.h"
#include <mem_manager.h>
#include <property_manager.h>
#include <bt_manager.h>
#include "soft_version/soft_version.h"
#include <power_manager.h>
#include <board.h>
#include "audio_policy.h"
#include "volume_manager.h"
#include "audio_system.h"
#include "self_media_effect/self_media_effect.h"
#include <hex_str.h>
#include <data_analy.h>
#include <app_ui.h>
#include <fw_version.h>
#include <sys_manager.h>
#include <nvram_config.h>
#include <sys_manager.h>

#include<led_manager.h>
#include<bt_manager_inner.h>
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "ats_cdc_shell"
#include <logging/sys_log.h>

#include <app_launch.h>
#include <wltmcu_manager_supply.h>
#include <sys_wakelock.h>
#include <sys_event.h>

#define CFG_USER_INFO_0   			"USER_INFO_0"
#define CFG_USER_INFO_1   			"USER_INFO_1"
#define	FAC_TEST_USER_DATA0			"TEST"//"WLT_CUSTOM_DATA_FOR_READ_WRITE"

static uint8_t *ats_cmd_resp_buf;
static  int ats_cmd_resp_buf_size = 50;

extern bool pd_mps52002_ats_switch_volt(u8_t PDO_index);
extern int bt_manager_bt_read_rssi(uint16_t handle);
extern int pd_manager_get_volt_cur_value(int16 *volt_value, int16 *cur_value);
extern int user_uuid_verify(void);
extern unsigned char is_authenticated(void);
extern int ext_dsp_set_bypass(int bypass);
extern int logic_mcu_read_ntc_status(void);
extern void hm_ext_pa_select_left_speaker(void);
extern void hm_ext_pa_select_right_speaker(void);
extern int pd_manager_test_set_sink_charge_current(u8_t step);
extern int pd_manager_test_get_sink_charge_current(u8_t *sink_charge_step);
extern u32_t fw_version_get_sw_code(void);
extern u8_t fw_version_get_hw_code(void);
extern int mcu_ui_get_logic_version(void);
// extern int bt_mcu_send_pw_cmd_powerdown(void);
extern void sys_event_notify(uint32_t event);
extern int charge_app_enter_cmd(void);
extern int16_t wlt_get_battery_volt(void);
extern int16_t wlt_get_battery_cur(void);
extern bool user_get_auracast_sink_connected(void);
extern uint8_t system_app_get_auracast_mode(void);
// 
void hex_to_string_4(u32_t num, u8_t *buf) {
	buf[0] = '0' + num%10000/1000;
	buf[1] = '0' + num%1000/100;
	buf[2] = '0' + num%100/10;
	buf[3] = '0' + num%10;
}
void hex_to_string_2(u8_t num, u8_t *buf) {
	/* for example */
	buf[0] = '0' + num%100/10;
	buf[1] = '0' + num%10;
}
void string_to_hex_u8(u8_t *buf,u8_t *num) {
	*num = (buf[0]-'0')*10 + (buf[1]-'0');
}

#if 0
static int check_buffer_all_byte_is_n(u8_t *buf, int len, u8_t n_char)
{
	int i;

	for (i=0; i<len; i++)
	{
		if (buf[i] != n_char)
		{
			return -1;
		}
	}

	return 0;
}
#endif
static int cdc_shell_ats_init(void)
{

	if (ats_cmd_resp_buf == NULL)
	{
		ats_cmd_resp_buf = malloc(ats_cmd_resp_buf_size);
		if (ats_cmd_resp_buf == NULL)
		{
	
			return 0;
		}
		memset(ats_cmd_resp_buf, 0, ats_cmd_resp_buf_size);
	}

	return 0;
}

static int ats_usb_cdc_acm_cmd_response_ok_or_fail(struct device *dev, u8_t is_ok)
{
	int index = 0;

	if (!ats_cmd_resp_buf)
	{
		return -1;
	}
	if (is_ok)
	{
      ats_cmd_resp_buf_size = sizeof(ATS_CMD_RESP_OK)+sizeof(ATS_AT_CMD_TAIL)-2;
	}
	else
	{
     ats_cmd_resp_buf_size = sizeof(ATS_CMD_RESP_FAIL)+sizeof(ATS_AT_CMD_TAIL)-2;
	}
		
	memset(ats_cmd_resp_buf, 0, ats_cmd_resp_buf_size);

	if (is_ok)
	{
		memcpy(&ats_cmd_resp_buf[index], ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);
		index += sizeof(ATS_CMD_RESP_OK)-1;
	}
	else
	{
		memcpy(&ats_cmd_resp_buf[index], ATS_CMD_RESP_FAIL, sizeof(ATS_CMD_RESP_FAIL)-1);
		index += sizeof(ATS_CMD_RESP_FAIL)-1;
	}
	memcpy(&ats_cmd_resp_buf[index], ATS_AT_CMD_TAIL, sizeof(ATS_AT_CMD_TAIL)-1);

	ats_usb_cdc_acm_cmd_response(dev, ats_cmd_resp_buf, ats_cmd_resp_buf_size);

	return 0;
}

int ats_usb_cdc_acm_cmd_response_at_data(struct device *dev, u8_t *cmd, int cmd_len, u8_t *ext_data, int ext_data_len)
{
	int index = 0;

	if (!ats_cmd_resp_buf)
	{
		return -1;
	}
	ats_cmd_resp_buf_size = cmd_len + ext_data_len + sizeof(ATS_AT_CMD_TAIL)-1;

	memset(ats_cmd_resp_buf, 0, ats_cmd_resp_buf_size);

	if (cmd != NULL || cmd_len != 0)
	{
		memcpy(&ats_cmd_resp_buf[index], cmd, cmd_len);
		index += cmd_len;
	}
	
	if (ext_data != NULL || ext_data_len != 0)
	{
		memcpy(&ats_cmd_resp_buf[index], ext_data, ext_data_len);
		index += ext_data_len;
	}
	
    memcpy(&ats_cmd_resp_buf[index], ATS_AT_CMD_TAIL, sizeof(ATS_AT_CMD_TAIL)-1);
	ats_usb_cdc_acm_cmd_response(dev, ats_cmd_resp_buf, ats_cmd_resp_buf_size);

	return 0;
}



static int cdc_shell_ats_exit_ats_and_reset(struct device *dev, u8_t *buf, int len)
{


	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	// 产测的时候烧录ATS的固件，产测结束的最后一步，发送这一条命令，将工厂区的这个配置项数据置0,
	// 下次开机就不会自动进入ATS模式。
	// 需要清除nvram factory的数据，而不是nvram factory rw区的数据，因为如果重新烧录ATS的固件，
	// 一般多媒体量产工具不能勾选擦除工厂分区数据，以免造成蓝牙地址等关键数据的丢失。
	// 而这个限制又会导致nvram factory rw区的数据无法擦除，导致重新烧录ATS固件后，无法自动进入ATS模式。

	//property_set_factory_no_use_rw_region(CFG_AUTO_ENTER_ATS, "0", 1);
	
	struct app_msg msg = { 0 };
    msg.type = MSG_INPUT_EVENT;
    msg.cmd = MSG_FACTORY_DEFAULT;
	msg.value = ATS_SYS_REBOOT_DELAY_TIME_MS;
    send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
	//ats_sys_power_off();by

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_DUT_SET, sizeof(ATS_CMD_RESP_DUT_SET)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	//sys_pm_reboot(0);
	return 0;
}

static int cdc_shell_ats_reboot(struct device *dev, u8_t *buf, int len)
{
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_DUT_REBOOT, sizeof(ATS_CMD_RESP_DUT_REBOOT)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);
	
	sys_pm_reboot(REBOOT_REASON_REBOOT_AND_POWERON);
	return 0;
}

static int cdc_shell_ats_power_off(struct device *dev, u8_t *buf, int len)
{
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_POWER_OFF, sizeof(ATS_CMD_RESP_POWER_OFF)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);
	
	charge_app_enter_cmd();

	return 0;
}

static int cdc_shell_ats_disconnect_bt_dev_and_enter_bt_pair(struct device *dev, u8_t *buf, int len)
{
	u8_t buffer[16] = {0};
	SYS_LOG_INF("\n");

	memcpy(buffer,ATS_CMD_RESP_ENTER_PAIR,sizeof(ATS_CMD_RESP_ENTER_PAIR)-1);
	//memcpy(buffer+14,buf,2);
	ats_usb_cdc_acm_cmd_response_at_data(
			dev, buffer, sizeof(buffer), 
			NULL, 0);

	bt_manager_auto_reconnect_stop();
	bt_manager_disconnect_all_device();
	bt_manager_end_pair_mode();
	bt_manager_enter_pair_mode();

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}

static int cdc_shell_ats_bt_rssi(struct device *dev, u8_t *buf, int len)
{
   int RSSI_VALUE;
   u8_t buffer[3+1] = "-99";
	
	RSSI_VALUE = -bt_manager_bt_read_rssi(0);
	hex_to_string_2((u8_t)RSSI_VALUE,buffer+1);
	//snprintf(buffer, sizeof(buffer), "%04d", RSSI_VALUE);
	ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_READ_RSSI, sizeof(ATS_CMD_RESP_READ_RSSI)-1, 
			buffer, 3);

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);	
	return 0;
}
static int cdc_shell_ats_bt_status(struct device *dev, u8_t *buf, int len)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	uint8_t buffer[2+1] = "00";

  if(bt_manager->bt_state==3)	{
	buffer[1] += 1;
	ats_usb_cdc_acm_cmd_response_at_data(dev,ATS_CMD_RESP_BT_STATUS_READ,\
	sizeof(ATS_CMD_RESP_BT_STATUS_READ)-1,buffer,sizeof(buffer)-1
	);
	 ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);	
  }
   else{
	buffer[1] += 0;
	ats_usb_cdc_acm_cmd_response_at_data(dev,ATS_CMD_RESP_BT_STATUS_READ,\
	sizeof(ATS_CMD_RESP_BT_STATUS_READ)-1,buffer,sizeof(buffer)-1
	);
  	 ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);	
   } 
	return 0;
}
#if 0
static int cdc_shell_ats_reset(struct device *dev, u8_t *buf, int len)
{
	
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	struct app_msg msg = { 0 };
    msg.type = MSG_INPUT_EVENT;
    msg.cmd = MSG_FACTORY_DEFAULT;
	msg.value = ATS_SYS_REBOOT_DELAY_TIME_MS;
    send_async_msg(CONFIG_FRONT_APP_NAME, &msg);

	 ats_sys_reboot();

	return 0;
}
#endif
static int cdc_shell_ats_color_write(struct device *dev, u8_t *buf, int len)
{
	int result;
	if(len>5 || len<1){
		/* limit length 1-5 */
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
		return 0;
	}

	ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_SET_COLOR, sizeof(ATS_CMD_RESP_SET_COLOR)-1, 
			buf, len);

	result = ats_color_write(buf, len);
	if(result<0){
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
	}
	else{
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	}

	return 0;
}

static int cdc_shell_ats_color_read(struct device *dev, u8_t *buf, int len)
{
	int result;
	uint8_t buffer[5+1] = {0};

	result = ats_color_read(buffer, sizeof(buffer)-1);
	if (result < 0)
	{
		ats_usb_cdc_acm_cmd_response_at_data(dev, ATS_CMD_RESP_COLOR_READ, sizeof(ATS_CMD_RESP_COLOR_READ)-1, NULL, 0);
	}
	else 
	{
		ats_usb_cdc_acm_cmd_response_at_data(dev, ATS_CMD_RESP_COLOR_READ, sizeof(ATS_CMD_RESP_COLOR_READ)-1, buffer, result);
	}
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}

static int cdc_shell_ats_gfps_model_id_write(struct device *dev, u8_t *buf, int len)
{
	int result;
	
	if(len!=6){
		/* limit length 1-5 */
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
		return 0;
	}

	ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_SET_MODEID, sizeof(ATS_CMD_RESP_SET_MODEID)-1, 
			buf, len);

	result = ats_gfps_model_id_write(buf, len);
	if(result<0){
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
	}
	else{
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	}

	return 0;
}

static int cdc_shell_ats_gfps_model_id_read(struct device *dev, u8_t *buf, int len)
{
	int result;
	u8_t buffer[3*2+1] = {0};

	result = ats_gfps_model_id_read(buffer, sizeof(buffer)-1);
	if (result < 0)
	{			
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_GFPS_MODEL_ID_READ, sizeof(ATS_CMD_RESP_GFPS_MODEL_ID_READ)-1, 
			NULL, 0);
	}
	else 
	{		
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_GFPS_MODEL_ID_READ, sizeof(ATS_CMD_RESP_GFPS_MODEL_ID_READ)-1, 
			buffer, result);
	}
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}

static int cdc_shell_ats_dsn_write(struct device *dev, u8_t *buf, int len)
{
	int result;

	if(len<16 || len>30){
		/* limit length 1-30 */
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
		return 0;
	}
	ats_usb_cdc_acm_cmd_response_at_data(
		dev,ATS_CMD_RESP_DSN_SET_ID,sizeof(ATS_CMD_RESP_DSN_SET_ID)-1,
		buf,len);

	result = ats_dsn_write(buf, len);
	if(result<0){
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
	}
	else{
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	}
	return 0;	
}

static int cdc_shell_ats_dsn_read(struct device *dev, u8_t *buf, int len)
{
	int result;
	u8_t buffer[30+1] = {0};

	result = ats_dsn_read(buffer, sizeof(buffer)-1);
	if (result < 0)
	{			
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_DSN_GET_ID, sizeof(ATS_CMD_RESP_DSN_GET_ID)-1, 
			NULL, 0);
	}
	else 
	{		
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_DSN_GET_ID, sizeof(ATS_CMD_RESP_DSN_GET_ID)-1, 
			buffer, result);
	}
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}
static int cdc_shell_ats_psn_write(struct device *dev, u8_t *buf, int len)
{
	int result;

	if(len<16 || len>30){
		/* limit length 1-30 */
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
		return 0;
	}
	ats_usb_cdc_acm_cmd_response_at_data(
		dev,ATS_CMD_RESP_PSN_SET_ID,sizeof(ATS_CMD_RESP_PSN_SET_ID)-1,
		buf,len);

	result = ats_psn_write(buf, len);

	if(result<0){
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
	}
	else{
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	}

	return 0;
}

static int cdc_shell_ats_psn_read(struct device *dev, u8_t *buf, int len)
{
	int result;
	u8_t buffer[30+1] = {0};

	result = ats_psn_read(buffer, sizeof(buffer)-1);
	if (result < 0)
	{			
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_PSN_GET_ID, sizeof(ATS_CMD_RESP_PSN_GET_ID)-1, 
			NULL, 0);
	}
	else 
	{		
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_PSN_GET_ID, sizeof(ATS_CMD_RESP_PSN_GET_ID)-1, 
			buffer, result);
	}
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}


static int cdc_shell_ats_read_bt_dev_name(struct device *dev, u8_t *buf, int len)
{
	int result;
	char buffer[32+1] = {0};

	result = ats_bt_dev_name_read(buffer, sizeof(buffer)-1);
	if (result < 0)
	{	
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_READ_BT_DEV_NAME, sizeof(ATS_CMD_RESP_READ_BT_DEV_NAME)-1, 
			NULL, 0);
	}
	else 
	{
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_READ_BT_DEV_NAME, sizeof(ATS_CMD_RESP_READ_BT_DEV_NAME)-1, 
			buffer, strlen(buffer));
	}
	
    ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}

static int cdc_shell_ats_read_bt_dev_mac_addr(struct device *dev, u8_t *buf, int len)
{
	int result;
	char buffer[12+1] = {0};

	result = ats_bt_dev_mac_addr_read(buffer, sizeof(buffer)-1);
	if (result < 0)
	{
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_READ_BT_DEV_MAC_ADDR, sizeof(ATS_CMD_RESP_READ_BT_DEV_MAC_ADDR)-1, 
			NULL, 0);
	}
	else 
	{
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_READ_BT_DEV_MAC_ADDR, sizeof(ATS_CMD_RESP_READ_BT_DEV_MAC_ADDR)-1, 
			buffer, sizeof(buffer)-1);
	}
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}

static int cdc_shell_ats_sw_version_info_dump(struct device *dev, u8_t *buf, int len)
{
	uint8_t buffer[] = "0000";
	uint32_t swver = fw_version_get_sw_code(); //for example 0x010700-1.7.0
	uint8_t hwver = fw_version_get_hw_code();
	uint32_t swver_hex;
	swver_hex = (((swver>>0)&0xf)*10) + (((swver>>8)&0xf)*100) + (((swver>>16)&0xf)*1000);
	hex_to_string_4(swver_hex,buffer);
	buffer[3] = (hwver%10)+'0';//sw + hw

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_SW_VERSION_INFO_DUMP, sizeof(ATS_CMD_RESP_SW_VERSION_INFO_DUMP)-1, 
		buffer, sizeof(buffer)-1);

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}
static int cdc_shell_ats_hw_version_info_dump(struct device *dev, u8_t *buf, int len)
{
	uint8_t buffer[] = "0000";
	u8_t hwver = fw_version_get_hw_code();

	hex_to_string_2(hwver,buffer+2);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_HW_VERSION_INFO_DUMP, sizeof(ATS_CMD_RESP_HW_VERSION_INFO_DUMP)-1, 
		buffer, sizeof(buffer)-1);

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}
static int cdc_shell_ats_battery_level(struct device *dev, u8_t *buf, int len)
{
	char buffer[3+1] = "000";
	char buffer1[2+1] = "00";
	int battery_capacity;

	battery_capacity = power_manager_get_battery_capacity();

	if(battery_capacity == 100) 
	{	
		snprintf(buffer, sizeof(buffer), "%03d", battery_capacity);
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_BATTERY_LEVEL, sizeof(ATS_CMD_RESP_BATTERY_LEVEL)-1, 
			buffer, sizeof(buffer)-1);
	}
	else
	{
		snprintf(buffer1, sizeof(buffer1), "%02d", battery_capacity);
		ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_BATTERY_LEVEL, sizeof(ATS_CMD_RESP_BATTERY_LEVEL)-1, 
		buffer1, sizeof(buffer1)-1);
	}
 	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);	
	return 0;
}

static int cdc_shell_ats_pd_version_info_dump(struct device *dev, u8_t *buf, int len)
{
	uint8_t buffer[2] = {0};

	//snprintf(buffer, sizeof(buffer), "%02d", PD_version);
	hex_to_string_2(pd_get_pd_version(),buffer);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_PD_VERSION_INFO_DUMP, sizeof(ATS_CMD_RESP_PD_VERSION_INFO_DUMP)-1, 
		buffer, sizeof(buffer));
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	return 0;
}
static int cdc_shell_ats_disable_charger(struct device *dev, u8_t *buf, int len)
{	
	 pd_manager_disable_charging(true);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_DISABLE_CHARGER, sizeof(ATS_CMD_RESP_DISABLE_CHARGER)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);
	return 0;
}

static int cdc_shell_ats_battery_ad_level(struct device *dev, u8_t *buf, int len)
{	
	int battery_volt;
	uint8_t buffer[4+1] = "0000";

	battery_volt = power_manager_get_battery_vol()*2;
	//battery_volt /= 1000; // change voltage unit uv to mv
	snprintf(buffer, sizeof(buffer), "%04d", battery_volt);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_BATTERY_AD_VALUE, sizeof(ATS_CMD_RESP_BATTERY_AD_VALUE)-1, 
		buffer, sizeof(buffer)-1);
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	return 0;
}
static int cdc_shell_ats_battery_curr(struct device *dev, u8_t *buf, int len)
{	
	int16 battery_volt,battery_curr;
	uint8_t buffer[9+1] = "0000:0000";
	//pd_mps2760_read_current(&battery_volt,&src_battery_volt,&battery_curr,&ext_curr);
	pd_manager_get_volt_cur_value(&battery_volt,&battery_curr);
	hex_to_string_4(battery_volt,buffer);
	hex_to_string_4(battery_curr,buffer+5);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_BATTERY_CUR_VALUE, sizeof(ATS_CMD_RESP_BATTERY_CUR_VALUE)-1, 
		buffer, sizeof(buffer)-1);
	 ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	return 0;
}
static int cdc_shell_ats_set_sink_charge_current(struct device *dev, u8_t *buf, int len)
{
	static u8_t step;
	
	string_to_hex_u8(buf,&step);
	pd_manager_test_set_sink_charge_current(step);	

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_SET_CHARGE_LEVEL, sizeof(ATS_CMD_RESP_SET_CHARGE_LEVEL)-1-2, 
		buf, len);

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}
static int cdc_shell_ats_get_sink_charge_current(struct device *dev, u8_t *buf, int len)
{
	u8_t step;
	uint8_t buffer[2+1] = "00";
	
	pd_manager_test_get_sink_charge_current(&step);
	hex_to_string_2(step,buffer);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_SET_CHARGE_LEVEL, sizeof(ATS_CMD_RESP_SET_CHARGE_LEVEL)-1-2, 
		buffer, 2);

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}

extern void bt_ui_charging_warning_handle(uint8_t status);
extern int mcu_ui_send_led_code(uint8_t type, int code);
static int cdc_shell_ats_all_turn_on(struct device *dev, u8_t *buf, int len)
{
	//ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
     
	 bt_ui_charging_warning_handle(1);
	 led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_LED_ALL_ON, sizeof(ATS_CMD_RESP_LED_ALL_ON)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_all_wled_turn_on(struct device *dev, u8_t *buf, int len)
{    
	/* trun all led off before turn one white leds */
    bt_ui_charging_warning_handle(0);
	led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);

	mcu_ui_send_led_code(1,1);
	mcu_ui_send_led_code(2,1);
	mcu_ui_send_led_code(3,1);
	mcu_ui_send_led_code(4,1);
	mcu_ui_send_led_code(5,1);
	mcu_ui_send_led_code(6,1);
	mcu_ui_send_led_code(7,1);
	led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_LED_ALL_ON, sizeof(ATS_CMD_RESP_LED_ALL_ON)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_all_turn_off(struct device *dev, u8_t *buf, int len)
{
	//ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

    bt_ui_charging_warning_handle(0);
	 led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_LED_ALL_OFF, sizeof(ATS_CMD_RESP_LED_ALL_OFF)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}

static int cdc_shell_ats_led_one_turn_on(struct device *dev, u8_t *buf, int len)
{
//ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	u8_t buffer[13] = {0};

	/* trun all led off before turn one led */
    bt_ui_charging_warning_handle(0);
	led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);

	if (!memcmp(buf, "01",2))
	{
		mcu_ui_send_led_code(0,1);
	}
	else if (!memcmp(buf, "02",2))
	{
		mcu_ui_send_led_code(1,1);
	}
	else if (!memcmp(buf, "03",2))
	{
		mcu_ui_send_led_code(2,1);
	}
	else if (!memcmp(buf, "04",2))
	{
		mcu_ui_send_led_code(3,1);
	}
	else if (!memcmp(buf, "05",2))
	{
		mcu_ui_send_led_code(4,1);
	}
	else if (!memcmp(buf, "06",2))
	{
		mcu_ui_send_led_code(5,1);
	}
	else if (!memcmp(buf, "07",2))
	{
		mcu_ui_send_led_code(6,1);
	}
	else if (!memcmp(buf, "08",2))
	{
		mcu_ui_send_led_code(7,1);
	}
	else if (!memcmp(buf, "09",2))
	{
		led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);
	}
	memcpy(buffer,ATS_CMD_RESP_LED_ON,sizeof(ATS_CMD_RESP_LED_ON)-1);

	memcpy(buffer+10,buf,2);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, buffer, sizeof(buffer), 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}



static int cdc_shell_ats_enter_key_check(struct device *dev, u8_t *buf, int len)
{
	SYS_LOG_INF("ok\n");

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	ats_enter_key_check(true);
	return 0;
}

static int cdc_shell_ats_exit_key_check(struct device *dev, u8_t *buf, int len)
{
	SYS_LOG_INF("ok\n");

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	ats_enter_key_check(false);
	return 0;
}

#define UUID_STR_DATA_LEN       (32)
#define UUID_MSG_SIGNATURE_LEN  (256)
#define UUID_HASH_DATA_LEN      (32)
#define UUID_SIGN_MSG_NAME "uuid_msg"


extern int hex2bin(uint8_t *dst, const char *src, unsigned long count);
extern char *bin2hex(char *dst, const void *src, unsigned long count);

static int cdc_shell_IC_UUID_dump(struct device *dev, u8_t *buf, int len)
{
	uint32_t uuid[4];
	uint8_t uuid_str[UUID_STR_DATA_LEN + 1];
    soc_get_system_uuid(uuid);
	bin2hex(uuid_str, uuid, 16);
	uuid_str[32] =  0;
	
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_AT_CMD_READ_CHIP_UUID, sizeof(ATS_AT_CMD_READ_CHIP_UUID)-1, 
		uuid_str, sizeof(uuid_str)-1);

	return 0;
}

static int cdc_shell_write_hash_uuid(struct device *dev, u8_t *buf, int len)
{
	int rlen;
    char *sign_msg = mem_malloc(UUID_MSG_SIGNATURE_LEN);
	char *hash_uuid = mem_malloc(513);
    if(len < 500)
       goto exit;
	
	memcpy(hash_uuid, &buf[sizeof(ATS_AT_CMD_WRITE_HASH_UUID)-1], 513);
    rlen = hex2bin(sign_msg, hash_uuid, UUID_MSG_SIGNATURE_LEN);	

	if(rlen == 0){
		nvram_config_set_factory(UUID_SIGN_MSG_NAME, sign_msg , UUID_MSG_SIGNATURE_LEN);
	}
	else{
		goto exit;
	}

	if(!user_uuid_verify()){
		/* uuid verify OK! */
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	}
	else{
		/* uuid verify NG! */
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
	}

	mem_free(sign_msg);
	mem_free(hash_uuid);
    return 0;
 exit:
    ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
	mem_free(sign_msg);
	mem_free(hash_uuid);	
    return 0;
}

static int cdc_shell_ats_ntc_temperature_dump(struct device *dev, u8_t *buf, int len)
{
	uint8_t buffer[2+1] = "00";
	int ntc = 0;

	ntc = power_manager_get_battery_temperature();//hal_ntc_get_ats_status();
    ntc /=10;
	snprintf(buffer, sizeof(buffer), "%02d", ntc);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_NTC_TEMPERATURE_DUMP, sizeof(ATS_CMD_RESP_NTC_TEMPERATURE_DUMP)-1, 
		buffer, sizeof(buffer)-1);
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	return 0;
}

static int cdc_shell_ats_rf_test_mode(struct device *dev, u8_t *buf, int len)
{
	int result;

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_DUT_MODE_IN, sizeof(ATS_CMD_RESP_DUT_MODE_IN)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	//ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	result = property_set_int(CFG_BT_TEST_MODE, 2);
	
	property_flush(CFG_BT_TEST_MODE);
	
	if (result != 0)
	{
		SYS_LOG_ERR("nvram set err\n");
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
	}
	else
	{
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
		//sys_reboot_by_user(REBOOT_REASON_GOTO_BQB_ATT);//(REBOOT_REASON_USER_ATS_ENTER_BQB);
		//ats_sys_reboot();
		sys_pm_reboot(REBOOT_REASON_GOTO_BQB_ATT);
	}

	return 0;
}

static int cdc_shell_ats_volume_adjust(struct device *dev, u8_t *buf, int len)
{
	int volume;
	int volume_max = audio_policy_get_volume_level();
	u8_t buffer[13];

	SYS_LOG_INF("ok\n");

	memcpy(buffer,ATS_CMD_RESP_SET_VOL,sizeof(ATS_CMD_RESP_SET_VOL)-1);
	memcpy(buffer+11,buf,2);
	ats_usb_cdc_acm_cmd_response_at_data(
			dev, buffer, sizeof(buffer), 
			NULL, 0);	

	if (buf[0] < '0' || buf[0] > '9' || 
		buf[1] < '0' || buf[1] > '9')
	{
		SYS_LOG_ERR("arg err\n");
		goto __err_exit;
	}

	volume = (buf[0]-'0')*10 + (buf[1]-'0');
	SYS_LOG_INF("arg volume:%d\n", volume);
	
	if (volume > volume_max)
	{
		SYS_LOG_ERR("arg_volume err\n");
		goto __err_exit;
	}

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	system_volume_set(AUDIO_STREAM_MUSIC, volume, true);
	system_volume_set(AUDIO_STREAM_SOUNDBAR, volume, true);
	system_volume_set(AUDIO_STREAM_LE_AUDIO, volume, true);
	
	return 0;

__err_exit:
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
	return 0;
}
extern uint8_t ReadODM(void);
static int cdc_shell_ats_odm_dump(struct device *dev, u8_t *buf, int len)
{
	char buffer[2+1] = "00";

	SYS_LOG_INF("ok\n");

	if(ReadODM() == 0) //3nod
	{
		memcpy(buffer, "00", 2);
	}
	else if(ReadODM() == 1) //tonly
	{
		memcpy(buffer, "01", 2);
	}
	else if(ReadODM() == 2)// GUOGUANG
	{
		memcpy(buffer, "02", 2);
	}	

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_ODM_DUMP, sizeof(ATS_CMD_RESP_ODM_DUMP)-1, 
		buffer, sizeof(buffer)-1);
	
	return 0;
}

static int cdc_shell_ats_music_enter_bypass(struct device *dev, u8_t *buf, int len)
{
	if(ext_dsp_set_bypass(true) == 0){
		/* enter bypass ok */
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_ENTER_SPK_BYPASS, sizeof(ATS_CMD_RESP_ENTER_SPK_BYPASS)-1, 
			ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);
	}	
	else{
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_ENTER_SPK_BYPASS, sizeof(ATS_CMD_RESP_ENTER_SPK_BYPASS)-1, 
			ATS_CMD_RESP_FAIL, sizeof(ATS_CMD_RESP_FAIL)-1);
	}

	return 0;
}


static int cdc_shell_ats_music_exit_bypass(struct device *dev, u8_t *buf, int len)
{
	if(ext_dsp_set_bypass(false) == 0){
		/* exit bypass ok */
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_EXIT_SPK_BYPASS, sizeof(ATS_CMD_RESP_EXIT_SPK_BYPASS)-1, 
			ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);
	}	
	else{
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_EXIT_SPK_BYPASS, sizeof(ATS_CMD_RESP_EXIT_SPK_BYPASS)-1, 
			ATS_CMD_RESP_FAIL, sizeof(ATS_CMD_RESP_FAIL)-1);
	}

	return 0;
}

static int cdc_shell_ats_music_bypass_status_read(struct device *dev, u8_t *buf, int len)
{
	char buffer[2+1] = {0};
	bool effect_enable;

	effect_enable = self_music_effect_ctrl_get_enable();
	snprintf(buffer, sizeof(buffer), "%02d", !effect_enable);

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_MUSIC_BYPASS_STATUS_READ, sizeof(ATS_CMD_RESP_MUSIC_BYPASS_STATUS_READ)-1, 
			buffer, sizeof(buffer)-1);

	return 0;
}

static int cdc_shell_ats_serial_number_write(struct device *dev, u8_t *buf, int len)
{
	int result;

	result = ats_serial_number_write(buf, len);
	if (result != 0)
	{
		SYS_LOG_ERR("err\n");
	}
	else
	{
		SYS_LOG_INF("ok\n");
	}

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	return 0;
}

static int cdc_shell_ats_serial_number_read(struct device *dev, u8_t *buf, int len)
{
	int result;
	char buffer[16+1] = {0};

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	result = ats_serial_number_read(buffer, sizeof(buffer)-1);
	if (result < 0)
	{
		SYS_LOG_ERR("err\n");

		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_SERIAL_NUMBER_READ, sizeof(ATS_CMD_RESP_SERIAL_NUMBER_READ)-1, 
			NULL, 0);
	}
	else
	{
		SYS_LOG_INF("ok\n");

		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_SERIAL_NUMBER_READ, sizeof(ATS_CMD_RESP_SERIAL_NUMBER_READ)-1, 
			buffer, result);
	}

	return 0;
}

static int cdc_shell_ats_data_analytics_clear(struct device *dev, u8_t *buf, int len)
{
	SYS_LOG_INF("ok\n");
	
	//ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	data_analy_play_clear();
	data_analy_data_clear();
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_RET_ANALYTICS, sizeof(ATS_CMD_RESP_RET_ANALYTICS)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);
	return 0;
}
extern void console_input_init(struct device *dev);
static int cdc_shell_ats_poweron_playing_time_clear(struct device *dev, u8_t *buf, int len)
{
	poweron_playing_clear();
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_CLEAR_TIME, sizeof(ATS_CMD_RESP_CLEAR_TIME)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);
  	//ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
  return 0;
}

static int cdc_shell_ats_music_play_time_read(struct device *dev, u8_t *buf, int len)
{
	u32_t play_min = data_analy_get_product_history_play_min();
	uint8_t buffer[4] = {0};

	if (play_min > 9999)
	{
		SYS_LOG_WRN("play_min > 9999, only show 9999\n");
		play_min = 9999;
	}
	//snprintf(buffer, sizeof(buffer), "%d", play_min);
	//SYS_LOG_INF("str:%s\n", buffer);

	hex_to_string_4(play_min,buffer);
	ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_MUSIC_PLAY_TIME_READ, sizeof(ATS_CMD_RESP_MUSIC_PLAY_TIME_READ)-1, 
			buffer, sizeof(buffer));

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}

static int cdc_shell_ats_power_on_run_time_dump(struct device *dev, u8_t *buf, int len)
{
	u32_t pwr_on_min = data_analy_get_product_history_pwr_on_min();
	uint8_t buffer[4] = {0};

	if (pwr_on_min > 9999)
	{
		pwr_on_min = 9999;
	}
	//snprintf(buffer, sizeof(buffer), "%d", pwr_on_min);
	hex_to_string_4(pwr_on_min,buffer);
	ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_POWER_ON_RUN_TIME_DUMP, sizeof(ATS_CMD_RESP_POWER_ON_RUN_TIME_DUMP)-1, 
			buffer, sizeof(buffer));

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}
static int cdc_shell_ats_speaker_all_on(struct device *dev, u8_t *buf, int len)
{	
	uint8_t hw_info = ReadODM();

	if(hw_info == 2)
	{
#ifdef CONFIG_C_AMP_AW85828
	extern int aw85xxx_init(void);
	aw85xxx_init();
#endif
	}	
	else{
   #ifdef CONFIG_C_AMP_TAS5828M
   int amp_tas5828m_registers_init(void);
	  amp_tas5828m_registers_init();
   #endif	
	}	

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_APK_ALL_ON, sizeof(ATS_CMD_RESP_APK_ALL_ON)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_speaker_all_off(struct device *dev, u8_t *buf, int len)
{	
	uint8_t hw_info = ReadODM();

	if(hw_info == 2)
	{
#ifdef CONFIG_C_AMP_AW85828
	extern int aw85xxx_pa_stop(void);
	aw85xxx_pa_stop();
#endif
	}	
	else{
   #ifdef CONFIG_C_AMP_TAS5828M
	int amp_tas5828m_registers_deinit(void);
	  amp_tas5828m_registers_deinit();
   #endif	
	}	

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_APK_ALL_OFF, sizeof(ATS_CMD_RESP_APK_ALL_OFF)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_key_test_in(struct device *dev, u8_t *buf, int len)
{	
	ats_enter_key_check(true);
	ats_set_all_key_check(false);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_KEY_TEST_IN, sizeof(ATS_CMD_RESP_KEY_TEST_IN)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_key_test_exit(struct device *dev, u8_t *buf, int len)
{	
	ats_enter_key_check(false);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_KEY_TEST_OUT, sizeof(ATS_CMD_RESP_KEY_TEST_OUT)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_enter_auracast(struct device *dev, u8_t *buf, int len)
{	
	system_app_switch_auracast(true);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_ENTER_AURACAST, sizeof(ATS_CMD_RESP_ENTER_AURACAST)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_exit_auracast(struct device *dev, u8_t *buf, int len)
{	
	system_app_switch_auracast(false);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_EXIT_AURACAST, sizeof(ATS_CMD_RESP_EXIT_AURACAST)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_nosignal_test_mode(struct device *dev, u8_t *buf, int len)
{
	int ret1;
	u8_t buffer[1+1] = {6,0};

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_NOSIGTESTMODE_IN, sizeof(ATS_CMD_RESP_NOSIGTESTMODE_IN)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

    ret1 = property_set(CFG_USER_IN_OUT_NOSIGNAL_TEST_MODE, buffer, 1);
	if(ret1==0){
		property_flush(CFG_USER_IN_OUT_NOSIGNAL_TEST_MODE);
	}

	if (ret1==0){
		sys_pm_reboot(REBOOT_REASON_GOTO_BQB);
	}

	return 0;
}
static int cdc_shell_ats_user_data_write(struct device *dev, u8_t *buf, int len)
{	
    char *user_data = mem_malloc(100);
	if(!user_data){
		/* failed malloc */
		goto exit;
	}
	if(len>512){
		/* failed data langth */
		mem_free(user_data);
		goto exit;
	}
	memcpy(user_data, FAC_TEST_USER_DATA0, sizeof(FAC_TEST_USER_DATA0)-1);	
	nvram_config_set_factory(CFG_USER_INFO_0, user_data , sizeof(FAC_TEST_USER_DATA0)-1);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_USER_DATA_WRITE, sizeof(ATS_CMD_RESP_USER_DATA_WRITE)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);	
	mem_free(user_data);
    return 0;
 exit:
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_USER_DATA_WRITE, sizeof(ATS_CMD_RESP_USER_DATA_WRITE)-1, 
		ATS_CMD_RESP_FAIL, sizeof(ATS_CMD_RESP_FAIL)-1);	
    return 0;
}
static int cdc_shell_ats_user_data_read(struct device *dev, u8_t *buf, int len)	
{	
    char *user_data = mem_malloc(100);
	if(!user_data){
		/* failed malloc */
		goto exit;
	}
	if(len>100){
		/* failed data langth */
		mem_free(user_data);
		goto exit;
	}

	nvram_config_get_factory(CFG_USER_INFO_0, user_data , sizeof(ATS_CMD_RESP_USER_DATA_READ)-1);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_USER_DATA_READ, sizeof(ATS_CMD_RESP_USER_DATA_READ)-1, 
		user_data, sizeof(FAC_TEST_USER_DATA0)-1);	
	mem_free(user_data);
    return 0;
 exit:
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_USER_DATA_READ, sizeof(ATS_CMD_RESP_USER_DATA_READ)-1, 
		ATS_CMD_RESP_FAIL, sizeof(ATS_CMD_RESP_FAIL)-1);	
    return 0;
}
static int cdc_shell_ats_get_usb_water(struct device *dev, u8_t *buf, int len)
{	
	char buffer[2+1] = "00";

	extern bool key_water_status_read(void);
	
	if(key_water_status_read()){
		buffer[1] = '1';
	}
	else{
		buffer[1] = '0';
	}

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_WATER_STATUS_READ, sizeof(ATS_CMD_RESP_WATER_STATUS_READ)-1, 
		buffer, sizeof(buffer)-1);

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	return 0;
}
static int cdc_shell_ats_enter_module_test_mode(struct device *dev, u8_t *buf, int len)
{	
	int result;
	char buffer[1+1] = {6,0};
	result = ats_module_test_mode_write(buffer, sizeof(buffer-1));

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_ENTER_ATS_MODULE, sizeof(ATS_CMD_RESP_ENTER_ATS_MODULE)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_exit_module_test_mode(struct device *dev, u8_t *buf, int len)
{	
	int result;
	char buffer[1+1] = {8,0};
	result = ats_module_test_mode_write(buffer, sizeof(buffer-1));

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_ENTER_ATS_MODULE, sizeof(ATS_CMD_RESP_ENTER_ATS_MODULE)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_haman_battery_key_check(struct device *dev, u8_t *buf, int len)
{	
	if(is_authenticated()){
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	}
	else{
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
	}

	return 0;
}

static int cdc_shell_ats_usb_ntc_read_status(struct device *dev, u8_t *buf, int len)
{	
	char buffer[2+1] = "00";
	if(logic_mcu_read_ntc_status()){
		/* voer temperature */
		buffer[1] = '1';
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_GET_USB_NTC_STATUS, sizeof(ATS_CMD_RESP_GET_USB_NTC_STATUS)-1, 
			buffer, sizeof(buffer)-1);
	}
	else{
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_GET_USB_NTC_STATUS, sizeof(ATS_CMD_RESP_GET_USB_NTC_STATUS)-1, 
			buffer, sizeof(buffer)-1);
	}
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	return 0;
}

static int cdc_shell_ats_select_one_speaker(struct device *dev, u8_t *buf, int len)
{	
	char buffer[13+1] = "0";
	if(!memcmp(buf, "01",2))
	{
		hm_ext_pa_select_left_speaker();
	}
	else if (!memcmp(buf, "02",2))
	{
		hm_ext_pa_select_right_speaker();
	}

	memcpy(buffer, ATS_CMD_RESP_TURN_ON_ONE_SPEAKER, sizeof(ATS_CMD_RESP_TURN_ON_ONE_SPEAKER)-1);
	memcpy(buffer+10,buf,2);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, buffer, sizeof(ATS_CMD_RESP_TURN_ON_ONE_SPEAKER)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}

static int cdc_shell_ats_bt_mac_write(struct device *dev, u8_t *buf, int len)
{
	int result;

	if(len!=12){
		/* limit length 12 */
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
		return 0;
	}
	ats_usb_cdc_acm_cmd_response_at_data(
		dev,ATS_CMD_RESP_MAC_SET,sizeof(ATS_CMD_RESP_MAC_SET)-1,
		buf,len);

	result = ats_mac_write(buf, len);
	if(result<0){
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
	}
	else{
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	}
	return 0;	
}

static int cdc_shell_ats_enter_standby(struct device *dev, u8_t *buf, int len)
{	
	char buffer[2] = {0};
	sys_standby_time_set(1,CONFIG_AUTO_POWEDOWN_TIME_SEC);
	//ats_usb_cdc_acm_cmd_response_at_data(
	//	dev, ATS_CMD_RESP_ENTER_STANDBY, sizeof(ATS_CMD_RESP_ENTER_STANDBY)-1, 
	//	ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);
	u8_t sta = 0;
	hex_to_string_2(sta, buffer);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_ENTER_STANDBY, sizeof(ATS_CMD_RESP_ENTER_STANDBY)-1, 
		buffer, sizeof(buffer));
	sys_wake_unlock(6);//unlock standby

	return 0;
}
static int cdc_shell_ats_exit_standby(struct device *dev, u8_t *buf, int len)
{	
	sys_standby_time_set(CONFIG_AUTO_STANDBY_TIME_SEC,CONFIG_AUTO_POWEDOWN_TIME_SEC);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_EXIT_STANDBY, sizeof(ATS_CMD_RESP_EXIT_STANDBY)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);			

	sys_wake_lock(6);//lock standby again

	return 0;
}
static int cdc_shell_ats_get_chip_id(struct device *dev, u8_t *buf, int len)
{	
	uint32_t uuid[4];
	uint8_t uuid_str[UUID_STR_DATA_LEN + 1];
    soc_get_system_uuid(uuid);
	bin2hex(uuid_str, uuid, 16);
	uuid_str[32] =  0;
	
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_GET_CHIP_ID, sizeof(ATS_CMD_RESP_GET_CHIP_ID)-1, 
		uuid_str, sizeof(uuid_str)-1);	

	return 0;
}
static int cdc_shell_ats_get_logic_ver(struct device *dev, u8_t *buf, int len)
{	
	int ver = mcu_ui_get_logic_version();
	char buffer[2] = {0};

	if(ver>=0){
		hex_to_string_2((u8_t)ver, buffer);
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_GET_LOGIC_VER, sizeof(ATS_CMD_RESP_GET_LOGIC_VER)-1, 
			buffer, sizeof(buffer));		
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	}
	else{
		ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
	}

	return 0;
}
static int cdc_shell_ats_get_all_ver(struct device *dev, u8_t *buf, int len)
{	
	uint8_t buffer[22+1] = "BT:XXXX-PD:XX-Logic:XX";
	
	u8_t logic_ver = (u8_t)mcu_ui_get_logic_version();
	u8_t pd_ver = pd_get_pd_version();
	
	uint32_t swver = fw_version_get_sw_code(); //for example 0x010700-1.7.0
	uint8_t hwver = fw_version_get_hw_code();
	uint32_t swver_hex;
	swver_hex = (((swver>>0)&0xf)*10) + (((swver>>8)&0xf)*100) + (((swver>>16)&0xf)*1000);
	hex_to_string_4(swver_hex,buffer+3);
	buffer[6] = (hwver%10)+'0';//sw + hw

	bin2hex(buffer+11, &pd_ver, 1);
	hex_to_string_2(logic_ver, buffer+20);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_GET_ALL_VER, sizeof(ATS_CMD_RESP_GET_ALL_VER)-1, 
		buffer, sizeof(buffer)-1);		
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);
	
	return 0;
}
int cdc_shell_ats_rst_reboot(struct device *dev, u8_t *buf, int len)
{	
	/* 1.factory info reset */
	struct app_msg msg = { 0 };
    msg.type = MSG_INPUT_EVENT;
    msg.cmd = MSG_FACTORY_DEFAULT;
	msg.value = 0x0606;//factor reset --> reboot
    send_async_msg(CONFIG_FRONT_APP_NAME, &msg);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_RST_REBOOT, sizeof(ATS_CMD_RESP_RST_REBOOT)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
int cdc_shell_ats_switch_demo_mode(struct device *dev, u8_t *buf, int len)
{	
	struct app_msg msg = { 0 };
    msg.type = MSG_INPUT_EVENT;
    msg.cmd = MSG_DEMO_SWITCH;
    send_async_msg("main", &msg);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_SWITCH_DEMO, sizeof(ATS_CMD_RESP_SWITCH_DEMO)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
int cdc_shell_ats_enter_demo_mode(struct device *dev, u8_t *buf, int len)
{	
	struct app_msg msg = { 0 };
    msg.type = MSG_INPUT_EVENT;
    msg.cmd = MSG_ATS_ENTER_DEMO;
    send_async_msg("main", &msg);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_ENTER_DEMO, sizeof(ATS_CMD_RESP_ENTER_DEMO)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
int cdc_shell_ats_exit_demo_mode(struct device *dev, u8_t *buf, int len)
{	
	struct app_msg msg = { 0 };
    msg.type = MSG_INPUT_EVENT;
    msg.cmd = MSG_ATS_EXIT_DEMO;
    send_async_msg("main", &msg);

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_EXIT_DEMO, sizeof(ATS_CMD_RESP_EXIT_DEMO)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
int cdc_shell_ats_get_battery_voltage_current(struct device *dev, u8_t *buf, int len)
{	
	uint8_t buffer[19+1] = "V:+XXXXmV-I:+XXXXmA";
	int16_t bat_voltage = wlt_get_battery_volt();
	int16_t bat_cur = wlt_get_battery_cur();

	if(bat_voltage>=0){
		buffer[2] = '+';
		hex_to_string_4(bat_voltage,buffer+3);
	}
	else{
		buffer[2] = '-';
		hex_to_string_4((-bat_voltage),buffer+3);
	}		
	if(bat_cur>=0){
		buffer[12] = '+';
		hex_to_string_4(bat_cur,buffer+13);
	}
	else{
		buffer[12] = '-';
		hex_to_string_4((-bat_cur),buffer+13);
	}

	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_BAT_VOLTAGE_CURRENT, sizeof(ATS_CMD_RESP_BAT_VOLTAGE_CURRENT)-1, 
		buffer, sizeof(buffer)-1);

	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	return 0;
}
int cdc_shell_ats_verify_uuid(struct device *dev, u8_t *buf, int len)
{	
	if(!user_uuid_verify()){
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_VERIFY_UUID, sizeof(ATS_CMD_RESP_VERIFY_UUID)-1, 
			ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);
	}
	else{
		ats_usb_cdc_acm_cmd_response_at_data(
			dev, ATS_CMD_RESP_VERIFY_UUID, sizeof(ATS_CMD_RESP_VERIFY_UUID)-1, 
			ATS_CMD_RESP_FAIL, sizeof(ATS_CMD_RESP_FAIL)-1);
	}
	return 0;
}
static int cdc_shell_ats_key_all_test_in(struct device *dev, u8_t *buf, int len)
{	
	ats_enter_key_check(true);
	ats_set_all_key_check(true);
	ats_set_all_key_pressed(false);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_KEY_ALL_TEST_IN, sizeof(ATS_CMD_RESP_KEY_ALL_TEST_IN)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_key_all_test_exit(struct device *dev, u8_t *buf, int len)
{	
	ats_enter_key_check(false);
	ats_set_pwr_key_pressed(false);
	ats_set_all_key_check(false);
	ats_set_all_key_pressed(false);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_KEY_ALL_TEST_OUT, sizeof(ATS_CMD_RESP_KEY_ALL_TEST_OUT)-1, 
		ATS_CMD_RESP_OK, sizeof(ATS_CMD_RESP_OK)-1);

	return 0;
}
static int cdc_shell_ats_get_bat_level_voltage(struct device *dev, u8_t *buf, int len)
{	
	uint8_t buffer[11+1] = "000%-XXXXmV";
	int battery_capacity = power_manager_get_battery_capacity();
	int16_t battery_voltage = wlt_get_battery_volt();

	if(battery_capacity == 100) {	
		buffer[0] = '1';
		buffer[1] = '0';
		buffer[2] = '0';
	}
	else{
		hex_to_string_2((uint8_t)battery_capacity,buffer+1);
	}
	
	hex_to_string_4(battery_voltage,buffer+5);
	
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_GET_BAT_LEV_VOL, sizeof(ATS_CMD_RESP_GET_BAT_LEV_VOL)-1, 
		buffer, sizeof(buffer)-1);	
	
 	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);	

	return 0;
}
static int cdc_shell_ats_get_auracast_state(struct device *dev, u8_t *buf, int len)
{	
	/* if only auracast is sink mode, can check state...*/
	uint8_t buffer[2+1] = "00";
	uint8_t auracast_mode = system_app_get_auracast_mode();
	uint8_t auracast_state = 0;
	if(auracast_mode == 2){
		/* in auracast sink mode... check if connected.. */
		if(user_get_auracast_sink_connected()){
			auracast_state = 2;
		}
		else{
			auracast_state = 3;
		}
	}
	else{
		auracast_state = auracast_mode;
	}
	hex_to_string_2(auracast_state,buffer);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_GET_AURACAST_STATE, sizeof(ATS_CMD_RESP_GET_AURACAST_STATE)-1, 
		buffer, sizeof(buffer)-1);
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	return 0;
}
static int cdc_shell_ats_get_all_key(struct device *dev, u8_t *buf, int len)
{	
	uint8_t buffer[2+1] = "00";
	uint8_t all_key_pressed = ats_get_all_key_pressed();
	hex_to_string_2(all_key_pressed,buffer);
	ats_usb_cdc_acm_cmd_response_at_data(
		dev, ATS_CMD_RESP_GET_KEY_ALL, sizeof(ATS_CMD_RESP_GET_KEY_ALL)-1, 
		buffer, sizeof(buffer)-1);
	ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 1);

	return 0;
}

int ats_usb_cdc_acm_shell_command_handler(struct device *dev, u8_t *buf, int size)
{
	int index = 0;
	int target_index;
	static u8_t init_flag;

	if (init_flag == 0)
	   {
		   init_flag = 1;
		   cdc_shell_ats_init();
	   }
	

	if (!memcmp(&buf[index], ATS_AT_CMD_EXIT_ATS_AND_RESET, sizeof(ATS_AT_CMD_EXIT_ATS_AND_RESET)-1))
	{
		cdc_shell_ats_exit_ats_and_reset(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_SYSTEM_REBOOT, sizeof(ATS_AT_CMD_SYSTEM_REBOOT)-1))
	{
		cdc_shell_ats_reboot(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_POWER_OFF, sizeof(ATS_AT_CMD_POWER_OFF)-1))
	{
		cdc_shell_ats_power_off(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_DISCONNECT_BT_DEV_AND_ENTER_BT_PAIR, sizeof(ATS_AT_CMD_DISCONNECT_BT_DEV_AND_ENTER_BT_PAIR)-1))
	{
		cdc_shell_ats_disconnect_bt_dev_and_enter_bt_pair(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_READ_BT_RSSI, sizeof(ATS_AT_CMD_READ_BT_RSSI)-1))
	{
		 cdc_shell_ats_bt_rssi(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_READ_BT_CONECTED, sizeof(ATS_AT_CMD_READ_BT_CONECTED)-1))
	{
		 cdc_shell_ats_bt_status(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_COLOR_WRITE, sizeof(ATS_AT_CMD_COLOR_WRITE)-1))
	{
       index += sizeof(ATS_AT_CMD_COLOR_WRITE)-1;
	   target_index = index;
		cdc_shell_ats_color_write(dev, &buf[target_index], 2);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_COLOR_READ, sizeof(ATS_AT_CMD_COLOR_READ)-1))
	{
		cdc_shell_ats_color_read(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_GFPS_MODEL_ID_WRITE, sizeof(ATS_AT_CMD_GFPS_MODEL_ID_WRITE)-1))
	{
        index += sizeof(ATS_AT_CMD_GFPS_MODEL_ID_WRITE)-1;
		target_index = index;
		cdc_shell_ats_gfps_model_id_write(dev, &buf[target_index], size-target_index-2);
	}
	else if(!memcmp(&buf[index], ATS_AT_CMD_GFPS_MODEL_ID_READ, sizeof(ATS_AT_CMD_GFPS_MODEL_ID_READ)-1))
	{
		index += sizeof(ATS_AT_CMD_GFPS_MODEL_ID_READ)-1;		
        target_index = index;
		cdc_shell_ats_gfps_model_id_read(dev, NULL, 0);
	}
	else if(!memcmp(&buf[index], ATS_AT_CMD_READ_BT_DEV_NAME, sizeof(ATS_AT_CMD_READ_BT_DEV_NAME)-1))
	{
		index += sizeof(ATS_AT_CMD_READ_BT_DEV_NAME)-1;
		target_index = index;
		//ats_usb_cdc_acm_write_data("AT+HMDevNAME1",sizeof("AT+HMDevNAME1")-1);		
		cdc_shell_ats_read_bt_dev_name(dev, NULL, 0);
	}
	else if(!memcmp(&buf[index], ATS_AT_CMD_DSN_WRITE, sizeof(ATS_AT_CMD_DSN_WRITE)-1))
	{
        index += sizeof(ATS_AT_CMD_DSN_WRITE)-1;
		target_index = index;
		cdc_shell_ats_dsn_write(dev, &buf[target_index], size-target_index-2);
	}
	else if(!memcmp(&buf[index], ATS_AT_CMD_DSN_READ, sizeof(ATS_AT_CMD_DSN_READ)-1))
	{
		index += sizeof(ATS_AT_CMD_DSN_READ)-1;		
        target_index = index;
		cdc_shell_ats_dsn_read(dev, NULL, 0);
	}
	else if(!memcmp(&buf[index], ATS_AT_CMD_PSN_WRITE, sizeof(ATS_AT_CMD_PSN_WRITE)-1))
	{
        index += sizeof(ATS_AT_CMD_PSN_WRITE)-1;
		target_index = index;
		cdc_shell_ats_psn_write(dev, &buf[target_index], size-target_index-2);
	}
	else if(!memcmp(&buf[index], ATS_AT_CMD_PSN_READ, sizeof(ATS_AT_CMD_PSN_READ)-1))
	{
		index += sizeof(ATS_AT_CMD_PSN_READ)-1;		
        target_index = index;
		cdc_shell_ats_psn_read(dev, NULL, 0);
	}
	else if(!memcmp(&buf[index], ATS_AT_CMD_READ_BT_DEV_MAC_ADDR, sizeof(ATS_AT_CMD_READ_BT_DEV_MAC_ADDR)-1))
	{
		index += sizeof(ATS_AT_CMD_READ_BT_DEV_MAC_ADDR)-1;
		 target_index = index;
		cdc_shell_ats_read_bt_dev_mac_addr(dev, NULL, 0);
	}
	else if(!memcmp(&buf[index], ATS_AT_CMD_LED_ALL_TURN_ON, sizeof(ATS_AT_CMD_LED_ALL_TURN_ON)-1))
	{
		index += sizeof(ATS_AT_CMD_LED_ALL_TURN_ON)-1;
		 target_index = index;
		cdc_shell_ats_all_turn_on(dev, NULL, 0);
	}
	else if(!memcmp(&buf[index], ATS_AT_CMD_LED_ONE_TURN_ON, sizeof(ATS_AT_CMD_LED_ONE_TURN_ON)-1))
	{
		index += sizeof(ATS_AT_CMD_LED_ONE_TURN_ON)-1;
		target_index = index;
		cdc_shell_ats_led_one_turn_on(dev, &buf[target_index], 2);
	}
	else if(!memcmp(&buf[index], ATS_AT_CMD_LED_ALL_TURN_OFF, sizeof(ATS_AT_CMD_LED_ALL_TURN_OFF)-1))
	{
		index += sizeof(ATS_AT_CMD_LED_ALL_TURN_OFF)-1;
		 target_index = index;
		cdc_shell_ats_all_turn_off(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_VERSION_INFO_DUMP, sizeof(ATS_AT_CMD_VERSION_INFO_DUMP)-1))
	{
		index += sizeof(ATS_AT_CMD_VERSION_INFO_DUMP)-1;
		target_index = index;
		cdc_shell_ats_sw_version_info_dump(dev, NULL, 0);
	}
	// must first check ATS_AT_CMD_BATTERY_AD_VALUE, and check ATS_AT_CMD_BATTERY_LEVEL.
	else if (!memcmp(&buf[index], ATS_AT_CMD_BATTERY_AD_VALUE, sizeof(ATS_AT_CMD_BATTERY_AD_VALUE)-1))
	{
		index += sizeof(ATS_AT_CMD_BATTERY_AD_VALUE)-1;
        target_index = index;
		cdc_shell_ats_battery_ad_level(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_BATTERY_CUR_VALUE, sizeof(ATS_AT_CMD_BATTERY_CUR_VALUE)-1))
	{
		index += sizeof(ATS_AT_CMD_BATTERY_CUR_VALUE)-1;
        target_index = index;
		cdc_shell_ats_battery_curr(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_BATTERY_LEVEL, sizeof(ATS_AT_CMD_BATTERY_LEVEL)-1))
	{
		index += sizeof(ATS_AT_CMD_BATTERY_LEVEL)-1;
        target_index = index;
		cdc_shell_ats_battery_level(dev, NULL, 0);
	}
	else if(!memcmp(&buf[index], ATS_AT_CMD_SET_CHAGER_CUR, sizeof(ATS_AT_CMD_SET_CHAGER_CUR)-1))
	{
		index += sizeof(ATS_AT_CMD_SET_CHAGER_CUR)-1;
		target_index = index;
		cdc_shell_ats_set_sink_charge_current(dev, &buf[target_index], 2);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_GET_PD_VERSION, sizeof(ATS_AT_CMD_GET_PD_VERSION)-1))
	{
		index += sizeof(ATS_AT_CMD_GET_PD_VERSION)-1;
		target_index = index;
		cdc_shell_ats_pd_version_info_dump(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_DISABLE_CHARGE, sizeof(ATS_AT_CMD_DISABLE_CHARGE)-1))
	{
		index += sizeof(ATS_AT_CMD_DISABLE_CHARGE)-1;
        target_index = index;
		cdc_shell_ats_disable_charger(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index], ATS_AT_CMD_ENTER_KEY_CHECK, sizeof(ATS_AT_CMD_ENTER_KEY_CHECK)-1))
	{ 
		cdc_shell_ats_enter_key_check(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_EXIT_KEY_CHECK, sizeof(ATS_AT_CMD_EXIT_KEY_CHECK)-1))
	{
		cdc_shell_ats_exit_key_check(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_NTC_TEMPERATURE_DUMP, sizeof(ATS_AT_CMD_NTC_TEMPERATURE_DUMP)-1))
	{
		cdc_shell_ats_ntc_temperature_dump(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_RF_TEST_MODE, sizeof(ATS_AT_CMD_RF_TEST_MODE)-1))
	{
		cdc_shell_ats_rf_test_mode(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_VOLUME_ADJUST, sizeof(ATS_AT_CMD_VOLUME_ADJUST)-1))
	{
	    index += sizeof(ATS_AT_CMD_VOLUME_ADJUST)-1;
		target_index = index;
		cdc_shell_ats_volume_adjust(dev, &buf[target_index], 2);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_ODM_DUMP, sizeof(ATS_AT_CMD_ODM_DUMP)-1))
	{
		cdc_shell_ats_odm_dump(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_EXIT_SPK_BYPASS, sizeof(ATS_AT_CMD_EXIT_SPK_BYPASS)-1))
	{
		/* must first check TL_SPK_BYPASS_OFF, and check TL_SPK_BYPASS after */
		cdc_shell_ats_music_exit_bypass(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index], ATS_AT_CMD_ENTER_SPK_BYPASS, sizeof(ATS_AT_CMD_ENTER_SPK_BYPASS)-1))
	{
		cdc_shell_ats_music_enter_bypass(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_MUSIC_BYPASS_STATUS_READ, sizeof(ATS_AT_CMD_MUSIC_BYPASS_STATUS_READ)-1))
	{

		cdc_shell_ats_music_bypass_status_read(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_SERIAL_NUMBER_WRITE, sizeof(ATS_AT_CMD_SERIAL_NUMBER_WRITE)-1))
	{
	    index += sizeof(ATS_AT_CMD_SERIAL_NUMBER_WRITE)-1;
		target_index = index;
		cdc_shell_ats_serial_number_write(dev, &buf[target_index], 16);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_SERIAL_NUMBER_READ, sizeof(ATS_AT_CMD_SERIAL_NUMBER_READ)-1))
	{
		cdc_shell_ats_serial_number_read(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_DATA_ANALYTICS_CLEAR, sizeof(ATS_AT_CMD_DATA_ANALYTICS_CLEAR)-1))
	{
		cdc_shell_ats_data_analytics_clear(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_POWER_ON_TIME_RST, sizeof(ATS_AT_CMD_POWER_ON_TIME_RST)-1))
	{
        cdc_shell_ats_poweron_playing_time_clear(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_MUSIC_PLAY_TIME_READ, sizeof(ATS_AT_CMD_MUSIC_PLAY_TIME_READ)-1))
	{
		cdc_shell_ats_music_play_time_read(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index], ATS_AT_CMD_POWER_ON_RUN_TIME_DUMP, sizeof(ATS_AT_CMD_POWER_ON_RUN_TIME_DUMP)-1))
	{
		cdc_shell_ats_power_on_run_time_dump(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_READ_CHIP_UUID, sizeof(ATS_AT_CMD_READ_CHIP_UUID)-1))
	{
		 cdc_shell_IC_UUID_dump(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_WRITE_HASH_UUID, sizeof(ATS_AT_CMD_WRITE_HASH_UUID)-1))
	{		   
	 //    memcpy(&ats_cmd_resp_buf[index], ATS_CMD_RESP_OK, sizeof(ATS_AT_CMD_WRITE_HASH_UUID)-1);
		 cdc_shell_write_hash_uuid(dev, buf, size);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_HW_VERSION_INFO_DUMP, sizeof(ATS_AT_CMD_HW_VERSION_INFO_DUMP)-1))
	{		   
		index += sizeof(ATS_AT_CMD_HW_VERSION_INFO_DUMP)-1;
		target_index = index;
		cdc_shell_ats_hw_version_info_dump(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_USER_WRITE, sizeof(ATS_AT_CMD_USER_WRITE)-1))
	{		   
		/* user data write */
		cdc_shell_ats_user_data_write(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_USER_READ, sizeof(ATS_AT_CMD_USER_READ)-1))
	{		   
		/* user date read */
		cdc_shell_ats_user_data_read(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_RF_WATER_STATUS_READ, sizeof(ATS_AT_CMD_RF_WATER_STATUS_READ)-1))
	{		   
		/* waterproof detect */
		cdc_shell_ats_get_usb_water(dev, NULL, 0);
	}	
/* haman battery start */
	else if (!memcmp(&buf[index],ATS_AT_CMD_BATTERY_CHK, sizeof(ATS_AT_CMD_BATTERY_CHK)-1))
	{		   
		/* check key of harman battery */
		cdc_shell_ats_haman_battery_key_check(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_BATTERY_SN_READ, sizeof(ATS_AT_CMD_BATTERY_SN_READ)-1))
	{		   
		/* read key1-sn of harman battery */
		cdc_shell_ats_haman_battery_key_check(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_BATTERY_RSN_READ, sizeof(ATS_AT_CMD_BATTERY_RSN_READ)-1))
	{		   
		/* read key2-rsn of harman battery */
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_BATTERY_SN_WRITE, sizeof(ATS_AT_CMD_BATTERY_SN_WRITE)-1))
	{		   
		/* read sn of harman battery */
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_BATTERY_BIND_SN_READ, sizeof(ATS_AT_CMD_BATTERY_BIND_SN_READ)-1))
	{		   
		/* read sn of harman battery */
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_DEV_BAT_RSN, sizeof(ATS_AT_CMD_DEV_BAT_RSN)-1))
	{		   
		/* delet */
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_DEV_BAT_SOH, sizeof(ATS_AT_CMD_DEV_BAT_SOH)-1))
	{		   
		/* read percent of battery health */
	}		
	else if (!memcmp(&buf[index],ATS_AT_CMD_DEV_BAT_CYCLE, sizeof(ATS_AT_CMD_DEV_BAT_CYCLE)-1))
	{		   
		/* read times of battery cycles */
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_DEV_BAT_CHECK_KEY, sizeof(ATS_AT_CMD_DEV_BAT_CHECK_KEY)-1))
	{		   
		/* authenticationkey of battery */
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_DevBatCheckSnRsn, sizeof(ATS_AT_CMD_DevBatCheckSnRsn)-1))
	{		   
		/* check sn and rsn */
	}									
/* haman battery stop */		
	else if (!memcmp(&buf[index],ATS_AT_CMD_USB_NTC_STATUS, sizeof(ATS_AT_CMD_USB_NTC_STATUS)-1))
	{		   
		/* read usb ntc status  */
		cdc_shell_ats_usb_ntc_read_status(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_TURN_ON_ALL_SPEAKER, sizeof(ATS_AT_CMD_TURN_ON_ALL_SPEAKER)-1))
	{		   
		/* speaker all on */
		cdc_shell_ats_speaker_all_on(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_TURN_OFF_ALL_SPEAKER, sizeof(ATS_AT_CMD_TURN_OFF_ALL_SPEAKER)-1))
	{		   
		/* speaker all off */
		cdc_shell_ats_speaker_all_off(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_KEY_TEST_IN, sizeof(ATS_AT_CMD_KEY_TEST_IN)-1))
	{		   
		/* key test in */
		cdc_shell_ats_key_test_in(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_KEY_TEST_OUT, sizeof(ATS_AT_CMD_KEY_TEST_OUT)-1))
	{		   
		/* key test exit */
		cdc_shell_ats_key_test_exit(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_ENTER_AURACAST, sizeof(ATS_AT_CMD_ENTER_AURACAST)-1))
	{		   
		/* enter auracast */
		cdc_shell_ats_enter_auracast(dev, NULL, 0);
	}		
	else if (!memcmp(&buf[index],ATS_AT_CMD_EXIT_AURACAST, sizeof(ATS_AT_CMD_EXIT_AURACAST)-1))
	{		   
		/* exit auracast */
		cdc_shell_ats_exit_auracast(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_NO_SIG_TEST_MODE_IN, sizeof(ATS_AT_CMD_NO_SIG_TEST_MODE_IN)-1))
	{		   
		/* enter nosignal teset mode */
		cdc_shell_ats_nosignal_test_mode(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_ENTER_ATS_MODULE, sizeof(ATS_AT_CMD_ENTER_ATS_MODULE)-1))
	{		   
		/* enter ats module test mode */
		cdc_shell_ats_enter_module_test_mode(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_EXIT_ATS_MODULE, sizeof(ATS_AT_CMD_EXIT_ATS_MODULE)-1))
	{		   
		/* exit ats module test mode */
		cdc_shell_ats_exit_module_test_mode(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_GET_PWR_TIME, sizeof(ATS_AT_CMD_GET_PWR_TIME)-1))
	{		   
		/* get poweron time */
		cdc_shell_ats_power_on_run_time_dump(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_GET_PALY_TIME, sizeof(ATS_AT_CMD_GET_PALY_TIME)-1))
	{		   
		/* get play time */
		cdc_shell_ats_music_play_time_read(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_ALL_WLED_ON, sizeof(ATS_AT_CMD_ALL_WLED_ON)-1))
	{		   
		/* light on all white led */
		cdc_shell_ats_all_wled_turn_on(dev, NULL, 0);
	}		
	else if (!memcmp(&buf[index],ATS_AT_CMD_TURN_ON_ONE_SPEAKER, sizeof(ATS_AT_CMD_TURN_ON_ONE_SPEAKER)-4))//at last is param
	{		   
		/* select one speaker */
		index += sizeof(ATS_AT_CMD_TURN_ON_ONE_SPEAKER)-1-2;
		target_index = index;
		cdc_shell_ats_select_one_speaker(dev, &buf[target_index], 2);	
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_GET_CHAGER_CUR, sizeof(ATS_AT_CMD_GET_CHAGER_CUR)-1))
	{		   
		/* get sink charge current */
		cdc_shell_ats_get_sink_charge_current(dev, NULL, 0);	
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_MAC_WRITE, sizeof(ATS_AT_CMD_MAC_WRITE)-1))
	{		   
		/* set bt mac */
        index += sizeof(ATS_AT_CMD_MAC_WRITE)-1;
		target_index = index;
		cdc_shell_ats_bt_mac_write(dev, &buf[target_index], size-target_index-2);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_ENTER_STANDBY, sizeof(ATS_AT_CMD_ENTER_STANDBY)-1))
	{		   
		/* enter standby */
		cdc_shell_ats_enter_standby(dev, NULL, 0);	
	}		
	else if (!memcmp(&buf[index],ATS_AT_CMD_EXIT_STANDBY, sizeof(ATS_AT_CMD_EXIT_STANDBY)-1))
	{		   
		/* exit standby */
		cdc_shell_ats_exit_standby(dev, NULL, 0);	
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_GET_CHIP_ID, sizeof(ATS_AT_CMD_GET_CHIP_ID)-1))
	{		   
		/* get chip id */
		cdc_shell_ats_get_chip_id(dev, NULL, 0);	
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_GET_LOGIC_VER, sizeof(ATS_AT_CMD_GET_LOGIC_VER)-1))
	{		   
		/* get logic version */
		cdc_shell_ats_get_logic_ver(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_GET_ALL_VER, sizeof(ATS_AT_CMD_GET_ALL_VER)-1))
	{		   
		/* get all ver */
		cdc_shell_ats_get_all_ver(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_RST_REBOOT, sizeof(ATS_AT_CMD_RST_REBOOT)-1))
	{		   
		/* reset and reboot */
		cdc_shell_ats_rst_reboot(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_SWITCH_DEMO, sizeof(ATS_AT_CMD_SWITCH_DEMO)-1))
	{		   
		/* switch demo mode */
		cdc_shell_ats_switch_demo_mode(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_ENTER_DEMO, sizeof(ATS_AT_CMD_ENTER_DEMO)-1))
	{		   
		/* enter demo mode */
		cdc_shell_ats_enter_demo_mode(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_EXIT_DEMO, sizeof(ATS_AT_CMD_EXIT_DEMO)-1))
	{		   
		/* exit demo mode */
		cdc_shell_ats_exit_demo_mode(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_BAT_VOLTAGE_CURRENT, sizeof(ATS_AT_CMD_BAT_VOLTAGE_CURRENT)-1))
	{		   
		/* get battery boltage and current */
		cdc_shell_ats_get_battery_voltage_current(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_VERIFY_UUID, sizeof(ATS_AT_CMD_VERIFY_UUID)-1))
	{		   
		/* verify uuid */
		cdc_shell_ats_verify_uuid(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_KEY_ALL_TEST_IN, sizeof(ATS_AT_CMD_KEY_ALL_TEST_IN)-1))
	{		   
		/* key all test in */
		cdc_shell_ats_key_all_test_in(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_KEY_ALL_TEST_OUT, sizeof(ATS_AT_CMD_KEY_ALL_TEST_OUT)-1))
	{		   
		/* key all test out */
		cdc_shell_ats_key_all_test_exit(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_GET_KEY_ALL, sizeof(ATS_AT_CMD_GET_KEY_ALL)-1))
	{		   
		/* get all key */
		cdc_shell_ats_get_all_key(dev, NULL, 0);
	}	
	else if (!memcmp(&buf[index],ATS_AT_CMD_GET_BAT_LEV_VOL, sizeof(ATS_AT_CMD_GET_BAT_LEV_VOL)-1))
	{		   
		/* get battery percent level and voltage */
		cdc_shell_ats_get_bat_level_voltage(dev, NULL, 0);
	}
	else if (!memcmp(&buf[index],ATS_AT_CMD_GET_AURACAST_STATE, sizeof(ATS_AT_CMD_GET_AURACAST_STATE)-1))
	{		   
		/* get auracast connect state */
		cdc_shell_ats_get_auracast_state(dev, NULL, 0);
	}	
	else	
	{
		//ats_usb_cdc_acm_write_data("live ats_usb_cdc_acm_shell_command_handler exit",sizeof("live ats_usb_cdc_acm_shell_command_handler exit")-1);
	    ats_usb_cdc_acm_cmd_response_ok_or_fail(dev, 0);
		goto __exit_exit;
	}
	return 0;

__exit_exit:
    return -1;
}


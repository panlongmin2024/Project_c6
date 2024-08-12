#include "selfapp_internal.h"
#include "selfapp_config.h"

#include <bt_manager.h>
#include <mem_manager.h>
#include <sys_event.h>
#include <power_manager.h>
#include <hex_str.h>
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif
#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif
#include <app_defines.h>
#include <desktop_manager.h>
#include <app_launch.h>
#include <media_player.h>
#include <bt_manager.h>
#include <broadcast.h>
#include <app_ui.h>

static bool bat_user_set = false;
static u8_t bat_percents = 100;
static u8_t model_id = BOX_DEFAULT_MODEL_ID;
#define MUTE_PLAYER_TIMEOUT	120

static u8_t selfapp_get_bat_cap(void)
{
	if (bat_user_set) {
		return bat_percents ;
	} else {
		return power_manager_get_battery_capacity();
	}
}

void selfapp_set_user_bat_cap(u8_t bat)
{
	selfapp_log_inf("bat: %d\n", bat);
	bat_user_set = true;
	bat_percents = bat;
}

u8_t selfapp_get_bat_power(void)
{
	u8_t value = 0;
	u8_t charging = 0;
	u8_t cap;
	int ret;

	ret = power_manager_get_charge_status();
	charging = (ret == POWER_SUPPLY_STATUS_CHARGING) ? 1 : 0;
	cap = selfapp_get_bat_cap();

	//fix illegal value.
	if(cap > 100) {
		cap = 100;
	}

	// fix App cannot connect when Bat empty(0x0)
	if(cap == 0) {
		cap = 1;
	}

	value = (charging << 7) | (cap & 0x7F);

	return value;
}

/*
	Bit 2-4 = Battery (
	BATT_CRITICAL: 0,
	BATT_LOW: 001,
	BATT_LEVEL0: 010,
	BATT_LEVEL1: 011,
	BATT_LEVEL2: 100,
	BATT_LEVEL3: 101,
	BATT_LEVEL4: 110
	)
*/
u8_t selfapp_get_bat_level(void)
{
	u8_t cap;
	u8_t level;

	cap = selfapp_get_bat_cap();
	//fix illegal value.
	if(cap > 100) {
		cap = 100;
	}

	level = cap/15;
	if(level > 6) {
		level = 6;
	}

	return level;
}

#ifdef SPEC_REMOTE_CONTROL
int selfapp_set_indication(u8_t dev_idx)
{
	int ret = -1;
	selfapp_context_t *selfctx = self_get_context();
	if (NULL == selfctx) {
		return ret;
	}

	if (dev_idx == 0) {
		if(!selfapp_get_lasting_stereo_mode()){
			selfapp_mute_player(1);
			if (thread_timer_is_running(&selfctx->mute_timer)){
				thread_timer_stop(&selfctx->mute_timer);
			}
			thread_timer_init(&selfctx->mute_timer, self_mute_handler,NULL);
			thread_timer_start(&selfctx->mute_timer, MUTE_PLAYER_TIMEOUT * 1000, 0);
		}
		sys_event_notify_single(SYS_EVENT_STEREO_GROUP_INDICATION);
		ret = 0;
	} else if (dev_idx == 1) {
#ifdef CONFIG_BT_LETWS
		broadcast_tws_vnd_send_indication();
		ret = 0;
#else
		selfapp_log_wrn("no twsdev\n");
#endif
	}

	return ret;
}
#endif

u8_t selfapp_get_role(void)
{
	u8_t mode, role;

	mode = system_app_get_auracast_mode();

	if (mode == 1) {
		role = DevRole_Broadcaster;
	} else if (mode == 2) {
		role = DevRole_Receiver;
	} else {
		role = DevRole_Normal;
	}

	return role;
}

u8_t selfapp_get_lasting_stereo_mode(void)
{
	u8_t mode, role;

	mode = system_app_get_auracast_mode();

	if (mode == 3 || mode == 4) {
		role = 1;
	} else {
		role = 0;
	}

	return role;
}

u8_t selfapp_lasting_stereo_is_primary_role(void)
{
	u8_t mode = system_app_get_auracast_mode();
	return (mode == 3);
}

u8_t selfapp_get_lasting_stereo_role(void)
{
	struct AURACAST_GROUP group;

	selfapp_config_get_ac_group(&group);

	return group.role;
}

/*
return: output channel mode
	0 - Stereo
	1 - channel left
	2 - channel right
*/
u8_t selfapp_get_channel(void)
{
	struct AURACAST_GROUP group;

	selfapp_config_get_ac_group(&group);

	return group.ch;
}

void selfapp_set_link_mode(u8_t mode)
{
	u8_t auracast;

	//{0-off, 1-partyboost, 2-Auracast}
	selfapp_log_inf("mode %d\n", mode);

	auracast = system_app_get_auracast_mode();
	if (mode == 2) {
		system_app_switch_auracast(1);
	} else if (mode == 0) {
		system_app_switch_auracast(0);
	} else {
		selfapp_log_wrn("not support mode %d", mode);
	}
}

/*
0 - no audio playing
1 - bluetooth A2DP
2 - Aux-in
3 - Receiver playing
*/
u8_t selfapp_get_audio_source(void)
{
	u8_t mode;
	u8_t source;

	if (!media_player_is_working()) {
		source = 0;
	} else {
		mode = system_app_get_auracast_mode();
		if (mode == 2) {
			source = 3;
		} else {
			source = 1;
		}
	}

	return source;
}

void selfapp_get_mac(u8_t* mac)
{
	u8_t i;
	bd_address_t bd_addr;

	btif_br_get_local_mac(&bd_addr);
	for (i = 0; i < 6; i++) {
		mac[i] = bd_addr.val[5-i];
	}

#if 0
	/* mac and it's CRC to test crc
	  MIVICRO mac 0 and crc
	  aa12080048 1028745ad76a
	  aa1204004a 86aa

	  MIVICRO mac 1 and crc
	  aa12080048 f85c7e291b7b
	  aa1204004a 4686
	*/
#if 1
	mac[0] = 0x10;
	mac[1] = 0x28;
	mac[2] = 0x74;
	mac[3] = 0x5a;
	mac[4] = 0xd7;
	mac[5] = 0x6a;
#else
	mac[0] = 0xf8;
	mac[1] = 0x5c;
	mac[2] = 0x7e;
	mac[3] = 0x29;
	mac[4] = 0x1b;
	mac[5] = 0x7b;
#endif
#endif

	return;
}

void selfapp_get_group_name(u8_t* name,int len)
{
	if(len < 16){
		selfapp_log_wrn("len %d < 16", len);
		return;
	}

	struct AURACAST_GROUP group;

	selfapp_config_get_ac_group(&group);
	//max len is 16
	memcpy(name,group.name,16);
	//memcpy(name,"MY group",16);
	return;
}

//callee supplied by selfapp
//-----------------------------------------------------------------------------
/*
return first byte of stereo group ID.
*/
u32_t selfapp_get_group_id(void)
{
	selfapp_context_t *selfctx = self_get_context();
	struct AURACAST_GROUP group;

	if (NULL == selfctx) {
		return 0;
	}

	selfapp_config_get_ac_group(&group);

	return group.id;
}

static u16_t selfapp_get_src_dev_name_crc(void)
{
	uint16_t crc;
	char *name = NULL;

	name = bt_manager_audio_get_last_connected_dev_name();
	if (NULL != name) {
		crc = self_crc16(0, name, strlen(name));
		//selfapp_log_dbg("name: %s, 0x%x\n", name, crc);
		return crc;
	}

	return 0;
}

static u16_t selfapp_get_addr_crc(void)
{
#define MAC_STR_LEN (12+1)
	int ret;
	uint16_t crc;
	uint8_t mac[6];
	uint8_t mac_str[MAC_STR_LEN];

	memset(mac_str, 0, MAC_STR_LEN);

#ifdef CONFIG_PROPERTY
	ret = property_get(CFG_BT_MAC, mac_str, (MAC_STR_LEN - 1));
#endif
	if (ret < (MAC_STR_LEN - 1))
		return 0;

	str_to_hex(mac, mac_str, 12);
	crc = self_crc16(0, mac, 6);

	return crc;
}

void selfapp_set_model_id(u8_t mid)
{
	model_id = mid;
	bt_sdp_modify_gatt_att_color_id(model_id);
}

u8_t selfapp_get_model_id(void)
{
	return model_id;
}

u8_t selfapp_get_manufacturer_data(u8_t * buf)
{
	u8_t i = 0;
	u16_t crc_name, crc_addr;
	u8_t role, ch, bat;

	if (NULL == buf) {
		return 0;
	}

	crc_name = selfapp_get_src_dev_name_crc();
	crc_addr = selfapp_get_addr_crc();

	//VID
	buf[i++] = (CONFIG_BT_COMPANY_ID & 0xFF);
	buf[i++] = (CONFIG_BT_COMPANY_ID >> 8);

	//PID
	buf[i++] = (BOX_PRODUCT_ID & 0xff);
	buf[i++] = (BOX_PRODUCT_ID >> 8);

	//MID
	buf[i++] = selfapp_get_model_id();

	/*
	   Role and connectable bits
	   bit 7 connectable status; (0-connectable, 1-Non-connectable)
	   bit 6-5 Sereo channel mode (00-old firmware, 01-party, 10-ch left, 11-ch right)
	   bit 4-2 Battery (000~110)
	   bit 1-0 Role (00-normal, 01-primary, 10-receive)
	 */
	role = selfapp_get_role() & 0x03;
	bat = (selfapp_get_bat_level()) & 0x07;
#ifdef CONFIG_BT_LETWS
	if(selfapp_get_lasting_stereo_mode()){
		ch = (selfapp_get_channel() + 1) & 0x03;
	}else{
		ch = 1;
	}
#else
	ch = (selfapp_get_channel() + 1) & 0x03;
#endif

	buf[i++] = ch << 5 | bat << 2 | role;

	//Note: VIMICRO uses little endian to transfer CRC16, and we follows it.

	//CRC of source device name
	buf[i++] = (crc_name >> 0) & 0xff;
	buf[i++] = (crc_name >> 8) & 0xff;

	/*CRC of BT device address */
	buf[i++] = (crc_addr >> 0) & 0xff;
	buf[i++] = (crc_addr >> 8) & 0xff;

	/*
	   Extra info of Auracast, LE mode, and spotify button actions
	   bit 7-4 Reserved
	   bit 3 Long Lasting Stereo Support Status
	   bit 2 Sporify quick access button is triggerd
	   bit 1 Auracast party status (0-off, 1-on)
	   bit 0 Suport Auracast (0-no, 1-yes)
	 */
#ifdef CONFIG_BT_LETWS
	buf[i++] = (((role == 0) ? 0 : 1) << 1) | 0x01 | 0x08;
#else
	buf[i++] = (((role == 0) ? 0 : 1) << 1) | 0x01;
#endif

	/*Auracast Stereo group ID (Reserved) */
	buf[i++] = selfapp_get_group_id() >> 24;

	//selfapp_log_dump(buf, i);

	return i;
}

void selfapp_switch_lasting_stereo_mode(int enable)
{
	selfapp_context_t *selfctx = self_get_context();
	struct AURACAST_GROUP group;
	bt_addr_le_t addr = {0};

	if (NULL == selfctx) {
		return;
	}

	selfapp_config_get_ac_group(&group);

	if(enable){
		if(selfctx->creat_group.role == 0x1){
			memcpy(addr.a.val,selfctx->creat_group.addr,6);
#ifdef CONFIG_BT_LETWS
			bt_mamager_set_remote_ble_addr(&addr);
			bt_manager_letws_start_pair_search(BTSRV_TWS_MASTER,selfctx->time_out);
#endif
		}else if(selfctx->creat_group.role == 0x2){
			memcpy(addr.a.val,selfctx->creat_group.addr,6);
#ifdef CONFIG_BT_LETWS
			bt_mamager_set_remote_ble_addr(&addr);
			bt_manager_letws_start_pair_search(BTSRV_TWS_SLAVE,selfctx->time_out);
#endif
		}
	}else{
#ifdef CONFIG_BT_LETWS
		bt_manager_letws_reset();
#endif
	}
}

void selfapp_set_lasting_stereo_group_info(struct AURACAST_GROUP *group){
	selfapp_context_t *selfctx = self_get_context();
	struct AURACAST_GROUP last_group;

	if (NULL == selfctx || NULL == group) {
		return;
	}
	selfapp_config_get_ac_group(&last_group);
	if(group->id){
		last_group.ch = group->ch;
		memcpy(last_group.name,group->name,strlen(group->name));
		selfapp_config_set_ac_group(&last_group);
	}else{
		selfapp_config_set_ac_group(group);
		bt_manager_letws_reset();
		selfapp_log_inf("clear group");
	}
}

void selfapp_set_lasting_stereo_group_info_to_slave(struct AURACAST_GROUP *group){
	selfapp_context_t *selfctx = self_get_context();

	if (NULL == selfctx || NULL == group) {
		return;
	}

	struct lasting_stereo_device_info info;
	memset(&info,0,sizeof(info));
	info.ch = group->ch;
	info.id = group->id;
	memcpy(info.name,group->name,strlen(group->name));

	broadcast_tws_vnd_set_dev_info(&info);
}

void selfapp_mute_player(u8_t mute)
{
	selfapp_log_inf("mute %d\n", mute);
	selfapp_context_t *selfctx = self_get_context();

	if (NULL == selfctx) {
		return;
	}
	selfctx->mute_player = mute;
	struct app_msg msg = { 0 };

	msg.type = MSG_INPUT_EVENT;
	if(mute)
		msg.cmd = MSG_MUTE_PLAYER;
	else
		msg.cmd = MSG_UNMUTE_PLAYER;
	msg.value = 0;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

void selfapp_reset_player()
{
	selfapp_log_inf("\n");

	struct app_msg msg = { 0 };

	msg.type = MSG_INPUT_EVENT;
	msg.cmd = MSG_PLAYER_RESET;
	msg.value = 0;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

void selfapp_set_device_info(selfapp_device_info_t *info){
	selfapp_context_t *selfctx = self_get_context();

	if (NULL == selfctx) {
		return;
	}
	selfapp_log_inf("%s\n",info->serial_num);

	if(info){
		memcpy(&selfctx->secondary_device,info,sizeof(*info));
		selfctx->secondary_device.validate = 1;
		selfapp_report_secondary_device_info();
	}else{
		memset(&selfctx->secondary_device,0,sizeof(selfctx->secondary_device));
	}
}


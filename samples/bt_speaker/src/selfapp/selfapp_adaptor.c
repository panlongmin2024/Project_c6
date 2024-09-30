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

static bool allowed_adv_crc = true;
static bool allowed_join_party = true;
static u8_t system_status = SELF_SYS_POWERON;

static uint16_t addr_crc = 0;
static uint16_t name_crc = 0;

void self_indication_handler(struct thread_timer *ttimer,
				   void *expiry_fn_arg);

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
	selfapp_log_inf("selfapp_get_bat_power %d\n",cap);
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
	BATT_CRITICAL: 0,  -> 0%
	BATT_LOW: 001,	   -> 10%
	BATT_LEVEL0: 010,  -> 30%
	BATT_LEVEL1: 011,  -> 50%
	BATT_LEVEL2: 100,  -> 70%
	BATT_LEVEL3: 101,  -> 100%
	)
*/
u8_t selfapp_get_bat_level(void)
{
	u8_t cap;
	u8_t level;

	cap = selfapp_get_bat_cap();
	selfapp_log_inf("selfapp_get_bat_level %d\n",cap);
	//fix illegal value.
	if(cap > 100) {
		cap = 100;
	}
	
	if (cap > 75) {
		level = 6; //100%
	}else if (cap > 60) {
		level = 5; //80%
	} else if (cap > 45) {
		level = 4; //60%
	} else if (cap > 30) {
		level = 3; //40%
	} else if (cap > 15) {
		level = 2; //20%
	} else if (cap > 5) {
		level = 1; //10%
	} else {
		level = 0;
	}

	return level;
}

void selfapp_cmd_thread_timer_start(struct thread_timer *cur_timer)
{
	selfapp_send_msg(MSG_SELFAPP_APP_EVENT, SELFAPP_CMD_THREAD_TIMER_START, 0, (uint32_t)cur_timer);
}

void selfapp_thread_timer_start(int value)
{
	struct thread_timer *cur_timer = (struct thread_timer *)value;
	selfapp_context_t *selfctx = self_get_context();

	if(cur_timer == &selfctx->indication_timer){
#ifdef SPEC_REMOTE_CONTROL
		if (thread_timer_is_running(&selfctx->indication_timer)){
			thread_timer_stop(&selfctx->indication_timer);
		}

		thread_timer_init(&selfctx->indication_timer, self_indication_handler,NULL);
		thread_timer_start(&selfctx->indication_timer, 200, 200);
		selfctx->indication_time_out = 0;
#endif
	}else if(cur_timer == &selfctx->creat_group_timer){
		if (thread_timer_is_running(&selfctx->creat_group_timer)){
			thread_timer_stop(&selfctx->creat_group_timer);
		}

		thread_timer_init(&selfctx->creat_group_timer, self_creat_group_handler,NULL);
		thread_timer_start(&selfctx->creat_group_timer, (selfctx->creat_group_time_out + 1) * 1000, 0);
	}
}

#ifdef SPEC_REMOTE_CONTROL
void self_indication_handler(struct thread_timer *ttimer,
				   void *expiry_fn_arg){
	selfapp_context_t *selfctx = self_get_context();
	if (NULL == selfctx) {
		return;
	}
	selfctx->indication_time_out++;
	if(selfctx->indication_time_out > 10){
		selfapp_log_inf("time out");
		thread_timer_stop(&selfctx->indication_timer);
		selfctx->indication_time_out = 0;
	}else if(BT_STATUS_PAUSED == bt_manager_media_get_status()){
		sys_event_notify_single(SYS_EVENT_STEREO_GROUP_INDICATION_NO_FILTER);
		thread_timer_stop(&selfctx->indication_timer);
	}
}


int selfapp_set_indication(u8_t dev_idx)
{
	int ret = -1;
	selfapp_context_t *selfctx = self_get_context();
	if (NULL == selfctx) {
		return ret;
	}

	if (dev_idx == 0) {
		if(!selfapp_get_lasting_stereo_mode() &&
			!thread_timer_is_running(&selfctx->creat_group_timer) &&
			BT_STATUS_PLAYING == bt_manager_media_get_status()){
			selfapp_pause_player();
		}
		if(selfapp_get_lasting_stereo_mode()){
			sys_event_notify_single(SYS_EVENT_STEREO_GROUP_INDICATION);
		}else if(selfctx->pause_player){
			selfapp_cmd_thread_timer_start(&selfctx->indication_timer);
		}else{
			sys_event_notify_single(SYS_EVENT_STEREO_GROUP_INDICATION);
		}
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

static int selfapp_get_src_dev_name_crc(u16_t *p_name_crc)
{
	uint16_t crc;
	char *name = NULL;

	if (!p_name_crc) {
		SYS_LOG_ERR("param invaild");
		return -1;
	}

	name = bt_manager_audio_get_last_connected_dev_name();
	if (NULL != name) {
		crc = self_crc16(0, name, strlen(name));
		//selfapp_log_dbg("name: %s, 0x%x\n", name, crc);
		*p_name_crc = crc;
		return 0;
	}

	*p_name_crc = 0;
	return -1;
}

static int selfapp_get_addr_crc(u16_t *p_addr_crc)
{
#define MAC_STR_LEN (12+1)
	int ret;
	uint16_t crc;
	uint8_t mac[MAC_STR_LEN];  // only 6bytes used
	uint8_t mac_str[MAC_STR_LEN];

	if (!p_addr_crc) {
		SYS_LOG_ERR("param invaild");
		return -1;
	}

	memset(mac_str, 0, MAC_STR_LEN);

#ifdef CONFIG_PROPERTY
	ret = property_get(CFG_BT_MAC, mac_str, (MAC_STR_LEN - 1));
#endif
	if (ret < (MAC_STR_LEN - 1)) {
		*p_addr_crc = 0;
		return -1;
	}

	str_to_hex(mac, mac_str, 12);
	crc = self_crc16(0, mac, 6);
	*p_addr_crc = crc;
	return 0;
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

void selfapp_set_allowed_adv_crc(bool allowed)
{
	os_sched_lock();
	if (allowed_adv_crc != allowed) {
		allowed_adv_crc = allowed;
		SYS_LOG_INF("%d", allowed_adv_crc);
	}
	os_sched_unlock();
}

bool selfapp_get_allowed_adv_crc(void)
{
	return allowed_adv_crc;
}

void selfapp_set_allowed_join_party(bool allowed)
{
	os_sched_lock();
	if (allowed_join_party != allowed) {
		allowed_join_party = allowed;
		SYS_LOG_INF("%d", allowed_join_party);
	}
	os_sched_unlock();
}

bool selfapp_get_allowed_join_party(void)
{
	return allowed_join_party;
}

void selfapp_set_system_status(u8_t status)
{
	os_sched_lock();
	if (system_status != status) {
		system_status = status;
		SYS_LOG_INF("%d", system_status);
	}
	os_sched_unlock();
}

u8_t selfapp_get_system_status(void)
{
	return system_status;
}

u8_t selfapp_get_manufacturer_data(u8_t * buf)
{
	u8_t i = 0;
	u16_t crc_name, crc_addr;
	u8_t role, ch, bat, join_party;

	if (NULL == buf) {
		return 0;
	}

	crc_name = crc_addr = 0;
	if (selfapp_get_allowed_adv_crc() != false) {
		selfapp_get_src_dev_name_crc(&crc_name);
	}
	selfapp_get_addr_crc(&crc_addr);

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

    if(name_crc != crc_name){
        SYS_LOG_INF("name crc %x", crc_name);
        name_crc = crc_name;
    }

    if(addr_crc != crc_addr){
        SYS_LOG_INF("addr crc %x", crc_addr);
        addr_crc = crc_addr;
    }

	/*
	   Extra info of Auracast, LE mode, and spotify button actions
	   bit 7-5 Reserved
	   bit 4 allowed to join Auracast Party
	   bit 3 Long Lasting Stereo Support Status
	   bit 2 Reserved
	   bit 1 Auracast party status (0-off, 1-on)
	   bit 0 Suport Auracast (0-no, 1-yes)
	 */
	join_party = (selfapp_get_allowed_join_party() != false)? 0 : 1;
#ifdef CONFIG_BT_LETWS
	buf[i++] = (((role == 0) ? 0 : 1) << 1) | 0x01 | 0x08 | (join_party << 4);
#else
	buf[i++] = (((role == 0) ? 0 : 1) << 1) | 0x01 | (join_party << 4);
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
			bt_manager_letws_start_pair_search(BTSRV_TWS_MASTER,selfctx->creat_group_time_out);
#endif
		}else if(selfctx->creat_group.role == 0x2){
			memcpy(addr.a.val,selfctx->creat_group.addr,6);
#ifdef CONFIG_BT_LETWS
			bt_mamager_set_remote_ble_addr(&addr);
			bt_manager_letws_start_pair_search(BTSRV_TWS_SLAVE,selfctx->creat_group_time_out);
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

void selfapp_pause_player()
{
	selfapp_context_t *selfctx = self_get_context();

	if (NULL == selfctx) {
		return;
	}
	selfctx->pause_player = 1;
	struct app_msg msg = { 0 };

	msg.type = MSG_INPUT_EVENT;
	msg.cmd = MSG_PAUSE_PLAYER;
	msg.value = 0;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

void selfapp_resume_player()
{
	selfapp_context_t *selfctx = self_get_context();

	if (NULL == selfctx || !selfctx->pause_player) {
		return;
	}
	selfctx->pause_player = 0;
	struct app_msg msg = { 0 };

	msg.type = MSG_INPUT_EVENT;
	msg.cmd = MSG_RESUME_PLAYER;
	msg.value = 0;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

static int selfapp_msg_match(void *msg,k_tid_t target_thread,k_tid_t source_thread){
	struct app_msg *p_app_msg = (struct app_msg *)msg;
	if(!p_app_msg || p_app_msg->cmd != MSG_PLAYER_RESET || p_app_msg->type != MSG_INPUT_EVENT
		|| source_thread != os_current_get()){
		return 0;
	}
	return 1;
}

void selfapp_reset_player()
{
	int ret = 0;
	struct app_msg msg = { 0 };

	msg.type = MSG_INPUT_EVENT;
	msg.cmd = MSG_PLAYER_RESET;
	msg.value = 0;
	if(!os_is_free_msg_enough()){
		ret = os_msg_delete(selfapp_msg_match);
	}
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
	selfapp_log_inf("%d\n",ret);
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

u8_t selfapp_get_leaudio_status(void)
{
	u8_t status;

	status = !!bt_manager_audio_is_lea_open();

	return status;
}


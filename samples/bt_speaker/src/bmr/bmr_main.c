/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bmr.h"
#include "broadcast.h"
#include <app_launch.h>

static struct bmr_app_t *p_bmr = NULL;

#define LOCAL_NAME	"bis_local"
#define LOCAL_NAME_LEN		(sizeof(LOCAL_NAME) - 1)

#define BROADCAST_NAME	"bis_broadcast"
#define BROADCAST_NAME_LEN		(sizeof(BROADCAST_NAME) - 1)

static uint8_t local_name[33];
static uint8_t broadcast_name[33];

static int bmr_vnd_ext_rx(uint32_t handle, const uint8_t *buf, uint16_t len)
{
	SYS_LOG_INF("h: 0x%x, l: %d, v: 0x%x 0x%x 0x%x 0x%x\n",
		handle, len, buf[0], buf[1], buf[2], buf[3]);

#ifdef CONFIG_AUARCAST_FILTER_POLICY_H
    if (buf[0] != (COMPANY_ID & 0xff) || buf[1] != ((COMPANY_ID >> 8) & 0xff) ||
        buf[18] != (SERIVCE_UUID & 0xff) || buf[19] != ((SERIVCE_UUID >> 8) & 0xff))
    {
        SYS_LOG_ERR("COMPANY_ID or SERVICE_UUID compare fail\n");
        return -1;
    }
#endif

#if ENABLE_ENCRYPTION
	if ((buf[0] == (VS_ID & 0xFF)) &&
		(buf[1] == (VS_ID >> 8)) &&
		(buf[2] != 0)) {
		p_bmr->encryption = 1;
		memcpy(p_bmr->broadcast_code, &buf[2], 16);

		SYS_LOG_INF("encryption\n");
	}
#endif

	return 0;
}

int bmr_sink_init(void)
{
	uint32_t value;
	uint16_t temp_acl_handle = 0;
	struct bmr_app_t *bmr = bmr_get_app();
	temp_acl_handle = bt_manager_audio_get_letws_handle();

#ifdef CONFIG_PROPERTY
	int ret;

	ret = property_get(CFG_BT_LOCAL_NAME, local_name,
			sizeof(local_name) - 1);
	if (ret <= 0) {
		SYS_LOG_WRN("failed to get local name\n");
		memcpy(local_name, LOCAL_NAME, LOCAL_NAME_LEN);
	}

	ret = property_get(CFG_BT_BROADCAST_NAME, broadcast_name,
			sizeof(broadcast_name) - 1);
	if (ret <= 0) {
		SYS_LOG_WRN("failed to get broadcast name\n");
		memcpy(broadcast_name, BROADCAST_NAME, BROADCAST_NAME_LEN);
	}
#endif

	SYS_LOG_INF("name: %s %s 0x%x\n", local_name, broadcast_name,temp_acl_handle);

	value = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;

	bt_manager_broadcast_sink_init();
#if BIS_SYNC_LOCATIONS
	bt_manager_broadcast_sink_sync_loacations(value);
#endif

#if FILTER_LOCAL_NAME
	bt_manager_broadcast_filter_local_name(local_name);
#endif
#if FILTER_BROADCAST_NAME
	bt_manager_broadcast_filter_broadcast_name(broadcast_name);
#endif
#if FILTER_COMPANY_ID
	bt_manager_broadcast_filter_company_id(VS_ID);
#endif
#if FILTER_UUID16
	bt_manager_broadcast_filter_uuid16(VS_ID);
#endif

#if FILTER_BROADCAST_ID
	bt_manager_broadcast_filter_broadcast_id(broadcast_get_broadcast_id());
#endif

	bt_manager_broadcast_sink_register_ext_rx_cb(bmr_vnd_ext_rx);

	if (get_pawr_enable())
	{
		bt_manager_pawr_receive_start();
	}
	if (temp_acl_handle) {
		bmr->use_past = 1;  
		//TODO:disconnect leaudio,stop leaudio adv
		bt_manager_set_user_visual(1,0,0,BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE);
		btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
		btif_br_disconnect_device(BTSRV_DISCONNECT_PHONE_MODE);
		bt_manager_broadcast_past_subscribe(temp_acl_handle);
		broadcast_tws_vnd_request_past_info();
		return 0;
	} else {
		struct bt_le_scan_param param = {0};
		param.type = BT_LE_SCAN_TYPE_PASSIVE;
		param.options = BT_LE_SCAN_OPT_NONE;
		/* [65ms, 110ms], almost 60% duty cycle by default */
		param.interval = 176;
		param.window = 104;
		param.timeout = 0;

		bt_manager_broadcast_scan_param_init(&param);

		return bt_manager_broadcast_scan_start();
	}
}

static int bmr_sink_exit(void)
{
#ifdef CONFIG_BT_LETWS
	uint16_t temp_acl_handle;
	struct bmr_app_t *bmr = bmr_get_app();

	temp_acl_handle = bt_manager_letws_get_handle();
	bt_manager_broadcast_past_unsubscribe(temp_acl_handle);
    bmr->use_past = 0;
	bt_manager_set_user_visual(0,0,0,0);
#endif

	bt_manager_broadcast_sink_exit();
	bt_manager_broadcast_scan_param_init(NULL);

	bt_manager_pawr_receive_stop();

	return 0;
}

static void bmr_clean_broad_dev(void)
{
	struct bmr_app_t *bmr = bmr_get_app();
	struct bmr_broad_dev *dev,*tmp;

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&bmr->broad_dev_list, dev, tmp, node) {
		SYS_LOG_INF("hdl:0x%x\n",dev->handle);
		sys_slist_find_and_remove(&bmr->broad_dev_list, &dev->node);
		mem_free(dev);
	}
}

static int _bmr_init(void *p1, void *p2, void *p3)
{
	if (p_bmr) {
		return 0;
	}
	int8_t temp_role = 0;

#ifdef CONFIG_LOGIC_ANALYZER
	logic_init();
#endif

	p_bmr = app_mem_malloc(sizeof(struct bmr_app_t));
	if (!p_bmr) {
		SYS_LOG_ERR("malloc failed!\n");
		return -ENOMEM;
	}
	memset(p_bmr, 0, sizeof(struct bmr_app_t));

	bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_SWITCH_SINGLE_POINT, NULL, 0);

	if(tts_manager_is_playing()){
		p_bmr->tts_playing = 1;
	}

	bmr_view_init();
	bmr_sink_init();

#ifdef CONFIG_BT_LETWS
	temp_role = bt_manager_letws_get_dev_role();
#endif
	if(temp_role == BTSRV_TWS_NONE){
		system_app_set_auracast_mode(2);
	}else if(temp_role == BTSRV_TWS_SLAVE){
		system_app_set_auracast_mode(4);
	}

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
#ifdef CONFIG_BT_LE_AUDIO
	soc_dvfs_set_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "bmr");
#else
	soc_dvfs_set_level(BMR_FREQ, "bmr");
#endif /*CONFIG_BT_LE_AUDIO*/
#endif
	SYS_LOG_INF("init ok\n");
	return 0;
}

static int _bmr_exit(void)
{
	if (!p_bmr) {
		goto exit;
	}

	bmr_stop_playback();
	bmr_exit_playback();

	bmr_sink_exit();

	bmr_view_deinit();

	bmr_clean_broad_dev();

#ifdef CONFIG_BT_LETWS
	if(p_bmr->broadcast_mode_reset){
		system_app_set_auracast_mode(0);
	}
#endif

	app_mem_free(p_bmr);

	p_bmr = NULL;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
#ifdef CONFIG_BT_LE_AUDIO
	soc_dvfs_unset_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "bmr");
#else
	soc_dvfs_unset_level(BMR_FREQ, "bmr");
#endif /*CONFIG_BT_LE_AUDIO*/
#endif

exit:

	bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_SWITCH_MULTI_POINT, NULL, 0);
	SYS_LOG_INF("exit ok\n");

	return 0;
}

struct bmr_app_t *bmr_get_app(void)
{
	return p_bmr;
}

static int _bmr_proc_msg(struct app_msg *msg)
{
	switch (msg->type) {
	case MSG_BT_EVENT:
		bmr_bt_event_proc(msg);
		break;
	case MSG_INPUT_EVENT:
		bmr_input_event_proc(msg);
		break;
	case MSG_TTS_EVENT:
		bmr_tts_event_proc(msg);
		break;
	case MSG_EXIT_APP:
		_bmr_exit();
		break;
	default:
		break;
	}

	return 0;
}

static int _bmr_dump_app_state(void)
{
	print_buffer_lazy(APP_ID_BMR, (void *)p_bmr, sizeof(struct bmr_app_t));
	return 0;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_BMR, _bmr_init, _bmr_exit, _bmr_proc_msg, \
	_bmr_dump_app_state, NULL, NULL, NULL);

/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt music app main.
 */

#include "btmusic.h"
#include "tts_manager.h"
#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

static struct btmusic_app_t *p_btmusic_app;

#define DEFAULT_CONTEXTS	(BT_AUDIO_CONTEXT_UNSPECIFIED | \
				BT_AUDIO_CONTEXT_MEDIA)

#define LOCAL_NAME	"bis_local"
#define LOCAL_NAME_LEN		(sizeof(LOCAL_NAME) - 1)

#define BROADCAST_NAME	"bis_broadcast"
#define BROADCAST_NAME_LEN		(sizeof(BROADCAST_NAME) - 1)

static uint8_t local_name[33];
static uint8_t broadcast_name[33];

static struct bt_broadcast_qos source_qos = {
	.packing = BT_AUDIO_PACKING_INTERLEAVED,
	.framing = BT_AUDIO_UNFRAMED,
	.phy = BT_AUDIO_PHY_2M,
	.rtn = 2,
	.max_sdu = BROADCAST_SDU,
	/* max_transport_latency, unit: ms */
	.latency = BROADCAST_LAT,
	/* sdu_interval, unit: us */
	.interval = BROADCAST_SDU_INTERVAL,
	/* presentation_delay, unit: us */
	.delay = BCST_QOS_DELAY,
	/* processing_time, unit: us */
	.processing = 9000,
};

#ifdef ENABLE_PAWR_APP
static int pawr_br_handle_response(const uint8_t *buf, uint16_t len)
{
	return pawr_handle_response(AUDIO_STREAM_SOUNDBAR, buf, len);
}
#endif

#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
static void btmusic_display_freqpoint_energy(struct thread_timer *ttimer,
					     void *expiry_fn_arg)
{
	media_freqpoint_energy_info_t temp_info;
	if (bt_music_a2dp_get_freqpoint_energy(&temp_info) == 0) {
		SYS_LOG_INF("fp energy:%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
			    temp_info.values[0],
			    temp_info.values[1],
			    temp_info.values[2],
			    temp_info.values[3],
			    temp_info.values[4],
			    temp_info.values[5],
			    temp_info.values[6],
			    temp_info.values[7], temp_info.values[8]);
	}
}
#endif

void btmusic_delay_start(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	if (!p_btmusic_app || p_btmusic_app->playback_player)
		return;

	bt_manager_audio_stream_restore(BT_TYPE_BR);
	SYS_LOG_INF(":\n");
}

int btmusic_bms_source_init(void)
{
	if (!p_btmusic_app)
		return -EINVAL;

	struct bt_broadcast_source_create_param param = { 0 };
	struct bt_broadcast_source_big_param big_param;
	struct bt_le_per_adv_param per_adv_param = { 0 };
	struct bt_le_adv_param adv_param = { 0 };
	struct broadcast_param_t bms_broadcast_param = { 0 };
	struct bt_broadcast_qos *qos = p_btmusic_app->qos;
	uint16_t temp_acl_handle = 0;
	u8_t ch = 0;
	int8_t sq_mode_enable = 0;

	sq_mode_enable = broadcast_get_sq_mode();

    ch = btmusic_get_stereo_channel();
	temp_acl_handle = bt_manager_audio_get_letws_handle();

#if (BROADCAST_NUM_BIS != 2)
	struct bt_broadcast_subgroup_1 subgroup = { 0 };
#else
	struct bt_broadcast_subgroup_2 subgroup = { 0 };
#endif
	// TODO:
	uint16_t appearance = 0x885;	/* Audio Source + Broadcasting Device */
	int ret;

	if (thread_timer_is_running(&p_btmusic_app->broadcast_start_timer))
		thread_timer_stop(&p_btmusic_app->broadcast_start_timer);

	if(p_btmusic_app->broadcast_id){
		SYS_LOG_WRN("already exist\n");
		return -EINVAL;
	}

#ifdef CONFIG_PROPERTY
	ret = property_get(CFG_BT_LOCAL_NAME, local_name, sizeof(local_name) - 1);
	if (ret <= 0) {
		SYS_LOG_WRN("failed to get local name\n");
		memcpy(local_name, LOCAL_NAME, LOCAL_NAME_LEN);
	}

	ret = property_get(CFG_BT_BROADCAST_NAME, broadcast_name, sizeof(broadcast_name) - 1);
	if (ret <= 0) {
		SYS_LOG_WRN("failed to get broadcast name\n");
		memcpy(broadcast_name, BROADCAST_NAME, BROADCAST_NAME_LEN);
	}
#endif

    if (sq_mode_enable) {
		p_btmusic_app->broadcast_sample_rate = 16;
		broadcast_get_source_param(32, BROADCAST_CH,BROADCAST_ISO_INTERVAL, &bms_broadcast_param);
	}else {
		p_btmusic_app->broadcast_sample_rate = 48;
		broadcast_get_source_param(80, BROADCAST_CH,BROADCAST_ISO_INTERVAL, &bms_broadcast_param);
	}
	p_btmusic_app->broadcast_duration = bms_broadcast_param.duration;

	qos->max_sdu = bms_broadcast_param.sdu;
	qos->interval = bms_broadcast_param.sdu_interval;
	qos->latency = bms_broadcast_param.latency;

	SYS_LOG_INF("qos:%d,%d,%d,%d,%d",qos->max_sdu,qos->interval,qos->latency,qos->delay,qos->processing);

#if (BROADCAST_NUM_BIS == 2)
	if ((ch) && (temp_acl_handle)) {
	    subgroup.num_bis = 1;		
	} else {
	    subgroup.num_bis = 2;		
	}
#else
	subgroup.num_bis = 1;
#endif
	subgroup.format = BT_AUDIO_CODEC_LC3;
	subgroup.frame_duration =  bms_broadcast_param.duration;
	subgroup.blocks = 1;
	subgroup.sample_rate = p_btmusic_app->broadcast_sample_rate;
	subgroup.octets = bms_broadcast_param.octets;
#if (BROADCAST_NUM_BIS != 2)
#if (BROADCAST_CH == 2)
	subgroup.locations = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;
#else
	subgroup.locations = BT_AUDIO_LOCATIONS_FL;
#endif /*(BROADCAST_CH == 2)*/
#else 
    if (temp_acl_handle) {
	    if (ch == MEDIA_EFFECT_OUTPUT_L_ONLY) {
		   subgroup.locations = BT_AUDIO_LOCATIONS_FR;
	    } else if (ch == MEDIA_EFFECT_OUTPUT_R_ONLY) {
		   subgroup.locations = BT_AUDIO_LOCATIONS_FL;
	    }
	}

#endif /*#if (BROADCAST_NUM_BIS != 2)*/
	subgroup.language = BT_LANGUAGE_UNKNOWN;
	subgroup.contexts = DEFAULT_CONTEXTS;

#if (BROADCAST_NUM_BIS == 2)
	if ((!temp_acl_handle) || (!subgroup.locations)) {
	    subgroup.bis[0].locations = BT_AUDIO_LOCATIONS_FL;
	    subgroup.bis[1].locations = BT_AUDIO_LOCATIONS_FR;
	}
#endif

	big_param.iso_interval = bms_broadcast_param.iso_interval;
	big_param.max_pdu = bms_broadcast_param.pdu;
	big_param.nse = bms_broadcast_param.nse;
	big_param.bn = bms_broadcast_param.bn;
	big_param.irc = bms_broadcast_param.irc;
	big_param.pto = bms_broadcast_param.pto;

	/* BT_LE_EXT_ADV_NCONN */
	adv_param.id = BT_ID_DEFAULT;
	/* [150ms, 150ms] by default */
	adv_param.interval_min = 128/*BT_GAP_ADV_FAST_INT_MAX_2*/;
	adv_param.interval_max = 128/*BT_GAP_ADV_FAST_INT_MAX_2*/;
	adv_param.options = BT_LE_ADV_OPT_EXT_ADV;
	adv_param.options |= BT_LE_ADV_OPT_USE_IDENTITY;
	adv_param.sid = BT_EXT_ADV_SID_BROADCAST;

	/* 100ms */
	per_adv_param.interval_min = bms_broadcast_param.pa_interval;
	per_adv_param.interval_max = bms_broadcast_param.pa_interval;

#if ENABLE_ENCRYPTION
	param.encryption = true;
	param.broadcast_code[0] = 0x55;
	param.broadcast_code[1] = 0xaa;
	memcpy(p_btmusic_app->broadcast_code, param.broadcast_code, 16);
#endif

	param.broadcast_id = broadcast_get_broadcast_id();
	if (param.broadcast_id == INVALID_BROADCAST_ID) {
		SYS_LOG_ERR("get broadcast failed\n");
		return -EINVAL;
	}

	param.local_name = local_name;
	param.broadcast_name = broadcast_name;
	param.qos = p_btmusic_app->qos;
	param.appearance = appearance;
	param.num_subgroups = 1;
	param.subgroup = (struct bt_broadcast_subgroup *)&subgroup;
	param.big_param = &big_param;
	param.per_adv_param = &per_adv_param;
	param.adv_param = &adv_param;

	if (temp_acl_handle) {
        param.past_enable = 1;
        param.acl_handle = temp_acl_handle;
		p_btmusic_app->use_past = 1;
	} else {
		p_btmusic_app->use_past = 0;		
	}

	ret = bt_manager_broadcast_source_create(&param);
	if (ret < 0) {
		SYS_LOG_ERR("failed %d\n",ret);
		thread_timer_start(&p_btmusic_app->broadcast_start_timer, 300, 0);
		return ret;
	}

	p_btmusic_app->broadcast_retransmit_number = (bms_broadcast_param.nse/bms_broadcast_param.bn);
	p_btmusic_app->broadcast_id = param.broadcast_id;

	SYS_LOG_INF("name: %s %s 0x%x\n", local_name, broadcast_name,temp_acl_handle);

#ifdef ENABLE_PAWR_APP
	if (get_pawr_enable()) {
		struct bt_le_per_adv_param per_adv_param1 = {0};
		per_adv_param1.interval_min = PAwR_INTERVAL;
		per_adv_param1.interval_max = PAwR_INTERVAL;
		per_adv_param1.subevent_interval = PAwR_SUB_INTERVAL;
		per_adv_param1.response_slot_delay = PAwR_RSP_DELAY;
		bt_manager_pawr_adv_start(pawr_br_handle_response, &per_adv_param1);
	}
#endif

	return 0;
}

int btmusic_bms_source_exit(void)
{
	if (!p_btmusic_app)
		return 0;
	if (thread_timer_is_running(&p_btmusic_app->broadcast_start_timer))
		thread_timer_stop(&p_btmusic_app->broadcast_start_timer);
	if(!p_btmusic_app->broadcast_source_exit && p_btmusic_app->broadcast_id){
		SYS_LOG_INF("\n");
		bt_manager_broadcast_source_disable(p_btmusic_app->broadcast_id);
		bt_manager_broadcast_source_release(p_btmusic_app->broadcast_id);
		p_btmusic_app->broadcast_source_exit = 1;
	}

#if 1
    bt_manager_pawr_adv_stop();
#endif
	return 0;
}

int btmusic_get_auracast_mode(void)
{
	return system_app_get_auracast_mode();
}

void btmusic_set_auracast_mode(int mode)
{
	if (!p_btmusic_app)
		return;

	if(mode == btmusic_get_auracast_mode())
		return;

	if (thread_timer_is_running(&p_btmusic_app->broadcast_switch_timer))
		thread_timer_stop(&p_btmusic_app->broadcast_switch_timer);

	SYS_EVENT_INF(EVENT_BTMUSIC_AURACAST_MODE, mode);
	if(!mode){
		btmusic_bms_source_exit();
		system_app_set_auracast_mode(0);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (p_btmusic_app->set_dvfs_level) {
			soc_dvfs_unset_level(p_btmusic_app->set_dvfs_level, "bms_br");
			p_btmusic_app->set_dvfs_level = 0;
		}
#endif
	}else if(mode == 1){
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		p_btmusic_app->set_dvfs_level = BCST_FREQ;
		soc_dvfs_set_level(p_btmusic_app->set_dvfs_level, "bms_br");
#endif
		btmusic_bms_source_init();
		system_app_set_auracast_mode(1);
	}else if(mode == 3){
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		p_btmusic_app->set_dvfs_level = BCST_FREQ;
		soc_dvfs_set_level(p_btmusic_app->set_dvfs_level, "bms_br");
#endif
		//wait for past req
		if(broadcast_tws_is_ready_for_past()){
			system_app_set_auracast_mode(mode);
			btmusic_bms_source_init();
		}else{
			p_btmusic_app->wait_for_past_req = 1;
			system_app_set_auracast_mode(mode);
		}
	}
}

static void btmusic_auracast_switch_handler(struct thread_timer *ttimer,
				   void *expiry_fn_arg){
	if (NULL == p_btmusic_app) {
		return;
	}
	int8_t temp_role = 0;
#ifdef CONFIG_BT_LETWS
	temp_role = bt_manager_letws_get_dev_role();
#endif

	if(!p_btmusic_app->playing && btmusic_get_auracast_mode()
		&& temp_role == BTSRV_TWS_NONE){
		SYS_LOG_INF("switch to bmr\n");
		system_app_set_auracast_mode(2);
		system_app_launch_switch(DESKTOP_PLUGIN_ID_BR_MUSIC,DESKTOP_PLUGIN_ID_BMR);
	}
}

void btmusic_player_reset_trigger(void)
{
	struct app_msg msg = { 0 };

	if (NULL == p_btmusic_app) {
		return;
	}

	if (!p_btmusic_app->restart) {
		SYS_LOG_INF("restart\n");
		msg.type = MSG_BTMUSIC_APP_EVENT;
		msg.cmd = MSG_BTMUSIC_MESSAGE_CMD_PLAYER_RESET;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		p_btmusic_app->restart = 1;
	}
}

void btmusic_playTimeBoost_trigger(void)
{
	struct app_msg msg = { 0 };

	msg.type = MSG_BTMUSIC_APP_EVENT;
	msg.cmd = MSG_BTMUSIC_MESSAGE_CMD_PLAY_TIME_BOOST;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

static void btmusic_bms_start_handler(struct thread_timer *ttimer,
				   void *expiry_fn_arg){
	if (NULL == p_btmusic_app) {
		return;
	}
	if(1 == btmusic_get_auracast_mode() || 
		3 == btmusic_get_auracast_mode()){
		btmusic_bms_source_init();
	}
}

static int _btmusic_init(void *p1, void *p2, void *p3)
{
	if (p_btmusic_app) {
		return 0;
	}
	int ret = 0;
	int8_t temp_role = 0;

	p_btmusic_app = app_mem_malloc(sizeof(struct btmusic_app_t));
	if (!p_btmusic_app) {
		SYS_LOG_ERR("malloc fail!\n");
		return -ENOMEM;
	}
	
	memset(p_btmusic_app, 0, sizeof(struct btmusic_app_t));
	p_btmusic_app->qos = &source_qos;

	btmusic_view_init();

	if(tts_manager_is_playing()){
		p_btmusic_app->tts_playing = 1;
	}

	//todo
	bt_manager_stream_enable(STREAM_TYPE_A2DP, true);

	bt_manager_set_stream_type(AUDIO_STREAM_MUSIC);

#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
	thread_timer_init(&p_btmusic_app->energy_timer,
			  btmusic_display_freqpoint_energy, NULL);
	thread_timer_start(&p_btmusic_app->energy_timer, 100, 100);
#endif

	thread_timer_init(&p_btmusic_app->resume_timer, btmusic_delay_resume, NULL);
	thread_timer_init(&p_btmusic_app->play_timer, btmusic_delay_start,
			  NULL);
	if(!btmusic_get_auracast_mode())
		thread_timer_start(&p_btmusic_app->play_timer, 800, 0);

	thread_timer_init(&p_btmusic_app->broadcast_start_timer, btmusic_bms_start_handler,
			  NULL);

	thread_timer_init(&p_btmusic_app->broadcast_switch_timer, btmusic_auracast_switch_handler,
			  NULL);

	//thread_timer_init(&p_btmusic_app->mute_timer,btmusic_mute_handler, NULL);

	if(btmusic_get_auracast_mode()){
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		p_btmusic_app->set_dvfs_level = BCST_FREQ;
		soc_dvfs_set_level(p_btmusic_app->set_dvfs_level, "bms_br");
#endif
#ifdef CONFIG_BT_LETWS
		temp_role = bt_manager_letws_get_dev_role();
#endif
		if(temp_role == BTSRV_TWS_MASTER){
			//wait for past req
			if(broadcast_tws_is_ready_for_past()){
				system_app_set_auracast_mode(3);
				btmusic_bms_source_init();
			}else{
				p_btmusic_app->wait_for_past_req = 1;
				system_app_set_auracast_mode(3);
			}
		}else if(temp_role == BTSRV_TWS_NONE){
			system_app_set_auracast_mode(1);
			btmusic_bms_source_init();
		}
	}

	SYS_LOG_INF("init finished\n");

#ifdef CONFIG_TWS
#ifndef CONFIG_TWS_BACKGROUND_BT
	bt_manager_resume_phone();
#endif
#endif

	return ret;
}

static int _btmusic_exit(void)
{
	if (!p_btmusic_app)
		goto exit;

#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
	if (thread_timer_is_running(&p_btmusic_app->energy_timer))
		thread_timer_stop(&p_btmusic_app->energy_timer);
#endif

	if (thread_timer_is_running(&p_btmusic_app->resume_timer))
		thread_timer_stop(&p_btmusic_app->resume_timer);

	if (thread_timer_is_running(&p_btmusic_app->play_timer))
		thread_timer_stop(&p_btmusic_app->play_timer);

	if (thread_timer_is_running(&p_btmusic_app->broadcast_start_timer))
		thread_timer_stop(&p_btmusic_app->broadcast_start_timer);

	if (thread_timer_is_running(&p_btmusic_app->broadcast_switch_timer))
		thread_timer_stop(&p_btmusic_app->broadcast_switch_timer);

	if (thread_timer_is_running(&p_btmusic_app->user_pause_timer))
		thread_timer_stop(&p_btmusic_app->user_pause_timer);

	if (thread_timer_is_running(&p_btmusic_app->user_pause_media_active_timer))
		thread_timer_stop(&p_btmusic_app->user_pause_media_active_timer);

	if (thread_timer_is_running(&p_btmusic_app->mute_timer))
		thread_timer_stop(&p_btmusic_app->mute_timer);
		
#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif

	bt_manager_stream_enable(STREAM_TYPE_A2DP, false);

	btmusic_stop_playback();
	
	btmusic_exit_playback();

	btmusic_bms_stop_capture();
	btmusic_bms_exit_capture();

	btmusic_bms_source_exit();

	btmusic_view_deinit();
	
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if(p_btmusic_app->set_dvfs_level){
		soc_dvfs_unset_level(p_btmusic_app->set_dvfs_level, "bms_br");
		p_btmusic_app->set_dvfs_level = 0;
	}
#endif

	app_mem_free(p_btmusic_app);
	p_btmusic_app = NULL;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif

 exit:
	SYS_LOG_INF("exit finished\n");

	return 0;
}

static int btmusic_proc_msg(struct app_msg *msg)
{
	SYS_LOG_INF("type %d, cmd %d, value %d\n", msg->type,
		    msg->cmd, msg->value);
	switch (msg->type) {
	case MSG_EXIT_APP:
		_btmusic_exit();
		break;
	case MSG_BT_EVENT:
		btmusic_bt_event_proc(msg);
		break;
	case MSG_INPUT_EVENT:
		btmusic_input_event_proc(msg);
		break;
	case MSG_TTS_EVENT:
		btmusic_tts_event_proc(msg);
		break;
	case MSG_BTMUSIC_APP_EVENT:
		btmusic_app_event_proc(msg);
		break;
#ifdef CONFIG_BT_LETWS
	case MSG_TWS_EVENT:
		btmusic_tws_event_proc(msg);
		break;
#endif
	default:
		SYS_LOG_ERR("error: 0x%x!\n", msg->type);
		break;
	}
	return 0;
}

struct btmusic_app_t *btmusic_get_app(void)
{
	return p_btmusic_app;
}

static int btmusic_dump_app_state(void)
{
	print_buffer_lazy(APP_ID_BTMUSIC, (void *)btmusic_get_app(),
					  sizeof(struct btmusic_app_t));
	return 0;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_BR_MUSIC, _btmusic_init, _btmusic_exit, btmusic_proc_msg, \
	btmusic_dump_app_state, NULL, NULL, NULL);

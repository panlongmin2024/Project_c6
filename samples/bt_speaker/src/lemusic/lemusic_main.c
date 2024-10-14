/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lemusic.h"

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

static struct lemusic_app_t *p_lemusic = NULL;

#define DEFAULT_CONTEXTS	(BT_AUDIO_CONTEXT_UNSPECIFIED | \
				BT_AUDIO_CONTEXT_MEDIA)

/* define BMS iso interval for cis and bis coexist */
#define BMS_ISO_INTERVAL_7_5MS   6
#define BMS_ISO_INTERVAL_10MS    8
#define BMS_ISO_INTERVAL_15MS    12
#define BMS_ISO_INTERVAL_20MS    16
#define BMS_ISO_INTERVAL_30MS    24

#define BMS_DEFAULT_AUDIO_CHAN   1
#define BMS_DEFAULT_KBPS         96
#define BMS_DEFAULT_ISO_INTERVAL BMS_ISO_INTERVAL_20MS

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

static int lemusic_bms_source_init(void);

#ifdef ENABLE_PAWR_APP
static int pawr_le_handle_response(const uint8_t *buf, uint16_t len)
{
	return pawr_handle_response(AUDIO_STREAM_LE_AUDIO, buf, len);//need check volume value(0-16 or 0-255)
}
#endif

static void lemusic_bms_restart_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	struct app_msg msg = { 0 };

	if (NULL == p_lemusic || !p_lemusic->bms.enable) {
		return;
	}
	struct lemusic_bms_device *bms = &p_lemusic->bms;

	if (bms->restart) {
		SYS_LOG_INF("restart\n");
		msg.type = MSG_LEMUSIC_APP_EVENT;
		msg.cmd = MSG_LEMUSIC_MESSAGE_CMD_PLAYER_RESET;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		bms->restart = 0;
		bms->restart_count = 0;
	}
}

static void lemusic_bms_start_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	if (NULL == p_lemusic) {
		return;
	}

	if (1 == lemusic_get_auracast_mode()
		|| 3 == lemusic_get_auracast_mode()) {
		lemusic_bms_source_init();
	}
}

static void lemusic_switch_bmr_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	if (NULL == p_lemusic) {
		return;
	}
	int8_t temp_role = 0;
#ifdef CONFIG_BT_LETWS
	temp_role = bt_manager_letws_get_dev_role();
#endif

	if (1 == lemusic_get_auracast_mode() && p_lemusic->bms.cur_stream == BT_AUDIO_STREAM_NONE
		&& temp_role == BTSRV_TWS_NONE) {
		lemusic_set_auracast_mode(2);
	}
}

static int bms_get_source_param(uint16_t kbps, uint8_t audio_chan, struct broadcast_param_t *broadcast_param)
{
	uint16_t iso_interval = 0;

	if (!broadcast_param) {
		SYS_LOG_ERR("param NULL\n");
		return -1;
	}

	if (kbps != 80 && kbps != 96) {
		SYS_LOG_ERR("unsupport kbps:%d\n",kbps);
		return -1;
	}

	if (audio_chan != 1 && audio_chan != 2) {
		SYS_LOG_ERR("unsupport audio chan:%d\n",audio_chan);
		return -1;
	}

	iso_interval = bt_manager_audio_get_active_channel_iso_interval() / 1250;
	if (!iso_interval) {
		iso_interval = BMS_DEFAULT_ISO_INTERVAL;
	}

	broadcast_param->iso_interval = iso_interval;
	broadcast_param->audio_chan = audio_chan;
	broadcast_param->kbps = kbps * audio_chan;

	switch(iso_interval) {
		case BMS_ISO_INTERVAL_7_5MS:
		{
			broadcast_param->sdu = 90 * audio_chan;
			broadcast_param->pdu = 90 * audio_chan;
			broadcast_param->octets = 90;
			broadcast_param->sdu_interval = 7500;
			broadcast_param->nse = 3;
			broadcast_param->bn = 1;
			broadcast_param->irc = 3;
			broadcast_param->pto = 0;
			broadcast_param->latency = 8;
			broadcast_param->duration = BT_FRAME_DURATION_7_5MS;
#if defined(CONFIG_BT_PAWR)
			broadcast_param->nse = 2;
			broadcast_param->irc = 2;
#endif
			broadcast_param->pa_interval = 72; //90ms
			break;
		}

		case BMS_ISO_INTERVAL_10MS:
		{
			uint16_t octets = 0;
			if (kbps == 80) {
				octets = 100;
				broadcast_param->nse = 4;
				broadcast_param->irc = 4;
			} else {
				octets = 120;
				broadcast_param->nse = 3;
				broadcast_param->irc = 3;
			}
#if defined(CONFIG_BT_PAWR)
			broadcast_param->nse = 2;
			broadcast_param->irc = 2;
#endif
			broadcast_param->sdu = octets * audio_chan;
			broadcast_param->pdu = octets * audio_chan;
			broadcast_param->octets = octets;
			broadcast_param->sdu_interval = 10000;
			broadcast_param->bn = 1;
			broadcast_param->pto = 0;
			broadcast_param->latency = 10;
			broadcast_param->duration = BT_FRAME_DURATION_10MS;
			broadcast_param->pa_interval = 80; //100ms
			break;
		}

		case BMS_ISO_INTERVAL_15MS:
		{
			broadcast_param->sdu = 90 * audio_chan;
			broadcast_param->pdu = 90 * audio_chan;
			broadcast_param->octets = 90;
			broadcast_param->sdu_interval = 7500;
			broadcast_param->nse = 6;
			broadcast_param->bn = 2;
			broadcast_param->irc = 3;
			broadcast_param->pto = 0;
			broadcast_param->latency = 15;
			broadcast_param->duration = BT_FRAME_DURATION_7_5MS;
#if defined(CONFIG_BT_PAWR)
			broadcast_param->nse = 4;
			broadcast_param->irc = 2;
#endif
			broadcast_param->pa_interval = 72; //90ms
			break;
		}

		case BMS_ISO_INTERVAL_20MS:
		{
			uint16_t octets = 0;
			if (kbps == 80) {
				octets = 100;
				broadcast_param->nse = 8;
				broadcast_param->irc = 4;
			} else {
				octets = 120;
				broadcast_param->nse = 6;
				broadcast_param->irc = 3;
			}
#if defined(CONFIG_BT_PAWR)
			broadcast_param->nse = 4;
			broadcast_param->irc = 2;
#endif
			broadcast_param->sdu = octets * audio_chan;
			broadcast_param->pdu = octets * audio_chan;
			broadcast_param->octets = octets;
			broadcast_param->sdu_interval = 10000;
			broadcast_param->bn = 2;
			broadcast_param->pto = 0;
			broadcast_param->latency = 20;
			broadcast_param->duration = BT_FRAME_DURATION_10MS;
			broadcast_param->pa_interval = 80; //100ms

			break;
		}

		case BMS_ISO_INTERVAL_30MS:
		{
			uint16_t octets = 0;
			if (kbps == 80) {
				octets = 100;
				broadcast_param->nse = 12;
				broadcast_param->irc = 4;
			} else {
				octets = 120;
				broadcast_param->nse = 9;
				broadcast_param->irc = 3;

			}
#if defined(CONFIG_BT_PAWR)
			broadcast_param->nse = 6;
			broadcast_param->irc = 2;
#endif
			broadcast_param->sdu = octets * audio_chan;
			broadcast_param->pdu = octets * audio_chan;
			broadcast_param->octets = octets;
			broadcast_param->sdu_interval = 10000;
			broadcast_param->bn = 3;
			broadcast_param->pto = 0;
			broadcast_param->latency = 40;
			broadcast_param->duration = BT_FRAME_DURATION_10MS;
			broadcast_param->pa_interval = 72; //90ms
			break;
		}

		default:
			SYS_LOG_ERR("unsupport iso_interval:%d\n", iso_interval);
			return -1;
	}
	return 0;
}

#ifdef ENABLE_PAWR_APP
static int bms_get_pawr_param(struct bt_le_per_adv_param *param)
{
	uint16_t iso_interval = 0;

	if (!param) {
		SYS_LOG_ERR("param NULL\n");
		return -1;
	}

	iso_interval = bt_manager_audio_get_active_channel_iso_interval() / 1250;
	if (!iso_interval) {
		iso_interval = BMS_DEFAULT_ISO_INTERVAL;
	}

	switch(iso_interval) {
		case BMS_ISO_INTERVAL_7_5MS:
		{
			param->interval_min = 72; //90ms
			param->interval_max = 72;
			param->subevent_interval = 24; //30ms
			param->response_slot_delay = 12; //15ms
			break;
		}

		case BMS_ISO_INTERVAL_10MS:
		{
			param->interval_min = 80; //100ms
			param->interval_max = 80;
			param->subevent_interval = 32; //40ms
			param->response_slot_delay = 9; //11.25ms
			break;
		}

		case BMS_ISO_INTERVAL_15MS:
		{
			param->interval_min = 72; //90ms
			param->interval_max = 72;
			param->subevent_interval = 24; //30ms
			param->response_slot_delay = 12; //15ms
			break;
		}

		case BMS_ISO_INTERVAL_20MS:
		{
			param->interval_min = 80; //100ms
			param->interval_max = 80;
			param->subevent_interval = 32; //40ms
			param->response_slot_delay = 9; //11.25ms
			break;
		}

		case BMS_ISO_INTERVAL_30MS:
		{
			param->interval_min = 72; //90ms
			param->interval_max = 72;
			param->subevent_interval = 24; //30ms
			param->response_slot_delay = 12; //15ms
			break;
		}

		default:
			SYS_LOG_ERR("unsupport iso_interval:%d\n", iso_interval);
			return -1;
	}
	return 0;
}
#endif /*ENABLE_PAWR_APP*/

static int lemusic_bms_source_init(void)
{
	struct lemusic_bms_device *bms = &p_lemusic->bms;

	struct bt_broadcast_source_create_param param = { 0 };
	struct bt_broadcast_source_big_param big_param;
	struct bt_le_adv_param adv_param = { 0 };
	struct bt_le_per_adv_param per_adv_param = { 0 };

	struct broadcast_param_t bms_broadcast_param = { 0 };
	struct bt_broadcast_qos *qos = bms->qos;
	uint8_t audio_chan = BMS_DEFAULT_AUDIO_CHAN;
	uint8_t kbps = BMS_DEFAULT_KBPS;
	uint16_t temp_acl_handle = 0;
	u8_t ch = 0;

	if (!p_lemusic)
		return -EINVAL;

#ifdef CONFIG_BT_SELF_APP
	ch = selfapp_get_channel();
#endif
	temp_acl_handle = bt_manager_audio_get_letws_handle();

	if (thread_timer_is_running(&bms->broadcast_start_timer))
		thread_timer_stop(&bms->broadcast_start_timer);

#if (BROADCAST_NUM_BIS != 2)
	struct bt_broadcast_subgroup_1 subgroup = { 0 };
#else
	struct bt_broadcast_subgroup_2 subgroup = { 0 };
#endif
	// TODO:
	uint16_t appearance = 0x885;	/* Audio Source + Broadcasting Device */
	int ret;

#ifdef CONFIG_PROPERTY
	ret = property_get("LNAME", local_name, sizeof(local_name) - 1);
	if (ret <= 0) {
		SYS_LOG_WRN("failed to get local name\n");
		memcpy(local_name, LOCAL_NAME, LOCAL_NAME_LEN);
	}

	ret = property_get("BNAME", broadcast_name, sizeof(broadcast_name) - 1);
	if (ret <= 0) {
		SYS_LOG_WRN("failed to get broadcast name\n");
		memcpy(broadcast_name, BROADCAST_NAME, BROADCAST_NAME_LEN);
	}
#endif

	if (bms_get_source_param(kbps, audio_chan, &bms_broadcast_param)) {
		SYS_LOG_ERR("source param err\n");
		return -1;
	}

	if (!qos) {
		SYS_LOG_ERR("qos NULL\n");
		return -1;
	}

	qos->max_sdu = bms_broadcast_param.sdu;
	qos->interval = bms_broadcast_param.sdu_interval;
	qos->latency = bms_broadcast_param.latency;
	qos->rtn = bms_broadcast_param.irc;
	bms->iso_interval = bms_broadcast_param.iso_interval;

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
	subgroup.frame_duration = bms_broadcast_param.duration;
	subgroup.blocks = 1;
	subgroup.sample_rate = 48;
	subgroup.octets = bms_broadcast_param.octets;
#if (BROADCAST_NUM_BIS != 2)
#if (BROADCAST_CH == 2)
	subgroup.locations = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;
#else
	subgroup.locations = BT_AUDIO_LOCATIONS_FL;
#endif
#endif
	subgroup.language = BT_LANGUAGE_UNKNOWN;
	subgroup.contexts = DEFAULT_CONTEXTS;

#if (BROADCAST_NUM_BIS == 2)
	subgroup.bis[0].locations = BT_AUDIO_LOCATIONS_FL;
	subgroup.bis[1].locations = BT_AUDIO_LOCATIONS_FR;
#endif

	big_param.iso_interval = bms_broadcast_param.iso_interval;
	big_param.max_pdu = bms_broadcast_param.pdu;
	big_param.nse = bms_broadcast_param.nse;
	big_param.bn = bms_broadcast_param.bn;
	big_param.irc = bms_broadcast_param.irc;
	big_param.pto = bms_broadcast_param.pto;

	/* 90ms or 100ms */
	per_adv_param.interval_min = bms_broadcast_param.pa_interval;
	per_adv_param.interval_max = bms_broadcast_param.pa_interval;

	/* BT_LE_EXT_ADV_NCONN */
	adv_param.id = BT_ID_DEFAULT;
	adv_param.interval_min = 256;//160ms
	adv_param.interval_max = 256;//160ms
	adv_param.options = BT_LE_ADV_OPT_EXT_ADV;
	adv_param.options |= BT_LE_ADV_OPT_USE_IDENTITY;
	adv_param.sid = BT_EXT_ADV_SID_BROADCAST;

#if ENABLE_ENCRYPTION
	param.encryption = true;
	param.broadcast_code[0] = 0x55;
	param.broadcast_code[1] = 0xaa;
	memcpy(bms->broadcast_code, param.broadcast_code, 16);
#endif

	param.broadcast_id = broadcast_get_broadcast_id();
	if (param.broadcast_id == INVALID_BROADCAST_ID) {
		SYS_LOG_ERR("get broadcast failed\n");
		return -EINVAL;
	}

	param.local_name = local_name;
	param.broadcast_name = broadcast_name;
	param.qos = bms->qos;
	param.appearance = appearance;
	param.num_subgroups = 1;
	param.subgroup = (struct bt_broadcast_subgroup *)&subgroup;
	param.big_param = &big_param;
	param.per_adv_param = &per_adv_param;
	param.adv_param = &adv_param;

	if (temp_acl_handle) {
		param.past_enable = 1;
		param.acl_handle = temp_acl_handle;
		bms->use_past = 1;
	} else {
		bms->use_past = 0;
	}

	ret = bt_manager_broadcast_source_create(&param);
	if (ret < 0) {
		SYS_LOG_ERR("failed");
		thread_timer_start(&bms->broadcast_start_timer, 300, 0);
		return ret;
	}

	bms->broadcast_retransmit_number = qos->rtn;
	bms->broadcast_id = param.broadcast_id;

	SYS_LOG_INF("name: %s %s\n", local_name, broadcast_name);

#ifdef ENABLE_PAWR_APP
	if (get_pawr_enable()) {
		struct bt_le_per_adv_param per_adv_param1 = {0};

		if (bms_get_pawr_param(&per_adv_param1)) {
			SYS_LOG_ERR("pawr param err\n");
			return -1;
		}

		bt_manager_pawr_adv_start(pawr_le_handle_response, &per_adv_param1);
	}
#endif

	thread_timer_init(&bms->restart_timer, lemusic_bms_restart_handler,
			  NULL);
	thread_timer_start(&bms->restart_timer, 200, 200);

	bms->enable = 1;

	return 0;
}

int lemusic_bms_source_exit(void)
{
	struct lemusic_bms_device *bms = &p_lemusic->bms;

	bt_manager_broadcast_source_disable(bms->broadcast_id);
	bt_manager_broadcast_source_release(bms->broadcast_id);

#ifdef ENABLE_PAWR_APP
    bt_manager_pawr_adv_stop();
#endif

	if (thread_timer_is_running(&bms->restart_timer))
		thread_timer_stop(&bms->restart_timer);
	if (thread_timer_is_running(&bms->broadcast_start_timer))
		thread_timer_stop(&bms->broadcast_start_timer);

	bms->enable = 0;

	return 0;
}

int lemusic_get_auracast_mode(void)
{
	return system_app_get_auracast_mode();
}

void lemusic_set_auracast_mode(int mode)
{
	if (!p_lemusic)
		return;

	if (thread_timer_is_running(&p_lemusic->switch_bmr_timer)) {
		thread_timer_stop(&p_lemusic->switch_bmr_timer);
	}

	if (mode == lemusic_get_auracast_mode())
		return;

	SYS_EVENT_INF(EVENT_LEMUSIC_AURACAST_MODE, mode);
	if (!mode) {
		lemusic_bms_source_exit();
		system_app_set_auracast_mode(0);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		lemusic_update_dvfs(LE_FREQ);
#endif
	} else if (mode == 1) {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		lemusic_update_dvfs(LE_BCST_FREQ);
#endif
		system_app_set_auracast_mode(1);
		lemusic_bms_source_init();

	}else if(mode == 3){
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		lemusic_update_dvfs(LE_BCST_FREQ);
#endif
		//wait for past req
		if(broadcast_tws_is_ready_for_past()){
			system_app_set_auracast_mode(3);
			lemusic_bms_source_init();
		}else{
			p_lemusic->bms.wait_for_past_req = 1;
			system_app_set_auracast_mode(3);
		}
	} else {
		system_app_set_auracast_mode(mode);
		system_app_launch_switch(DESKTOP_PLUGIN_ID_LE_MUSIC,DESKTOP_PLUGIN_ID_BMR);
	}
	SYS_LOG_INF("mode:%d\n", mode);
}

void lemusic_bms_player_reset_trigger(void)
{
	struct lemusic_bms_device *bms = &p_lemusic->bms;

	if (p_lemusic != NULL) {
		bms->restart = 1;
		bms->restart_count++;
		SYS_LOG_INF("%d", bms->restart_count);
	}
}

static int _lemusic_init(void *p1, void *p2, void *p3)
{
	if (p_lemusic) {
		return 0;
	}
	int8_t temp_role = 0;

	p_lemusic = app_mem_malloc(sizeof(struct lemusic_app_t));
	if (!p_lemusic) {
		SYS_LOG_ERR("malloc failed!\n");
		return -ENOMEM;
	}

#if 0
	bt_manager_set_user_visual(1,0,0,BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE);
	btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
	btif_br_disconnect_device(BTSRV_DISCONNECT_PHONE_MODE);
#endif

	lemusic_view_init();
	struct lemusic_bms_device *bms = &p_lemusic->bms;

	memset(p_lemusic, 0, sizeof(struct lemusic_app_t));
	bms->qos = &source_qos;
	thread_timer_init(&p_lemusic->switch_bmr_timer, lemusic_switch_bmr_handler, NULL);
	thread_timer_init(&bms->broadcast_start_timer, lemusic_bms_start_handler,
					  NULL);

	if(tts_manager_is_playing()){
		p_lemusic->tts_playing = 1;
	}

	if (lemusic_get_auracast_mode()) {
#ifdef CONFIG_BT_LETWS
		temp_role = bt_manager_letws_get_dev_role();
#endif
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		lemusic_update_dvfs(LE_BCST_FREQ);
#endif
		if(temp_role == BTSRV_TWS_MASTER){
			//wait for past req
			if(broadcast_tws_is_ready_for_past()){
				system_app_set_auracast_mode(3);
				lemusic_bms_source_init();
			}else{
				p_lemusic->bms.wait_for_past_req = 1;
				system_app_set_auracast_mode(3);
			}
		}else if(temp_role == BTSRV_TWS_NONE){
			system_app_set_auracast_mode(1);
			lemusic_bms_source_init();
		}
	}else{
		bt_manager_audio_stream_restore(BT_TYPE_LE);
		//bt_manager_audio_le_resume_scan();
		//bt_manager_tws_sync_enable_lea_dir_adv(true);
		//bt_manager_tws_set_leaudio_active(1);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		lemusic_update_dvfs(LE_FREQ);
#endif
	}

	SYS_LOG_INF("init ok\n");
	return 0;
}

static int  _lemusic_exit(void)
{
	if (!p_lemusic) {
		goto exit;
	}

	if (thread_timer_is_running(&p_lemusic->monitor_timer)) {
		thread_timer_stop(&p_lemusic->monitor_timer);
	}

	if (thread_timer_is_running(&p_lemusic->switch_bmr_timer)) {
		thread_timer_stop(&p_lemusic->switch_bmr_timer);
		lemusic_switch_bmr_handler(NULL, NULL);
	}

#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif

	lemusic_bms_stop_capture();
	lemusic_bms_exit_capture();
	lemusic_bms_source_exit();

	lemusic_stop_playback();
	lemusic_exit_playback();

	lemusic_stop_capture();
	lemusic_exit_capture();

	lemusic_view_deinit();

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	lemusic_clear_current_dvfs();
#endif

	app_mem_free(p_lemusic);

	p_lemusic = NULL;

#ifdef CONFIG_PROPERTY
	//property_flush_req(NULL);
#endif

exit:
	SYS_LOG_INF("exit ok\n");

	return 0;
}

struct lemusic_app_t *lemusic_get_app(void)
{
	return p_lemusic;
}

static int _lemusic_proc_msg(struct app_msg *msg)
{
	SYS_LOG_INF("type %d, value %d\n", msg->type, msg->value);
	switch (msg->type) {
	case MSG_BT_EVENT:
		lemusic_bt_event_proc(msg);
		break;
	case MSG_INPUT_EVENT:
		lemusic_input_event_proc(msg);
		break;
	case MSG_TTS_EVENT:
		lemusic_tts_event_proc(msg);
		break;
	case MSG_LEMUSIC_APP_EVENT:
		lemusic_app_event_proc(msg);
		break;
#ifdef CONFIG_BT_LETWS
	case MSG_TWS_EVENT:
		lemusic_tws_event_proc(msg);
		break;
#endif
	case MSG_EXIT_APP:
		_lemusic_exit();
		break;
	default:
		break;
	}
	return 0;
}

static int _lemusic_dump_app_state(void)
{
	print_buffer_lazy(APP_ID_LEMUSIC, (void *)lemusic_get_app(),
					  sizeof(struct lemusic_app_t));
	return 0;
}

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
static void get_dvfs_info(uint8_t dvfs, uint8_t *dvfs_info)
{
	if (dvfs == LE_FREQ) {
		memcpy(dvfs_info, "lemusic", 7);
	} else if (dvfs == LE_BCST_FREQ_HIGH) {
		memcpy(dvfs_info, "bms_le_H", 8);
	} else {
		memcpy(dvfs_info, "bms_le", 6);
	}
}

void lemusic_update_dvfs(uint8_t dvfs)
{
	if (!dvfs || p_lemusic->set_dvfs_level == dvfs) {
		SYS_LOG_INF("%d\n",dvfs);
		return;
	}

	uint8_t dvfs_info[10] = {0};
	/*set new dvfs*/
	get_dvfs_info(dvfs, dvfs_info);
	soc_dvfs_set_level(dvfs, dvfs_info);

	if (p_lemusic->set_dvfs_level != dvfs
		&& p_lemusic->set_dvfs_level != 0) {
		/*unset current dvfs*/
		get_dvfs_info(p_lemusic->set_dvfs_level, dvfs_info);
		soc_dvfs_unset_level(p_lemusic->set_dvfs_level, dvfs_info);
	}
	p_lemusic->set_dvfs_level = dvfs;
}

void lemusic_clear_current_dvfs(void)
{
	uint8_t dvfs_info[10] = {0};

	if (p_lemusic->set_dvfs_level != 0) {
		get_dvfs_info(p_lemusic->set_dvfs_level, dvfs_info);
		soc_dvfs_unset_level(p_lemusic->set_dvfs_level, dvfs_info);
		p_lemusic->set_dvfs_level = 0;
	}
}
#endif /*CONFIG_SOC_DVFS_DYNAMIC_LEVEL*/

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_LE_MUSIC, _lemusic_init, _lemusic_exit, _lemusic_proc_msg, \
	_lemusic_dump_app_state, NULL, NULL, NULL);

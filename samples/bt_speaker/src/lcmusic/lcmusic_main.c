/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief lcmusic app main.
 */

#include "lcmusic.h"
#include "tts_manager.h"
#include "fs_manager.h"
#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif

static struct lcmusic_app_t *p_local_music = NULL;
static u8_t sd_music_init_state = LCMUSIC_STATUS_PLAYING;
static u8_t uhost_music_init_state = LCMUSIC_STATUS_PLAYING;
static u8_t nor_music_init_state = LCMUSIC_STATUS_PLAYING;

#define LOCAL_NAME	"bis_local"
#define LOCAL_NAME_LEN		(sizeof(LOCAL_NAME) - 1)

#define BROADCAST_NAME	"bis_broadcast"
#define BROADCAST_NAME_LEN		(sizeof(BROADCAST_NAME) - 1)
static uint8_t local_name[33];
static uint8_t broadcast_name[33];

int lcmusic_get_status(void)
{
	if (!p_local_music)
		return LCMUSIC_STATUS_NULL;
	return p_local_music->music_state;
}

#ifdef CONFIG_LCMUSIC_FEATURE_RESTART
void bms_lcmusic_player_reset_trigger(void)
{
	if (p_local_music != NULL) {
		p_local_music->restart = 1;
		p_local_music->restart_count++;
		SYS_LOG_INF("%d\n", p_local_music->restart_count);
	}
}

static void bms_lcmusic_restart_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	struct app_msg msg = { 0 };

	if (p_local_music && p_local_music->restart) {
		SYS_LOG_INF("restart\n");
		msg.type = MSG_LCMUSIC_EVENT;
		msg.cmd = MSG_LCMUSIC_STREAM_RESTART;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		p_local_music->restart = 0;
		p_local_music->restart_count = 0;
	}
}
#endif


#ifdef CONFIG_BMS_LCMUSIC_APP

#define DEFAULT_CONTEXTS (BT_AUDIO_CONTEXT_UNSPECIFIED | BT_AUDIO_CONTEXT_MEDIA)

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

int bms_lcmusic_source_init(void)
{
#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_INFO)
	static int count = 0;
#endif
	struct bt_broadcast_source_create_param param = { 0 };
	struct bt_broadcast_source_big_param big_param;
	struct bt_le_per_adv_param per_adv_param = { 0 };
#if (BROADCAST_NUM_BIS != 2)
	struct bt_broadcast_subgroup_1 subgroup = { 0 };
#else
	struct bt_broadcast_subgroup_2 subgroup = { 0 };
#endif
	// TODO:
	uint16_t appearance = 0x885;	/* Audio Source + Broadcasting Device */
	int ret;

	if (!p_local_music) {
		return -EINVAL;
	}

	p_local_music->bms_source = 1;

	SYS_LOG_INF("%d\n", count++);
	if (thread_timer_is_running(&p_local_music->broadcast_start_timer))
		thread_timer_stop(&p_local_music->broadcast_start_timer);

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

#if (BROADCAST_NUM_BIS == 2)
	subgroup.num_bis = 2;
#else
	subgroup.num_bis = 1;
#endif
	subgroup.format = BT_AUDIO_CODEC_LC3;
	subgroup.frame_duration = BROADCAST_DURATION;
	subgroup.blocks = 1;
	subgroup.sample_rate = 48;
	subgroup.octets = BROADCAST_OCTETS;
#if (BROADCAST_NUM_BIS != 2)
#if (BROADCAST_CH == 2)
	subgroup.locations = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;
#else
	subgroup.locations = BT_AUDIO_LOCATIONS_FL;
#endif
#endif
	subgroup.language = 0;	//BT_LANGUAGE_UNKNOWN
	subgroup.contexts = DEFAULT_CONTEXTS;

#if (BROADCAST_NUM_BIS == 2)
	subgroup.bis[0].locations = BT_AUDIO_LOCATIONS_FL;
	subgroup.bis[1].locations = BT_AUDIO_LOCATIONS_FR;
#endif

#if 0
	bt_manager_stream_enable(STREAM_TYPE_A2DP, true);
	bt_manager_set_stream_type(AUDIO_STREAM_MUSIC);
#endif

	big_param.iso_interval = BROADCAST_ISO_INTERVAL;
	big_param.max_pdu = BROADCAST_PDU;
	big_param.nse = BROADCAST_NSE;
	big_param.bn = BROADCAST_BN;
	big_param.irc = BROADCAST_IRC;
	big_param.pto = BROADCAST_PTO;

	/* 100ms */
	per_adv_param.interval_min = PA_INTERVAL;
	per_adv_param.interval_max = PA_INTERVAL;

#if ENABLE_ENCRYPTION
	param.encryption = true;
	param.broadcast_code[0] = 0x55;
	param.broadcast_code[1] = 0xaa;
	memcpy(p_local_music->broadcast_code, param.broadcast_code, 16);
#endif

	param.broadcast_id = broadcast_get_broadcast_id();
	if (param.broadcast_id == INVALID_BROADCAST_ID) {
		SYS_LOG_ERR("get broadcast failed\n");
		p_local_music->bms_source = 0;
		return -EINVAL;
	}

	param.local_name = local_name;
	param.broadcast_name = broadcast_name;
	param.qos = p_local_music->qos;
	param.appearance = appearance;
	param.num_subgroups = 1;
	param.subgroup = (struct bt_broadcast_subgroup *)&subgroup;
	param.big_param = &big_param;
	param.per_adv_param = &per_adv_param;

	ret = bt_manager_broadcast_source_create(&param);
	if (ret < 0) {
		SYS_LOG_ERR("failed %d\n", ret);
		thread_timer_start(&p_local_music->broadcast_start_timer, 300, 0);
		p_local_music->bms_source = 0;
		return ret;
	}

	p_local_music->broadcast_id = param.broadcast_id;
	return 0;
}

int bms_lcmusic_source_exit(void)
{
	if (thread_timer_is_running(&p_local_music->broadcast_start_timer))
		thread_timer_stop(&p_local_music->broadcast_start_timer);

	bt_manager_broadcast_source_disable(p_local_music->broadcast_id);
	bt_manager_broadcast_source_release(p_local_music->broadcast_id);

	//bt_manager_pawr_adv_stop();
	p_local_music->bms_source = 0;

	return 0;
}

void lcmusic_set_auracast_mode(bool mode)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (NULL == lcmusic) {
		return;
	}

	SYS_LOG_INF("%d->%d\n", lcmusic_is_bms_mode(), mode);
	if (mode == lcmusic_is_bms_mode()) {
		return;
	}

	if (!mode) {
		if (1 == lcmusic->bms_source) {
			bms_lcmusic_source_exit();
		}
		//bt_manager_pawr_adv_stop();
		system_app_set_auracast_mode(0);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (lcmusic->set_dvfs_level) {
			soc_dvfs_unset_level(lcmusic->set_dvfs_level, "bms_lcmusic");
			lcmusic->set_dvfs_level = 0;
		}
#endif
	} else {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		lcmusic->set_dvfs_level = BCST_FREQ;
		soc_dvfs_set_level(lcmusic->set_dvfs_level, "bms_lcmusic");
#endif
		bms_lcmusic_source_init();
		system_app_set_auracast_mode(1);
	}
}

static void bms_lcmusic_start_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	if (NULL == p_local_music) {
		return;
	}
	SYS_LOG_INF("%d\n", system_app_get_auracast_mode());
	if (lcmusic_is_bms_mode()) {
		bms_lcmusic_source_init();
	}
}
#endif

bool lcmusic_is_bms_mode(void)
{
#ifdef CONFIG_BMS_LCMUSIC_APP
	return (1 == system_app_get_auracast_mode())? true : false;
#else
	return false;
#endif
}


u8_t lcmusic_get_play_state(const char *dir)
{
	if (memcmp(dir, "SD:", strlen("SD:")) == 0)
		return sd_music_init_state;
	else if (memcmp(dir, "USB:", strlen("USB:")) == 0)
		return uhost_music_init_state;
	else if (memcmp(dir, "NOR:", strlen("NOR:")) == 0)
		return nor_music_init_state;
	else {
		SYS_LOG_INF("dir :%s error\n", dir);
		return LCMUSIC_STATUS_PLAYING;
	}
}
void lcmusic_store_play_state(const char *dir)
{
	if (memcmp(dir, "SD:", strlen("SD:")) == 0)
		sd_music_init_state = p_local_music->music_state;
	else if (memcmp(dir, "USB:", strlen("USB:")) == 0)
		uhost_music_init_state = p_local_music->music_state;
	else if (memcmp(dir, "NOR:", strlen("NOR:")) == 0)
		nor_music_init_state = p_local_music->music_state;
	else {
		SYS_LOG_INF("dir :%s error\n", dir);
	}
#ifdef CONFIG_ESD_MANAGER
	u8_t playing = p_local_music->music_state;

	esd_manager_save_scene(TAG_PLAY_STATE, &playing, 1);
#endif

}

void _lcmusic_restore_play_state(void)
{
	p_local_music->is_init = 1;

    lcmusic_scan_disk();

	thread_timer_start(&p_local_music->monitor_timer, 2000, 0);
}

static int _lcmusic_init(const char *dir)
{
	if (p_local_music)
		return 0;

	p_local_music = app_mem_malloc(sizeof(struct lcmusic_app_t));
	if (!p_local_music) {
		SYS_LOG_ERR("malloc failed!\n");
		return -ENOMEM;
	}

	memset(p_local_music, 0, sizeof(struct lcmusic_app_t));

    /* clean tts list, fix tts play when capture init ,becase tts may come twice*/
#ifdef CONFIG_PLAYTTS
    tts_manager_wait_finished(true);
#endif

	bt_manager_set_stream_type(AUDIO_STREAM_LOCAL_MUSIC);

	snprintf(p_local_music->cur_dir, sizeof(p_local_music->cur_dir), "%s", dir);

	/* resume track_no */
	_lcmusic_bpinfo_resume(p_local_music->cur_dir, &p_local_music->music_bp_info);

	lcmusic_view_init(p_local_music);

#ifdef CONFIG_LCMUSIC_FEATURE_RESTART
	thread_timer_init(&p_local_music->restart_timer, bms_lcmusic_restart_handler, NULL);
	thread_timer_start(&p_local_music->restart_timer, 200, 200);
#endif

#ifdef CONFIG_APP_TWS_SNOOP
	if (app_tws_get_tws_status()) {
		app_tws_on_source_switch(false);
	}
#endif

#ifdef CONFIG_BMS_LCMUSIC_APP
    p_local_music->qos = &source_qos;
    thread_timer_init(&p_local_music->broadcast_start_timer, bms_lcmusic_start_handler, NULL);

    if (lcmusic_is_bms_mode()) {
        system_app_set_auracast_mode(1);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
        p_local_music->set_dvfs_level = BCST_FREQ;
        soc_dvfs_set_level(p_local_music->set_dvfs_level, "bms_lcmusic");
#endif
        bms_lcmusic_source_init();
    }
#endif

	lcmusic_thread_timer_init(p_local_music);

	SYS_LOG_INF("init ok\n");
	return 0;
}

static int _lcmusic_exit(void)
{
	if (!p_local_music)
		goto exit;

#ifdef CONFIG_LCMUSIC_FEATURE_RESTART
    if (thread_timer_is_running(&p_local_music->restart_timer)) {
        thread_timer_stop(&p_local_music->restart_timer);
    }
#endif
    if (thread_timer_is_running(&p_local_music->tts_resume_timer)) {
        thread_timer_stop(&p_local_music->tts_resume_timer);
    }

	lcmusic_thread_timer_deinit(p_local_music);

	lcmusic_bp_info_save(p_local_music);

#ifdef CONFIG_BMS_LCMUSIC_APP
        if (thread_timer_is_running(&p_local_music->broadcast_start_timer))
            thread_timer_stop(&p_local_music->broadcast_start_timer);

#if ENABLE_PADV_APP
        padv_tx_deinit();
#endif
#endif

	lcmusic_stop_play(p_local_music, false);

#ifdef CONFIG_BMS_LCMUSIC_APP
        bms_lcmusic_stop_capture();
        bms_lcmusic_exit_capture();
        bms_lcmusic_source_exit();
#endif

	lcmusic_exit_iterator();

	lcmusic_view_deinit();

	app_mem_free(p_local_music);

	p_local_music = NULL;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif

exit:

	SYS_LOG_INF("exit ok\n");

	return 0;
}


static int _lcmplayer_proc_msg(struct app_msg *msg)
{
	int ret = 0;

	SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg->type, msg->cmd, msg->value);
	switch (msg->type) {
	case MSG_INPUT_EVENT:
		lcmusic_input_event_proc(msg);
		break;
	case MSG_TTS_EVENT:
		lcmusic_tts_event_proc(msg);
		break;
	case MSG_LCMUSIC_EVENT:
		lcmusic_event_proc(msg);
		break;
#ifdef CONFIG_BMS_LCMUSIC_APP
	case MSG_BT_EVENT:
		bms_lcmusic_bt_event_proc(msg);
		break;
#endif
	case MSG_EXIT_APP:
		_lcmusic_exit();
		break;
	default:
		ret = -1;
	}

	return ret;
}

static void _lcmusic_main_prc(const char *dir)
{

	if (_lcmusic_init(dir)) {
		SYS_LOG_ERR("init failed\n");
		_lcmusic_exit();
		return;
	}
	_lcmusic_restore_play_state();
}

struct lcmusic_app_t *lcmusic_get_app(void)
{
	return p_local_music;
}

static int _sd_mplayer_init(void *p1, void *p2, void *p3)
{
	SYS_LOG_INF("sd Enter\n");

#ifdef CONFIG_FS_MANAGER
	fs_manager_sdcard_enter_high_speed();
#endif

	sys_event_notify(SYS_EVENT_ENTER_SDMPLAYER);

	_lcmusic_main_prc("SD:");

#ifdef CONFIG_FS_MANAGER
	fs_manager_sdcard_exit_high_speed();
#endif

	SYS_LOG_INF("sd Exit\n");
	return 0;
}

static int sd_mplayer_dump_app_data(void)
{
	print_buffer_lazy(APP_ID_SD_MPLAYER, (void *)p_local_music,
			  sizeof(struct lcmusic_app_t));
	return 0;
}


DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_SDCARD_PLAYER, _sd_mplayer_init,
        _lcmusic_exit, _lcmplayer_proc_msg, sd_mplayer_dump_app_data, &sd_music_init_state, NULL, NULL);



static int _usb_mplayer_init(void *p1, void *p2, void *p3)
{
	SYS_LOG_INF("uhost Enter\n");

	sys_event_notify(SYS_EVENT_ENTER_UMPLAYER);

	_lcmusic_main_prc("USB:");

	return 0;
}

static int usb_mplayer_dump_app_data(void)
{
	print_buffer_lazy(APP_ID_UHOST_MPLAYER, (void *)p_local_music,
			  sizeof(struct lcmusic_app_t));
	return 0;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_USB_PLAYER, _usb_mplayer_init, _lcmusic_exit,
	_lcmplayer_proc_msg, usb_mplayer_dump_app_data, &uhost_music_init_state, NULL, NULL);


#ifdef CONFIG_DISK_ACCESS_NOR
static int _nor_mplayer_init(void *p1, void *p2, void *p3)
{
	SYS_LOG_INF("nor Enter\n");

	sys_event_notify(SYS_EVENT_ENTER_NORMPLAYER);

	_lcmusic_main_prc("NOR:");

	SYS_LOG_INF("nor Exit\n");
	return 0;
}

int nor_mplayer_dump_app_data(void)
{
	print_buffer_lazy(APP_ID_NOR_MPLAYER, (void *)p_local_music,
			  sizeof(struct lcmusic_app_t));
	return 0;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_NOR_PLAYER, _nor_mplayer_init, _lcmusic_exit,\
		      _lcmplayer_proc_msg, nor_mplayer_dump_app_data, &nor_music_init_state, NULL, NULL);

#endif


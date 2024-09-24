/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief usound app main.
 */

#include "usound.h"
#include <app_launch.h>

#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif
#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

extern void *bt_manager_get_halt_phone(uint8_t *halt_cnt);

static struct usound_app_t *p_usound = NULL;

struct usound_app_t *usound_get_app(void)
{
	return p_usound;
}


int usound_get_status(void)
{
	if (p_usound == NULL) {
		return USOUND_STATUS_NULL;
	}
	else if (p_usound->playing) {
		return USOUND_STATUS_PLAYING;
	}
	else {
		return USOUND_STATUS_PAUSED;
	}
}

#ifdef USOUND_FEATURE_RESTART
void usound_restart_player_trigger(void)
{
	if (p_usound) {
		p_usound->restart_count++;
		SYS_LOG_INF("trigger %d", p_usound->restart_count);
	}
}

static void usound_restart_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	struct app_msg msg = { 0 };

	if (p_usound && p_usound->restart_count > 0) {
		SYS_LOG_INF("count %d", p_usound->restart_count);
		msg.type = MSG_USOUND_APP_EVENT;
		msg.cmd = MSG_USOUND_STREAM_RESTART;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		p_usound->restart_count = 0;
	}
}
#endif // USOUND_FEATURE_RESTART


static void usound_delay_resume(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	SYS_LOG_INF("playing %d", p_usound->playing);
	if (!p_usound->playing) {
		//usb_hid_control_pause_play();
	}
	usound_playback_start();
}

static void usound_restore_play_state(void)
{
	u8_t init_play_state = USOUND_STATUS_PLAYING;
#ifdef CONFIG_ESD_MANAGER
	if (esd_manager_check_esd()) {
		esd_manager_restore_scene(TAG_PLAY_STATE, &init_play_state, 1);
	}
#endif

	if (init_play_state == USOUND_STATUS_PLAYING) {
		if (thread_timer_is_running(&p_usound->monitor_timer)) {
			thread_timer_stop(&p_usound->monitor_timer);
		}
		thread_timer_init(&p_usound->monitor_timer, usound_delay_resume, NULL);
		thread_timer_start(&p_usound->monitor_timer, 2000, 0);
		usound_show_play_status(true);
	} else {
		usound_show_play_status(false);
	}

	SYS_LOG_INF("%d", init_play_state);
}

#if 0
void usound_sync_tws_vol(u8_t level)
{
	struct app_msg msg = { 0 };

	msg.type = MSG_USOUND_APP_EVENT;
	msg.cmd = MSG_USOUND_VOL_UPDATE;
	msg.value = level;

	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}
#endif

static void usound_sync_host_vol(int vol_db)
{
	u8_t level, do_update = 1;
	if (p_usound == NULL) {
		return;
	}

	level = (u8_t) audio_policy_get_volume_level_by_db(AUDIO_STREAM_USOUND, vol_db);
	p_usound->current_volume_level = level;
	SYS_LOG_INF("%ddB -> level %d, require %d_%d", vol_db, level, p_usound->volume_req_type, p_usound->volume_req_level);

	usound_view_volume_show(level);

	switch (p_usound->volume_req_type) {
	case USOUND_VOLUME_NONE: {
		break;
	}
	case USOUND_VOLUME_INC: {
		if (p_usound->volume_req_level > level) {
			usb_hid_control_volume_inc();
			do_update = 0;
		} else {
			p_usound->volume_req_type = USOUND_VOLUME_NONE;
		}
		break;
	}
	case USOUND_VOLUME_DEC: {
		if (p_usound->volume_req_level < level) {
			usb_hid_control_volume_dec();
			do_update = 0;
		} else {
			p_usound->volume_req_type = USOUND_VOLUME_NONE;
		}
		break;
	}
	default: {
		SYS_LOG_WRN("type %d wrong", p_usound->volume_req_type);
		break;
	}
	}

	if (do_update) {
		system_volume_set(AUDIO_STREAM_USOUND, level, false);
#ifdef CONFIG_USOUND_BROADCAST_SUPPROT
#if ENABLE_PADV_APP
		padv_tx_data(padv_volume_map(level, 1));
#endif
#endif
	}
}

static void usound_usb_audio_event_callback(u8_t info_type, int pstore_info)
{
	struct app_msg msg = { 0 };
	SYS_LOG_INF("%d: %d", info_type, pstore_info);

	switch (info_type) {
	case USOUND_SYNC_HOST_MUTE: {  // 8
		audio_system_set_stream_mute(AUDIO_STREAM_USOUND, 1);
#ifdef CONFIG_TWS
		bt_manager_tws_sync_volume_to_slave(NULL, AUDIO_STREAM_USOUND, 0);
#endif
		break;
	}
	case USOUND_SYNC_HOST_UNMUTE: {  // 9
		audio_system_set_stream_mute(AUDIO_STREAM_USOUND, 0);
#ifdef CONFIG_TWS
		bt_manager_tws_sync_volume_to_slave(NULL, AUDIO_STREAM_USOUND, audio_system_get_stream_volume(AUDIO_STREAM_USOUND));
#endif
		break;
	}
	case USOUND_SYNC_HOST_VOL_TYPE: {  // 0
		usound_sync_host_vol(pstore_info);
		break;
	}

	case UMIC_SYNC_HOST_VOL_TYPE: {  // 15
		audio_system_set_microphone_volume(AUDIO_STREAM_USOUND, pstore_info);
		break;
	}
	case UMIC_SYNC_HOST_MUTE: {    // 16
		audio_system_mute_microphone(1);
		break;
	}
	case UMIC_SYNC_HOST_UNMUTE: {  // 17
		audio_system_mute_microphone(0);
		break;
	}

	case USOUND_STREAM_STOP: {   // 5
		msg.type = MSG_USOUND_APP_EVENT;
		msg.cmd = MSG_USOUND_STREAM_STOP;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		break;
	}
	case USOUND_STREAM_START: {  // 4
		msg.type = MSG_USOUND_APP_EVENT;
		msg.cmd = MSG_USOUND_STREAM_START;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		break;
	}

	case USOUND_OPEN_UPLOAD_CHANNEL: {  // 2
		msg.type = MSG_USOUND_APP_EVENT;
		msg.cmd = MSG_USOUND_MIC_OPEN;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		break;
	}
	case USOUND_CLOSE_UPLOAD_CHANNEL: {  // 3
		msg.type = MSG_USOUND_APP_EVENT;
		msg.cmd = MSG_USOUND_MIC_CLOSE;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		break;
	}
	case USOUND_SAMPLERATE_CHANGE: {  //14
		usound_restart_player_trigger();
		break;

	}
	default:
		break;
	}
}

static int usound_init(void *p1, void *p2, void *p3)
{
	if (p_usound) {
		return 0;
	}

	p_usound = app_mem_malloc(sizeof(struct usound_app_t));
	if (!p_usound) {
		SYS_LOG_ERR("malloc failed!");
		return -ENOMEM;
	}
	memset(p_usound, 0, sizeof(struct usound_app_t));

#ifdef USOUND_FEATURE_DISABLE_BLUETOOTH
	// not-discoverable, not connnectable, disconnect phone/tws
	bt_manager_halt_phone();
	//bt_manager_disconnect_all_device();
#endif
#ifdef CONFIG_BT_SELFAPP_ADV
	selfapp_set_system_status(SELF_SYS_UAC);
	selfapp_set_allowed_join_party(false);
#endif

#ifndef CONFIG_USOUND_BROADCAST_SUPPROT
	if (0 != system_app_get_auracast_mode()) {
		system_app_set_auracast_mode(0);
	}
#endif

	usound_view_init();

	usb_audio_init(usound_usb_audio_event_callback);
	p_usound->current_volume_level = audio_system_get_stream_volume(AUDIO_STREAM_USOUND);
	
#ifdef USOUND_FEATURE_RESTART
	thread_timer_init(&p_usound->restart_timer, usound_restart_handler, NULL);
	thread_timer_start(&p_usound->restart_timer, 200, 200);
#endif
#ifdef CONFIG_USOUND_BROADCAST_SUPPROT
	usound_broadcast_init();
#endif

	usound_restore_play_state();
	SYS_LOG_INF("init ok");
	return 0;
}

static int usound_exit(void)
{
	if (!p_usound) {
		return -1;
	}

	if (thread_timer_is_running(&p_usound->monitor_timer)) {
		thread_timer_stop(&p_usound->monitor_timer);
	}
#ifdef USOUND_FEATURE_RESTART
	if (thread_timer_is_running(&p_usound->restart_timer)) {
		thread_timer_stop(&p_usound->restart_timer);
	}
#endif
#ifdef CONFIG_USOUND_BROADCAST_SUPPROT
	if (thread_timer_is_running(&p_usound->broadcast_start_timer))
		thread_timer_stop(&p_usound->broadcast_start_timer);

#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif
#endif

	usound_playback_stop();

#ifdef CONFIG_USOUND_BROADCAST_SUPPROT
	bms_uac_stop_capture();
	bms_uac_exit_capture();
	bms_uac_source_exit();
#endif

	if (p_usound->playing) {
		usb_hid_control_pause_play();
	}
	usb_audio_deinit();

	usound_view_deinit();

#ifdef CONFIG_BT_SELFAPP_ADV
	selfapp_set_system_status(SELF_SYS_RUNNING);
	selfapp_set_allowed_join_party(true);
#endif

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if (p_usound->set_dvfs_level) {
		soc_dvfs_unset_level(p_usound->set_dvfs_level, "bms_uac");
		p_usound->set_dvfs_level = 0;
	}
#endif

	app_mem_free(p_usound);
	p_usound = NULL;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif
	SYS_LOG_INF("exit ok");

#ifdef USOUND_FEATURE_DISABLE_BLUETOOTH
	u8_t  retval = 0;
	bt_manager_get_halt_phone(&retval);
	bt_manager_resume_phone();  // try re-connect halted phone, do nothing if retval=0

	// retval=0: abort re-connect before phone conc succ, because entering usound. But there are phones to re-connect
	if (retval == 0) {
		retval = bt_manager_manual_reconnect();
	}
	// enter pair-mode for 3mins if havn't connected phone when enter usound
	if (retval == 0) {
		bt_manager_start_pair_mode();  // default 3mins already
	}
#endif

	/* fix bug: Auto switch to usound-app when BT stream-suspend, by liushuihua 20240809
	 * According to Spec, short-press BT key should exit usound and switch back to btmusic, with USB plug-in.
	 * In this case, BT stream-start and suspend, Our code woule switch to LAST app -- usound.
	 * But it's not mentioned by Harman USB-Audio Spec. So here delete desktop-plugin to avoid auto switch.
	 * print log could be found: [desktop] [ERR] desktop_manager_switch: plugin 30 no Exist.
	**/
	desktop_manager_del(DESKTOP_PLUGIN_ID_UAC);

	return 0;
}

static int usound_proc_msg(struct app_msg *msg)
{
	int ret = 0;

	SYS_LOG_INF("type %d, cmd %d, value 0x%x", msg->type, msg->cmd, msg->value);
	switch (msg->type) {
	case MSG_INPUT_EVENT:
		usound_input_event_proc(msg);
		break;
	case MSG_TTS_EVENT:
		usound_tts_event_proc(msg);
		break;
	case MSG_USOUND_APP_EVENT:
		usound_app_event_proc(msg);
		break;
#ifdef CONFIG_USOUND_BROADCAST_SUPPROT
	case MSG_BT_EVENT:
		bms_uac_bt_event_proc(msg);
		break;
#endif
	case MSG_EXIT_APP:
		usound_exit();
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

int usound_dump_app_data(void)
{
	print_buffer_lazy(APP_ID_USOUND, (void *)p_usound, sizeof(struct usound_app_t));
	return 0;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_UAC, usound_init, usound_exit,
		      usound_proc_msg, usound_dump_app_data, NULL, NULL, NULL);

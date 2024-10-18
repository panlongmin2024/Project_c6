/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief usound event
 */

#include "usound.h"

#include <usb/class/usb_audio.h>
#include <app_launch.h>

#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif
#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif


static void _usound_tts_delay_resume(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	struct usound_app_t *usound = usound_get_app();
	usound->need_resume_play = 0;
	usound_play_resume();
}

static int usound_handle_start(void)
{
	struct usound_app_t *usound = usound_get_app();
	if (usound == NULL || usound->playing) {
		return 0;
	}
	SYS_LOG_INF("");

	usound_play_resume();
	usound_show_play_status(usound->playing);

#ifdef CONFIG_ESD_MANAGER
	{
		u8_t playing = usound->playing;
		esd_manager_save_scene(TAG_PLAY_STATE, &playing, 1);
	}
#endif

	return 0;
}

static int usound_handle_stop(void)
{
	struct usound_app_t *usound = usound_get_app();
	if (usound == NULL || usound->playing == 0) {
		return 0;
	}
	SYS_LOG_INF();

	usound_play_pause();
	usound_show_play_status(usound->playing);

#ifdef CONFIG_ESD_MANAGER
	{
		u8_t playing = usound->playing;
		esd_manager_save_scene(TAG_PLAY_STATE, &playing, 1);
	}
#endif

	return 0;
}

#ifdef USOUND_FEATURE_RESTART
static int usound_handle_restart(void)
{
	struct usound_app_t *usound = usound_get_app();
	if (usound == NULL || usound->restart_count == 0) {
		return 0;
	}
	SYS_LOG_INF("%d", usound->restart_count);

	usound_play_pause();

	// clear restart flag to avoid restart handler to do dumplicated handling.
	usound->restart_count = 0;

	usound_play_resume();
	return 0;
}
#endif

static void usound_vol_request_host(bool inc, u8_t level)
{
	struct usound_app_t *usound = usound_get_app();
	if (usound == NULL) {
		return;
	}
	SYS_LOG_INF("%d, %d", inc, level);

	usound->volume_req_level = level;
	if(inc) {
		usound->volume_req_type = USOUND_VOLUME_INC;
		usb_hid_control_volume_inc();
	} else {
		usound->volume_req_type = USOUND_VOLUME_DEC;
		usb_hid_control_volume_dec();
	}
}

static void usound_vol_step_adjust(bool inc)
{
	struct usound_app_t *usound = usound_get_app();
	u8_t level;
	if (usound == NULL) {
		return;
	}

	level = usound->current_volume_level;
	SYS_LOG_INF("%d, %d", inc, level);
	if(inc) {
		if (level < audio_policy_get_volume_level()) {
			level += 1;
		}
	} else {
		if (level >= 1) {
			level -= 1;
		}
	}

	usound_vol_request_host(inc, level);
}

static void usound_vol_update(u8_t level)
{
	struct usound_app_t *usound = usound_get_app();
	u8_t cl;
	if (usound == NULL) {
		return;
	}

	if (level > audio_policy_get_volume_level()) {
		level = audio_policy_get_volume_level();
	}
	cl = usound->current_volume_level;
	SYS_LOG_INF("%d->%d", cl, level);

	if(level > cl) {
		usound_vol_request_host(true, level);
	}
	else if(level < cl) {
		usound_vol_request_host(false, level);
	}
}

void usound_input_event_proc(struct app_msg *msg)
{
	struct usound_app_t *usound = usound_get_app();
	if (usound == NULL) {
		return;
	}
	SYS_LOG_INF("cmd %d", msg->cmd);

	switch (msg->cmd) {
	case MSG_USOUND_EXIT_TO_BT: {
		struct app_msg  exitmsg = {0};
		exitmsg.type  = MSG_SWITCH_APP;
		exitmsg.cmd   = DESKTOP_SWITCH_CURR;
		exitmsg.value = DESKTOP_PLUGIN_ID_BR_MUSIC;
		send_async_msg(CONFIG_FRONT_APP_NAME, &exitmsg);
		break;
	}
	case MSG_USOUND_PLAY_PAUSE_RESUME: {
		if (usound->playing) {
			usb_hid_control_pause_play();
			sys_event_notify(SYS_EVENT_PLAY_PAUSE);
		} else {
			sys_event_notify(SYS_EVENT_PLAY_START);
			usb_hid_control_pause_play();
		}
		break;
	}
	case MSG_USOUND_PLAY_VOLUP: {
		usound_vol_step_adjust(true);
		break;
	}
	case MSG_USOUND_PLAY_VOLDOWN: {
		usound_vol_step_adjust(false);
		break;
	}
	case MSG_USOUND_PLAY_NEXT: {
		sys_event_notify(SYS_EVENT_PLAY_NEXT);
		usb_hid_control_play_next();
		break;
	}
	case MSG_USOUND_PLAY_PREV: {
		sys_event_notify(SYS_EVENT_PLAY_PREVIOUS);
		usb_hid_control_play_prev();
		break;
	}
#ifdef CONFIG_USOUND_BROADCAST_SUPPROT
	case MSG_USOUND_SWITCH_BROADCAST: {
		usound_switch_auracast();
		break;
	}
	case MSG_AURACAST_ENTER: {
		if (system_app_get_auracast_mode() == 0) {
			usound_switch_auracast();
		}
		else if(system_app_get_auracast_mode() == 1) {
			usound_handle_restart();
		}
		break;
	}
	case MSG_AURACAST_EXIT: {
		if(system_app_get_auracast_mode() != 0){
			usound_switch_auracast();
		}
		break;
	}
#endif
	default:
		break;
	}
}

void usound_app_event_proc(struct app_msg *msg)
{
	struct usound_app_t *usound = usound_get_app();
	if (usound == NULL) {
		return;
	}
	SYS_LOG_INF("cmd %d", msg->cmd);

	switch (msg->cmd) {
	case MSG_USOUND_STREAM_START: {
		usound_handle_start();
		break;
	}
	case MSG_USOUND_STREAM_STOP: {
		usound_handle_stop();
		break;
	}
#ifdef USOUND_FEATURE_RESTART
	case MSG_USOUND_STREAM_RESTART: {
		usound_handle_restart();
		break;
	}
#endif
	case MSG_USOUND_VOL_UPDATE: {
		usound_vol_update(msg->value);
		break;
	}

	case MSG_USOUND_RE_ENUME: {
		usound_usb_audio_re_enum();
	}

	default:
		break;
	}
}

void usound_tts_event_proc(struct app_msg *msg)
{
	struct usound_app_t *usound = usound_get_app();
	if (usound == NULL) {
		return;
	}
	SYS_LOG_INF("%d", msg->value);

	switch (msg->value) {
	case TTS_EVENT_START_PLAY: {
		usound_play_pause();
		usound->need_resume_play = 1;
		break;
	}
	case TTS_EVENT_STOP_PLAY: {
		if (usound->need_resume_play) {
			if (thread_timer_is_running(&usound->monitor_timer)) {
				thread_timer_stop(&usound->monitor_timer);
			}
			thread_timer_init(&usound->monitor_timer, _usound_tts_delay_resume, NULL);
			thread_timer_start(&usound->monitor_timer, 200, 0);
		}
		break;
	}
	default:
		break;
	}
}

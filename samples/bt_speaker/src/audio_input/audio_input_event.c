/*
 * Copyright (c) 2022 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio_input event
 */

#include "audio_input.h"
#include <app_launch.h>
#include <tts_manager.h>

#ifdef CONFIG_ESD_MANAGER
#include <esd_manager.h>
#endif

void audio_input_play_pause(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();
	if (NULL == audio_input) {
		return;
	}

	SYS_LOG_INF("");

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	SYS_LOG_INF("mode %d, source %d", audio_input_is_bms_mode(),
		    audio_input->broadcast_source_enabled);
	if (audio_input_is_bms_mode()) {
		if (audio_input->capture_player_run) {
			audio_input_stop_capture();
			audio_input_exit_capture();
		}
		audio_input->tx_start = 0;
		audio_input->tx_sync_start = 0;
	}
#endif

	if (audio_input->playback_player_run) {
		audio_input_stop_playback();
		audio_input_exit_playback();
	}

}

void audio_input_play_resume(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();
	if (NULL == audio_input) {
		return;
	}

	SYS_LOG_INF("audio_input_play_resume:%d\n", audio_input->playback_player_run);

	if (!audio_input->playback_player_run) {
		audio_input_init_playback();
		audio_input_start_playback();
	}
#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	SYS_LOG_INF("mode %d, source %d", audio_input_is_bms_mode(),
		    audio_input->broadcast_source_enabled);
	if (audio_input_is_bms_mode()) {
		if (audio_input->broadcast_source_enabled) {
			if (!audio_input->capture_player_run) {
				audio_input_init_capture();
				audio_input_start_capture();
			}
		}
	}
#endif
}

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
/* find the existed broad chan */
static struct bt_broadcast_chan *find_broad_chan(uint32_t handle, uint8_t id)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	if (NULL == audio_input) {
		return NULL;
	}

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &audio_input->broad_chan[i];
		if (chan->handle == handle && chan->id == id) {
			return chan;
		}
	}
	return NULL;
}

/* find a new broad chan */
static struct bt_broadcast_chan *new_broad_chan(uint32_t handle, uint8_t id)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	if (NULL == audio_input) {
		return NULL;
	}

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &audio_input->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			return chan;
		}
	}
	return NULL;
}

static void audio_input_bms_tx_start(uint32_t handle, uint8_t id,
				 uint16_t audio_handle)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (NULL == audio_input) {
		return;
	}

	if (audio_input->tx_start) {
		return;
	}

	if (audio_input->broadcast_source_enabled && audio_input->capture_player_run) {
		media_player_trigger_audio_record_start(audio_input->capture_player);
		audio_input->tx_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger record\n");
	}
}

static void audio_input_bms_tx_sync_start(uint32_t handle, uint8_t id,
				      uint16_t audio_handle)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (NULL == audio_input) {
		return;
	}

	if (audio_input->tx_sync_start) {
		return;
	}

	if (audio_input->broadcast_source_enabled
	    && audio_input->playback_player_run) {
		media_player_trigger_audio_track_start
		    (audio_input->playback_player);
		audio_input->tx_sync_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger track\n");
	}
}

static int audio_input_bms_handle_source_config(struct bt_braodcast_configured *rep)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();
	struct bt_broadcast_chan *chan;
	uint8_t vnd_buf[20] = { 0 };
	uint8_t type = 0;

	if (NULL == audio_input) {
		return -1;
	}

	SYS_LOG_INF("h:0x%x id:%d\n", rep->handle, rep->id);

	chan = find_broad_chan(rep->handle, rep->id);
	if (!chan) {
		chan = new_broad_chan(rep->handle, rep->id);
		if (!chan) {
			SYS_LOG_ERR("no broad chan handle: %d, id: %d\n",
				    rep->handle, rep->id);
			return -EINVAL;
		}
		audio_input->num_of_broad_chan++;
	}

	chan->handle = rep->handle;
	chan->id = rep->id;

	vnd_buf[0] = VS_ID & 0xFF;
	vnd_buf[1] = VS_ID >> 8;
	memcpy(&vnd_buf[2], audio_input->broadcast_code, 16);
	vnd_buf[18] = SERIVCE_UUID & 0xFF;
	vnd_buf[19] = SERIVCE_UUID >> 8;

#if !VS_COMPANY_ID
	type = BT_DATA_SVC_DATA16;
#endif

	if (audio_input->num_of_broad_chan == 1) {
		bt_manager_broadcast_stream_cb_register(chan,
							audio_input_bms_tx_start,
							NULL);
		bt_manager_broadcast_stream_tws_sync_cb_register(chan,
								 audio_input_bms_tx_sync_start);
		bt_manager_broadcast_stream_set_tws_sync_cb_offset(chan,
								   broadcast_get_tws_sync_offset
								   (audio_input->qos));
		bt_manager_broadcast_source_vnd_ext_send(chan->handle, vnd_buf,
							 sizeof(vnd_buf), type);
		audio_input->chan = chan;
		audio_input_init_playback();
		audio_input_start_playback();
	}

	if (audio_input->num_of_broad_chan >= 2) {
		bt_manager_broadcast_source_enable(audio_input->chan->handle);
	}

	return 0;
}

static int audio_input_bms_handle_source_enable(struct bt_broadcast_report *rep)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (NULL == audio_input) {
		return -1;
	}

	SYS_LOG_INF("handle 0x%x, id %d\n", rep->handle, rep->id);
	SYS_LOG_INF("resume %d, chan %d\n", audio_input->need_resume_play,
		    audio_input->num_of_broad_chan);
	if (2 > rep->id) {
		SYS_LOG_INF("wait all done.");
		return 0;
	}

	audio_input->broadcast_source_enabled = 1;
#if ENABLE_PADV_APP
	if (audio_input->chan != NULL) {
		padv_tx_init(audio_input->chan->handle, audio_input_get_audio_stream_type());
	}
#endif
	if (audio_input->need_resume_play) {
		SYS_LOG_INF("wait tts.");
		return 0;
	}

	audio_input_init_capture();
	audio_input_start_capture();

	return 0;
}

static int audio_input_bms_handle_source_disable(struct bt_broadcast_report *rep)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (NULL == audio_input) {
		return -1;
	}

#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif

	audio_input_stop_capture();
	audio_input_exit_capture();

	audio_input->tx_start = 0;
	audio_input->tx_sync_start = 0;

	SYS_LOG_INF("handle 0x%x, id %d\n", rep->handle, rep->id);
	struct bt_broadcast_chan * chan = find_broad_chan(rep->handle, rep->id);
	if (chan) {
		if (audio_input->num_of_broad_chan) {
			audio_input->num_of_broad_chan--;
			chan->handle = 0;
			chan->id = 0;
			if (chan == audio_input->chan) {
				audio_input->chan = NULL;
			}
			if (!audio_input->num_of_broad_chan) {
				audio_input->broadcast_source_enabled = 0;
				SYS_LOG_INF("all disabled\n");
			}
		} else {
			SYS_LOG_WRN("should not be here\n");
		}
	}

	return 0;
}

static int audio_input_bms_handle_source_release(struct bt_broadcast_report *rep)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (NULL == audio_input) {
		return -1;
	}

	SYS_LOG_INF("handle 0x%x, id %d\n", rep->handle, rep->id);
	audio_input->chan = NULL;
	audio_input->broadcast_source_enabled = 0;
	audio_input->num_of_broad_chan = 0;
	memset(audio_input->broad_chan,0,sizeof(audio_input->broad_chan));

	return 0;
}

void audio_input_bt_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd=%d\n", msg->cmd);

	switch (msg->cmd) {
	case BT_REQ_RESTART_PLAY:
		SYS_LOG_WRN("skip BT_REQ_RESTART_PLAY\n");
		break;

	case BT_BROADCAST_SOURCE_CONFIG:
		audio_input_bms_handle_source_config(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_ENABLE:
		audio_input_bms_handle_source_enable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_DISABLE:
		audio_input_bms_handle_source_disable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_RELEASE:
		audio_input_bms_handle_source_release(msg->ptr);
		break;

#ifdef ENABLE_PAWR_APP
	case BT_PAWR_SYNCED:
		SYS_LOG_INF("pawr synced");
		break;
	case BT_PAWR_SYNC_LOST:
		SYS_LOG_INF("pawr sync lost");
		break;
#endif

	case BT_BIS_CONNECTED:
		SYS_LOG_INF("bis connected");
		break;
	case BT_BIS_DISCONNECTED:
		SYS_LOG_INF("bis disconnected");
		break;
	default:
		break;
	}
}
#endif

void audio_input_tts_event_proc(struct app_msg *msg)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (!audio_input)
		return;

	SYS_LOG_INF("value=%d, %d\n", msg->value, audio_input->need_resume_play);

	switch (msg->value) {
	case TTS_EVENT_START_PLAY:
		audio_input_play_pause();
		audio_input->need_resume_play = 1;
		break;
	case TTS_EVENT_STOP_PLAY:
		if (audio_input->need_resume_play) {
			if (thread_timer_is_running(&audio_input->resume_timer)) {
				thread_timer_stop(&audio_input->resume_timer);
			}
			thread_timer_start(&audio_input->resume_timer, 2000, 0);
		}
		break;
	default:
		break;
	}
}

static int audio_input_handle_client_volume(bool up)
{
	int ret;
	u8_t max;
	u8_t vol;
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (NULL == audio_input) {
		return -1;
	}

	ret = system_volume_get(audio_input_get_audio_stream_type());
	if (ret < 0) {
		SYS_LOG_ERR("%d\n", ret);
		return -EINVAL;
	} else {
		vol = ret;
	}

	max = audio_policy_get_volume_level();

	if (up) {
		vol += 1;
		if (vol > max) {
			vol = max;
		}
	} else {
		if (vol >= 1) {
			vol -= 1;
		}
	}

	SYS_LOG_INF("vol=%d/%d\n", vol, max);
	system_volume_set(audio_input_get_audio_stream_type(), vol, true);
#ifdef CONFIG_AUDIO_INPUT_BMS_APP
#if ENABLE_PADV_APP
	if(audio_input_is_bms_mode()) {
		padv_tx_data(padv_volume_map(vol,1));
	}
#endif
#endif
	return 0;
}


#ifdef CONFIG_AUDIO_INPUT_BMS_APP
static void audio_input_switch_auracast(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (audio_input_is_bms_mode()) {
		audio_input_stop_playback();
		audio_input_exit_playback();

		audio_input_stop_capture();
		audio_input_exit_capture();

		audio_input->tx_start = 0;
		audio_input->tx_sync_start = 0;

		audio_input_set_auracast_mode(false);
		//usound with be started after tts restart.
	} else {
		if (bt_manager_audio_get_leaudio_dev_num()) {
			SYS_LOG_ERR("LE conneted!");
			return;
		}

		//bis maybe in releasing
		if (!audio_input->chan) {
			if (audio_input->ain_handle == NULL) {
				if (audio_input->playback_player_run) {
					audio_input_stop_playback();
				}
				audio_input_set_auracast_mode(true);
			} else {
				SYS_LOG_WRN("audio source is not ready.");
			}
		} else {
			SYS_LOG_WRN("chan not released %p", audio_input->chan);
		}
	}
}
#endif

#ifdef CONFIG_AUDIO_INPUT_RESTART
static int audio_input_handle_restart(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	SYS_LOG_INF("%d", audio_input->playing);

	audio_input_play_pause();

	// clear restart flag to avoid restart handler to do dumplicated handling.
	audio_input->restart = 0;

	audio_input_play_resume();

	return 0;
}
#endif

void audio_input_input_event_proc(struct app_msg *msg)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (!audio_input)
		return;

	SYS_LOG_INF("cmd %d", msg->cmd);
	switch (msg->cmd) {
	case MSG_AUDIO_INPUT_PLAY_PAUSE_RESUME:
		if (audio_input->playing) {
			audio_input_play_pause();
			sys_event_notify(SYS_EVENT_PLAY_PAUSE);
		} else {
			audio_input_play_resume();
			sys_event_notify(SYS_EVENT_PLAY_START);
		}
		audio_input_show_play_status(audio_input->playing);
		audio_input_store_play_state();
#ifdef CONFIG_ESD_MANAGER
		{
			u8_t playing = audio_input->playing;

			esd_manager_save_scene(TAG_PLAY_STATE, &playing, 1);
		}
#endif
		break;
	case MSG_AUDIO_INPUT_PLAY_VOLUP:
		audio_input_handle_client_volume(true);
		break;
	case MSG_AUDIO_INPUT_PLAY_VOLDOWN:
		audio_input_handle_client_volume(false);
		break;
#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	case MSG_SWITCH_BROADCAST:
		audio_input_switch_auracast();
		break;
	case MSG_AURACAST_ENTER:
		if(system_app_get_auracast_mode() == 0){
			audio_input_switch_auracast();
		} else if(system_app_get_auracast_mode() == 1){
			audio_input_handle_restart();
		}
		break;
	case MSG_AURACAST_EXIT:
		if(system_app_get_auracast_mode() != 0){
			audio_input_switch_auracast();
		}
		break;
#endif

	default:
		break;
	}
}

void audio_input_app_event_proc(struct app_msg *msg)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (NULL == audio_input) {
		return;
	}

	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
#ifdef CONFIG_AUDIO_INPUT_RESTART
	case MSG_APP_PLAYER_RESET:
		audio_input_handle_restart();
		break;
#endif
	case MSG_AUDIO_INPUT_SAMPLE_RATE_CHANGE:
		audio_input_handle_restart();
		break;
	}
}

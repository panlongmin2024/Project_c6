/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt music event
 */
#include "btmusic.h"
#include <ui_manager.h>
#include <esd_manager.h>
#include "tts_manager.h"
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif
#include <audio_track.h>
#include <wltmcu_manager_supply.h>
#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

/* find the existed broad chan */
static struct bt_broadcast_chan *find_broad_chan(uint32_t handle, uint8_t id)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &btmusic->broad_chan[i];
		if (chan->handle == handle && chan->id == id) {
			return chan;
		}
	}
	return NULL;
}

/* find a new broad chan */
static struct bt_broadcast_chan *new_broad_chan(uint32_t handle, uint8_t id)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &btmusic->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			return chan;
		}
	}
	return NULL;
}

static int btmusic_handle_enable(struct bt_audio_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_audio_chan *br_sink_chan = &btmusic->sink_chan;

	if (BT_TYPE_BR == bt_manager_audio_get_type(rep->handle)) {
		br_sink_chan->handle = rep->handle;
		br_sink_chan->id = rep->id;
		SYS_EVENT_INF(EVENT_BTMUSIC_STREAM_ENABLE,br_sink_chan->handle,br_sink_chan->id);
		return 0;
	}

	return -EINVAL;
}

static int btmusic_handle_disable(struct bt_audio_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_audio_chan *br_sink_chan = &btmusic->sink_chan;

	SYS_LOG_INF("\n");

	if ((rep->handle == br_sink_chan->handle) &&
	    (rep->id == br_sink_chan->id)) {
		SYS_LOG_INF("BR\n");
		return 0;
	}

	return -EINVAL;
}

static int btmusic_handle_start(struct bt_audio_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_audio_chan *br_sink_chan = &btmusic->sink_chan;

	SYS_LOG_INF("h:0x%x id:%d\n", rep->handle, rep->id);

	if ((rep->handle == br_sink_chan->handle) &&
	    (rep->id == br_sink_chan->id)) {
		if (!btmusic->playing){
			tts_manager_enable_filter();
		}
		btmusic->playing = 1;
		btmusic->ios_dev = bt_manager_audio_is_ios_dev(rep->handle);

#if ENABLE_BROADCAST
		if(btmusic_get_auracast_mode()){
			if (thread_timer_is_running(&btmusic->broadcast_switch_timer)){
				thread_timer_stop(&btmusic->broadcast_switch_timer);
			}
			if (!btmusic->chan) {
				SYS_LOG_INF("broad_chan not config:\n");
				if(3 == btmusic_get_auracast_mode()){
					btmusic_bms_source_init();
				}
				return -EINVAL;
			}else{
				if(btmusic->broadcast_source_exit){
					SYS_LOG_INF("broadcast exiting\n");
					return -EINVAL;
				}else if(!btmusic->broadcast_source_enabled){
					bt_manager_broadcast_source_enable(btmusic->chan->handle);
				}
			}
		}
#endif

		if(btmusic->tts_playing || btmusic->user_pause){
			SYS_LOG_INF("tts playing or user pause\n");
			return -EINVAL;
		}
		btmusic_init_playback();
		btmusic_start_playback();

#if ENABLE_BROADCAST
		if(btmusic_get_auracast_mode()){
			if (btmusic->broadcast_source_enabled &&
				!btmusic->capture_player_run) {
				btmusic_bms_init_capture();
				btmusic_bms_start_capture();

				bt_manager_audio_sink_stream_set(&btmusic->sink_chan,
								 btmusic->sink_stream);
			}
		}
#endif
		return 0;
	}

	return -EINVAL;
}

static int btmusic_handle_stop(struct bt_audio_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_audio_chan *br_sink_chan = &btmusic->sink_chan;

	if ((rep->handle == br_sink_chan->handle) &&
	    (rep->id == br_sink_chan->id)) {
		btmusic->playing = 0;
		btmusic_stop_playback();
		/* always exit playback for BR */
		btmusic_exit_playback();
		SYS_EVENT_INF(EVENT_BTMUSIC_STREAM_STOP,br_sink_chan->handle,br_sink_chan->id);
#if ENABLE_BROADCAST
		if(btmusic_get_auracast_mode()){
			if (btmusic->capture_player_run) {
				btmusic_bms_stop_capture();
				btmusic_bms_exit_capture();
			}
			btmusic->tx_start = 0;
			btmusic->tx_sync_start = 0;
			if(3 == btmusic_get_auracast_mode()){
				btmusic_bms_source_exit();
			}else{
				thread_timer_start(&btmusic->broadcast_switch_timer, 1000, 0);
			}
		}
#endif
        btmusic->ios_dev = 0;
		return 0;
	}

	return -EINVAL;
}

static int btmusic_handle_release(struct bt_audio_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_audio_chan *br_sink_chan = &btmusic->sink_chan;

	if ((rep->handle == br_sink_chan->handle) &&
	    (rep->id == br_sink_chan->id)) {
	    btmusic->playing = 0;
		btmusic_stop_playback();
		/* always exit playback for BR */
		btmusic_exit_playback();
		if (thread_timer_is_running(&btmusic->user_pause_timer)){
			thread_timer_stop(&btmusic->user_pause_timer);
#if ENABLE_PADV_APP
			if(btmusic_get_auracast_mode() && btmusic->chan
				&& !btmusic->padv_tx_enable){
				padv_tx_init(btmusic->chan->handle, AUDIO_STREAM_SOUNDBAR);
				btmusic->padv_tx_enable = 1;
			}
#endif
		}
		tts_manager_disable_filter();
		btmusic->user_pause = 0;
		return 0;
	}

	return -EINVAL;
}

static void btmusic_handle_disconnect(uint16_t * handle)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	if(*handle == btmusic->sink_chan.handle)
		memset(&btmusic->sink_chan, 0, sizeof(struct bt_audio_chan));
}

static void btmusic_handle_media_connect(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	SYS_LOG_INF("playing: %d\n", btmusic->playing);

#ifdef CONFIG_ESD_MANAGER
	btmusic_esd_restore();
#endif
	btmusic->media_ctrl_connected = 1;
}

static void btmusic_handle_passthrough_release(struct bt_media_play_status *status)
{
    struct btmusic_app_t *btmusic = (struct btmusic_app_t *)btmusic_get_app();

    SYS_LOG_INF("op_id:%d\n", status->status);
    if(status->status == BT_MANAGER_AVRCP_PASSTHROUGH_PLAY){
        if(thread_timer_is_running(&btmusic->user_pause_timer)){
            thread_timer_stop(&btmusic->user_pause_timer);

            thread_timer_init(&btmusic->user_pause_timer, btmusic_user_pause_handler,NULL);
            thread_timer_start(&btmusic->user_pause_timer, 100, 0);
        }
    }
}

static void btmusic_handle_playback_status(struct bt_media_play_status *status)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	SYS_LOG_INF("%d\n", status->status);

	if (status->status == BT_STATUS_PAUSED) {
		if(bt_manager_media_get_status() == BT_STATUS_PAUSED){
			SYS_LOG_INF("pause\n");
#ifdef CONFIG_EXTERNAL_DSP_DELAY
			if (btmusic->media_state && btmusic->ios_dev)
				media_player_fade_out(btmusic->playback_player, 290);
#endif
			if (thread_timer_is_running(&btmusic->user_pause_timer)){
				thread_timer_stop(&btmusic->user_pause_timer);
				btmusic->user_pause = 0;
#if ENABLE_PADV_APP
				if(btmusic_get_auracast_mode() && btmusic->chan
					&& !btmusic->padv_tx_enable){
					padv_tx_init(btmusic->chan->handle, AUDIO_STREAM_SOUNDBAR);
					btmusic->padv_tx_enable = 1;
				}
#endif
			}
		}
		btmusic->media_state = 0;
	} else if (status->status == BT_STATUS_PLAYING) {
		SYS_LOG_INF("play\n");
#ifdef CONFIG_EXTERNAL_DSP_DELAY
		if (!btmusic->media_state && btmusic->ios_dev)
			media_player_fade_in(btmusic->playback_player, 290);
#endif
		btmusic->media_state = 1;
		if(btmusic->playing){
			tts_manager_enable_filter();
		}
		bt_manager_audio_stream_restore(BT_TYPE_BR);
	}
}

static void btmusic_bms_tx_start(uint32_t handle, uint8_t id, uint16_t audio_handle)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic || btmusic->tx_start) {
		return;
	}

	if (btmusic->broadcast_source_enabled && btmusic->capture_player) {
		media_player_trigger_audio_record_start(btmusic->capture_player);
		btmusic->tx_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger record\n");
	}
}

void btmusic_bms_tx_sync_start(uint32_t handle, uint8_t id,
				 uint16_t audio_handle)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic || btmusic->tx_sync_start) {
		return;
	}

	if (btmusic->broadcast_source_enabled && btmusic->playback_player) {
		media_player_trigger_audio_track_start(btmusic->playback_player);
		btmusic->tx_sync_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger track\n");
	}
}

static int btmusic_bms_handle_source_config(struct bt_braodcast_configured *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_broadcast_chan *chan;
	uint8_t vnd_buf[20] = { 0 };
	uint8_t type = 0;

	SYS_LOG_INF("h:0x%x id:%d\n", rep->handle, rep->id);

	chan = find_broad_chan(rep->handle, rep->id);
	if (!chan) {
		chan = new_broad_chan(rep->handle, rep->id);
		if (!chan) {
			SYS_LOG_ERR("no broad chan handle: %d, id: %d\n",
				    rep->handle, rep->id);
			return -EINVAL;
		}
		btmusic->num_of_broad_chan++;
	}

	chan->handle = rep->handle;
	chan->id = rep->id;

	vnd_buf[0] = VS_ID & 0xFF;
	vnd_buf[1] = VS_ID >> 8;
	memcpy(&vnd_buf[2], btmusic->broadcast_code, 16);
	vnd_buf[18] = SERIVCE_UUID & 0xFF;
	vnd_buf[19] = SERIVCE_UUID >> 8;

#if !VS_COMPANY_ID
	type = BT_DATA_SVC_DATA16;
#endif

	if (btmusic->num_of_broad_chan == 1) {
		SYS_EVENT_INF(EVENT_BTMUSIC_BIS_SOURCE_CONFIG,chan->handle,chan->id);
		bt_manager_broadcast_stream_cb_register(chan, btmusic_bms_tx_start,
							NULL);
		//bt_manager_broadcast_stream_tws_sync_cb_register(chan,
		//						 btmusic_bms_tx_sync_start);
		bt_manager_broadcast_stream_set_tws_sync_cb_offset(chan,
								   broadcast_get_tws_sync_offset
								   (btmusic->qos));
		if (btmusic->use_past == 0) {
		    bt_manager_broadcast_source_vnd_ext_send(chan->handle, vnd_buf,
							    sizeof(vnd_buf), type);
			SYS_LOG_INF("no use past:\n");
		}

		btmusic->chan = chan;
		bt_manager_audio_stream_restore(BT_TYPE_BR);
	}

	return 0;
}

static int btmusic_bms_handle_source_enable(struct bt_broadcast_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (NULL == btmusic) {
		return -1;
	}

	SYS_LOG_INF("handle 0x%x, id %d\n", rep->handle, rep->id);
	if (NULL == btmusic->chan) {
		SYS_LOG_INF("no chan.");
		return -2;
	}

	if (rep->id != btmusic->chan->id) {
		SYS_LOG_INF("Skip this chan.");
		return -3;
	}

	SYS_EVENT_INF(EVENT_BTMUSIC_BIS_SOURCE_ENABLE,btmusic->chan->handle,btmusic->chan->id);
	btmusic->broadcast_source_enabled = 1;

#if ENABLE_PADV_APP
	padv_tx_init(btmusic->chan->handle, AUDIO_STREAM_SOUNDBAR);
	btmusic->padv_tx_enable = 1;
#endif

	if(btmusic->tts_playing || btmusic->user_pause){
		SYS_LOG_INF("tts playing or user_pause\n");
		return -4;
	}

	if (!btmusic->playing){
		SYS_LOG_INF("br stream stoped\n");
		return -5;
	}

	btmusic_bms_init_capture();
	btmusic_bms_start_capture();

	bt_manager_audio_sink_stream_set(&btmusic->sink_chan,
					 btmusic->sink_stream);
	return 0;
}

static int btmusic_bms_handle_source_disable(struct bt_broadcast_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

#if ENABLE_PADV_APP
	padv_tx_deinit();
	btmusic->padv_tx_enable = 0;
#endif

	btmusic_bms_stop_capture();
	btmusic_bms_exit_capture();

	btmusic->tx_start = 0;
	btmusic->tx_sync_start = 0;

	struct bt_broadcast_chan * chan = find_broad_chan(rep->handle, rep->id);
	if(chan){
		if(btmusic->num_of_broad_chan){
			btmusic->num_of_broad_chan--;
			chan->handle = 0;
			chan->id = 0;
			if(chan == btmusic->chan){
				btmusic->chan = NULL;
			}
			if(!btmusic->num_of_broad_chan){
				btmusic->broadcast_source_enabled = 0;
				btmusic->broadcast_source_exit = 0;
				btmusic->broadcast_id = 0;
				SYS_LOG_INF("\n");
			}
		}else{
			SYS_LOG_WRN("should not be here\n");
		}
	}

	return 0;
}

static int btmusic_bms_handle_source_release(struct bt_broadcast_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (NULL == btmusic) {
		return -1;
	}

	btmusic->chan = NULL;
	btmusic->broadcast_source_enabled = 0;
	btmusic->broadcast_source_exit = 0;
	btmusic->num_of_broad_chan = 0;
	btmusic->broadcast_id = 0;
	memset(btmusic->broad_chan,0,sizeof(btmusic->broad_chan));
	SYS_LOG_INF("\n");
	SYS_EVENT_INF(EVENT_BTMUSIC_BIS_SOURCE_RELEASE);
	if(btmusic_get_auracast_mode() && btmusic->playing){
		thread_timer_start(&btmusic->broadcast_start_timer, 0, 0);
	}

	return 0;
}

static int btmusic_handle_volume_value(struct bt_volume_report *rep)
{
	SYS_LOG_INF("%d\n", rep->value);
#if ENABLE_PADV_APP
	if(btmusic_get_auracast_mode())
		padv_tx_data(padv_volume_map(rep->value,1));
#endif
	return 0;
}

static int btmusic_handle_letws_connected(uint16_t *handle)
{
#ifdef CONFIG_BT_LETWS
	uint8_t  tws_role;
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (bt_manager_audio_get_type(*handle) == BT_TYPE_BR) {
		return 0;
	}

    tws_role = bt_manager_letws_get_dev_role();
	SYS_LOG_INF("mtu: %d,%d\n", bt_manager_audio_le_conn_max_len(*handle),tws_role);

	if (tws_role == BTSRV_TWS_SLAVE) {
        /*
		 switch bmr
		*/
		system_app_set_auracast_mode(4);
		system_app_launch_switch(DESKTOP_PLUGIN_ID_BR_MUSIC,DESKTOP_PLUGIN_ID_BMR);
	}else if (tws_role == BTSRV_TWS_MASTER){
		btmusic_stop_playback();
		btmusic_exit_playback();

		btmusic_bms_stop_capture();
		btmusic_bms_exit_capture();

		btmusic->tx_start = 0;
		btmusic->tx_sync_start = 0;

		btmusic_set_auracast_mode(3);
	}
#endif
	return 0;
}

static int btmusic_handle_letws_disconnected(uint8_t *reason)
{
#ifdef CONFIG_BT_LETWS
	struct btmusic_app_t *btmusic = btmusic_get_app();

#ifdef CONFIG_BT_SELF_APP
	//08 link loss or ungroup
	if(*reason != 0x16 || !selfapp_get_group_id()){
#else
	if(*reason != 0x16){
#endif
		btmusic_stop_playback();
		btmusic_exit_playback();

		btmusic_bms_stop_capture();
		btmusic_bms_exit_capture();

		btmusic->tx_start = 0;
		btmusic->tx_sync_start = 0;

		btmusic_set_auracast_mode(0);
		bt_manager_audio_stream_restore(BT_TYPE_BR);
		return 0;
	}

	if(btmusic->playing){
		if (btmusic->playback_player_run) {
			btmusic_stop_playback();
			btmusic_exit_playback();
		}

		if (btmusic->capture_player_run) {
			btmusic_bms_stop_capture();
			btmusic_bms_exit_capture();
		}
		btmusic->tx_start = 0;
		btmusic->tx_sync_start = 0;

		btmusic_bms_source_exit();
		btmusic_set_auracast_mode(1);
	}else{
		system_app_set_auracast_mode(2);
		system_app_launch_switch(DESKTOP_PLUGIN_ID_BR_MUSIC,DESKTOP_PLUGIN_ID_BMR);
	}
#endif
	return 0;
}

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
static void btmusic_exit_auracast(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	if(btmusic_get_auracast_mode()){
		btmusic_stop_playback();
		btmusic_exit_playback();

		btmusic_bms_stop_capture();
		btmusic_bms_exit_capture();

		btmusic->tx_start = 0;
		btmusic->tx_sync_start = 0;

		btmusic_set_auracast_mode(0);
		bt_manager_audio_stream_restore(BT_TYPE_BR);
	}
}
#endif

void btmusic_bt_event_proc(struct app_msg *msg)
{
	struct bt_audio_report rep;
	struct btmusic_app_t *btmusic =
	    (struct btmusic_app_t *)btmusic_get_app();
#ifdef CONFIG_ESD_MANAGER
	u8_t playing = 0;
#endif
	if (NULL == btmusic) {
		return;
	}
	SYS_LOG_INF("bt event %d\n", msg->cmd);
	switch (msg->cmd) {

#ifdef CONFIG_C_TEST_MUTE_PA
extern void wlt_pa_media_unmute(u8_t tts_flag, u8_t meda_flag);
extern void wlt_pa_media_mute(u8_t tts_mute_flag, u8_t meda_mute_flag);
	case BT_MEDIA_PLAY:
		wlt_pa_media_unmute(0,1);
		break;

	case BT_MEDIA_STOP:
	case BT_MEDIA_PAUSE:
		wlt_pa_media_mute(0,1);
		break;	
#endif

	case BT_CONNECTED:
		btmusic_view_show_connected();
		btmusic_volume_check();
		break;
	case BT_DISCONNECTED:
		btmusic_view_show_disconnected();
		btmusic_handle_disconnect(msg->ptr);
		btmusic_volume_check();
		break;
	case BT_AUDIO_STREAM_ENABLE:
		btmusic_handle_enable(msg->ptr);
		break;
	case BT_AUDIO_STREAM_UPDATE:
		break;
	case BT_AUDIO_STREAM_DISABLE:
		btmusic_handle_disable(msg->ptr);
		break;
	case BT_AUDIO_STREAM_START:
		btmusic_handle_start(msg->ptr);
#ifdef CONFIG_ESD_MANAGER
		playing = btmusic->playing;
		esd_manager_save_scene(TAG_PLAY_STATE, &playing, 1);
#endif
		break;
	case BT_AUDIO_STREAM_STOP:
		btmusic_handle_stop(msg->ptr);
#ifdef CONFIG_ESD_MANAGER
		playing = btmusic->playing;
		esd_manager_save_scene(TAG_PLAY_STATE, &playing, 1);
#endif
		break;
	case BT_AUDIO_STREAM_RELEASE:
		btmusic_handle_release(msg->ptr);
		break;
	case BT_VOLUME_VALUE:
		btmusic_handle_volume_value(msg->ptr);
		break;

	case BT_MEDIA_CONNECTED:
		btmusic_handle_media_connect();
		break;
	case BT_MEDIA_DISCONNECTED:
		btmusic->media_ctrl_connected = 0;
		break;
	case BT_MEDIA_PLAYBACK_STATUS_CHANGED_EVENT:
		btmusic_handle_playback_status((struct bt_media_play_status *)
					      msg->ptr);
		break;

	case BT_MEDIA_PASSTHROU_RELEASE_EVENT:
		btmusic_handle_passthrough_release((struct bt_media_play_status *)
					      msg->ptr);
		break;
	/* BT Audio switched, need to restore */
	case BT_AUDIO_SWITCH:
	case BT_REQ_RESTART_PLAY:
		SYS_EVENT_INF(EVENT_BTMUSIC_RESTART_PLAY);
		if(!btmusic_get_auracast_mode()){
			if (!btmusic->tts_playing && !btmusic->user_pause) {
				rep.handle = btmusic->sink_chan.handle;
				rep.id = btmusic->sink_chan.id;

				btmusic_handle_stop(&rep);
				btmusic_handle_release(&rep);
				bt_manager_audio_stream_restore(BT_TYPE_BR);
			}
		}else{
			// Do not restart player when bt data is not enough.
			SYS_LOG_INF("skip BT_REQ_RESTART_PLAY");
		}
		break;

		/* Broadcast Source */
	case BT_BROADCAST_SOURCE_CONFIG:
		btmusic_bms_handle_source_config(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_ENABLE:
		btmusic_bms_handle_source_enable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_DISABLE:
		btmusic_bms_handle_source_disable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_RELEASE:
		btmusic_bms_handle_source_release(msg->ptr);
		break;
#ifdef ENABLE_PAWR_APP
	case BT_PAWR_SYNCED:
		SYS_LOG_INF("pawr synced");
		break;
	case BT_PAWR_SYNC_LOST:
		SYS_LOG_INF("pawr sync lost");
		break;
#endif
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	case BT_MSG_AURACAST_EXIT:
		btmusic_exit_auracast();
		break;
#endif	
    case BT_TWS_CONNECTION_EVENT:
		if(msg->ptr){
		    SYS_LOG_INF("tws conn:");
	        btmusic_handle_letws_connected(msg->ptr);
		}
	    break;
	case BT_TWS_DISCONNECTION_EVENT:
		if(msg->ptr){
			btmusic_handle_letws_disconnected(msg->ptr);
		}
		break;
	default:
		break;
	}
}

static int btmusic_handle_client_volume(bool up)
{
	int ret;
	u8_t max;
	u8_t vol;
	int stream_type = AUDIO_STREAM_MUSIC;
	if(bt_manager_get_connected_dev_num() == 0){
		SYS_LOG_INF("no phone connect\n");
		return -EINVAL;  
	}
	if(btmusic_get_auracast_mode())
		stream_type = AUDIO_STREAM_SOUNDBAR;

	ret = system_volume_get(stream_type);
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
	system_volume_set(stream_type, vol, true);

#if ENABLE_PADV_APP
	if(btmusic_get_auracast_mode())
		padv_tx_data(padv_volume_map(vol,1));
#endif

	return 0;
}

static void btmusic_player_reset(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	SYS_EVENT_INF(EVENT_BTMUSIC_PLAYER_RESET);

	//stop
	if (btmusic->playback_player_run) {
		btmusic_stop_playback();
		btmusic_exit_playback();
	}

	if (btmusic->capture_player_run) {
		btmusic_bms_stop_capture();
		btmusic_bms_exit_capture();
	}
	btmusic->tx_start = 0;
	btmusic->tx_sync_start = 0;
	//clear restart flag to avoid restart handler to do dumplicated handling.
	btmusic->restart = 0;

	if(btmusic->tts_playing || btmusic->user_pause){
		SYS_LOG_INF("tts playing or user pause\n");
		return;
	}

	if (!btmusic->playing){
		SYS_LOG_INF("br stream stoped\n");
		return;
	}

	//wait a while (use a timer?)
	//os_sleep(10);

	//start
	if(btmusic_get_auracast_mode()){
		if(!btmusic->broadcast_source_exit){
			if (!btmusic->playback_player_run && btmusic->chan) {
				btmusic_init_playback();
				btmusic_start_playback();
			}
			if (btmusic->broadcast_source_enabled) {
				if (!btmusic->capture_player_run) {
					btmusic_bms_init_capture();
					btmusic_bms_start_capture();

					bt_manager_audio_sink_stream_set(&btmusic->sink_chan,
									btmusic->sink_stream);
				}
			}
		}
	}else{
		bt_manager_audio_stream_restore(BT_TYPE_BR);
	}
}

static void btmusic_playTimeBoost_reset(void)
{
	uint8_t delay_flag = 0;
	struct btmusic_app_t *btmusic = btmusic_get_app();
	if(btmusic == NULL){
		SYS_LOG_INF("bt music not init\n");
		return;
	}

#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(true);
#endif

	//stop
	if (btmusic->playback_player_run) {
		btmusic_stop_playback();
		btmusic_exit_playback();
		delay_flag = 1;
	}

	if (btmusic->capture_player_run) {
		btmusic_bms_stop_capture();
		btmusic_bms_exit_capture();
	}
	btmusic->tx_start = 0;
	btmusic->tx_sync_start = 0;
	//clear restart flag to avoid restart handler to do dumplicated handling.
	btmusic->restart = 0;

	if (delay_flag) {
		//os_sleep(OS_MSEC(50));
	}
	os_sleep(OS_MSEC(100));
/* 	if (selfapp_config_get_PB_state()) {
		flip7_reload_default_eq(true);
	} else {
		if (btmusic_get_auracast_mode() == 0) {
			flip7_reload_default_eq(false);
		}
	} */

	extern void hm_ext_pa_stop(void);
	extern int ext_dsp_set_bypass(int bypass);
	//hm_ext_pa_stop();
	//EQ模式暂停后DSP data还会有数据，复位DSP data直接拉低概率性导致PA有噪音
	ext_dsp_set_bypass(1);
	os_sleep(OS_MSEC(10));
	extern int external_dsp_ats3615_reset(void);
	external_dsp_ats3615_reset();

	extern void hm_ext_pa_start(void);
	//hm_ext_pa_start();
	bt_manager_audio_stream_restore(BT_TYPE_BR);
}


static void btmusic_switch_auracast()
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
#ifdef CONFIG_BT_LETWS
	if(3 == btmusic_get_auracast_mode()){
		bt_mamager_letws_disconnect(BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		return;
	}else if(1 == btmusic_get_auracast_mode()){
#else
	if (btmusic_get_auracast_mode()) {
#endif

		sys_event_notify(SYS_EVENT_PLAY_EXIT_AURACAST_TTS);
		btmusic_stop_playback();
		btmusic_exit_playback();

		btmusic_bms_stop_capture();
		btmusic_bms_exit_capture();

		btmusic->tx_start = 0;
		btmusic->tx_sync_start = 0;

		btmusic_set_auracast_mode(0);
		bt_manager_audio_stream_restore(BT_TYPE_BR);
		bt_manager_check_visual_mode();
	}else{
#if !defined(CONFIG_BT_LE_AUDIO)
		if(bt_manager_audio_get_leaudio_dev_num()){
			SYS_LOG_ERR("LE conneted!");
			return;
		}
#endif

		sys_event_notify(SYS_EVENT_PLAY_ENTER_AURACAST_TTS);
		if(btmusic->playing){
			if(!btmusic->broadcast_source_exit){
				if (btmusic->playback_player_run) {
					btmusic_stop_playback();
					btmusic_exit_playback();
				}
				btmusic_set_auracast_mode(1);
			}
		}else{
			system_app_set_auracast_mode(2);
			system_app_launch_switch(DESKTOP_PLUGIN_ID_BR_MUSIC,DESKTOP_PLUGIN_ID_BMR);
		}
		bt_manager_auto_reconnect_stop();
		bt_manager_end_pair_mode();
		bt_manager_check_visual_mode_immediate();
		k_sleep(10);
		//disconnect_inactive_audio_conn();
		
	}
}


extern void pd_switch_volt(void);
extern void pd_mps52002_switch_volt(void);
extern void  pd_sink_test_mode(bool flag);
extern void  pd_mps52002_sink_test_mode(bool flag);

void btmusic_user_pause_handler(struct thread_timer *ttimer,
				   void *expiry_fn_arg){
	struct btmusic_app_t *btmusic =
	    (struct btmusic_app_t *)btmusic_get_app();
	tts_manager_enable_filter();
	btmusic->user_pause = 0;
	SYS_LOG_INF("\n");

#if ENABLE_PADV_APP
	if(btmusic_get_auracast_mode() && btmusic->chan
		&& !btmusic->padv_tx_enable){
		padv_tx_init(btmusic->chan->handle, AUDIO_STREAM_SOUNDBAR);
		btmusic->padv_tx_enable = 1;
	}
#endif
	if(btmusic->tts_playing){
		SYS_LOG_INF("tts playing\n");
		return;
	}

	if (!btmusic->playing){
		SYS_LOG_INF("br stream stoped\n");
		return;
	}

	if(btmusic_get_auracast_mode()){
		if(!btmusic->broadcast_source_exit){
			if (!btmusic->playback_player_run && btmusic->chan) {
				btmusic_init_playback();
				btmusic_start_playback();
			}
			if (btmusic->broadcast_source_enabled) {
				if (!btmusic->capture_player_run) {
					btmusic_bms_init_capture();
					btmusic_bms_start_capture();

					bt_manager_audio_sink_stream_set(&btmusic->sink_chan,
									btmusic->sink_stream);
				}
			}
		}
	}else{
		bt_manager_audio_stream_restore(BT_TYPE_BR);
	}
}

void btmusic_input_event_proc(struct app_msg *msg)
{
	struct btmusic_app_t *btmusic =
	    (struct btmusic_app_t *)btmusic_get_app();
	int status;
//    extern uint8_t ReadODM(void);
//	static bool pd_volt_flag = 0;	

	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case MSG_BT_PLAY_PAUSE_RESUME:
		status = bt_manager_media_get_status();
		bt_manager_media_playpause();
#ifdef CONFIG_USER_PAUSE_POLICY
		if(status == BT_STATUS_PLAYING && btmusic->playing){
			btmusic->user_pause = 1;
			tts_manager_disable_filter();
			if (btmusic->playback_player_run) {
				btmusic_stop_playback();
				btmusic_exit_playback();
			}

			if (btmusic->capture_player_run) {
				btmusic_bms_stop_capture();
				btmusic_bms_exit_capture();
			}
			btmusic->tx_start = 0;
			btmusic->tx_sync_start = 0;
#if ENABLE_PADV_APP
			if(btmusic_get_auracast_mode()){
				padv_tx_data(0);
				padv_tx_deinit();
				btmusic->padv_tx_enable = 0;
			}
#endif
			if (thread_timer_is_running(&btmusic->user_pause_timer)){
				thread_timer_stop(&btmusic->user_pause_timer);
			}
			thread_timer_init(&btmusic->user_pause_timer, btmusic_user_pause_handler,
				NULL);
			thread_timer_start(&btmusic->user_pause_timer, 2000, 0);
		}else if(status == BT_STATUS_PAUSED && btmusic->playing){
			
			if (thread_timer_is_running(&btmusic->user_pause_timer)){
				thread_timer_stop(&btmusic->user_pause_timer);
			}
			thread_timer_init(&btmusic->user_pause_timer, btmusic_user_pause_handler,
				NULL);
			thread_timer_start(&btmusic->user_pause_timer, 50, 0);
		}
#endif
		break;

	case MSG_BT_PLAY_NEXT:
		bt_manager_media_play_next();
		break;

	case MSG_BT_PLAY_PREVIOUS:
		bt_manager_media_play_previous();
		break;

	case MSG_BT_PLAY_VOLUP:
		btmusic_handle_client_volume(true);
		break;

	case MSG_BT_PLAY_VOLDOWN:
		btmusic_handle_client_volume(false);
		break;

	case MSG_BT_PLAY_SEEKFW_START:
		if (btmusic->playing) {
			bt_manager_media_fast_forward(true);
		}
		break;

	case MSG_BT_PLAY_SEEKFW_STOP:
		if (btmusic->playing) {
			bt_manager_media_fast_forward(false);
		}
		break;

	case MSG_BT_PLAY_SEEKBW_START:
		if (btmusic->playing) {
			bt_manager_media_fast_rewind(true);
		}
		break;

	case MSG_BT_PLAY_SEEKBW_STOP:
		if (btmusic->playing) {
			bt_manager_media_fast_rewind(false);
		}
		break;

	case MSG_SWITCH_BROADCAST:
		// if(pd_volt_flag)
		// {
		//  if(!ReadODM())
		// 	pd_switch_volt();
		//  else
		//  	pd_mps52002_switch_volt();
		// }
		// else
		{
			btmusic_switch_auracast();
		}
		break;

	case MSG_SWITCH_PD_VOLT:
		// pd_volt_flag = true;
		// if(!ReadODM())
		// pd_sink_test_mode(pd_volt_flag);
		// else
		// pd_mps52002_sink_test_mode(pd_volt_flag);
		// pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,LOW_POWER_OFF_LED_STATUS);
		// k_sleep(100);
		break;


	case MSG_AURACAST_ENTER:
		SYS_LOG_INF("MSG_AURACAST_ENTER: %d\n", system_app_get_auracast_mode());
		if(system_app_get_auracast_mode() != 1){
			btmusic_switch_auracast();
		} else if(system_app_get_auracast_mode() == 1){
			btmusic_player_reset();
		}
		break;
	case MSG_AURACAST_EXIT:
		if(1 == system_app_get_auracast_mode() ||
			2 == system_app_get_auracast_mode()){
			btmusic_switch_auracast();
		}
		break;
	case MSG_PAUSE_PLAYER:
		status = bt_manager_media_get_status();
		if(status == BT_STATUS_PLAYING){
			bt_manager_media_pause();
		}
		break;
	case MSG_RESUME_PLAYER:
		status = bt_manager_media_get_status();
		if(status == BT_STATUS_PAUSED){
			bt_manager_media_play();
		}
		break;
	case MSG_PLAYER_RESET:
		btmusic_player_reset();
		break;
	default:
		break;
	}
}

void btmusic_tts_event_proc(struct app_msg *msg)
{
	struct btmusic_app_t *btmusic =
	    (struct btmusic_app_t *)btmusic_get_app();

	if (!btmusic)
		return;

	SYS_LOG_INF("playing status %d %d\n", btmusic->capture_player_run,
		    btmusic->playback_player_run);

	switch (msg->value) {
	case TTS_EVENT_START_PLAY:
		if (btmusic->playback_player_run) {
			btmusic_stop_playback();
			btmusic_exit_playback();
		}

		if (btmusic->capture_player_run) {
			btmusic_bms_stop_capture();
			btmusic_bms_exit_capture();
		}
		btmusic->tx_start = 0;
		btmusic->tx_sync_start = 0;
		btmusic->tts_playing = true;
		break;
	case TTS_EVENT_STOP_PLAY:
		btmusic->tts_playing = false;
		if (btmusic->user_pause){
			SYS_LOG_INF("user pause\n");
			return;
		}
		if (!btmusic->playing){
			SYS_LOG_INF("br stream stoped\n");
			return;
		}
		if (btmusic_get_auracast_mode()){
			if(!btmusic->broadcast_source_exit){
				if (!btmusic->playback_player_run && btmusic->chan) {
					btmusic_init_playback();
					btmusic_start_playback();
				}
				if(btmusic->broadcast_source_enabled){
					if (!btmusic->capture_player_run) {
						btmusic_bms_init_capture();
						btmusic_bms_start_capture();

						bt_manager_audio_sink_stream_set(&btmusic->sink_chan,
										 btmusic->sink_stream);
					}
				}
			}
		}else{
			bt_manager_audio_stream_restore(BT_TYPE_BR);
		}
		break;
	}
}

void btmusic_tws_event_proc(struct app_msg *msg)
{
	struct btmusic_app_t *btmusic =
	    (struct btmusic_app_t *)btmusic_get_app();

	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case TWS_EVENT_REQ_PAST_INFO:
		{
			if (btmusic->wait_for_past_req){
				if (thread_timer_is_running(&btmusic->broadcast_start_timer)){
					thread_timer_stop(&btmusic->broadcast_start_timer);
				}
				thread_timer_start(&btmusic->broadcast_start_timer, 0, 0);
				btmusic->wait_for_past_req = 0;
			} else {
				SYS_LOG_INF("pa_set_info_transfer:%d \n",btmusic->broadcast_id);
				bt_manager_broadcast_pa_set_info_transfer(btmusic->broadcast_id);
			}
			break;
		}
	}

}

void btmusic_app_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case MSG_BTMUSIC_MESSAGE_CMD_PLAYER_RESET:
		btmusic_player_reset();
		break;
	case MSG_BTMUSIC_MESSAGE_CMD_PLAY_TIME_BOOST:
		btmusic_playTimeBoost_reset();
		break;
	}
}


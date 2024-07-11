/*
 * Copyright (c) 2021 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bmr.h"
#include <audio_track.h>
#include "app_common.h"
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

#include <app_launch.h>

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#define INVALID_VOL 0xFF
static u8_t synced_vol = INVALID_VOL;

#ifndef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
static int bmr_sync_padv_volume(u8_t sync_vol)
{
	//Sync remote volume only one time to avoid shielding local volume change.
	if (synced_vol != sync_vol) {
		SYS_LOG_INF("sync %d, prev=%d", sync_vol, synced_vol);
		if (get_pawr_enable()){
			if(sync_vol != system_volume_get(AUDIO_STREAM_LE_AUDIO)) {
				SYS_LOG_INF("pawr sync to %d", sync_vol);
				system_volume_set(AUDIO_STREAM_LE_AUDIO, sync_vol, false);
			}
		} else {
			if (synced_vol == INVALID_VOL) {
				//First time, sync to exact volume level.
				SYS_LOG_INF("sync to %d firstly", sync_vol);
				system_volume_set(AUDIO_STREAM_LE_AUDIO, sync_vol, false);
			} else {
				//Sync diff volume instead of exact volume level.
				int vol;
				vol = system_volume_get(AUDIO_STREAM_LE_AUDIO);
				if (vol >= 0) {
					if (sync_vol > synced_vol) {
						vol += (sync_vol - synced_vol);
						if (vol > audio_policy_get_volume_level()) {
							vol = audio_policy_get_volume_level();
						}
					} else {
						vol -= (synced_vol - sync_vol);
						if(vol < 0) {
							vol = 0;
						}
					}
					SYS_LOG_INF("sync to %d, diff=%d", vol, sync_vol - synced_vol);
					system_volume_set(AUDIO_STREAM_LE_AUDIO, vol, false);
				} else {
					SYS_LOG_ERR("vol get err\n");
					return 0;
				}
			}

		}
		synced_vol = sync_vol;
	}

	return 0;
}
#else
static int bmr_sync_padv_volume(u8_t sync_vol)
{
	//Sync remote volume only one time to avoid shielding local volume change.
	if (synced_vol != sync_vol) {
		SYS_LOG_INF("sync %d, prev=%d", sync_vol, synced_vol);
		if (get_pawr_enable()){
			if(sync_vol != system_volume_get(AUDIO_STREAM_LE_AUDIO)) {
				SYS_LOG_INF("pawr sync to %d", sync_vol);
				system_volume_set(AUDIO_STREAM_LE_AUDIO, sync_vol, false);
			}
		} else {
			if (synced_vol == INVALID_VOL) {
				//First time, sync to exact volume level.
				SYS_LOG_INF("sync to %d firstly", sync_vol);
				system_volume_set(AUDIO_STREAM_LE_AUDIO, sync_vol, false);
			} 
#if 0 // if sync_vol is 0 or max, force sync vol ?
			else if (sync_vol == 0 || sync_vol == audio_policy_get_volume_level() || synced_vol == 0) {
				SYS_LOG_INF("force sync to %d", sync_vol);
				system_volume_set(AUDIO_STREAM_LE_AUDIO, sync_vol, false);
			}
#endif
			else {
				//Sync diff volume instead of exact volume level.
				int vol;
				vol = system_volume_get(AUDIO_STREAM_LE_AUDIO);
				if (vol >= 0) {
					if (sync_vol > synced_vol) {
						vol += (sync_vol - synced_vol);
						if (vol > audio_policy_get_volume_level()) {
							vol = audio_policy_get_volume_level();
						}
					} else {
						vol -= (synced_vol - sync_vol);
						if(vol < 0) {
							vol = 0;
						}
					}
					SYS_LOG_INF("sync to %d, diff=%d", vol, sync_vol - synced_vol);
					system_volume_set(AUDIO_STREAM_LE_AUDIO, vol, false);
				} else {
					SYS_LOG_ERR("vol get err\n");
					return 0;
				}
			}

		}
		synced_vol = sync_vol;
	}

	return 0;  
}
#endif

#ifndef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
static int padv_rx_data(uint32_t handle, const uint8_t *buf, uint16_t len)
{
	uint8_t cmd_length = 0;
	uint8_t cmd_type = 0;
	uint8_t sync_vol;
	uint16_t offset = 0;

	SYS_LOG_DBG("h:0x%x len:%d", handle, len);

	if (((buf[offset]) | (buf[offset+1] << 8)) != SERIVCE_UUID) {
		SYS_LOG_INF("Unknow data service.");
		return -1;
	}

	offset+=2;
	while (offset < len)
	{
		cmd_length = buf[offset++];
		cmd_type = buf[offset++];
		if(cmd_length < 1 || cmd_length > 30) {
			SYS_LOG_INF("wrong length.");
			break;
		}
		switch (cmd_type)
		{
		case Lighting_effects_type:
			//Fixme: do light effect
			SYS_LOG_DBG("light:%d", buf[offset]);
			break;
		case Volume_type:
			SYS_LOG_DBG("vol:%d", buf[offset]);
			sync_vol = padv_volume_map(buf[offset], 0);
			bmr_sync_padv_volume(sync_vol);
			break;
		default:
			SYS_LOG_INF("Unkown type");
			break;
		}
		offset += (cmd_length - 1);
	}

	return 0;
}
#else
static int padv_rx_data(uint32_t handle, const uint8_t *buf, uint16_t len)
{
	uint8_t cmd_length = 0;
	uint8_t cmd_type = 0;
	uint8_t sync_vol;
	uint16_t offset = 0;

	SYS_LOG_DBG("h:0x%x len:%d", handle, len);

	if (((buf[offset]) | (buf[offset+1] << 8)) != SERIVCE_UUID) {
		SYS_LOG_INF("Unknow data service.");
		return -1;
	}

	offset+=2;
	while (offset < len)
	{
		cmd_length = buf[offset++];
		cmd_type = buf[offset++];
		if(cmd_length < 1 || cmd_length > 30) {
			SYS_LOG_INF("wrong length.");
			break;
		}
		switch (cmd_type)
		{
		case Volume_type:
			SYS_LOG_DBG("vol:%d", buf[offset]);
			sync_vol = padv_volume_map(buf[offset], 0);
			bmr_sync_padv_volume(sync_vol);
			break;
		default:
			SYS_LOG_INF("Unkown type");
			break;
		}
		offset += (cmd_length - 1);
	}

	return 0;
}
#endif

static struct bmr_broad_dev *bmr_find_broad_dev(uint32_t handle)
{
	struct bmr_app_t *bmr = bmr_get_app();
	struct bmr_broad_dev *dev;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&bmr->broad_dev_list, dev, node) {
		if (dev->handle == handle) {
			os_sched_unlock();
			return dev;
		}
	}

	os_sched_unlock();

	return NULL;
}

static struct bmr_broad_dev *bmr_new_broad_dev(uint32_t handle)
{
	struct bmr_app_t *bmr = bmr_get_app();
	struct bmr_broad_dev *dev;

	dev = mem_malloc(sizeof(struct bmr_broad_dev));
	if (!dev) {
		SYS_LOG_ERR("no mem");
		return NULL;
	}

	dev->handle = handle;

	os_sched_lock();
	sys_slist_append(&bmr->broad_dev_list, &dev->node);
	os_sched_unlock();

	SYS_LOG_INF("handle: 0x%x\n", handle);

	return dev;
}

static int bmr_delete_broad_dev(uint32_t handle)
{
	struct bmr_app_t *bmr = bmr_get_app();
	struct bmr_broad_dev *dev;

	dev = bmr_find_broad_dev(handle);
	if (!dev) {
		SYS_LOG_ERR("not found (0x%x)", handle);
		return -ENODEV;
	}
	SYS_LOG_INF("handle: 0x%x\n", handle);

	os_sched_lock();

	sys_slist_find_and_remove(&bmr->broad_dev_list, &dev->node);

	mem_free(dev);

	os_sched_unlock();

	return 0;
}

/* find the existed broad chan */
static struct bt_broadcast_chan *find_broad_chan(uint32_t handle, uint8_t id)
{
	struct bmr_app_t *bmr = bmr_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &bmr->broad_chan[i];
		if (chan->handle == handle && chan->id == id) {
			return chan;
		}
	}
	return NULL;
}

/* find a new broad chan */
static struct bt_broadcast_chan *new_broad_chan(uint32_t handle, uint8_t id)
{
	struct bmr_app_t *bmr = bmr_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &bmr->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			return chan;
		}
	}
	return NULL;
}


void bmr_rx_start(uint32_t handle, uint8_t id, uint16_t audio_handle)
{
	struct bmr_app_t *bmr = bmr_get_app();

	if (bmr->rx_start) {
		return;
	}

	if (bmr->broadcast_sink_enabled && bmr->player) {
		media_player_trigger_audio_track_start(bmr->player);
		bmr->rx_start = 1;
#if 1
		//Time sensitive, short log to save time.
		printk("["SYS_LOG_DOMAIN"] trigger track\n");
#endif

	}
}

static int bmr_handle_sink_config(struct bt_braodcast_configured *rep)
{
    struct bmr_app_t *bmr = bmr_get_app();
	struct bt_broadcast_chan *chan;
	struct bt_broadcast_chan_info chan_info;

    chan = find_broad_chan(rep->handle,rep->id);
	if (!chan) {
		chan = new_broad_chan(rep->handle, rep->id);
		if (!chan) {
			SYS_LOG_ERR("no broad chan handle: %d, id: %d\n",
					rep->handle, rep->id);
			return -EINVAL;
		}
		bmr->num_of_broad_chan++;
	}

	chan->handle = rep->handle;
	chan->id = rep->id;

	SYS_LOG_INF("h:0x%x,id:%d,n:%d\n", chan->handle,chan->id,bmr->num_of_broad_chan);
    if (bmr->num_of_broad_chan == 1) {
		SYS_EVENT_INF(EVENT_BMR_SINK_CONFIG,chan->handle,chan->id);
	    //bt_manager_broadcast_stream_cb_register(chan, bmr_rx_start, NULL);
	    //2000 for media effect , 1000 for audio dma,32 sample for resample lib
		int ret = bt_manager_broadcast_stream_info(chan, &chan_info);
		int output_sample_rate = audio_system_get_output_sample_rate();
		if (!output_sample_rate)
			output_sample_rate = 48;
		if (!ret && chan_info.sample != output_sample_rate){
			//res_s 2x 75 3x 114
			int ext = 75;
			if (output_sample_rate == chan_info.sample * 2)
				ext = 75;
			else if (output_sample_rate == chan_info.sample * 3)
				ext = 114;
			bt_manager_broadcast_stream_set_media_delay(chan, 2000 + 1000 + 32 * 1000 / chan_info.sample + ext * 1000 / output_sample_rate
			+ EXTERNAL_DSP_DELAY);
		} else {
			bt_manager_broadcast_stream_set_media_delay(chan, 2000 + 1000 + 32 * 1000 / 48
			+ EXTERNAL_DSP_DELAY);
		}
	    bmr->chan = chan;
	}
#if !BIS_SYNC_LOCATIONS
	if (bmr->encryption) {
//		bt_manager_broadcast_sink_sync(chan->handle, BIT(chan->id),
//					bmr->broadcast_code);
	} else {
//		bt_manager_broadcast_sink_sync(chan->handle, BIT(chan->id), NULL);
	}
#endif

	return 0; 
}

static int bmr_handle_sink_base_config(struct bt_broadcast_base_cfg_info *rep)
{
 #if !BIS_SYNC_LOCATIONS
    struct bmr_app_t *bmr = bmr_get_app();

	if (bmr->encryption) {
		bt_manager_broadcast_sink_sync(rep->broadcast_id, rep->bis_indexes,
					bmr->broadcast_code);
	} else {
//		bt_manager_broadcast_sink_sync(chan->handle, BIT(chan->id), NULL);
	}
#endif

	//register padv rx handler ahead, to get remote volume value before player starting.
	synced_vol = INVALID_VOL;
	bt_manager_broadcast_sink_register_per_rx_cb(rep->broadcast_id, padv_rx_data);

	return 0;
}

static int bmr_handle_sink_enable(struct bt_broadcast_report *rep)
{
	struct bmr_app_t *bmr = bmr_get_app();
	struct bt_broadcast_chan *chan;

	chan = find_broad_chan(rep->handle,rep->id);
	if(NULL == chan) {
		SYS_LOG_WRN("No chan 0x%x, %d", rep->handle, rep->id);
		return -1;
	}
	SYS_EVENT_INF(EVENT_BMR_SINK_ENABLE,chan->handle,chan->id);

	tts_manager_enable_filter();

	bmr->broadcast_sink_enabled = 1;

	if(bmr->tts_playing){
		SYS_LOG_INF("tts playing\n");
		return -1;
	}

	bmr_init_playback();
	bmr_start_playback();

	return 0;
}

static void bmr_past_subscribe_restore(void)
{
 	uint16_t temp_acl_handle;

 	temp_acl_handle = bt_manager_audio_get_letws_handle();

    if (temp_acl_handle) {
 		bt_manager_broadcast_past_subscribe(temp_acl_handle);
 	}
     SYS_LOG_INF(":");
}

static int bmr_handle_sink_disable(struct bt_broadcast_report *rep)
{
	struct bmr_app_t *bmr = bmr_get_app();
    //todo
    struct bt_broadcast_chan *chan;
	int ret;

    chan = find_broad_chan(rep->handle,rep->id);
	bmr_stop_playback();
	bmr_exit_playback();

	tts_manager_disable_filter();

	bmr->broadcast_sink_enabled = 0;
	bmr->rx_start = 0;
#if 0
	if (chan) {
		chan->handle = 0;
		chan->id = 0;
		if (bmr->num_of_broad_chan) {
		    bmr->num_of_broad_chan--;
		}
	}
#else
   int i;
   for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
	   bmr->broad_chan[i].handle = 0;
	   bmr->broad_chan[i].id = 0;
   }
   bmr->num_of_broad_chan = 0;
#endif
   ret = bmr_delete_broad_dev(rep->handle);
   if ((bmr->use_past)&&(!ret)) {
      bmr_past_subscribe_restore();
   }
	return 0;
}

static int bmr_handle_sink_release(struct bt_broadcast_report *rep)
{
	struct bmr_app_t *bmr = bmr_get_app();
    struct bt_broadcast_chan *chan;
    chan = find_broad_chan(rep->handle,rep->id);

	bmr_stop_playback();
	bmr_exit_playback();
	SYS_EVENT_INF(EVENT_BMR_SINK_RELEASE,chan->handle,chan->id);

	bmr->broadcast_sink_enabled = 0;
	bmr->rx_start = 0;
	tts_manager_disable_filter();
#if 0
	if (chan) {
		chan->handle = 0;
		chan->id = 0;
		if (bmr->num_of_broad_chan) {
		    bmr->num_of_broad_chan--;
		}
	}
#else
   int i;
   for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
	   bmr->broad_chan[i].handle = 0;
	   bmr->broad_chan[i].id = 0;
   }
   bmr->num_of_broad_chan = 0;
#endif
    bmr_delete_broad_dev(rep->handle);
	return 0;
}

static int bmr_handle_sink_sync(void *data)
{
    uint32_t sync_hdl;
	struct bmr_broad_dev *dev;
	memcpy((uint8 *)&sync_hdl, (uint8 *)data, 4);

	dev = bmr_find_broad_dev(sync_hdl);
	if (!dev) {

		dev = bmr_new_broad_dev(sync_hdl);
		if (!dev) {
			SYS_LOG_ERR("no dev");
			return -ENOMEM;
		}
	}
	SYS_LOG_INF("handle: 0x%x\n", sync_hdl);
	return 0;
}

static int bmr_handle_letws_connected(uint16_t *handle)
{
#ifdef CONFIG_BT_LETWS
	uint8_t  tws_role;
	struct bmr_app_t *bmr = bmr_get_app();

	if (bt_manager_audio_get_type(*handle) == BT_TYPE_BR) {
		return 0;
	}

    tws_role = bt_manager_letws_get_dev_role();
	SYS_LOG_INF("mtu: %d,%d\n", bt_manager_audio_le_conn_max_len(*handle),tws_role);

	if (tws_role == BTSRV_TWS_MASTER) {
        /*
		 switch to bms
		*/
		system_app_set_auracast_mode(3);
		system_app_launch_switch(DESKTOP_PLUGIN_ID_BMR,DESKTOP_PLUGIN_ID_BR_MUSIC);
	}else if(tws_role == BTSRV_TWS_SLAVE){
		if(!bmr->use_past){
			bmr_stop_playback();
			bmr_exit_playback();

			bt_manager_broadcast_sink_exit();
			bt_manager_broadcast_scan_param_init(NULL);

			bt_manager_pawr_receive_stop();
			bmr_sink_init();
		}
		system_app_set_auracast_mode(4);
	}
#endif
	return 0;
}

static int bmr_handle_letws_disconnected(uint8_t *reason)
{
#ifdef CONFIG_BT_LETWS
	struct bmr_app_t *bmr = bmr_get_app();

	if(*reason == 0x8 || *reason == 0x15 || !selfapp_get_group_id()){
		tts_manager_disable_filter();
		//system_app_set_auracast_mode(0);
		bmr->broadcast_mode_reset = 1;
		system_app_launch_switch(DESKTOP_PLUGIN_ID_BMR,DESKTOP_PLUGIN_ID_BR_MUSIC);
		return 0;
	}

	bmr_stop_playback();
	bmr_exit_playback();

	bt_manager_broadcast_sink_exit();
	bt_manager_broadcast_scan_param_init(NULL);
	bt_manager_broadcast_past_unsubscribe(0);

	bt_manager_pawr_receive_stop();
	system_app_set_auracast_mode(2);
	bmr_sink_init();
#endif
	return 0;
}

static void bmr_handle_media_play(void)
{
	if (BT_AUDIO_STREAM_LE_MUSIC == bt_manager_audio_current_stream()) {
		/* audio stream does not stop after pause-play operations, need switch app by bmr */
		system_app_set_auracast_mode(1);
		system_app_launch_switch(DESKTOP_PLUGIN_ID_BMR,DESKTOP_PLUGIN_ID_LE_MUSIC);
	}
}

void bmr_bt_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case BT_BROADCAST_SINK_CONFIG:
		bmr_handle_sink_config(msg->ptr);
		break;
	case BT_BROADCAST_SINK_ENABLE:
		bmr_handle_sink_enable(msg->ptr);
		bmr_view_show_connected(true);
		break;
	case BT_BROADCAST_SINK_DISABLE:
		bmr_handle_sink_disable(msg->ptr);
		bmr_view_show_connected(false);
		break;
	case BT_BROADCAST_SINK_RELEASE:
		bmr_handle_sink_release(msg->ptr);
		break;
	case BT_BROADCAST_SINK_BASE_CONFIG:
		bmr_handle_sink_base_config(msg->ptr);
		break;
	case BT_BROADCAST_SINK_SYNC:
		bmr_handle_sink_sync(msg->ptr);
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
		tts_manager_disable_filter();
		system_app_set_auracast_mode(0);
		system_app_launch_switch(DESKTOP_PLUGIN_ID_BMR,DESKTOP_PLUGIN_ID_BR_MUSIC);
		bt_manager_check_visual_mode();
		break;
#endif	
	case BT_TWS_CONNECTION_EVENT:
		if(msg->ptr){
		    SYS_LOG_INF("tws conn:");
	        bmr_handle_letws_connected(msg->ptr);
		}
	    break;
	case BT_TWS_DISCONNECTION_EVENT:
		if(msg->ptr){
			bmr_handle_letws_disconnected(msg->ptr);
		}
	    break;

	/* media controller */
	case BT_MEDIA_PLAY:
		bmr_handle_media_play();
		break;

	default:
		break;
	}
}

static int bmr_handle_volume(bool up)
{
	int ret;
	u8_t max;
	u8_t vol;
	struct bmr_app_t *bmr = bmr_get_app();
	int stream_type = AUDIO_STREAM_LE_AUDIO;

	if(bmr->player == NULL){
		SYS_LOG_ERR("no play\n");
		if(bt_manager_get_connected_dev_num() == 0){
			SYS_LOG_INF("no phone connect\n");
			return -EINVAL;  
		}
		stream_type = AUDIO_STREAM_MUSIC;
	}
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
		if(vol >= 1){
			vol -= 1;
		}
	}

	SYS_LOG_INF("vol=%d/%d\n", vol, max);
	//if((synced_vol != 0) || (get_pawr_enable()))
		system_volume_set(stream_type, vol, true);

#ifdef ENABLE_PAWR_APP
	if (get_pawr_enable()) {
		//Do not response volume until synced Central device's value
		if(synced_vol != INVALID_VOL) {
			pawr_response_vol(padv_volume_map(vol, 1));
		}
	}
#endif

	return 0;
}

void bmr_input_event_proc(struct app_msg *msg)
{
	struct bmr_app_t *bmr = bmr_get_app();

	if (bmr == NULL){
		return;
	}

	SYS_LOG_INF("cmd: %d\n", msg->cmd);
	switch (msg->cmd) {
	case MSG_BMR_PLAY_VOLUP:
		bmr_handle_volume(true);
		break;

	case MSG_BMR_PLAY_VOLDOWN:
		bmr_handle_volume(false);
		break;

	case MSG_BMR_PAUSE_RESUME:
		SYS_LOG_INF("pause: %d\n", bmr->player_paused);
		{
			struct audio_track_t * track;
			track = audio_system_get_audio_track_handle(AUDIO_STREAM_LE_AUDIO);

			if (NULL != track && bmr->player_run) {
				if (bmr->player_paused) {
					media_player_fade_in(bmr->player, 120);
					audio_track_mute(track, 0);
					bmr->player_paused = 0;
				} else {
					media_player_fade_out(bmr->player, 60);
					os_sleep(90);
					audio_track_mute(track, 1);
					bmr->player_paused = 1;
				}
			}else if(bmr->tts_playing && bmr->broadcast_sink_enabled){
				if(bmr->player_paused)
					bmr->player_paused = 0;
				else
					bmr->player_paused = 1;
			}else{
				bt_manager_media_play();
			}
		}
		break;
	case MSG_BT_PLAY_NEXT:
		if((!bmr->broadcast_sink_enabled) || (bmr->player_paused))
			bt_manager_media_play_next();
		break;
	case MSG_BT_PLAY_PREVIOUS:
		if((!bmr->broadcast_sink_enabled) || (bmr->player_paused))
			bt_manager_media_play_previous();
		break;
	case MSG_BMR_PLAY_APP_EXIT:
	case MSG_AURACAST_EXIT:
	{
		tts_manager_disable_filter();
		sys_event_notify(SYS_EVENT_PLAY_EXIT_AURACAST_TTS);
#ifdef CONFIG_BT_LETWS
		bmr->broadcast_mode_reset = 1;
#else
		system_app_set_auracast_mode(0);
#endif
		system_app_launch_switch(DESKTOP_PLUGIN_ID_BMR,DESKTOP_PLUGIN_ID_BR_MUSIC);
		bt_manager_check_visual_mode();
	}
		break;
	default:
		break;
	}


}

void bmr_tts_event_proc(struct app_msg *msg)
{
	struct bmr_app_t *bmr = bmr_get_app();

	if (!bmr){
		return;
	}

	switch (msg->value) {
	case TTS_EVENT_START_PLAY:
		if(bmr->player_run){
			bmr_stop_playback();
			bmr_exit_playback();
			bmr->rx_start = 0;
		}
		bmr->tts_playing = 1;
		break;
	case TTS_EVENT_STOP_PLAY:
		if(bmr->broadcast_sink_enabled){
			bmr_init_playback();
			bmr_start_playback();
		}
		bmr->tts_playing = 0;
		break;
	}
}

/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt music media.
 */
#include "btmusic.h"
#include "media_mem.h"
#include <ringbuff_stream.h>
#include "tts_manager.h"
#include "ui_manager.h"
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif
#include <audio_track.h>

#define SUPPORT_RETRANSMIT_NUM_ADJUST
#define MEDIA_PACKET_TIME_THRESHOLD       20
#define MEDIA_LEVEL_HOLD_TIME              20

void btmusic_event_notify(u32_t event, void *data, u32_t len, void *user_data)
{
	if (event == PLAYBACK_EVENT_DATA_INDICATE) {
		btmusic_player_reset_trigger();
	}
}

static io_stream_t _btmusic_a2dp_create_inputstream(int stream_type)
{
	int ret = 0;
	io_stream_t input_stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (INPUT_PLAYBACK, stream_type),
				       media_mem_get_cache_pool_size
				       (INPUT_PLAYBACK, stream_type));

	if (!input_stream) {
		goto exit;
	}

	ret =
	    stream_open(input_stream,
			MODE_IN_OUT | MODE_READ_BLOCK | MODE_BLOCK_TIMEOUT);
	if (ret) {
		stream_destroy(input_stream);
		input_stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF(" %p type %d\n", input_stream,stream_type);
	return input_stream;
}

static void set_player_effect_output_mode(media_player_t *player)
{
	int mode = CONFIG_MEDIA_EFFECT_OUTMODE;
#ifndef CONFIG_BT_LETWS
	if(btmusic_get_auracast_mode()) {
#else
	//stereo mode
	if(3 == btmusic_get_auracast_mode()) {
#endif
#ifdef CONFIG_BT_SELF_APP
		u8_t ch;
		ch = selfapp_get_channel();
		if(ch == 0) {
			mode = MEDIA_EFFECT_OUTPUT_DEFAULT;
		} else if (ch == 1) {
			mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
		}else {
			mode = MEDIA_EFFECT_OUTPUT_R_ONLY;
		}
#endif
#ifdef ENABLE_PAWR_APP
		if (get_pawr_enable()) {
			mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
		}
#endif
	}

	SYS_LOG_INF("%d\n", mode);
	if(mode != CONFIG_MEDIA_EFFECT_OUTMODE) {
		media_player_set_effect_output_mode(player, mode);
	}
}

#ifdef CONFIG_EXTERNAL_DSP_DELAY
#ifdef CONFIG_MUSIC_MULTI_DEVICE_ALIGN_TO_RESET_CLK
static void btmusic_mute_handler(struct thread_timer *ttimer,
					     void *expiry_fn_arg)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	if (!btmusic)
		return;
	struct audio_track_t * track = audio_system_get_track();
	if(track){
		if(btmusic->mute_delay_flag == 0 && acts_dma_is_start((dma_reg_t *)track->phy_dma)){
			btmusic->mute_delay_flag = 1;
			btmusic->mute_delay_time = os_uptime_get_32();
		}

		if (btmusic->mute_delay_flag) {
			if (os_uptime_get_32() - btmusic->mute_delay_time >= 20 + CONFIG_EXTERNAL_DSP_DELAY / 1000) {
				// Wait to make sure unmute AMP when output zero data.
				// unmute AMP
				//audio i2s unmute
				extern void hm_ext_pa_start(void);
				hm_ext_pa_start();
				thread_timer_stop(&btmusic->mute_timer);
			}
		}
	}
}
#endif
#endif

int btmusic_init_playback()
{
	media_init_param_t init_param;
	io_stream_t input_stream = NULL;
	struct btmusic_app_t *btmusic = btmusic_get_app();
#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
	media_freqpoint_energy_info_t freqpoint_energy_info;
#endif
	struct bt_audio_chan *chan = &btmusic->sink_chan;
	struct bt_audio_chan_info chan_info;
	int ret;
	int stream_type = btmusic_get_auracast_mode() ? AUDIO_STREAM_SOUNDBAR : AUDIO_STREAM_MUSIC;

	if (!btmusic)
		return -EINVAL;

	if (btmusic->playback_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	ret = bt_manager_audio_stream_info(chan, &chan_info);
	if (ret) {
		return ret;
	}

	SYS_LOG_INF("codec_id: %d sample_rate %d mode %d\n", chan_info.format,
		    chan_info.sample,btmusic_get_auracast_mode());

#ifdef CONFIG_PLAYTTS
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	//tts_manager_filter_music_lock(__FUNCTION__);
#endif

	tts_manager_wait_finished(false);

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	//tts_manager_filter_music_unlock(__FUNCTION__);
#endif
#endif

	bt_manager_sync_volume_before_playing();

	btmusic_view_show_play_paused(true);

	input_stream = _btmusic_a2dp_create_inputstream(stream_type);
	if (!input_stream) {
		goto exit;
	}

	if (bt_manager_audio_codec_type(chan_info.format) == SBC_TYPE){
		btmusic->sbc_playing = true;
	}

	audio_system_set_output_sample_rate(48);

	memset(&init_param, 0, sizeof(media_init_param_t));
	init_param.type = MEDIA_SRV_TYPE_PLAYBACK;
	init_param.format = bt_manager_audio_codec_type(chan_info.format);
	init_param.stream_type = stream_type;
	init_param.efx_stream_type = AUDIO_STREAM_MUSIC;
	init_param.sample_rate = chan_info.sample;
	init_param.input_stream = input_stream;
	init_param.output_stream = NULL;
	init_param.event_notify_handle = NULL;
	init_param.support_tws = 0;
	init_param.dumpable = 1;
	init_param.sample_bits = 32;
	init_param.event_notify_handle = btmusic_event_notify;
	init_param.user_data = (void *)btmusic;
#if ENABLE_BROADCAST
	if(btmusic_get_auracast_mode()){
		init_param.waitto_start = 1;
		init_param.bind_to_capture = 1;

    if (btmusic->broadcast_duration == BT_FRAME_DURATION_7_5MS) {
		audio_policy_set_nav_frame_size_us(7500);		
	} else {
		audio_policy_set_nav_frame_size_us(10000);	
	}
		audio_policy_set_bis_link_delay_ms(broadcast_get_bis_link_delay(btmusic->qos));

#ifdef CONFIG_EXTERNAL_DSP_DELAY
#ifdef CONFIG_MUSIC_MULTI_DEVICE_ALIGN_TO_RESET_CLK
		// Wait to make sure mute AMP when output zero data.
		os_sleep(20 + CONFIG_EXTERNAL_DSP_DELAY / 1000);
		// mute AMP
		// -->
		extern void hm_ext_pa_stop(void);
		hm_ext_pa_stop();
		btmusic->mute_delay_flag = 0;
		thread_timer_init(&btmusic->mute_timer,
			  btmusic_mute_handler, NULL);
		thread_timer_start(&btmusic->mute_timer, 0, 10);
#endif
#endif
	}
#endif

	if (audio_policy_get_out_audio_mode(init_param.stream_type) ==
	    AUDIO_MODE_STEREO) {
		init_param.channels = 2;
	} else {
		init_param.channels = 1;
	}

	btmusic->sink_stream = input_stream;

	audio_system_mutex_lock();
	bt_manager_media_set_active_br_handle();
	bt_manager_volume_set(audio_system_get_stream_volume
			      (stream_type),BT_VOLUME_TYPE_BR_MUSIC);
	audio_system_mutex_unlock();

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if(stream_type == AUDIO_STREAM_MUSIC){
		btmusic->set_dvfs_level = BR_FREQ;
		soc_dvfs_set_level(btmusic->set_dvfs_level, "btmusic");
	}else{
		if(!btmusic->sbc_playing)
			soc_dvfs_set_level(BCST_FREQ_HIGH, "bms_br");
	}
#endif

	if (btmusic->ios_dev) {
		audio_policy_set_user_dynamic_waterlevel_ms(100);
	} else {
		audio_policy_set_user_dynamic_waterlevel_ms(0);
	}
	btmusic->playback_player = media_player_open(&init_param);
	if (!btmusic->playback_player) {
		goto err_exit;
	}
	set_player_effect_output_mode(btmusic->playback_player);

	btmusic->media_opened = 1;

	if(!btmusic_get_auracast_mode())
		bt_manager_audio_sink_stream_set(chan, input_stream);

#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
	memset(&freqpoint_energy_info, 0x0, sizeof(freqpoint_energy_info));
	freqpoint_energy_info.num_points = 9;
	freqpoint_energy_info.values[0] = 64;
	freqpoint_energy_info.values[1] = 125;
	freqpoint_energy_info.values[2] = 250;
	freqpoint_energy_info.values[3] = 500;
	freqpoint_energy_info.values[4] = 1000;
	freqpoint_energy_info.values[5] = 2000;
	freqpoint_energy_info.values[6] = 4000;
	freqpoint_energy_info.values[7] = 8000;
	freqpoint_energy_info.values[8] = 16000;
	media_player_set_freqpoint_energy(btmusic->playback_player,
					  &freqpoint_energy_info);
#endif

	SYS_LOG_INF("iso:%d Opened %p\n", btmusic->ios_dev,btmusic->playback_player);

	return 0;

 err_exit:
	if (input_stream) {
		stream_close(input_stream);
		stream_destroy(input_stream);
		btmusic->sink_stream = NULL;
	}
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if(stream_type == AUDIO_STREAM_MUSIC){
		soc_dvfs_unset_level(btmusic->set_dvfs_level, "btmusic");
		btmusic->set_dvfs_level = 0;
	}else{
		if(!btmusic->sbc_playing)
			soc_dvfs_unset_level(BCST_FREQ_HIGH, "bms_br");
	}
#endif

exit:
	SYS_LOG_ERR("open failed\n");
	return -EINVAL;

}

int btmusic_start_playback(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic->playback_player) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	if (btmusic->playback_player_run) {
		SYS_LOG_INF("already\n");
		return -EINVAL;
	}

	if(btmusic_get_auracast_mode()){
		bt_manager_broadcast_stream_tws_sync_cb_register_1(btmusic->chan,
			media_player_audio_track_trigger_callback,audio_system_get_track());
	}

	if (btmusic->mute_player) {
		if(audio_system_get_track()){
			audio_track_mute(audio_system_get_track(), 1);
		}
	}else{
#ifdef CONFIG_EXTERNAL_DSP_DELAY
		media_player_fade_in(btmusic->playback_player, 150 + CONFIG_EXTERNAL_DSP_DELAY / 1000);
#else
		media_player_fade_in(btmusic->playback_player, 200);
#endif
	}
	media_player_play(btmusic->playback_player);

	btmusic->playback_player_run = 1;
    btmusic->pcm_time = 0;
    btmusic->bis_delay = broadcast_get_bis_link_delay(btmusic->qos);

	SYS_LOG_INF("%p bis_delay:%d\n", btmusic->playback_player,btmusic->bis_delay);

	return 0;
}

int btmusic_stop_playback(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic || !btmusic->playback_player || !btmusic->playback_player_run) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	MSG_SEND_TIME_STAT_START();

	if(btmusic_get_auracast_mode()){
		bt_manager_broadcast_stream_tws_sync_cb_register_1(btmusic->chan,
			NULL,NULL);
#ifdef CONFIG_EXTERNAL_DSP_DELAY
#ifdef CONFIG_MUSIC_MULTI_DEVICE_ALIGN_TO_RESET_CLK
		extern void hm_ext_pa_start(void);
		hm_ext_pa_start();
		if (thread_timer_is_running(&btmusic->mute_timer))
			thread_timer_stop(&btmusic->mute_timer);
#endif
#endif
	}

	SYS_LOG_INF("%p\n", btmusic->playback_player);

	media_player_fade_out(btmusic->playback_player, 60);

	/** reserve time to fade out*/
	os_sleep(audio_policy_get_bis_link_delay_ms() + 80);

	btmusic_view_show_play_paused(false);

	bt_manager_audio_sink_stream_set(&btmusic->sink_chan, NULL);

	if (btmusic->sink_stream) {
		stream_close(btmusic->sink_stream);
	}

	media_player_stop(btmusic->playback_player);

	MSG_SEND_TIME_STAT_STOP();

	btmusic->media_opened = 0;
    btmusic->pcm_time = 0;

#ifdef CONFIG_EXTERNAL_DSP_DELAY
	// wait ext-dsp data flushed to avoid noise.
	os_sleep(CONFIG_EXTERNAL_DSP_DELAY / 1000);
#ifdef CONFIG_MUSIC_MULTI_DEVICE_ALIGN_TO_RESET_CLK
	// if mute AMP, should umute
#endif
#endif

	return 0;
}

int btmusic_exit_playback()
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic->playback_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	media_player_close(btmusic->playback_player);

	if (btmusic->sink_stream) {
		stream_destroy(btmusic->sink_stream);
		btmusic->sink_stream = NULL;
	}

	if(btmusic_get_auracast_mode())
		audio_policy_set_bis_link_delay_ms(0);

	btmusic->playback_player_run = 0;
	btmusic->playback_player = NULL;

	SYS_LOG_INF("%p mode %d\n", btmusic->playback_player,btmusic_get_auracast_mode());

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if(!btmusic_get_auracast_mode()){
		if (btmusic->set_dvfs_level) {
			soc_dvfs_unset_level(btmusic->set_dvfs_level, "btmusic");
			btmusic->set_dvfs_level = 0;
		}
	}else{
		if(!btmusic->sbc_playing)
			soc_dvfs_unset_level(BCST_FREQ_HIGH, "bms_br");
	}
#endif
	btmusic->sbc_playing = false;
	return 0;
}

#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
int bt_music_a2dp_get_freqpoint_energy(media_freqpoint_energy_info_t * info)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic || !btmusic->media_opened)
		return -1;

	if (!btmusic->playback_player)
		return -1;

	return media_player_get_freqpoint_energy(btmusic->playback_player, info);
}
#endif

static io_stream_t broadcast_create_source_stream(int mem_type, int block_num)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	io_stream_t stream = NULL;
	int buff_size;
	int ret;

	buff_size = media_mem_get_cache_pool_size_ext(mem_type, AUDIO_STREAM_SOUNDBAR, NAV_TYPE, btmusic->qos->max_sdu, block_num);	// TODO: 3?
	if (buff_size <= 0) {
		goto exit;
	}

	stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (mem_type, AUDIO_STREAM_SOUNDBAR),
				       buff_size);
	if (!stream) {
		goto exit;
	}

	ret = stream_open(stream, MODE_OUT);
	if (ret) {
		stream_destroy(stream);
		stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF("%p\n", stream);
	return stream;
}

static io_stream_t _btmusic_bms_create_capture_input_stream(void)
{
	io_stream_t input_stream;
	int ret;

	input_stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (INPUT_CAPTURE, AUDIO_STREAM_SOUNDBAR),
				       media_mem_get_cache_pool_size
				       (INPUT_CAPTURE, AUDIO_STREAM_SOUNDBAR));

	if (!input_stream) {
		goto exit;
	}

	ret =
	    stream_open(input_stream,
			MODE_IN_OUT | MODE_READ_BLOCK | MODE_BLOCK_TIMEOUT);
	if (ret) {
		stream_destroy(input_stream);
		input_stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF("%p\n", input_stream);
	return input_stream;
}

static void _btmusic_bms_broadcast_source_stream_set(uint8 enable_flag)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &btmusic->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			continue;
		}
		if (enable_flag) {
			bt_manager_broadcast_source_stream_set(chan,
							       btmusic->stream[i]);
		} else {
			bt_manager_broadcast_source_stream_set(chan, NULL);
		}

	}
}

uint8_t btmusic_get_stereo_channel(void)
{
	uint8_t ch = 0;
#ifdef CONFIG_BT_SELF_APP
	ch = selfapp_get_channel();
#endif
//    ch = MEDIA_EFFECT_OUTPUT_L_ONLY;
	SYS_LOG_INF("channel:%d \n",ch);
	return ch;
}

int btmusic_bms_init_capture(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_broadcast_chan *chan;
	struct bt_broadcast_chan_info chan_info;
	io_stream_t source_stream = NULL;
	io_stream_t source_stream2 = NULL;
	io_stream_t input_stream = NULL;
	media_init_param_t init_param;
	int ret;

	if (NULL == btmusic) {
		return -EINVAL;
	}
	if (btmusic->capture_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	chan = btmusic->chan;
	ret = bt_manager_broadcast_stream_info(chan, &chan_info);
	if (ret) {
		return ret;
	}

	/* wait until start_capture */
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	source_stream = broadcast_create_source_stream(OUTPUT_CAPTURE, 6);
	if (!source_stream) {
		goto exit;
	}
#if (BROADCAST_NUM_BIS == 2)
	if (btmusic->num_of_broad_chan == 2) {
	   source_stream2 = broadcast_create_source_stream(OUTPUT_CAPTURE2, 6);
	   if (!source_stream2) {
	    	goto exit;
	   }
	}
#endif

	input_stream = _btmusic_bms_create_capture_input_stream();
	if (!input_stream) {
		goto exit;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));

	init_param.type = MEDIA_SRV_TYPE_CAPTURE;
	init_param.stream_type = AUDIO_STREAM_SOUNDBAR;
	init_param.efx_stream_type = AUDIO_STREAM_SOUNDBAR;
	// TODO: broadcast_source no need to get chan_info???
	init_param.capture_format = NAV_TYPE;	// bt_manager_audio_codec_type(chan_info.format);
	init_param.capture_sample_rate_input = 48;	// chan_info.sample;
	init_param.capture_sample_rate_output = btmusic->broadcast_sample_rate;	// chan_info.sample;
	init_param.capture_sample_bits = 32;
	//to do?
	init_param.capture_channels_input = 2;	//BROADCAST_CH;   // chan_info.channels;
    init_param.capture_channels_output = btmusic->num_of_broad_chan;
    init_param.capture_bit_rate = chan_info.kbps*btmusic->num_of_broad_chan;

	init_param.capture_input_stream = input_stream;
	init_param.capture_output_stream = source_stream;
	init_param.capture_output_stream2 = source_stream2;
	init_param.waitto_start = 1;
	init_param.device_info.tx_chan.validate = 1;
	init_param.device_info.tx_chan.type = MEDIA_AUDIO_DEVICE_TYPE_BIS;
	init_param.device_info.tx_chan.bis_chan.handle = chan->handle;
	init_param.device_info.tx_chan.bis_chan.id = chan->id;
	init_param.device_info.tx_chan.timeline_owner = chan_info.chan;

	if (chan_info.duration == 7) {
		audio_policy_set_nav_frame_size_us(7500);
	} else {
		audio_policy_set_nav_frame_size_us(10000);
	}
	SYS_LOG_INF("chn_count:%d\n",btmusic->num_of_broad_chan);

	SYS_LOG_INF
	    ("format: %d, sample: %d, channels: %d, kbps: %d, duration: %d,handle:%d,id:%d,tl_owner:%p\n",
	     chan_info.format, chan_info.sample, chan_info.channels,
	     chan_info.kbps, chan_info.duration, chan->handle, chan->id,
	     chan_info.chan);

	btmusic->capture_player = media_player_open(&init_param);
	if (!btmusic->capture_player) {
		goto exit;
	}

	btmusic->stream[0] = source_stream;
	btmusic->stream[1] = source_stream2;
	btmusic->input_stream = input_stream;
	SYS_LOG_INF("%p\n", btmusic->capture_player);

	return 0;

 exit:
	if (source_stream) {
		stream_close(source_stream);
		stream_destroy(source_stream);
	}
	if (source_stream2) {
		stream_close(source_stream2);
		stream_destroy(source_stream2);
	}
	if (input_stream) {
		stream_close(input_stream);
		stream_destroy(input_stream);
	}

	SYS_LOG_ERR("open failed\n");
	return -EINVAL;
}

int btmusic_bms_check_br_audio_stream(u16_t pcm_time,u16_t normal_level,u8_t aps_status)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	uint8_t need_adjust = false;
    u16_t level_low = 0;
    u16_t level_normal = normal_level;

	if (!btmusic->capture_player_run) {
		return -EINVAL;
	}

	if (!btmusic->broadcast_source_enabled) {
		return 0;
	}

    if(normal_level > btmusic->bis_delay){
        level_normal = normal_level - btmusic->bis_delay;
    }

	if (!btmusic->dsp_run) {
		if (pcm_time >= level_normal){
			btmusic->dsp_run = 1;
		}
	}

    if(btmusic->dsp_run){
        level_low = level_normal  >> 2;
        if (pcm_time <= (level_low + btmusic->bis_delay + MEDIA_PACKET_TIME_THRESHOLD)){
		    btmusic->broadcast_level_hold_time = 0;
            if(btmusic->broadcast_retransmit_number != 1){
                btmusic->broadcast_retransmit_number = 1;
                need_adjust = true;
            }
        }
        else if (pcm_time <= ((level_normal >> 1) + btmusic->bis_delay) + MEDIA_PACKET_TIME_THRESHOLD){
            btmusic->broadcast_level_hold_time = 0;
            if(pcm_time >= (level_normal >> 1) + btmusic->bis_delay){
                if(btmusic->broadcast_retransmit_number != 2){
                    btmusic->broadcast_retransmit_number = 2;
                    need_adjust = true;
                }
            }
            else{
                if(btmusic->broadcast_retransmit_number > 2){
                    btmusic->broadcast_retransmit_number = 2;
                    need_adjust = true;
                }
            }
        }
		else if (pcm_time <= level_low * 3 + btmusic->bis_delay +  MEDIA_PACKET_TIME_THRESHOLD) {
		    btmusic->broadcast_level_hold_time = 0;
            if(pcm_time >= level_low * 3 + btmusic->bis_delay){
                if(btmusic->broadcast_retransmit_number < (BROADCAST_TRANSMIT_NUMBER -1)){
                    btmusic->broadcast_retransmit_number += 1;
                    need_adjust = true;
                }
                else{
                    if(btmusic->broadcast_retransmit_number != BROADCAST_TRANSMIT_NUMBER -1){
                        btmusic->broadcast_retransmit_number = BROADCAST_TRANSMIT_NUMBER -1;
                        need_adjust = true;
                    }
                }
            }
            else{
                if(btmusic->broadcast_retransmit_number > BROADCAST_TRANSMIT_NUMBER -1){
                    btmusic->broadcast_retransmit_number = BROADCAST_TRANSMIT_NUMBER -1;
                    need_adjust = true;
                }
                else if(btmusic->broadcast_retransmit_number < 2){
                    btmusic->broadcast_retransmit_number = 2;
                    need_adjust = true;
                }
            }
		}
		else{
            if(pcm_time >= level_normal + btmusic->bis_delay){
                if(btmusic->broadcast_retransmit_number < BROADCAST_TRANSMIT_NUMBER){
                    btmusic->broadcast_retransmit_number += 1;
                    need_adjust = true;
                }
            }
            else{
                if(btmusic->broadcast_retransmit_number < (BROADCAST_TRANSMIT_NUMBER -1)){
                    btmusic->broadcast_retransmit_number = BROADCAST_TRANSMIT_NUMBER -1;
                    need_adjust = true;
                }
                else{
                    if(btmusic->broadcast_retransmit_number == BROADCAST_TRANSMIT_NUMBER -1){
                        if(pcm_time >= (level_normal + btmusic->bis_delay - MEDIA_PACKET_TIME_THRESHOLD)){
                            btmusic->broadcast_level_hold_time++;
                            if(btmusic->broadcast_level_hold_time >= MEDIA_LEVEL_HOLD_TIME){
                                btmusic->broadcast_level_hold_time = 0;
                                btmusic->broadcast_retransmit_number = BROADCAST_TRANSMIT_NUMBER;
                                need_adjust = true;
                            }
                        }
                    }
                }
            }
		}

		if (need_adjust) {
			btif_broadcast_source_set_retransmit(btmusic->chan,
                btmusic->broadcast_retransmit_number);
			SYS_LOG_INF("pcm:%d level:%d re:%d\n",pcm_time,level_normal,
				    btmusic->broadcast_retransmit_number);
            btmusic->pcm_time = pcm_time;
		}
	}

	return 0;
}

int btmusic_bms_start_capture(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic->capture_player) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	if (btmusic->capture_player_run) {
		SYS_LOG_INF("already\n");
		return -EINVAL;
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	media_player_play(btmusic->capture_player);

	_btmusic_bms_broadcast_source_stream_set(1);

	btmusic->capture_player_run = 1;

#ifdef SUPPORT_RETRANSMIT_NUM_ADJUST
	audio_asp_set_pcm_monitor_callback(btmusic_bms_check_br_audio_stream);
	btif_broadcast_source_set_retransmit(btmusic->chan,
					     BROADCAST_TRANSMIT_NUMBER);
#endif

	SYS_LOG_INF("%p\n", btmusic->capture_player);

	return 0;
}

int btmusic_bms_stop_capture(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	int i;

	if (!btmusic->capture_player || !btmusic->capture_player_run) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	_btmusic_bms_broadcast_source_stream_set(0);

	media_player_stop(btmusic->capture_player);

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (btmusic->stream[i]) {
			stream_close(btmusic->stream[i]);
		}
	}

	if (btmusic->input_stream) {
		stream_close(btmusic->input_stream);
	}

	btmusic->capture_player_run = 0;
	btmusic->dsp_run = 0;

#ifdef SUPPORT_RETRANSMIT_NUM_ADJUST
	audio_asp_set_pcm_monitor_callback(NULL);
#endif

	SYS_LOG_INF("%p\n", btmusic->capture_player);

	return 0;
}

int btmusic_bms_exit_capture(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	int i;

	if (!btmusic->capture_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	media_player_close(btmusic->capture_player);
	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (btmusic->stream[i]) {
			stream_destroy(btmusic->stream[i]);
			btmusic->stream[i] = NULL;
		}
	}
	if (btmusic->input_stream) {
		stream_destroy(btmusic->input_stream);
		btmusic->input_stream = NULL;
	}

	SYS_LOG_INF("%p\n", btmusic->capture_player);

	btmusic->capture_player = NULL;
	btmusic->capture_player_run = 0;
	btmusic->dsp_run = 0;

#ifdef SUPPORT_RETRANSMIT_NUM_ADJUST
	audio_asp_set_pcm_monitor_callback(NULL);
#endif
	return 0;
}

/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief usound media
 */

#include "usound.h"
#include "buffer_stream.h"
#include "ringbuff_stream.h"
#include "media_mem.h"


static void usound_media_palyer_event_notify(u32_t event, void *data, u32_t len, void *user_data)
{
	if (event == PLAYBACK_EVENT_DATA_INDICATE) {
#ifdef USOUND_FEATURE_RESTART
		usound_restart_player_trigger();
#endif
	}
}

static io_stream_t usound_media_create_playback_input_stream(void)
{
	int ret = 0;
	io_stream_t input_stream = NULL;

	input_stream = ringbuff_stream_create_ext(media_mem_get_cache_pool(INPUT_PLAYBACK, AUDIO_STREAM_USOUND),
						media_mem_get_cache_pool_size(INPUT_PLAYBACK, AUDIO_STREAM_USOUND));
	if (!input_stream) {
		goto exit;
	}

	ret = stream_open(input_stream, MODE_IN_OUT | MODE_READ_BLOCK | MODE_BLOCK_TIMEOUT);
	if (ret) {
		stream_destroy(input_stream);
		input_stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF("%p", input_stream);
	return input_stream;
}

static void usound_media_set_effect_output_mode(media_player_t *player)
{
	int mode = CONFIG_MEDIA_EFFECT_OUTMODE;

#ifdef CONFIG_USOUND_BROADCAST_SUPPROT
	if (usound_broadcast_is_bms_mode()) {
#ifdef CONFIG_BT_SELF_APP
		u8_t ch = selfapp_get_channel();
		if (ch == 0) {
			mode = MEDIA_EFFECT_OUTPUT_DEFAULT;
		} else if (ch == 1) {
			mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
		} else {
			mode = MEDIA_EFFECT_OUTPUT_R_ONLY;
		}
#endif
	}
#endif

	SYS_LOG_INF("%d", mode);
	if (mode != CONFIG_MEDIA_EFFECT_OUTMODE) {
		media_player_set_effect_output_mode(player, mode);
	}
}

void usound_playback_start(void)
{
	struct usound_app_t *usound = usound_get_app();
	int volume = 0;
	media_init_param_t init_param;
	io_stream_t input_stream = NULL;

	if (!usound)
		return;

#ifdef CONFIG_PLAYTTS
	if (tts_manager_is_playing()) {
		SYS_LOG_INF("abort ttsplaying");
		usound->need_resume_play = 1;
		return ;
	}
#endif

	if (usound->playback_player) {
		SYS_LOG_INF("already");
		return;
	}

	audio_system_set_output_sample_rate(48);

	memset(&init_param, 0, sizeof(media_init_param_t));

	input_stream = usound_media_create_playback_input_stream();
	if (!input_stream) {
		goto err_exit;
	}

	init_param.type = MEDIA_SRV_TYPE_PLAYBACK;
	init_param.format = PCM_TYPE;
	init_param.stream_type = AUDIO_STREAM_USOUND;
	init_param.efx_stream_type = AUDIO_STREAM_USOUND;
	init_param.input_stream = input_stream;
	init_param.output_stream = NULL;
	init_param.support_tws = 0;
	init_param.dumpable = 1;
	init_param.channels = CONFIG_USB_AUDIO_DOWNLOAD_CHANNEL_NUM;
	init_param.sample_rate = (uint8_t)(USB_AudioSinkSampleRateGet() / 1000);
	init_param.sample_bits = 32;  // usb-audio driver gives 32bit data
	init_param.event_notify_handle = usound_media_palyer_event_notify;

	usound->download_sr = USB_AudioSinkSampleRateGet();

#if CONFIG_USOUND_BROADCAST_SUPPROT
	if (usound_broadcast_is_bms_mode() && usound->bms_source) {
		int frameus = (BROADCAST_DURATION == BT_FRAME_DURATION_7_5MS) ? 7500 : 10000;
		init_param.waitto_start = 1;
		init_param.bind_to_capture = 1;
		init_param.user_data = (void *)usound;

		audio_policy_set_nav_frame_size_us(frameus);
		audio_policy_set_bis_link_delay_ms(broadcast_get_bis_link_delay(usound->qos));
	}
#endif

	usound->playback_player = media_player_open(&init_param);
	if (!usound->playback_player) {
		SYS_LOG_ERR("open failed");
		goto err_exit;
	}

	usound->usound_stream = init_param.input_stream;
	usound_media_set_effect_output_mode(usound->playback_player);

#if CONFIG_USOUND_BROADCAST_SUPPROT
	//For bms mode, set usound stream with playback input stream later after capture is started.
	if (usound_broadcast_is_bms_mode() && usound->bms_source == 1) {
		bt_manager_broadcast_stream_tws_sync_cb_register_1(usound->chan, media_player_audio_track_trigger_callback, audio_system_get_track());
	} else
#endif
	{
		usb_audio_set_stream(usound->usound_stream);
	}

	media_player_fade_in(usound->playback_player, 80);
	media_player_play(usound->playback_player);

	volume = audio_system_get_stream_volume(AUDIO_STREAM_USOUND);
	media_player_set_volume(usound->playback_player, volume, volume);

	usound->playing = 1;
	usound->playback_player_run = 1;
	SYS_LOG_INF("%p ok", usound->playback_player);
	return;

 err_exit:
	if (input_stream) {
		stream_close(input_stream);
		stream_destroy(input_stream);
	}
	SYS_LOG_INF("failed");
}

void usound_playback_stop(void)
{
	struct usound_app_t *usound = usound_get_app();

	if (usound == NULL || usound->playback_player == NULL) {
		return;
	}

#if CONFIG_USOUND_BROADCAST_SUPPROT
	if (usound_broadcast_is_bms_mode()) {
		bt_manager_broadcast_stream_tws_sync_cb_register_1(usound->chan, NULL, NULL);
	}
#endif

	media_player_fade_out(usound->playback_player, 60);

	/** reserve time to fade out*/
	os_sleep(audio_policy_get_bis_link_delay_ms() + 80);

	if (usound->usound_stream) {
		usb_audio_set_stream(NULL);
		stream_close(usound->usound_stream);
	}

	media_player_stop(usound->playback_player);
	media_player_close(usound->playback_player);
	SYS_LOG_INF("%p ok", usound->playback_player);
	usound->playback_player = NULL;

#if CONFIG_USOUND_BROADCAST_SUPPROT
	if (usound_broadcast_is_bms_mode()) {
		audio_policy_set_bis_link_delay_ms(0);
	}
#endif

	if (usound->usound_stream) {
		stream_destroy(usound->usound_stream);
		usound->usound_stream = NULL;
	}

	usound->playback_player_run = 0;
	usound->playing = 0;
	//clear restart at player stop.
	usound->restart_count = 0;
}

void usound_play_pause(void)
{
	struct usound_app_t *usound = usound_get_app();
	if (usound == NULL) {
		return;
	}

#ifdef CONFIG_USOUND_BROADCAST_SUPPROT
	SYS_LOG_INF("mode %d, source %d", usound_broadcast_is_bms_mode(), usound->broadcast_source_enabled);
	if (usound_broadcast_is_bms_mode()) {
		if (usound->capture_player_run) {
			bms_uac_stop_capture();
			bms_uac_exit_capture();
		}
		usound->tx_start = 0;
		usound->tx_sync_start = 0;
	}
#endif

	if (usound->playback_player_run) {
		usound_playback_stop();
	}

}

void usound_play_resume(void)
{
	struct usound_app_t *usound = usound_get_app();
	if (usound == NULL) {
		return;
	}

	if (!usound->playback_player_run) {
		usound_playback_start();
	}

#ifdef CONFIG_USOUND_BROADCAST_SUPPROT
	SYS_LOG_INF("mode %d, source %d", usound_broadcast_is_bms_mode(), usound->broadcast_source_enabled);
	if (usound_broadcast_is_bms_mode()) {
		if (usound->broadcast_source_enabled) {
			if (!usound->capture_player_run) {
				bms_uac_init_capture();
				bms_uac_start_capture();
			}
		}
	}
#endif
}



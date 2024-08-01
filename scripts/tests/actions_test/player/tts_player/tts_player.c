/********************************************************************************
 *                            USDK(ATS350B_linux)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-8-20-下午3:33:35             1.0             build this file
 ********************************************************************************/
/*!
 * \file     tts_player.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-8-20-下午3:33:35
 *******************************************************************************/
#include <sdk.h>
#include <greatest_api.h>
#include "tts_test_acts.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TTS_PLAYER

#if 0
typedef struct
{
    void *player;
    uint32 source_handle;
}tts_player_handle_t;

static int tts_open_source(void *player)
{
    int source_handle;

    audio_manager_runtime_info_t *cfg;

    mix_source_param_t tts_source_param;

    cfg = audio_get_player_device_cfg(PLAYER_MAJOR_TYPE_LOCALMUSIC, ACT_TYPE);

    tts_source_param.gain = 0x8000;
    tts_source_param.player_handle = player;
    tts_source_param.use_asrc = FALSE;
    tts_source_param.sink_id = cfg->playback_cfg->snd_id;
    tts_source_param.sample_rate = SAMPLE_16KHZ;
    tts_source_param.read_data = player_get_playback_data;

	source_handle = mix_source_open(&tts_source_param);

    return source_handle;
}

int play_tts_file(void)
{
	int res = 0;

	void *io;

	void *snd_cfg;

	io_stream_t  tonestream;

	uint32 source_handle;

    app_soundcard_param_t souncard_param;

    audio_stream_t stream_info;

    tts_player_handle_t tts_handle;

    struct buffer_t buffer;

    mix_source_param_t tts_source_param;

    player_status_e run_status;

	buffer.base = acV_POWON;
	buffer.length = sizeof(acV_POWON);
	buffer.cache_size = 0;

	tonestream = stream_create(TYPE_BUFFER_STREAM, &buffer);

	if(tonestream == NULL)
	{
		ACT_LOG_ID_ERR(ALF_STR_XXX__STREAM_CREATE_FAILED, 0);
		res = -1;
		return res;
	}

	res = stream_open(tonestream, MODE_IN);

	if (res) {
		ACT_LOG_ID_ERR(ALF_STR_XXX__STREAM_OPEN_FAILED_N, 0);
		stream_destroy(tonestream);
		tonestream = NULL;
		return res;
	}

	souncard_param.major_type = PLAYER_MAJOR_TYPE_LOCALMUSIC;
	souncard_param.minor_type = ACT_TYPE;
	souncard_param.input_sr = SAMPLE_16KHZ;
	souncard_param.output_sr = SAMPLE_16KHZ;

	tts_handle.player = audio_player_open();

	tonestream->format = ACT_TYPE;

	memset(&stream_info, 0, sizeof(stream_info));

	stream_info.audio_down_stream = tonestream;
	stream_info.audio_up_stream = NULL;
	stream_info.app_callback = audio_player_callback;
	stream_info.app_data = (void *)(tts_handle.player);

	io = storage_io_init(souncard_param.major_type, stream_info.audio_down_stream, \
	    stream_info.audio_up_stream);
	
    //获取硬件配置信息
    snd_cfg = audio_soundcard_get_config(&souncard_param);

	audio_media_set_stream(tts_handle.player, snd_cfg, &stream_info);
	
	player_set_file(tts_handle.player, io, NULL);    

    //设置声卡
    audio_soundcard_open(snd_cfg, souncard_param.input_sr, souncard_param.output_sr);

	source_handle = tts_open_source(tts_handle.player);

    audio_player_play(tts_handle.player);

    while(1){
        player_get_status(tts_handle.player, &run_status);
        if(run_status == PLAYER_STATUS_READY){
            break;
        }

        os_sleep(2000);
    }

    player_clear_file(tts_handle.player);

    audio_player_close(tts_handle.player);
   
    mix_source_close(source_handle);

    stream_close(tonestream);

    audio_soundcard_close(snd_cfg);

    storage_io_deinit(io);

	return res;
}

TEST tts_play_test(void){
    play_tts_file();
}


ADD_TEST_CASE("tts_test")
{
    RUN_TEST(tts_play_test);
}

#endif

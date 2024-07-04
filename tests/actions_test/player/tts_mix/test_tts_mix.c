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
#include "tts_test_mix.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TEST_TTS_MIX

typedef struct
{
    void *player;
    uint32 source_handle;
	io_stream_t  tonestream;
    void *io;    
    void *snd_cfg;
}tts_player_handle_t;

#if 0

io_stream_t tts_create_stram(uint8 *stream_buffer, uint32 stream_len)
{
    int res = 0;

    struct buffer_t buffer;

    io_stream_t  tonestream;

	buffer.base = stream_buffer;
	buffer.length = stream_len;
	buffer.cache_size = 0;

	tonestream = stream_create(TYPE_BUFFER_STREAM, &buffer);

	if(tonestream == NULL)
	{
		ACT_LOG_ID_ERR(ALF_STR_XXX__STREAM_CREATE_FAILED, 0);
		return NULL;
	}

	res = stream_open(tonestream, MODE_IN);

	if (res) {
		ACT_LOG_ID_ERR(ALF_STR_XXX__STREAM_OPEN_FAILED_N, 0);
		stream_destroy(tonestream);
		return NULL;
	}

	tonestream->format = ACT_TYPE;

	return tonestream;
}


void *tts_open_player(tts_player_handle_t *tts_handle)
{    
    audio_stream_t stream_info;
  
    app_soundcard_param_t souncard_param;

    memset(&stream_info, 0, sizeof(audio_stream_t));
   
	tts_handle->player = audio_player_open();

	souncard_param.major_type = PLAYER_MAJOR_TYPE_LOCALMUSIC;
	souncard_param.minor_type = ACT_TYPE;
	souncard_param.input_sr = SAMPLE_16KHZ;
	souncard_param.output_sr = SAMPLE_16KHZ;

	stream_info.audio_down_stream = tts_handle->tonestream;
	stream_info.audio_up_stream = NULL;
	stream_info.app_callback = audio_player_callback;
	stream_info.app_data = (void *)(tts_handle->player);

	tts_handle->io = storage_io_init(souncard_param.major_type, stream_info.audio_down_stream, \
	    stream_info.audio_up_stream);

    //获取硬件配置信息
    tts_handle->snd_cfg = audio_soundcard_get_config(&souncard_param);

	audio_media_set_stream(tts_handle->player, tts_handle->snd_cfg, &stream_info);
	
	player_set_file(tts_handle->player, tts_handle->io, NULL);    

    //设置声卡
    audio_soundcard_open(tts_handle->snd_cfg, souncard_param.input_sr, souncard_param.output_sr);

	return tts_handle->player;
}

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

const int16 keytone_data[] =
{
    0x66, 0xFD,
    0x1E, 0x01,
    0x28, 0x03,
    0xBC, 0xFC,
    0xB0, 0x00,
    0x70, 0x00,
    0x88, 0x00,
    0x36, 0xFC,
    0xDA, 0x03,
    0x00, 0x01,
    0x9E, 0xFD,
    0xC2, 0xFE,
    0x48, 0x00,
    0xE6, 0x04,
    0x1A, 0xFC,
    0xEE, 0xFE,
};

int read_keytone_data(void *player_handle, uint8_t * buf, uint32_t sample_len)
{
    int i;

    int read_len = sample_len * 4;

    i = 0;
    for(i = 0; i < read_len; i += 32){
        memcpy(buf, keytone_data, 32);
        buf += 32;
    }

    return sample_len;
}

void *keytone_open_source(void)
{
    void *source_handle;

    audio_manager_runtime_info_t *cfg;

    mix_source_param_t keytone_source_param;

    cfg = audio_get_player_device_cfg(PLAYER_MAJOR_TYPE_LOCALMUSIC, ACT_TYPE);

    keytone_source_param.gain = 0x8000;
    keytone_source_param.player_handle = NULL;
    keytone_source_param.use_asrc = FALSE;
    keytone_source_param.sink_id = cfg->playback_cfg->snd_id;
    keytone_source_param.sample_rate = SAMPLE_16KHZ;
    keytone_source_param.read_data = read_keytone_data;

	source_handle = mix_source_open(&keytone_source_param);

    return source_handle;

}

int play_tts_mix_file(void)
{
	int res = 0;

	tts_player_handle_t tts_handle;

	int keytone_source_handle;

	player_status_e run_status;

    tts_handle.tonestream = tts_create_stram(acV_POWON, sizeof(acV_POWON));
   
	tts_handle.tonestream->format = ACT_TYPE;
    
    tts_open_player(&tts_handle); 

    tts_handle.source_handle = tts_open_source(tts_handle.player);

    keytone_source_handle = keytone_open_source();

    audio_player_play(tts_handle.player);

    while(1){
        player_get_status(tts_handle.player, &run_status);

        ACT_LOG_ID_INF(ALF_STR_XXX__TTS_RUN_STATUS_DN, 1, run_status);

        if(run_status == PLAYER_STATUS_READY){
            break;
        }

        os_sleep(2000);
    }
 
    player_clear_file(tts_handle.player);

    mix_source_close(tts_handle.source_handle);

    mix_source_close(keytone_source_handle);

    audio_player_close(tts_handle.player);

    audio_soundcard_close(tts_handle.snd_cfg);

    stream_close(tts_handle.tonestream);

    storage_io_deinit(tts_handle.io);

	return res;
}

TEST tts_play_mix(void){
    play_tts_mix_file();
}


ADD_TEST_CASE("tts_mix")
{
    RUN_TEST(tts_play_mix);
    RUN_TEST(tts_play_mix);
}
#endif

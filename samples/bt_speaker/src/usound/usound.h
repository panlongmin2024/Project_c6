/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief usound function.
 */


#ifndef _USOUND_APP_H
#define _USOUND_APP_H


#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "usound"
#include <logging/sys_log.h>

#include <mem_manager.h>
#include <volume_manager.h>
#include <thread_timer.h>
#include <media_player.h>
#include <audio_system.h>
#include <audio_policy.h>
#include <bt_manager.h>
#include <usb_audio_hal.h>
#include <soc_dvfs.h>
#include <desktop_manager.h>

#include "app_defines.h"
#include "app_ui.h"
#include "broadcast.h"
#include "tts_manager.h"
#include "selfapp_api.h"



#if (0 == CONFIG_DSP_COMPUTE_COMPLEXITY)
#define BCST_FREQ                SOC_DVFS_LEVEL_HIGH_PERFORMANCE
#define BCST_FREQ_HIGH           SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE
#define BR_FREQ                  SOC_DVFS_LEVEL_ALL_PERFORMANCE
#elif (1 == CONFIG_DSP_COMPUTE_COMPLEXITY)
#define BCST_FREQ                SOC_DVFS_LEVEL_HIGH_PERFORMANCE
#define BCST_FREQ_HIGH           SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE
#define BR_FREQ                  SOC_DVFS_LEVEL_ALL_PERFORMANCE
#elif (2 <= CONFIG_DSP_COMPUTE_COMPLEXITY)
#define BCST_FREQ                SOC_DVFS_LEVEL_FULL_PERFORMANCE
#define BCST_FREQ_HIGH           SOC_DVFS_LEVEL_FULL_PERFORMANCE
#define BR_FREQ                  SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE
#endif


#define USOUND_FEATURE_RESTART				(1)
#define USOUND_FEATURE_DISABLE_BLUETOOTH	(1)

enum {
	MSG_USOUND_PLAY_PAUSE_RESUME = MSG_APP_INPUT_MESSAGE_CMD_START,
	MSG_USOUND_PLAY_VOLUP,
	MSG_USOUND_PLAY_VOLDOWN,
	MSG_USOUND_PLAY_NEXT,
	MSG_USOUND_PLAY_PREV,
	MSG_USOUND_SWITCH_BROADCAST,
	MSG_USOUND_EXIT_TO_BT,
};

enum {
	MSG_USOUND_STREAM_START = 1,
	MSG_USOUND_STREAM_STOP,
	MSG_USOUND_STREAM_RESTART,
	MSG_USOUND_MIC_OPEN,
	MSG_USOUND_MIC_CLOSE,
	MSG_USOUND_VOL_UPDATE,
};

enum USOUND_PLAY_STATUS {
	USOUND_STATUS_NULL = 0x0000,
	USOUND_STATUS_PLAYING = 0x0001,
	USOUND_STATUS_PAUSED = 0x0002,
};

enum USOUND_VOLUME_REQ_TYPE {
	USOUND_VOLUME_NONE = 0x0000,
	USOUND_VOLUME_DEC = 0x0001,
	USOUND_VOLUME_INC = 0x0002,
};

#define USOUND_STATUS_ALL  (USOUND_STATUS_PLAYING | USOUND_STATUS_PAUSED)


struct usound_app_t {
	media_player_t *playback_player;
	struct thread_timer monitor_timer;
	io_stream_t  usound_stream;

	u8_t  playing :1;
	u8_t  playback_player_run :1;
	u8_t  need_resume_play :1;
	u8_t  media_opened :1;
	u8_t  volume_req_type :2;
	u8_t  reservedbit :2;

	u8_t  volume_req_level;
	u8_t  current_volume_level;
	u8_t  set_dvfs_level;

#ifdef USOUND_FEATURE_RESTART
	u32_t restart_count;
	struct thread_timer  restart_timer;
#endif

#ifdef CONFIG_USOUND_BROADCAST_SUPPROT
	media_player_t *broadcast_capture_player;
	struct thread_timer  broadcast_start_timer;

	u8_t  capture_player_load :1;
	u8_t  capture_player_run  :1;
	u8_t  tx_start :1;
	u8_t  tx_sync_start :1;
	u8_t  broadcast_source_enabled :1;
	u8_t  encryption :1;
	u8_t  bms_source :1;
	u8_t  reserved   :1;
	int32_t broadcast_id;

	io_stream_t stream[NUM_OF_BROAD_CHAN];
	io_stream_t input_stream;
	struct bt_broadcast_chan broad_chan[NUM_OF_BROAD_CHAN];
	struct bt_broadcast_chan *chan;
	u8_t  num_of_broad_chan;

	u8_t  broadcast_code[16];
	struct bt_broadcast_qos *qos;
#endif

	u8_t need_test_hid;
	int host_enmu_db;
	int host_last_db;
};

struct usound_app_t *usound_get_app(void);

#ifdef USOUND_FEATURE_RESTART
void usound_restart_player_trigger(void);
#endif

int  usound_get_status(void);
void usound_playback_start(void);
void usound_playback_stop(void);
void usound_play_pause(void);
void usound_play_resume(void);

void usound_input_event_proc(struct app_msg *msg);
void usound_tts_event_proc(struct app_msg *msg);
void usound_app_event_proc(struct app_msg *msg);
void usound_show_play_status(bool status);

void usound_view_init(void);
void usound_view_deinit(void);
void usound_view_clear_screen(void);
void usound_view_volume_show(int volume_value);

void bms_uac_bt_event_proc(struct app_msg *msg);

int bms_uac_init_capture(void);
int bms_uac_start_capture(void);
int bms_uac_stop_capture(void);
int bms_uac_exit_capture(void);


bool usound_broadcast_is_bms_mode(void);
void usound_set_auracast_mode(bool mode);

int bms_uac_source_init(void);
int bms_uac_source_exit(void);

#endif  // _USOUND_APP_H

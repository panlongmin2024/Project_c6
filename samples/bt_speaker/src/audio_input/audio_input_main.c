/*
 * Copyright (c) 2022 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio_input app main.
 */

#include "audio_input.h"
#include "tts_manager.h"
#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif

#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif
#include <audio_hal.h>

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(audio_input, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

static struct audio_input_app_t *p_audio_input = NULL;

#ifdef CONFIG_LINE_IN_APP
static u8_t linein_init_state = AUDIO_INPUT_STATUS_PLAYING;
#endif

#ifdef CONFIG_SPDIF_IN_APP
static u8_t spdif_in_init_state = AUDIO_INPUT_STATUS_PLAYING;
#endif

#ifdef CONFIG_MIC_IN_APP
static u8_t mic_in_init_state = AUDIO_INPUT_STATUS_PLAYING;
#endif

#ifdef CONFIG_I2SRX_IN_APP
static u8_t i2srx_in_init_state = AUDIO_INPUT_STATUS_PLAYING;
#endif

#if defined(CONFIG_I2SRX_IN_APP) || defined(CONFIG_SPDIF_IN_APP)
static u8_t temp_sample_rate = 0;
#endif

#ifdef CONFIG_I2SRX_IN_APP
int audio_input_i2srx_event_cb(void *cb_data, u32_t cmd, void *param)
{
	if (!p_audio_input)
		return 0;
	SYS_LOG_INF("cmd=%d, param=%d\n", cmd, *(u8_t *) param);
	if (cmd == I2SRX_SRD_FS_CHANGE) {
		temp_sample_rate = *(u8_t *) param;
		p_audio_input->i2srx_srd_event = I2SRX_SAMPLE_RATE_CHANGE;
		os_delayed_work_submit(&p_audio_input->i2srx_cb_work, 2);
	} else if (cmd == SPDIFRX_SRD_TIMEOUT) {
		p_audio_input->i2srx_srd_event = I2SRX_TIME_OUT;
		os_delayed_work_submit(&p_audio_input->i2srx_cb_work, 2);
	}
	return 0;
}

static void _i2srx_isr_event_callback_work(struct k_work *work)
{
	struct app_msg msg = { 0 };

	if (!p_audio_input)
		return;
	if (p_audio_input->ain_handle) {
		hal_ain_channel_close(p_audio_input->ain_handle);
		p_audio_input->ain_handle = NULL;
	}

	if (p_audio_input->i2srx_srd_event == I2SRX_SAMPLE_RATE_CHANGE) {
		if (temp_sample_rate != p_audio_input->i2srx_sample_rate
		    || !p_audio_input->playback_player) {
			SYS_LOG_INF("sr %d", temp_sample_rate);
			p_audio_input->i2srx_sample_rate = temp_sample_rate;
			msg.type = MSG_AUDIO_INPUT_APP_EVENT;
			msg.cmd = MSG_AUDIO_INPUT_SAMPLE_RATE_CHANGE;
			send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		} else {
			SYS_LOG_INF("skip");
		}
	} else if (p_audio_input->i2srx_srd_event == I2SRX_TIME_OUT) {
		SYS_LOG_INF("timeout");
		msg.type = MSG_AUDIO_INPUT_APP_EVENT;
		msg.cmd = MSG_APP_PLAYER_RESET;
		msg.value = system_app_get_fault_app();
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
	}
}
#endif

#ifdef CONFIG_SPDIF_IN_APP

int audio_input_spdifrx_event_cb(void *cb_data, u32_t cmd, void *param)
{
	if (!p_audio_input)
		return 0;

	if (cmd == SPDIFRX_SRD_FS_CHANGE) {
		temp_sample_rate = *(u8_t *) param;
		os_delayed_work_submit(&p_audio_input->spdifrx_cb_work, 2);
		p_audio_input->spdifrx_srd_event = SPDIFRX_SAMPLE_RATE_CHANGE;
	} else if (cmd == SPDIFRX_SRD_TIMEOUT) {
		p_audio_input->spdifrx_srd_event = SPDIFRX_TIME_OUT;
		os_delayed_work_submit(&p_audio_input->spdifrx_cb_work, 2);
	}
	SYS_LOG_INF("cmd=%d, param=%d\n", cmd, *(u8_t *) param);
	return 0;
}

static void _spdifrx_isr_event_callback_work(struct k_work *work)
{
	struct app_msg msg = { 0 };

	if (!p_audio_input)
		return;

	SYS_LOG_INF("event %d\n", p_audio_input->spdifrx_srd_event);

	if (p_audio_input->ain_handle) {
		//hal_ain_channel_stop(p_audio_input->ain_handle);
		hal_ain_channel_close(p_audio_input->ain_handle);
		p_audio_input->ain_handle = NULL;
	}

	if (p_audio_input->spdifrx_srd_event == SPDIFRX_SAMPLE_RATE_CHANGE) {
		if (temp_sample_rate != p_audio_input->spdif_sample_rate
		    || !p_audio_input->playback_player) {
			p_audio_input->spdif_sample_rate = temp_sample_rate;
			msg.type = MSG_AUDIO_INPUT_APP_EVENT;
			msg.cmd = MSG_AUDIO_INPUT_SAMPLE_RATE_CHANGE;
			send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		}
	} else if (p_audio_input->spdifrx_srd_event == SPDIFRX_TIME_OUT) {
		msg.type = MSG_AUDIO_INPUT_APP_EVENT;
		msg.cmd = MSG_APP_PLAYER_RESET;
		msg.value = system_app_get_fault_app();
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
	}
}
#endif

void audio_input_delay_resume(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	p_audio_input->need_resume_play = 0;

	SYS_LOG_INF("%d %p", p_audio_input->plugin_id, p_audio_input->ain_handle);
	/*
	For spdif and i2s audio input, detect audio input before start player.
	Player will be triggered when sample rate detecting (SRD) successfully.
	*/
#ifdef CONFIG_SPDIF_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_SPDIFRX_IN) {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (p_audio_input->set_dvfs_level == 0) {
			p_audio_input->set_dvfs_level = SOC_DVFS_LEVEL_ALL_PERFORMANCE;
			soc_dvfs_set_level(SOC_DVFS_LEVEL_ALL_PERFORMANCE,
					   "spdif");
		}
#endif
		if (NULL == p_audio_input->ain_handle) {
			audio_in_init_param_t init_param = { 0 };

			init_param.channel_type = AUDIO_CHANNEL_SPDIFRX;
			p_audio_input->ain_handle = hal_ain_channel_open(&init_param);
			if (NULL != p_audio_input->ain_handle) {
				//hal_ain_channel_start(p_audio_input->ain_handle);
			}
		}

		return;
	}
#endif

#ifdef CONFIG_I2SRX_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_I2SRX_IN) {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (p_audio_input->set_dvfs_level == 0) {
			p_audio_input->set_dvfs_level = SOC_DVFS_LEVEL_ALL_PERFORMANCE;
			soc_dvfs_set_level(SOC_DVFS_LEVEL_ALL_PERFORMANCE,
					   "i2srx");
		}
#endif
#if 0
		if (NULL == p_audio_input->ain_handle) {
			audio_in_init_param_t init_param = { 0 };

			init_param.channel_type = AUDIO_CHANNEL_I2SRX;
			p_audio_input->ain_handle = hal_ain_channel_open(&init_param);
			if (NULL != p_audio_input->ain_handle) {
				//hal_ain_channel_start(p_audio_input->ain_handle);
			}
		}
#endif
        audio_input_play_resume();

		return;
	}
#endif

	audio_input_play_resume();
}

static void _audio_input_restore_play_state(u8_t init_play_state)
{
#ifdef CONFIG_ESD_MANAGER
	if (esd_manager_check_esd()) {
		esd_manager_restore_scene(TAG_PLAY_STATE, &init_play_state, 1);
	}
#endif
	SYS_LOG_INF("%d\n", init_play_state);

	if (init_play_state == AUDIO_INPUT_STATUS_PLAYING) {
		p_audio_input->playing = 1;
		if (thread_timer_is_running(&p_audio_input->resume_timer)) {
			thread_timer_stop(&p_audio_input->resume_timer);
		}
		//wait tts finish, then start app player.
		thread_timer_start(&p_audio_input->resume_timer, 2500, 0);
		audio_input_show_play_status(true);
	} else {
		p_audio_input->playing = 0;
		audio_input_show_play_status(false);
	}
	audio_input_store_play_state();

}

void audio_input_store_play_state(void)
{
	uint8_t *play_state = NULL;

#ifdef CONFIG_SPDIF_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_SPDIFRX_IN) {
		play_state = &spdif_in_init_state;
	}
#endif

#ifdef CONFIG_I2SRX_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_I2SRX_IN) {
		play_state = &i2srx_in_init_state;
	}
#endif

#ifdef CONFIG_MIC_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_MIC_IN) {
		play_state = &mic_in_init_state;
	}
#endif

#ifdef CONFIG_LINE_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_LINE_IN) {
		play_state = &linein_init_state;
	}
#endif

	if (NULL != play_state) {
		if (p_audio_input->playing) {
			*play_state = AUDIO_INPUT_STATUS_PLAYING;
		} else {
			*play_state = AUDIO_INPUT_STATUS_PAUSED;
		}
	}
}

u32_t audio_input_get_audio_stream_type(void)
{
#ifdef CONFIG_SPDIF_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_SPDIFRX_IN) {
		return AUDIO_STREAM_SPDIF_IN;
	}
#endif

#ifdef CONFIG_I2SRX_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_I2SRX_IN) {
		return AUDIO_STREAM_I2SRX_IN;
	}
#endif

#ifdef CONFIG_MIC_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_MIC_IN) {
		return AUDIO_STREAM_MIC_IN;
	}
#endif

#ifdef CONFIG_LINE_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_LINE_IN) {
		return AUDIO_STREAM_LINEIN;
	}
#endif

	return AUDIO_STREAM_DEFAULT;
}

#ifdef CONFIG_AUDIO_INPUT_RESTART
void audio_input_player_reset_trigger(void)
{
	if (p_audio_input != NULL) {
		p_audio_input->restart = 1;
		p_audio_input->restart_count++;
		SYS_LOG_INF("%d", p_audio_input->restart_count);
	}
}

static void audio_input_restart_handler(struct thread_timer *ttimer,
					void *expiry_fn_arg)
{
	struct app_msg msg = { 0 };

	if (p_audio_input && p_audio_input->restart) {
		SYS_LOG_INF("restart\n");
		msg.type = MSG_AUDIO_INPUT_APP_EVENT;
		msg.cmd = MSG_APP_PLAYER_RESET;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		p_audio_input->restart = 0;
		p_audio_input->restart_count = 0;
	}
}
#endif

#ifdef CONFIG_AUDIO_INPUT_BMS_APP

#define DEFAULT_CONTEXTS	(BT_AUDIO_CONTEXT_UNSPECIFIED | \
				BT_AUDIO_CONTEXT_MEDIA)

static struct bt_broadcast_qos source_qos = {
	.packing = BT_AUDIO_PACKING_INTERLEAVED,
	.framing = BT_AUDIO_UNFRAMED,
	.phy = BT_AUDIO_PHY_2M,
	.rtn = 2,
	.max_sdu = BROADCAST_SDU,
	/* max_transport_latency, unit: ms */
	.latency = BROADCAST_LAT,
	/* sdu_interval, unit: us */
	.interval = BROADCAST_SDU_INTERVAL,
	/* presentation_delay, unit: us */
	.delay = BCST_QOS_DELAY,
	/* processing_time, unit: us */
	.processing = 7000,
};

#ifdef ENABLE_PAWR_APP
static int pawr_audioin_handle_response(const uint8_t *buf, uint16_t len)
{
	return pawr_handle_response(audio_input_get_audio_stream_type(), buf, len);
}
#endif

static int audio_input_bms_source_init(void)
{
	struct bt_broadcast_source_create_param param = { 0 };
	struct bt_broadcast_source_big_param big_param;
	struct bt_le_per_adv_param per_adv_param = { 0 };
#if (BROADCAST_NUM_BIS != 2)
	struct bt_broadcast_subgroup_1 subgroup = { 0 };
#else
	struct bt_broadcast_subgroup_2 subgroup = { 0 };
#endif
	// TODO:
	uint16_t appearance = 0x885;	/* Audio Source + Broadcasting Device */
	int ret;

	if (!p_audio_input)
		return -EINVAL;

	if (thread_timer_is_running(&p_audio_input->broadcast_start_timer))
		thread_timer_stop(&p_audio_input->broadcast_start_timer);

#if (BROADCAST_NUM_BIS == 2)
	subgroup.num_bis = 2;
#else
	subgroup.num_bis = 1;
#endif
	subgroup.format = BT_AUDIO_CODEC_LC3;
	subgroup.frame_duration = BROADCAST_DURATION;
	subgroup.blocks = 1;
	subgroup.sample_rate = 48;
	subgroup.octets = BROADCAST_OCTETS;
#if (BROADCAST_NUM_BIS != 2)
#if (BROADCAST_CH == 2)
	subgroup.locations = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;
#else
	subgroup.locations = BT_AUDIO_LOCATIONS_FL;
#endif
#endif
	subgroup.language = BT_LANGUAGE_UNKNOWN;
	subgroup.contexts = DEFAULT_CONTEXTS;

#if (BROADCAST_NUM_BIS == 2)
	subgroup.bis[0].locations = BT_AUDIO_LOCATIONS_FL;
	subgroup.bis[1].locations = BT_AUDIO_LOCATIONS_FR;
#endif

#if 0
	bt_manager_stream_enable(STREAM_TYPE_A2DP, true);
	bt_manager_set_stream_type(AUDIO_STREAM_MUSIC);
#endif

	big_param.iso_interval = BROADCAST_ISO_INTERVAL;
	big_param.max_pdu = BROADCAST_PDU;
	big_param.nse = BROADCAST_NSE;
	big_param.bn = BROADCAST_BN;
	big_param.irc = BROADCAST_IRC;
	big_param.pto = BROADCAST_PTO;

	/* 100ms */
	per_adv_param.interval_min = PA_INTERVAL;
	per_adv_param.interval_max = PA_INTERVAL;

#if ENABLE_ENCRYPTION
	param.encryption = true;
	param.broadcast_code[0] = 0x55;
	param.broadcast_code[1] = 0xaa;
	memcpy(p_audio_input->broadcast_code, param.broadcast_code, 16);
#endif

	param.broadcast_id = broadcast_get_broadcast_id();
	if (param.broadcast_id == INVALID_BROADCAST_ID) {
		SYS_LOG_ERR("get broadcast failed\n");
		return -EINVAL;
	}

	param.local_name = broadcast_get_local_name();
	param.broadcast_name = broadcast_get_broadcast_name();
	param.qos = p_audio_input->qos;
	param.appearance = appearance;
	param.num_subgroups = 1;
	param.subgroup = (struct bt_broadcast_subgroup *)&subgroup;
	param.big_param = &big_param;
	param.per_adv_param = &per_adv_param;

	ret = bt_manager_broadcast_source_create(&param);
	if (ret < 0) {
		SYS_LOG_ERR("failed");
		thread_timer_start(&p_audio_input->broadcast_start_timer, 300, 0);
		return ret;
	}

//	p_audio_input->broadcast_retransmit_number = BROADCAST_TRANSMIT_NUMBER;
//	p_audio_input->broadcast_id = param.broadcast_id;

#ifdef ENABLE_PAWR_APP
	if (get_pawr_enable()) {
		struct bt_le_per_adv_param per_adv_param1 = {0};
		per_adv_param1.interval_min = PAwR_INTERVAL;
		per_adv_param1.interval_max = PAwR_INTERVAL;
		per_adv_param1.subevent_interval = PAwR_SUB_INTERVAL;
		per_adv_param1.response_slot_delay = PAwR_RSP_DELAY;
		bt_manager_pawr_adv_start(pawr_audioin_handle_response, &per_adv_param1);
	}
#endif
	return 0;
}

static int audio_input_bms_source_exit(void)
{
	if (thread_timer_is_running(&p_audio_input->broadcast_start_timer))
		thread_timer_stop(&p_audio_input->broadcast_start_timer);
	bt_manager_broadcast_source_disable(p_audio_input->broad_chan[0].
					    handle);
	bt_manager_broadcast_source_release(p_audio_input->broad_chan[0].
					    handle);

	bt_manager_pawr_adv_stop();

	return 0;
}

bool audio_input_is_bms_mode(void)
{
#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	return (1 == system_app_get_auracast_mode())? true : false;
#else
	return false;
#endif
}

void audio_input_set_auracast_mode(bool mode)
{
	if (NULL == p_audio_input) {
		return;
	}

	SYS_LOG_INF("%d->%d", audio_input_is_bms_mode(), mode);
	if (mode == audio_input_is_bms_mode()) {
		return;
	}

	if (!mode) {
		audio_input_bms_source_exit();
		system_app_set_auracast_mode(0);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (p_audio_input->set_dvfs_level) {
			soc_dvfs_unset_level(p_audio_input->set_dvfs_level, "bms_audioin");
			p_audio_input->set_dvfs_level = 0;
		}
#endif
	} else {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		p_audio_input->set_dvfs_level = BCST_FREQ;
		soc_dvfs_set_level(p_audio_input->set_dvfs_level, "bms_audioin");
#endif
		audio_input_bms_source_init();
		system_app_set_auracast_mode(1);
	}
}

static void audio_input_bms_start_handler(struct thread_timer *ttimer,
				   void *expiry_fn_arg){
	if (NULL == p_audio_input) {
		return;
	}
	if(audio_input_is_bms_mode()){
		audio_input_bms_source_init();
	}
}
#endif

static int _audio_input_init(void *p1, void *p2, void *p3)
{
	u8_t init_play_state = *(u8_t *) p1;

	if (p_audio_input)
		return 0;

	p_audio_input = app_mem_malloc(sizeof(struct audio_input_app_t));
	if (!p_audio_input) {
		SYS_LOG_ERR("malloc failed!\n");
		return -ENOMEM;
	}

#ifdef CONFIG_TWS
#ifndef CONFIG_TWS_BACKGROUND_BT
	bt_manager_halt_phone();
#endif
#endif

	memset(p_audio_input, 0, sizeof(struct audio_input_app_t));

	p_audio_input->plugin_id = desktop_manager_get_plugin_id();
	SYS_LOG_INF("id %d", p_audio_input->plugin_id);

	thread_timer_init(&p_audio_input->resume_timer, audio_input_delay_resume, NULL);
#ifdef CONFIG_AUDIO_INPUT_RESTART
	thread_timer_init(&p_audio_input->restart_timer, audio_input_restart_handler, NULL);
	thread_timer_start(&p_audio_input->restart_timer, 200, 200);
#endif

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	thread_timer_init(&p_audio_input->broadcast_start_timer, audio_input_bms_start_handler, NULL);
#endif

	audio_input_view_init();

#ifdef CONFIG_SPDIF_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_SPDIFRX_IN) {
		hal_ain_set_contrl_callback(AUDIO_CHANNEL_SPDIFRX,
					    audio_input_spdifrx_event_cb);
		os_delayed_work_init(&p_audio_input->spdifrx_cb_work,
				     _spdifrx_isr_event_callback_work);

		sys_event_notify(SYS_EVENT_ENTER_SPDIF_IN);

		bt_manager_set_stream_type(AUDIO_STREAM_SPDIF_IN);

	}
#endif

#ifdef CONFIG_I2SRX_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_I2SRX_IN) {
		hal_ain_set_contrl_callback(AUDIO_CHANNEL_I2SRX,
					    audio_input_i2srx_event_cb);
		os_delayed_work_init(&p_audio_input->i2srx_cb_work,
				     _i2srx_isr_event_callback_work);

		sys_event_notify(SYS_EVENT_ENTER_I2SRX_IN);

		bt_manager_set_stream_type(AUDIO_STREAM_I2SRX_IN);
	}
#endif

#ifdef CONFIG_MIC_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_MIC_IN) {
		sys_event_notify(SYS_EVENT_ENTER_MIC_IN);

		bt_manager_set_stream_type(AUDIO_STREAM_MIC_IN);
	}
#endif

#ifdef CONFIG_LINE_IN_APP
	if (p_audio_input->plugin_id == DESKTOP_PLUGIN_ID_LINE_IN) {
		sys_event_notify(SYS_EVENT_ENTER_LINEIN);

		bt_manager_set_stream_type(AUDIO_STREAM_LINEIN);
	}
#endif

	_audio_input_restore_play_state(init_play_state);

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	p_audio_input->qos =  &source_qos;
	if(audio_input_is_bms_mode()){
		system_app_set_auracast_mode(1);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		p_audio_input->set_dvfs_level = BCST_FREQ;
		soc_dvfs_set_level(p_audio_input->set_dvfs_level, "bms_audioin");
#endif
		audio_input_bms_source_init();
	}
#endif
	SYS_LOG_INF("init ok\n");

	return 0;
}

static int _audio_input_exit(void)
{
	if (!p_audio_input)
		goto exit;

	if (thread_timer_is_running(&p_audio_input->resume_timer)) {
		thread_timer_stop(&p_audio_input->resume_timer);
	}
#ifdef CONFIG_AUDIO_INPUT_RESTART
	if (thread_timer_is_running(&p_audio_input->restart_timer)) {
		thread_timer_stop(&p_audio_input->restart_timer);
	}
#endif
#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	if (thread_timer_is_running(&p_audio_input->broadcast_start_timer))
		thread_timer_stop(&p_audio_input->broadcast_start_timer);
#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif
#endif

	audio_input_stop_playback();
	audio_input_exit_playback();

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	audio_input_stop_capture();
	audio_input_exit_capture();
	audio_input_bms_source_exit();
#endif

	if (p_audio_input->ain_handle) {
		hal_ain_channel_close(p_audio_input->ain_handle);
		p_audio_input->ain_handle = NULL;
		os_delayed_work_cancel(&p_audio_input->spdifrx_cb_work);
	}

	audio_input_view_deinit();

	app_mem_free(p_audio_input);

	p_audio_input = NULL;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif

 exit:
	SYS_LOG_INF("ok\n");

	return 0;
}

static int _audio_input_proc_msg(struct app_msg *msg)
{
	switch (msg->type) {
	case MSG_INPUT_EVENT:
		audio_input_input_event_proc(msg);
		break;
	case MSG_TTS_EVENT:
		audio_input_tts_event_proc(msg);
		break;
	case MSG_AUDIO_INPUT_APP_EVENT:
		audio_input_app_event_proc(msg);
		break;
#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	case MSG_BT_EVENT:
		audio_input_bt_event_proc(msg);
		break;
#endif
	case MSG_EXIT_APP:
		_audio_input_exit();
		break;

	default:
		break;
	}

	return 0;
}

struct audio_input_app_t *audio_input_get_app(void)
{
	return p_audio_input;
}

static int  audio_input_dump_state(void)
{
	print_buffer_lazy("Audio Input", (void *)p_audio_input,
			  sizeof(struct audio_input_app_t));

	return 0;
}

#ifdef CONFIG_LINE_IN_APP

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_LINE_IN, _audio_input_init, _audio_input_exit, \
	_audio_input_proc_msg, audio_input_dump_state, &linein_init_state, NULL, NULL);
#endif

#ifdef CONFIG_SPDIF_IN_APP
DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_SPDIFRX_IN, _audio_input_init, _audio_input_exit, \
	_audio_input_proc_msg, audio_input_dump_state, &spdif_in_init_state, NULL, NULL);
#endif

#ifdef CONFIG_I2SRX_IN_APP
DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_I2SRX_IN, _audio_input_init, _audio_input_exit, \
	_audio_input_proc_msg, audio_input_dump_state, &i2srx_in_init_state, NULL, NULL);
#endif

#ifdef CONFIG_MIC_IN_APP

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_MIC_IN, _audio_input_init, _audio_input_exit, \
	_audio_input_proc_msg, audio_input_dump_state, &mic_in_init_state, NULL, NULL);

#endif



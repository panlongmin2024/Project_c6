/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief usound app main.
 */

#ifdef CONFIG_USOUND_BROADCAST_SUPPROT

#include "usound.h"

#define DEFAULT_CONTEXTS (BT_AUDIO_CONTEXT_UNSPECIFIED | BT_AUDIO_CONTEXT_MEDIA)

#define LOCAL_NAME              "bis_local"
#define LOCAL_NAME_LEN	        (sizeof(LOCAL_NAME) - 1)

#define BROADCAST_NAME	        "bis_broadcast"
#define BROADCAST_NAME_LEN	    (sizeof(BROADCAST_NAME) - 1)

static uint8_t local_name[33];
static uint8_t broadcast_name[33];


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
	.processing = 9000,
};

int bms_uac_source_init(void)
{
	static int count = 0;
	struct bt_broadcast_source_create_param  param = { 0 };
	struct bt_broadcast_source_big_param     big_param;
	struct bt_le_per_adv_param               per_adv_param = { 0 };
#if (BROADCAST_NUM_BIS != 2)
	struct bt_broadcast_subgroup_1 subgroup = { 0 };
#else
	struct bt_broadcast_subgroup_2 subgroup = { 0 };
#endif
	// TODO:
	uint16_t appearance = 0x885;	/* Audio Source + Broadcasting Device */
	int ret;

	if (!p_usound) {
		return -EINVAL;
	}

	p_usound->bms_source = 1;

	SYS_LOG_INF("%d", count++);
	if (thread_timer_is_running(&p_usound->broadcast_start_timer))
		thread_timer_stop(&p_usound->broadcast_start_timer);

#ifdef CONFIG_PROPERTY
	ret = property_get(CFG_BT_LOCAL_NAME, local_name, sizeof(local_name) - 1);
	if (ret <= 0) {
		SYS_LOG_WRN("failed to get local name");
		memcpy(local_name, LOCAL_NAME, LOCAL_NAME_LEN);
	}

	ret = property_get(CFG_BT_BROADCAST_NAME, broadcast_name, sizeof(broadcast_name) - 1);
	if (ret <= 0) {
		SYS_LOG_WRN("failed to get broadcast name");
		memcpy(broadcast_name, BROADCAST_NAME, BROADCAST_NAME_LEN);
	}
#endif

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
	subgroup.language = 0;	//BT_LANGUAGE_UNKNOWN
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
	memcpy(p_usound->broadcast_code, param.broadcast_code, 16);
#endif

	param.broadcast_id = broadcast_get_broadcast_id();
	if (param.broadcast_id == INVALID_BROADCAST_ID) {
		SYS_LOG_ERR("get broadcast failed");
		p_usound->bms_source = 0;
		return -EINVAL;
	}

	param.local_name = local_name;
	param.broadcast_name = broadcast_name;
	param.qos = p_usound->qos;
	param.appearance = appearance;
	param.num_subgroups = 1;
	param.subgroup = (struct bt_broadcast_subgroup *)&subgroup;
	param.big_param = &big_param;
	param.per_adv_param = &per_adv_param;

	ret = bt_manager_broadcast_source_create(&param);
	if (ret < 0) {
		SYS_LOG_ERR("failed %d", ret);
		thread_timer_start(&p_usound->broadcast_start_timer, 300, 0);
		p_usound->bms_source = 0;
		return ret;
	}
	//p_usound->broadcast_retransmit_number = BROADCAST_TRANSMIT_NUMBER;
	p_usound->broadcast_id = param.broadcast_id;

	//usound_pawr_adv_start();

	return 0;
}

int bms_uac_source_exit(void)
{
	if (thread_timer_is_running(&p_usound->broadcast_start_timer))
		thread_timer_stop(&p_usound->broadcast_start_timer);

	bt_manager_broadcast_source_disable(p_usound->broadcast_id);
	bt_manager_broadcast_source_release(p_usound->broadcast_id);

	//bt_manager_pawr_adv_stop();
	p_usound->bms_source = 0;

	return 0;
}

void usound_set_auracast_mode(bool mode)
{
	struct usound_app_t *usound = usound_get_app();

	if (NULL == usound) {
		return;
	}

	SYS_LOG_INF("%d->%d", usound_broadcast_is_bms_mode(), mode);
	if (mode == usound_broadcast_is_bms_mode()) {
		return;
	}

	if (!mode) {
		if (1 == usound->bms_source) {
			bms_uac_source_exit();
		}
		//bt_manager_pawr_adv_stop();
		system_app_set_auracast_mode(0);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (usound->set_dvfs_level) {
			soc_dvfs_unset_level(usound->set_dvfs_level, "bms_uac");
			usound->set_dvfs_level = 0;
		}
#endif
	} else {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		usound->set_dvfs_level = BCST_FREQ;
		soc_dvfs_set_level(usound->set_dvfs_level, "bms_uac");
#endif
		bms_uac_source_init();
		system_app_set_auracast_mode(1);
	}
}

static void bms_uac_start_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	if (NULL == p_usound) {
		return;
	}
	if (usound_broadcast_is_bms_mode()) {
		bms_uac_source_init();
	}
}

void usound_broadcast_init(void)
{
    usound_app_t *usound = usound_get_app();
    if (usound == NULL) {
        return ;
    }

	usound->qos = &source_qos;
	thread_timer_init(&usound->broadcast_start_timer, bms_uac_start_handler, NULL);

	if (usound_broadcast_is_bms_mode()) {
		system_app_set_auracast_mode(1);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		usound->set_dvfs_level = BCST_FREQ;
		soc_dvfs_set_level(usound->set_dvfs_level, "bms_uac");
#endif
		bms_uac_source_init();
	}
}

static io_stream_t broadcast_create_source_stream(int mem_type, int block_num)
{
	io_stream_t stream = NULL;
	int buff_size;
	int ret;

	buff_size = media_mem_get_cache_pool_size_ext(mem_type, AUDIO_STREAM_USOUND, NAV_TYPE, BROADCAST_SDU, block_num);
	if (buff_size <= 0) {
		goto exit;
	}

	stream = ringbuff_stream_create_ext(media_mem_get_cache_pool(mem_type, AUDIO_STREAM_USOUND), buff_size);
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
	SYS_LOG_INF("%p", stream);
	return stream;
}

static io_stream_t bms_uac_create_capture_input_stream(void)
{
	io_stream_t input_stream;
	int ret;

	input_stream = ringbuff_stream_create_ext(media_mem_get_cache_pool(INPUT_CAPTURE, AUDIO_STREAM_USOUND),
						media_mem_get_cache_pool_size(INPUT_CAPTURE, AUDIO_STREAM_USOUND));
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

static void bms_uac_broadcast_source_stream_set(uint8 enable_flag)
{
	struct usound_app_t *usound = usound_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &usound->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			continue;
		}
		if (enable_flag) {
			bt_manager_broadcast_source_stream_set(chan, usound->stream[i]);
		} else {
			bt_manager_broadcast_source_stream_set(chan, NULL);
		}
	}
}

int bms_uac_init_capture(void)
{
	struct usound_app_t *usound = usound_get_app();
	struct bt_broadcast_chan *chan;
	struct bt_broadcast_chan_info chan_info;
	io_stream_t source_stream = NULL;
	io_stream_t source_stream2 = NULL;
	io_stream_t input_stream = NULL;
	media_init_param_t init_param;
	int ret;

	if (NULL == usound) {
		return -1;
	}

	SYS_LOG_INF("");

	if (usound->capture_player) {
		SYS_LOG_INF("already");
		return -2;
	}

	chan = usound->chan;
	if (NULL == chan) {
		SYS_LOG_INF("no chan");
		return -3;
	}
	ret = bt_manager_broadcast_stream_info(chan, &chan_info);
	if (ret) {
		return ret;
	}

	SYS_LOG_INF("chan: handle 0x%x id %d", chan->handle, chan->id);

	SYS_LOG_INF("chan_info: f%d s%d c%d k%d d%d t%p",
		    chan_info.format, chan_info.sample, chan_info.channels,
		    chan_info.kbps, chan_info.duration, chan_info.chan);

	/* wait until start_capture */
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	source_stream = broadcast_create_source_stream(OUTPUT_CAPTURE, 6);
	if (!source_stream) {
		goto exit;
	}
#if (BROADCAST_NUM_BIS == 2)
	source_stream2 = broadcast_create_source_stream(OUTPUT_CAPTURE2, 6);
	if (!source_stream2) {
		goto exit;
	}
#endif

	input_stream = bms_uac_create_capture_input_stream();
	if (!input_stream) {
		goto exit;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));

	init_param.type = MEDIA_SRV_TYPE_CAPTURE;
	init_param.stream_type = AUDIO_STREAM_USOUND;
	init_param.efx_stream_type = AUDIO_STREAM_USOUND;
	// TODO: broadcast_source no need to get chan_info???
	init_param.capture_format = NAV_TYPE;	// bt_manager_audio_codec_type(chan_info.format);
	init_param.capture_sample_rate_input = audio_system_get_output_sample_rate();	// chan_info.sample;
	init_param.capture_sample_rate_output = 48;	// chan_info.sample;
	init_param.capture_sample_bits = 32;
	//to do?
	init_param.capture_channels_input = 2;	//BROADCAST_CH;   // chan_info.channels;
	init_param.capture_channels_output = 2;	//BROADCAST_CH;  // chan_info.channels;
	init_param.capture_bit_rate = chan_info.kbps * BROADCAST_NUM_BIS;	//BROADCAST_KBPS;
	init_param.capture_input_stream = input_stream;
	init_param.capture_output_stream = source_stream;
	init_param.capture_output_stream2 = source_stream2;
	init_param.waitto_start = 1;
	init_param.device_info.tx_chan.validate = 1;
	//init_param.device_info.tx_chan.use_trigger = 1;
	init_param.device_info.tx_chan.type = MEDIA_AUDIO_DEVICE_TYPE_BIS;
	init_param.device_info.tx_chan.bt_chan.handle = chan->handle;
	init_param.device_info.tx_chan.bt_chan.id = chan->id;
	init_param.device_info.tx_chan.timeline_owner = chan_info.chan;

	if (chan_info.duration == 7) {
		audio_policy_set_nav_frame_size_us(7500);
	} else {
		audio_policy_set_nav_frame_size_us(10000);
	}

	usound->capture_player = media_player_open(&init_param);
	if (!usound->capture_player) {
		goto exit;
	}

	usound->stream[0] = source_stream;
	usound->stream[1] = source_stream2;
	usound->input_stream = input_stream;
	SYS_LOG_INF("%p opened", usound->capture_player);

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

	SYS_LOG_ERR("open failed");
	return -EINVAL;
}

int bms_uac_start_capture(void)
{
	struct usound_app_t *usound = usound_get_app();

	if (NULL == usound) {
		return -1;
	}
	SYS_LOG_INF("");

	if (!usound->capture_player) {
		SYS_LOG_INF("not ready");
		return -EINVAL;
	}

	if (usound->capture_player_run) {
		SYS_LOG_INF("already");
		return -EINVAL;
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	media_player_play(usound->capture_player);
	/*
	 * NOTE: sleep for resample page miss, capture/playback are
	 * shared, so only need to sleep once.
	 */
	if (!usound->capture_player_load) {
		os_sleep(OS_MSEC(10));
	}
	usound->capture_player_load = 1;

	bms_uac_broadcast_source_stream_set(1);

	usound->capture_player_run = 1;

	usb_audio_set_stream(usound->usound_stream);

	SYS_LOG_INF("%p played", usound->capture_player);

	return 0;
}

int bms_uac_stop_capture(void)
{
	struct usound_app_t *usound = usound_get_app();
	int i;

	if (!usound->capture_player || !usound->capture_player_run) {
		SYS_LOG_INF("not ready");
		return -EINVAL;
	}
	//bt_manager_broadcast_source_stream_set(&usound->broad_chan, NULL);
	bms_uac_broadcast_source_stream_set(0);

	if (usound->capture_player_load) {
		media_player_stop(usound->capture_player);
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_unlock();
#endif

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (usound->stream[i]) {
			stream_close(usound->stream[i]);
		}
	}

	if (usound->input_stream) {
		stream_close(usound->input_stream);
	}

	usound->capture_player_run = 0;

	SYS_LOG_INF("%p", usound->capture_player);

	return 0;
}

int bms_uac_exit_capture(void)
{
	struct usound_app_t *usound = usound_get_app();
	int i;

	if (!usound->capture_player) {
		SYS_LOG_INF("already");
		return -EALREADY;
	}

	media_player_close(usound->capture_player);
	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (usound->stream[i]) {
			stream_destroy(usound->stream[i]);
			usound->stream[i] = NULL;
		}
	}
	if (usound->input_stream) {
		stream_destroy(usound->input_stream);
		usound->input_stream = NULL;
	}

	SYS_LOG_INF("%p", usound->capture_player);

	usound->capture_player = NULL;
	usound->capture_player_load = 0;
	usound->capture_player_run = 0;

	return 0;
}



/* find the existed broad chan */
static struct bt_broadcast_chan *find_broad_chan(uint32_t handle, uint8_t id)
{
	struct usound_app_t *usound = usound_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &usound->broad_chan[i];
		if (chan->handle == handle && chan->id == id) {
			return chan;
		}
	}
	return NULL;
}

/* find a new broad chan */
static struct bt_broadcast_chan *new_broad_chan(uint32_t handle, uint8_t id)
{
	struct usound_app_t *usound = usound_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &usound->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			return chan;
		}
	}
	return NULL;
}

static void bms_uac_tx_start(uint32_t handle, uint8_t id, uint16_t audio_handle)
{
	struct usound_app_t *usound = usound_get_app();

	if (usound->tx_start) {
		return;
	}

	if (usound->broadcast_source_enabled && usound->capture_player_run) {
		media_player_trigger_audio_record_start(usound->capture_player);
		usound->tx_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger record\n");
	}
}

static void bms_uac_tx_sync_start(uint32_t handle, uint8_t id, uint16_t audio_handle)
{
	struct usound_app_t *usound = usound_get_app();

	if (usound->tx_sync_start) {
		return;
	}

	if (usound->broadcast_source_enabled && usound->playback_player_run) {
		media_player_trigger_audio_track_start(usound->playback_player);
		usound->tx_sync_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger track\n");
	}
}

static int bms_uac_handle_source_config(struct bt_braodcast_configured *rep)
{
	struct usound_app_t *usound = usound_get_app();
	struct bt_broadcast_chan *chan;
	uint8_t vnd_buf[20] = { 0 };
	uint8_t type = 0;

	SYS_LOG_INF("h:0x%x id:%d", rep->handle, rep->id);

	chan = find_broad_chan(rep->handle, rep->id);
	if (!chan) {
		chan = new_broad_chan(rep->handle, rep->id);
		if (!chan) {
			SYS_LOG_ERR("no broad chan handle: %d, id: %d", rep->handle, rep->id);
			return -EINVAL;
		}
		usound->num_of_broad_chan++;
	}

	chan->handle = rep->handle;
	chan->id = rep->id;

	vnd_buf[0] = VS_ID & 0xFF;
	vnd_buf[1] = VS_ID >> 8;
	memcpy(&vnd_buf[2], usound->broadcast_code, 16);
	vnd_buf[18] = SERIVCE_UUID & 0xFF;
	vnd_buf[19] = SERIVCE_UUID >> 8;

#if !VS_COMPANY_ID
	type = BT_DATA_SVC_DATA16;
#endif

	if (usound->num_of_broad_chan == 1) {
		bt_manager_broadcast_stream_cb_register(chan, bms_uac_tx_start, NULL);
		bt_manager_broadcast_stream_tws_sync_cb_register(chan, bms_uac_tx_sync_start);
		bt_manager_broadcast_stream_set_tws_sync_cb_offset(chan, broadcast_get_tws_sync_offset(usound->qos));
		bt_manager_broadcast_source_vnd_ext_send(chan->handle, vnd_buf, sizeof(vnd_buf), type);
		usound->chan = chan;
		usound_playback_start();
	}

	if (usound->num_of_broad_chan >= 2) {
		bt_manager_broadcast_source_enable(usound->chan->handle);
	}

	return 0;
}

static int bms_uac_handle_source_enable(struct bt_broadcast_report *rep)
{
	struct usound_app_t *usound = usound_get_app();

	if (NULL == usound) {
		return -1;
	}

	SYS_LOG_INF("handle 0x%x, id %d", rep->handle, rep->id);
	SYS_LOG_INF("resume %d, chan %d", usound->need_resume_play, usound->num_of_broad_chan);
	if (2 > rep->id) {
		SYS_LOG_INF("wait all done.");
		return 0;
	}

	usound->broadcast_source_enabled = 1;
#if ENABLE_PADV_APP
	if (usound->chan != NULL) {
		padv_tx_init(usound->chan->handle, AUDIO_STREAM_USOUND);
	}
#endif
	if (usound->need_resume_play) {
		SYS_LOG_INF("wait tts.");
		return 0;
	}

	bms_uac_init_capture();
	bms_uac_start_capture();

	return 0;
}

static int bms_uac_handle_source_disable(struct bt_broadcast_report *rep)
{
	struct usound_app_t *usound = usound_get_app();

#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif
	bms_uac_stop_capture();
	bms_uac_exit_capture();

	usb_audio_set_stream(NULL);

	//usound->broadcast_source_enabled = 0;
	usound->tx_start = 0;
	usound->tx_sync_start = 0;

	struct bt_broadcast_chan *chan = find_broad_chan(rep->handle, rep->id);
	if (chan) {
		if (usound->num_of_broad_chan) {
			usound->num_of_broad_chan--;
			chan->handle = 0;
			chan->id = 0;
			if (chan == usound->chan) {
				usound->chan = NULL;
			}
			if (!usound->num_of_broad_chan) {
				usound->broadcast_source_enabled = 0;
				SYS_LOG_INF("all disabled");
			}
		} else {
			SYS_LOG_WRN("should not be here");
		}
	}

	return 0;
}

static int bms_uac_handle_source_release(struct bt_broadcast_report *rep)
{
	return 0;
}

static void usound_switch_auracast()
{
	struct usound_app_t *usound = usound_get_app();

	if (usound_broadcast_is_bms_mode()) {
		usound_playback_stop();

		bms_uac_stop_capture();
		bms_uac_exit_capture();

		usound->tx_start = 0;
		usound->tx_sync_start = 0;

		usound_set_auracast_mode(false);
		//usound with be started after tts restart.
	} else {
		if (bt_manager_audio_get_leaudio_dev_num()) {
			SYS_LOG_ERR("LE conneted!");
			return;
		}

		//bis maybe in releasing
		if (!usound->chan) {
			if (usound->playback_player_run) {
				usound_playback_stop();
			}
			usound_set_auracast_mode(true);
		}
	}
}

void bms_uac_bt_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd: %d", msg->cmd);

	switch (msg->cmd) {
	case BT_REQ_RESTART_PLAY:
		SYS_LOG_WRN("skip BT_REQ_RESTART_PLAY");
		break;

	case BT_BROADCAST_SOURCE_CONFIG:
		bms_uac_handle_source_config(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_ENABLE:
		bms_uac_handle_source_enable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_DISABLE:
		bms_uac_handle_source_disable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_RELEASE:
		bms_uac_handle_source_release(msg->ptr);
		break;

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


#endif  // CONFIG_USOUND_BROADCAST_SUPPROT


u8_t usound_broadcast_is_bms_mode(void)
{
#ifdef CONFIG_USOUND_BROADCAST_SUPPROT
	return (system_app_get_auracast_mode() == 0) ? 0 : 1;
#else
	return 0;
#endif
}

/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SYS_LOG_DOMAIN "broadcast"

#include <logging/sys_log.h>
#include <btservice_api.h>
#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif
#include <audio_system.h>
#include <volume_manager.h>
#include "broadcast.h"
#include "app_common.h"

int8_t broadcast_sq_mode = 0;

int broadcast_get_broadcast_id(void)
{
	int v;

	v = property_get_int("BROADCAST_ID", INVALID_BROADCAST_ID);

	SYS_LOG_INF("0x%x\n", v);
	return v;
}

int broadcast_get_bis_link_delay_ext(struct bt_broadcast_qos *qos, uint8_t iso_interval)
{
	u32_t offset;

	if(NULL == qos) {
		SYS_LOG_ERR("NULL qos");
		return 0;
	}

	offset = qos->delay - EXTERNAL_DSP_DELAY;

	if (qos->interval == 7500) {	//lc3 codec delay
		offset += 4000;
	} else {
		offset += 2500;
	}

	offset += qos->processing;
	offset += 10000;	//TODO:use sync delay
	offset += (iso_interval * 1250);
	offset /= 1000;

	SYS_LOG_INF("%dms\n", offset);
	return offset;
}

int broadcast_get_bis_link_delay(struct bt_broadcast_qos *qos)
{
	return broadcast_get_bis_link_delay_ext(qos, BROADCAST_ISO_INTERVAL);
}

int broadcast_get_tws_sync_offset(struct bt_broadcast_qos *qos)
{
	int offset;

	if(NULL == qos) {
		SYS_LOG_ERR("NULL qos");
		return 0;
	}

	offset = qos->delay - EXTERNAL_DSP_DELAY;
	if (qos->interval == 7500) {	// lc3 codec delay
		offset += 4000;
	} else {
		offset += 2500;
	}
	// offset -= 1000;

	SYS_LOG_INF("%dus\n", offset);
	return offset;
}

int broadcast_get_source_param(uint16_t kbps, uint8_t audio_chan,uint16_t iso_interval,struct broadcast_param_t *broadcast_param)
{
	if (!broadcast_param) {
		SYS_LOG_ERR("param NULL\n");
		return -1;
	}

	if (audio_chan != 1 && audio_chan != 2) {
		SYS_LOG_ERR("unsupport audio chan:%d\n",audio_chan);
		return -1;
	}

	if (!iso_interval) {
		iso_interval = BROADCAST_DEFAULT_ISO_INTERVAL;
	}

	broadcast_param->iso_interval = iso_interval;
	broadcast_param->audio_chan = audio_chan;
	broadcast_param->kbps = kbps * audio_chan;

	switch(iso_interval) {
		case BROADCAST_ISO_INTERVAL_7_5MS:
		{
			broadcast_param->sdu = 90 * audio_chan;
			broadcast_param->pdu = 90 * audio_chan;
			broadcast_param->octets = 90;
			broadcast_param->sdu_interval = 7500;
			broadcast_param->nse = 3;
			broadcast_param->bn = 1;
			broadcast_param->irc = 3;
			broadcast_param->pto = 0;
			broadcast_param->latency = 8;
			broadcast_param->duration = BT_FRAME_DURATION_7_5MS;
#if defined(CONFIG_BT_PAWR)
			broadcast_param->nse = 2;
			broadcast_param->irc = 2;
#endif
			broadcast_param->pa_interval = 72; //90ms
			break;
		}

		case BROADCAST_ISO_INTERVAL_10MS:
		{
			uint16_t octets = 0;
			octets = (kbps/8)*10;
			if (kbps == 80) {
				octets = 100;
				broadcast_param->nse = 4;
				broadcast_param->irc = 4;
			} else {
				octets = 120;
				broadcast_param->nse = 3;
				broadcast_param->irc = 3;
			}
#if defined(CONFIG_BT_PAWR)
			broadcast_param->nse = 2;
			broadcast_param->irc = 2;
#endif
			broadcast_param->sdu = octets * audio_chan;
			broadcast_param->pdu = octets * audio_chan;
			broadcast_param->octets = octets;
			broadcast_param->sdu_interval = 10000;
			broadcast_param->bn = 1;
			broadcast_param->pto = 0;
			broadcast_param->latency = 10;
			broadcast_param->duration = BT_FRAME_DURATION_10MS;
			broadcast_param->pa_interval = 80; //100ms
			break;
		}

		case BROADCAST_ISO_INTERVAL_15MS:
		{
			broadcast_param->sdu = 90 * audio_chan;
			broadcast_param->pdu = 90 * audio_chan;
			broadcast_param->octets = 90;
			broadcast_param->sdu_interval = 7500;
			broadcast_param->nse = 6;
			broadcast_param->bn = 2;
			broadcast_param->irc = 3;
			broadcast_param->pto = 0;
			broadcast_param->latency = 15;
			broadcast_param->duration = BT_FRAME_DURATION_7_5MS;
#if defined(CONFIG_BT_PAWR)
			broadcast_param->nse = 4;
			broadcast_param->irc = 2;
#endif
			broadcast_param->pa_interval = 72; //90ms
			break;
		}

		case BROADCAST_ISO_INTERVAL_20MS:
		{
			uint16_t octets = 0;
			octets = (kbps/8)*10;

			broadcast_param->sdu = octets * audio_chan;
			broadcast_param->pdu = octets * audio_chan;
			broadcast_param->octets = octets;
			broadcast_param->sdu_interval = 10000;
			broadcast_param->nse = 8;
			broadcast_param->bn = 2;
			broadcast_param->irc = 4;
			broadcast_param->pto = 0;
			broadcast_param->latency = 20;
			broadcast_param->duration = BT_FRAME_DURATION_10MS;
			broadcast_param->pa_interval = 80; //100ms

			break;
		}

		case BROADCAST_ISO_INTERVAL_30MS:
		{
			uint16_t octets = 0;
			octets = (kbps/8)*10;

			broadcast_param->sdu = octets * audio_chan;
			broadcast_param->pdu = octets * audio_chan;
			broadcast_param->octets = octets;
			broadcast_param->sdu_interval = 10000;
			broadcast_param->nse = 9;
			broadcast_param->bn = 3;
			broadcast_param->irc = 3;
			broadcast_param->pto = 0;
			broadcast_param->latency = 40;
			broadcast_param->duration = BT_FRAME_DURATION_10MS;
			broadcast_param->pa_interval = 72; //90ms
			break;
		}

		default:
			SYS_LOG_ERR("unsupport iso_interval:%d\n", iso_interval);
			return -1;
	}
	SYS_LOG_INF("kbps:%d,ch:%d,iso:%d,oct:%d,sdu:%d,pa_interval:%d,nse:%d,bn:%d,irc:%d",
	broadcast_param->kbps,broadcast_param->audio_chan,broadcast_param->iso_interval,broadcast_param->octets,
	broadcast_param->sdu,broadcast_param->pa_interval,broadcast_param->nse,broadcast_param->bn,broadcast_param->irc);

	return 0;
}

int8_t broadcast_get_sq_mode()
{
	return broadcast_sq_mode;
}

/*
 0:disable
 1:enable
*/
int8_t broadcast_set_sq_mode(int8_t sq_mode)
{
	broadcast_sq_mode = sq_mode;
	SYS_LOG_INF("%d",broadcast_sq_mode);
	return 0;
}


#if ENABLE_PADV_APP
#define INVALID_VOL 0xFF

static u8_t synced_vol = INVALID_VOL;
static os_delayed_work per_adv_dwork;
static bool per_adv_dwork_inited;
static u32_t padv_handle = 0;
#ifndef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
static u8_t padv_buf[32];
#else
static u8_t padv_buf[5];
#endif
static u8_t padv_stream_type = 0;

u8_t padv_volume_map(u8_t vol, u8_t to_adv)
{
	u8_t map;
	int i;
#if (MAX_AUDIO_VOL_LEVEL == 16)
	static const u8_t table[MAX_AUDIO_VOL_LEVEL + 1] = {
		0,
		6,	12,	18,	25,
		31,	37,	43,	50,
		56,	62,	68, 75,
		81,	87, 93,	100,
	};	
#elif (MAX_AUDIO_VOL_LEVEL == 31)
	static const u8_t table[MAX_AUDIO_VOL_LEVEL + 1] = {
		0,
		3,	6,	9,	12,
		15,	18,	21,	24,
		27,	30,	33, 36,
		39,	42, 45,	48,
		51,	54,	57,	60,
		63,	67,	70,	73,
		76,	79,	82, 85,
		88,	91, 100,
	};	
#else
	static const u8_t table[MAX_AUDIO_VOL_LEVEL + 1] = {
		0,
		3,	6,	9,	12,
		15,	18,	21,	25,
		28,	31,	34, 37,
		40,	43, 46,	50,
		53,	56,	59,	62,
		65,	68,	71,	75,
		78,	81,	84, 87,
		90,	93, 96, 100,
	};	
#endif
	if (to_adv != 0) {
		//local vol to adv vol
		if (vol > MAX_AUDIO_VOL_LEVEL)
		{
			SYS_LOG_ERR("vol max");
			vol = MAX_AUDIO_VOL_LEVEL;
		}
		map = table[vol];
	} else {
		//adv vol to local vol
		if (vol > 100) {
			SYS_LOG_ERR("%d", vol);
			vol = 100;
		}
		for (i = 0; i < MAX_AUDIO_VOL_LEVEL + 1; i++) {
			if (vol <= table[i]) {
				map = i;
				break;
			}
		}
		if (i == MAX_AUDIO_VOL_LEVEL + 1) {
			map = MAX_AUDIO_VOL_LEVEL;
		}
	}

	//SYS_LOG_INF("%d->%d, %d", vol, map, to_adv);

	return map;
}

static void per_adv_handler(struct k_work *work)
{
	u8_t vol;
	int ret_volume;

	ret_volume = system_volume_get(padv_stream_type);
	if (ret_volume >= 0) {
		vol = padv_volume_map(ret_volume, 1);
		padv_tx_data(vol);
	}

	os_delayed_work_submit(&per_adv_dwork, 1000);
}

/*
HM party series private data packet format definition, v1.8.1
UUID(2B) + PRIVATE(LTV+LTV+..+LTV)
LTV: Length(1B) + Type(1B) + Value(nB, n<=29)
Length: the length of Type + Value.
Type:
	0x01 - Lighting effects - 21 bytes data
	0x02 - Volume - 1 byte data [0-100]
	0x03 -
	0x04 -
	0x05 -
	0xF0 -
*/

int padv_tx_init(u32_t handle, u8_t stream_type)
{
	SYS_LOG_INF("0x%x", handle);
	padv_handle = handle;
	padv_stream_type = stream_type;

	synced_vol = INVALID_VOL;

	if (!per_adv_dwork_inited) {
		per_adv_dwork_inited = true;
		os_delayed_work_init(&per_adv_dwork, per_adv_handler);
		os_delayed_work_submit(&per_adv_dwork, 1000);
		int ret_volume = system_volume_get(stream_type);
		if (ret_volume >= 0) {
			padv_tx_data(padv_volume_map(ret_volume, 1));
		}
	}
	return 0;
}

int padv_tx_deinit(void)
{
	SYS_LOG_INF("\n");
	padv_handle = 0;
	padv_stream_type = 0;
	if (per_adv_dwork_inited) {
		k_delayed_work_cancel_sync(&per_adv_dwork, K_FOREVER);
		per_adv_dwork_inited = false;
	}
	return 0;
}

#ifndef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
int padv_tx_data(u8_t vol100)
{
	//Fixme: load actual lighting effect data
	static u8_t light = 0;

	u8_t type = 0; //Fixme: ? BT_DATA_SVC_DATA16
	u8_t offset = 0;

	__ASSERT(sizeof(padv_buf) >= 2+23+3, "buffer too small");

	if(0 == padv_handle) {
		return -1;
	}

	SYS_LOG_INF("light %d, vol %d", light, vol100);
	memset(padv_buf, 0, sizeof(padv_buf));

	//uuid
	padv_buf[offset++] = SERIVCE_UUID & 0xFF;
	padv_buf[offset++] = SERIVCE_UUID >> 8;

	//light effect
	padv_buf[offset++] = 1 + 21;
	padv_buf[offset++] = Lighting_effects_type;
	padv_buf[offset] = light++;
	offset+=21;

	//volume
	padv_buf[offset++] = 2;
	padv_buf[offset++] = Volume_type;
	padv_buf[offset++] = vol100;

	bt_manager_broadcast_source_vnd_per_send(padv_handle,
						 padv_buf,
						 offset, type);
	return 0;
}
#else
int padv_tx_data(u8_t vol100)
{
	u8_t type = 0;
	u8_t offset = 0;

	__ASSERT(sizeof(padv_buf) >= 2+3, "buffer too small");

	if(0 == padv_handle) {
		return -1;
	}

	memset(padv_buf, 0, sizeof(padv_buf));

	//uuid
	padv_buf[offset++] = SERIVCE_UUID & 0xFF;
	padv_buf[offset++] = SERIVCE_UUID >> 8;

	//volume
	padv_buf[offset++] = 2;
	padv_buf[offset++] = Volume_type;
	padv_buf[offset++] = vol100;

	bt_manager_broadcast_source_vnd_per_send(padv_handle,
						 padv_buf,
						 offset, type);
	return 0;
}
#endif

#ifdef ENABLE_PAWR_APP
//BMR sends volume in response of PAWR
int pawr_response_vol(u8_t vol100)
{
	u8_t buf[5];
	u8_t offset = 0;

	SYS_LOG_INF("vol %d", vol100);

	//UUID
	buf[offset++] = SERIVCE_UUID & 0xFF;
	buf[offset++] = SERIVCE_UUID >> 8;

	//volume
	buf[offset++] = 2;
	buf[offset++] = Volume_type;
	buf[offset++] = vol100;

	bt_manager_pawr_vnd_rsp_send(buf, sizeof(buf), BT_DATA_MANUFACTURER_DATA);
	return 0;
}

static int pawr_sync_volume(u8_t stream_type, u8_t sync_vol)
{
	//Sync remote volume only one time to avoid shielding local volume change.
	if (synced_vol != sync_vol) {
		SYS_LOG_INF("sync %d, prev=%d", sync_vol, synced_vol);
		if(sync_vol != system_volume_get(stream_type)) {
			SYS_LOG_INF("sync to %d", sync_vol);
			system_volume_set(stream_type, sync_vol, false);
		}
		synced_vol = sync_vol;
	}

	return 0;
}


//BMS's volume follows BMR's response
int pawr_handle_response(u8_t stream_type, const u8_t *buf, u16_t len)
{
	u8_t cmd_length = 0;
	u8_t cmd_type = 0;
	u8_t sync_vol;
	u16_t offset = 0;

	//SYS_LOG_INF("stream:0x%x len:%d", stream_type, len);

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
			SYS_LOG_INF("light:%d\n", buf[offset]);
			break;

		case Volume_type:
			sync_vol = padv_volume_map(buf[offset], 0);
			pawr_sync_volume(stream_type, sync_vol);
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

#endif
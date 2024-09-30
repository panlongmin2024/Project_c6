#include "selfapp_internal.h"
#include "selfapp_cmd_def.h"
#include "selfapp_config.h"

#include <power_manager.h>
#include <audio_system.h>

#ifdef CONFIG_DATA_ANALY
#include <data_analy.h>

enum Analytics_Category_e {
	ANACategory_Universal = 0x01,
	ANACategory_LightPattern,
	ANACategory_Canvas,
};

enum Universal_Feature_e {
	Times_Using_JBLConc = 0x01,
	Duration_Using_JBLConc,

	Time_Music,		// 0x03, in mins
	Time_Music_Charging,	// in mins
	Time_Charging,		// in mins
	Time_Music_Aux_in,

	Times_DUT_Poweron,	// 0x07
	Duration_Between_Power_OnOff,

	Times_Press_PlayPause,	// 0x09
	Times_DUT_Power_Bank,
	Times_TooHot_Shutdown,
	Times_SmartCtrl_Triggered,	// 0x0C
	Times_Waterproof_Alarm_Triggered,
	Times_Volume_Button_Adjusted,
	Times_Volume_AVRCP_Adjusted,

	Most_Popular_Volume_Level,	// 0x10
	Time_Most_Popolar_Volume_Used,

	Duration_Both_Light_On,	// 0x12
	Duration_Both_Light_Off,
	Duration_BodyLight_On,
	Duration_ProjectionLight_On,
	Time_ProjectionLight_Protection_Actived,
	Duration_using_Stereo,
	Duration_using_Auracast_Party,

	Universal_Feature_Max,
};

enum PlayAnalytics_Feature_e {
	Play_Number = 0x01,	// Play number in this uploading content
	Charging_Status,	// 0 DC, 1 AC charging
	Volume_Level,
	Partyboost_Status,	// 0x00 Normal, 0x01 PB Broadcasting, 0x02 PB Receiver
	Auracast_Status,	// {0-nornaml, 1-Broadcasting, 2-Receiver, 3-Stereo}
	Audio_In_Status,	// {1-A2DP, 2-LE Audio}

	EQ_Category_ID = 0x10,	// EQ_Category_Id_e
	EQ_Band_1,		// EQ level, [1, 13]
	EQ_Band_2,
	EQ_Band_3,
	EQ_Band_4,
	EQ_Band_5,

	Play_Duration = 0x30,	// Playing duration in seconds, 2bytes PayloadLen
};

static int pack_universal_package(u8_t * buf)
{
	u16_t len = 0;
	u8_t id;
	u16_t val;
	data_analytics_upload_t* data = NULL;
	int ret;
	u8_t features = 0;

	data = mem_malloc(sizeof(data_analytics_upload_t));
	if(NULL == data) {
		selfapp_log_wrn("malloc fails.");
		return len;
	}
	ret = data_analy_data_get(data, sizeof(data_analytics_upload_t));
	if(0 != ret){
		selfapp_log_wrn("analy ret=%d", ret);
		if (NULL != data) {
			mem_free(data);
		}
		return len;
	}

	buf[0] = ANACategory_Universal;
	// buf[1] is dynamic value.

	len = 2;
	features = 0;
	id = Times_Using_JBLConc;
	val = data->auracast_enter_times;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Duration_Using_JBLConc;
	val = data->auracast_rx_sec / 60;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Time_Music;
	val = data->music_playback_sec / 60;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Time_Music_Charging;
	val = data->music_playback_charge_sec / 60;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Time_Charging;
	val = data->power_on_charge_sec / 60;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Times_DUT_Poweron;
	val = data->power_on_times;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Duration_Between_Power_OnOff;
	val = data->power_on_sec / 60;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Times_Press_PlayPause;
	val = data->press_PP_times;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Times_SmartCtrl_Triggered;
	val = data->smart_ctl_times;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Times_Waterproof_Alarm_Triggered;
	val = data->usb_waterproof_alarm_times;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Times_Volume_Button_Adjusted;
	val = data->volume_changed_by_phy_times;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Times_Volume_AVRCP_Adjusted;
	val = data->volume_changed_by_avrcp_times;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Duration_using_Stereo;
	val = data->auracast_stereo_sec / 60;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	id = Duration_using_Auracast_Party;
	val = data->auracast_party_sec / 60;
	len += selfapp_pack_int(buf + len, id, 1);
	len += selfapp_pack_int(buf + len, val, 2);
	features++;

	buf[1] = features;;

	if (NULL != data) {
		mem_free(data);
	}
	return len;
}

static int pack_lightpattern_package(u8_t * buf)
{
	//TODO: lightpattern
	return 0;
}

static int pack_canvas_package(u8_t * buf)
{
	//TODO: canvas
	return 0;
}

static int send_analytics_data(u8_t * buf)
{
	u8_t i, pack_count = 0;
	u16_t hdrlen = 0, packlen = 0, sublen = 0;
	int ret;

	u8_t category[] = { ANACategory_Universal, ANACategory_LightPattern,
		ANACategory_Canvas
	};
	hdrlen = selfapp_get_header_len(APICMD_RetAnalyticsCmd);
	//skip number of categories.
	packlen += 1;

	for (i = 0; i < sizeof(category); i++) {
		if (category[i] == ANACategory_Universal) {
			sublen = pack_universal_package(buf + hdrlen + packlen);
			pack_count += sublen > 0 ? 1 : 0;
			packlen += sublen;
		} else if (category[i] == ANACategory_LightPattern) {
			sublen =
			    pack_lightpattern_package(buf + hdrlen + packlen);
			pack_count += sublen > 0 ? 1 : 0;
			packlen += sublen;
		} else if (category[i] == ANACategory_Canvas) {
			sublen = pack_canvas_package(buf + hdrlen + packlen);
			pack_count += sublen > 0 ? 1 : 0;
			packlen += sublen;
		} else {
			selfapp_log_wrn("ana category 0x%x unsupported\n",
					category[i]);
		}
	}

	//number of categories.
	buf[hdrlen] = pack_count;
	selfapp_log_inf("anadata %d categories\n", pack_count);

	packlen += selfapp_pack_header(buf, APICMD_RetAnalyticsCmd, packlen);
	ret = self_send_data(buf, packlen);
	return ret;
}

static int send_play_analytics_data(u8_t *buf, u16_t buflen)
{
	u16_t hl, pl, paylen, paycount;
	int datacount;
	u8_t* payload;
	play_analytics_upload_t* data = NULL;
	play_analytics_upload_t* p_analy = NULL;
	int ret;
	int i;

	data = mem_malloc(DATA_ANALY_PLAY_ID_MAX * sizeof(play_analytics_upload_t));
	if(NULL == data) {
		selfapp_log_wrn("malloc fails.");
		return -1;
	}
	datacount = data_analy_play_get(data, DATA_ANALY_PLAY_ID_MAX);
	if(0 > datacount){
		selfapp_log_wrn("analy ret=%d", datacount);
		if (NULL != data) {
			mem_free(data);
		}
		return -1;
	}

	selfapp_log_inf("got %d*%d play data", datacount, sizeof(play_analytics_upload_t));

	hl = selfapp_get_header_len(APICMD_RetPlayAnalyticsCmd);
	paycount = 1;
	paylen   = 2;
	buf[hl+0] = datacount;
	buf[hl+1] = paycount;

	if (datacount == 0) {
		selfapp_pack_header(buf, APICMD_RetPlayAnalyticsCmd, 2);
		ret = self_send_data(buf, hl + 2);
		goto Label_exit;
	}

	for (i = 0; i < datacount; i++) {
		p_analy = data + (datacount - i - 1);  // send play record from old to new
		payload = &(buf[hl + paylen]);         // point to the end of packed-data
		pl = 0;

#ifdef SPEC_ANALYTICS_PLAYDATA_UNUSED_ITEMS
		payload[pl++] = Play_Number;
		payload[pl++] = i;
#endif

		payload[pl++] = Charging_Status;
		payload[pl++] = p_analy->charge_status;

		payload[pl++] = Volume_Level;
		payload[pl++] = p_analy->vol_level;

#ifdef SPEC_ANALYTICS_PLAYDATA_UNUSED_ITEMS
		payload[pl++] = Partyboost_Status;
		payload[pl++] = p_analy->party_boost;
#endif

		payload[pl++] = Auracast_Status;
		payload[pl++] = p_analy->auracast_status;

		payload[pl++] = Audio_In_Status;
		payload[pl++] = p_analy->audio_in_type;

		payload[pl++] = EQ_Category_ID;
		payload[pl++] = selfapp_eq_get_id_by_index(p_analy->eq_id);

		//2 bytes value
		payload[pl++] = Play_Duration;
		payload[pl++] = 0; // play minutes high byte
		payload[pl++] = p_analy->play_min; // play minutes low byte

		paylen += pl;

		// buff full, split into multi-packages
		if (hl + paylen + pl >= buflen) {
			selfapp_pack_header(buf, APICMD_RetPlayAnalyticsCmd, paylen);
			ret = self_send_data(buf, hl + paylen);
			if (ret != 0) {
				selfapp_log_err("pack %d_%d(%d/%d) fail(%d)", paycount, paylen, i, datacount, ret);
				paylen = 2;
				break;
			}

			// another splited-package, paycount to distinguish
			memset(buf, 0, buflen);
			paycount += 1;  // sub-package index
			paylen    = 2;
			buf[hl+0] = datacount;
			buf[hl+1] = paycount;
			continue;
		}
	}

	if (paylen > 2) {
		selfapp_pack_header(buf, APICMD_RetPlayAnalyticsCmd, paylen);
		ret = self_send_data(buf, hl + paylen);
		if (ret != 0) {
			selfapp_log_err("pack %d_%d(%d/%d) fail(%d)", paycount, paylen, i, datacount, ret);
		}
	}

Label_exit:
	if (NULL != data) {
		mem_free(data);
	}

	return ret;
}

int cmdgroup_analytics(u8_t CmdID, u8_t * Payload, u16_t PayloadLen)
{
	u8_t *buf = self_get_sendbuf();
	int ret = -1;

	if (buf == NULL) {
		return ret;
	}

	switch (CmdID) {
	case APICMD_ReqAnalyticsData:
		ret = send_analytics_data(buf);
		break;

	case APICMD_ReqPlayAnalyticsData:
		ret = send_play_analytics_data(buf, SELF_SENDBUF_SIZE);
		break;
	default:
		break;
	}

	return ret;
}
#endif
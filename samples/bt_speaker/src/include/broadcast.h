#ifndef _BROADCAST_H_
#define _BROADCAST_H_

#if CONFIG_EXTERNAL_DSP_DELAY > 0
#define EXTERNAL_DSP_DELAY         (CONFIG_EXTERNAL_DSP_DELAY)
#define BCST_QOS_DELAY             (30000 + CONFIG_EXTERNAL_DSP_DELAY)
#else
#define EXTERNAL_DSP_DELAY         (0)
#define BCST_QOS_DELAY             (80000)
#endif

#define ENABLE_BROADCAST 1
#define ENABLE_PADV_APP	1
#ifdef CONFIG_BT_PAWR
#define ENABLE_PAWR_APP	1
#endif

#define FILTER_LOCAL_NAME	0

#ifndef CONFIG_AUARCAST_FILTER_POLICY_H

#define FILTER_BROADCAST_NAME	1
#define FILTER_BROADCAST_ID	1

#else

#define FILTER_BROADCAST_NAME	0
#define FILTER_BROADCAST_ID	0

#endif

#define BIS_SYNC_LOCATIONS	1	/* NOT support encryption yet */
#if BIS_SYNC_LOCATIONS
#define ENABLE_ENCRYPTION	0
#else
#define ENABLE_ENCRYPTION	1
#endif


#define COMPANY_ID	CONFIG_BT_COMPANY_ID

#define SERIVCE_UUID	0xFDDF

#define BROADCAST_NUM_BIS  2

/* 0: VS with SERIVCE_UUID; 1: VS with COMPANY_ID */
#define VS_COMPANY_ID	1
#if VS_COMPANY_ID
#define VS_ID	COMPANY_ID
#define FILTER_COMPANY_ID	1
#define FILTER_UUID16	0
#else
#define VS_ID	SERIVCE_UUID
#define FILTER_COMPANY_ID	0
#define FILTER_UUID16	1
#endif

#define NUM_OF_BROAD_CHAN  2

/*type of Party series private data*/
#define Lighting_effects_type 0x1
#define Volume_type 0x2


/*
long lasting stereo protocol
*/
#define COMMAND_IDENTIFIER         0xAA
#define COMMAND_DEVACK             0x00

#define COMMAND_SETDEVINFO         0x13
#define COMMAND_SETKEYEVENT        0x31
#define COMMAND_SETLIGHTINFO       0x41
#define COMMAND_REQUSEREQ          0x91
#define COMMAND_RSPUSEREQ          0x92
#define COMMAND_SETUSEREQ          0x93
#define COMMAND_REQ_PASTINFO       0xf1
#define COMMAND_SENDSYSEVENT       0xf2

enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PLAY,
	KEY_EVENT_PAUSE,
	KEY_EVENT_VOLUME_UP,
	KEY_EVENT_VOLUNE_DOWN,
	KEY_EVENT_PARTY,
	KEY_EVENT_BT,
	KEY_EVENT_POWER_OFF,
};

struct lasting_stereo_device_info {
	uint8_t ch; // 0x00-full channel; 0x01-Left channel, 0x02-right channel.
	uint32_t id;
	uint8_t name[30];
	uint8_t addr[6];
};

enum {
	EQ_NAME_OFF = 0,
	EQ_NAME_BASSBOOST_1,
	EQ_NAME_BASSBOOST_2,
	EQ_NAME_VOCAL,
	EQ_NAME_METAL,
	EQ_NAME_CLASSICAL,
	EQ_NAME_JBL_SIGNATURE,
	EQ_NAME_RELAXING,
	EQ_NAME_ENERGETIC,
	EQ_NAME_PARTY,
	EQ_NAME_CUSTOM        = 0xC1,
};

struct lasting_stereo_eq_info {
	uint8_t eq_name;
	uint8_t eq_enable;
	uint8_t band_level01;
	uint8_t band_level02;
	uint8_t band_level03;
};

struct broadcast_param_t {
	uint16_t pa_interval;
	uint32_t sdu_interval;
	uint16_t kbps;
	uint16_t sdu;
	uint16_t pdu;
	uint8_t octets;
	uint8_t audio_chan;
	uint8_t iso_interval;
	uint8_t nse;
	uint8_t bn;
	uint8_t irc;
	uint8_t pto;
	uint8_t latency;
	uint8_t duration;
};

/* 96kbp 1ch 7.5ms iso_interval */
#if 0
#define BROADCAST_KBPS	96
#define BROADCAST_SDU	90
#define BROADCAST_OCTETS	90
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	6
#define BROADCAST_SDU_INTERVAL	7500
#define BROADCAST_PDU	90
#define BROADCAST_NSE	3
#define BROADCAST_BN	1
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	8
#define BROADCAST_DURATION	BT_FRAME_DURATION_7_5MS
#define PA_INTERVAL	72	// 90ms
#endif



/* 96kbp 1ch 15ms iso_interval */
#if 0
#define BROADCAST_KBPS	96
#define BROADCAST_SDU	90
#define BROADCAST_OCTETS	90
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	12
#define BROADCAST_SDU_INTERVAL	7500
#define BROADCAST_PDU	90
#define BROADCAST_NSE	6
#define BROADCAST_BN	2
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	15
#define BROADCAST_DURATION	BT_FRAME_DURATION_7_5MS
#define PA_INTERVAL	72	// 90ms
#endif



/* 96kbp 2ch 7.5ms iso_interval */
#if 0
#define BROADCAST_KBPS	192
#define BROADCAST_SDU	180
#define BROADCAST_OCTETS	90
#define BROADCAST_CH	2
#define BROADCAST_ISO_INTERVAL	6
#define BROADCAST_SDU_INTERVAL	7500
#define BROADCAST_PDU	180
#define BROADCAST_NSE	3
#define BROADCAST_BN	1
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	8
#define BROADCAST_DURATION	BT_FRAME_DURATION_7_5MS
#define PA_INTERVAL	72	// 90ms
#endif



/* 96kbp 2ch 15ms iso_interval */
#if 0
#define BROADCAST_KBPS	192
#define BROADCAST_SDU	180
#define BROADCAST_OCTETS	90
#define BROADCAST_CH	2
#define BROADCAST_ISO_INTERVAL	12
#define BROADCAST_SDU_INTERVAL	7500
#define BROADCAST_PDU	180
#define BROADCAST_NSE	6
#define BROADCAST_BN	2
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	15
#define BROADCAST_DURATION	BT_FRAME_DURATION_7_5MS
#define PA_INTERVAL	72	// 90ms
#endif



/* 96kbp 1ch 20ms iso_interval */
#if 0
#define BROADCAST_KBPS	96
#define BROADCAST_SDU	120
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	16
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	120
#define BROADCAST_NSE	6
#define BROADCAST_BN	2
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	20
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	80	// 100ms

#ifdef CONFIG_BT_PAWR
#define PAwR_INTERVAL	80	// 100ms
#define PAwR_SUB_INTERVAL	32	// 40ms
#define PAwR_RSP_DELAY	9	// 11.25ms

#endif
#endif


/* 80kbp 1ch 20ms iso_interval */
#if 1
#define BROADCAST_KBPS	80
#define BROADCAST_SDU	100
#define BROADCAST_OCTETS	100
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	16
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	100
#define BROADCAST_NSE	8
#define BROADCAST_BN	2
#define BROADCAST_IRC	4
#define BROADCAST_PTO	0
#define BROADCAST_LAT	20
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	80	// 100ms

#ifdef CONFIG_BT_PAWR
#define PAwR_INTERVAL	80	// 100ms
#define PAwR_SUB_INTERVAL	32	// 40ms
#define PAwR_RSP_DELAY	9	// 11.25ms

#endif
#endif


/* 96kbp 1ch 10ms iso_interval */
#if 0
#define BROADCAST_KBPS	96
#define BROADCAST_SDU	120
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	8
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	120

#ifndef CONFIG_BT_PAWR
#define BROADCAST_NSE	3
#define BROADCAST_IRC	3
#else
#define BROADCAST_NSE	2
#define BROADCAST_IRC	2
#endif

#define BROADCAST_BN	1

#define BROADCAST_PTO	0
#define BROADCAST_LAT	10
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	80	// 100ms

#ifdef CONFIG_BT_PAWR
#define PAwR_INTERVAL	80	// 100ms
#define PAwR_SUB_INTERVAL	32	// 40ms
#define PAwR_RSP_DELAY	5	// 6.25ms

#endif

#endif



/* 96kbp 2ch 20ms iso_interval */
#if 0
#define BROADCAST_KBPS	192
#define BROADCAST_SDU	240
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	2
#define BROADCAST_ISO_INTERVAL	16
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	240
#define BROADCAST_NSE	6
#define BROADCAST_BN	2
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	20
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	80	// 100ms
#endif



/* 96kbp 2ch 10ms iso_interval */
#if 0
#define BROADCAST_KBPS	192
#define BROADCAST_SDU	240
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	2
#define BROADCAST_ISO_INTERVAL	8
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	240
#define BROADCAST_NSE	3
#define BROADCAST_BN	1
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	10
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	80	// 100ms
#endif



/* 96kbp 2ch 30ms iso_interval */
#if 0
#define BROADCAST_KBPS	192
#define BROADCAST_SDU	240
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	2
#define BROADCAST_ISO_INTERVAL	24
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	240
#define BROADCAST_NSE	9
#define BROADCAST_BN	3
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	40
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	72	// 90ms
#endif



/* 96kbp 1ch 30ms iso_interval */
#if 0
#define BROADCAST_KBPS	96
#define BROADCAST_SDU	120
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	24
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	120
#define BROADCAST_NSE	9
#define BROADCAST_BN	3
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	40
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	72	// 90ms

#ifdef CONFIG_BT_PAWR
#define PAwR_INTERVAL	72	// 90ms
#define PAwR_SUB_INTERVAL	24	// 30ms
#define PAwR_RSP_DELAY	12	// 15ms

#endif

#endif

/* 80kbp 1ch 30ms iso_interval */
#if 0
#define BROADCAST_KBPS	80
#define BROADCAST_SDU	100
#define BROADCAST_OCTETS	100
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	24
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	100
#define BROADCAST_NSE	9
#define BROADCAST_BN	3
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	40
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	72	// 90ms

#ifdef CONFIG_BT_PAWR
#define PAwR_INTERVAL	72	// 90ms
#define PAwR_SUB_INTERVAL	24	// 30ms
#define PAwR_RSP_DELAY	12	// 15ms

#endif

#endif

#define BROADCAST_TRANSMIT_NUMBER (BROADCAST_NSE/BROADCAST_BN)

int broadcast_get_broadcast_id(void);
int broadcast_get_bis_link_delay(struct bt_broadcast_qos *qos);
int broadcast_get_bis_link_delay_ext(struct bt_broadcast_qos *qos, uint8_t iso_interval);
int broadcast_get_tws_sync_offset(struct bt_broadcast_qos *qos);

#ifdef ENABLE_PADV_APP
int padv_tx_init(u32_t handle, u8_t stream_type);
int padv_tx_deinit(void);
int padv_tx_data(u8_t vol100);
u8_t padv_volume_map(u8_t vol, u8_t to_adv);
#endif

#ifdef ENABLE_PAWR_APP
int pawr_response_vol(u8_t vol100);
int pawr_handle_response(u8_t stream_type, const u8_t *buf, u16_t len);
#endif
uint8_t get_pawr_enable(void);

void broadcast_tws_vnd_send_ack(uint8_t ack_cmdid,uint8_t StatusCode);
void broadcast_tws_vnd_send_key(uint32_t key);
int broadcast_tws_vnd_rx_cb(const uint8_t *buf, uint16_t len);
void broadcast_tws_vnd_request_past_info(void);
void broadcast_tws_vnd_send_sys_event(uint8_t event);
void broadcast_tws_vnd_set_dev_info(struct lasting_stereo_device_info *info);
#endif

#include <selfapp_api.h>
#include <os_common_api.h>
#include <thread_timer.h>
#include <logging/sys_log.h>
#include "selfapp_crc16.h"
#include "selfapp_adaptor.h"
#include <bt_manager.h>

#ifndef _SELFAPP_INTERNEL_H_
#define _SELFAPP_INTERNEL_H_

//Configration
#define SELFAPP_DEBUG 1
//#define LED_PULSE_2 1
//#define LED_PULSE_3 1
//#define LED_PULSE_5 1  // BOX_PRODUCT_ID = 0x2050
#ifdef LED_PULSE_5
#define SPEC_LED_PATTERN_CONTROL
#endif
//#define SPEC_ONE_TOUCH_MUSIC_BUTTON
#ifdef CONFIG_BT_LETWS
#define SPEC_REMOTE_CONTROL
#endif
//#define SPEC_EQ_GENERAL
//#define SPEC_PARTYBOOST


#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "Self"

#ifdef SELFAPP_DEBUG
#define selfapp_log_dbg(str, ...) SYS_LOG_INF(str, ##__VA_ARGS__)
#define selfapp_log_dump(addr, count) print_buffer(addr, 1, count, 16, -1)
#else
#define selfapp_log_dbg(str, ...)
#define selfapp_log_dump(addr, count)
#endif
#define selfapp_log_inf(str, ...) SYS_LOG_INF(str, ##__VA_ARGS__)
#define selfapp_log_wrn(str, ...) SYS_LOG_WRN(str, ##__VA_ARGS__)
#define selfapp_log_err(str, ...) SYS_LOG_ERR(str, ##__VA_ARGS__)

enum Status_e {
	STA_ON = 1,
	STA_OFF = 0,
};

enum DeviceInfo_Role_e {
	DevRole_Normal = 0,
	DevRole_Broadcaster,
	DevRole_Receiver,
};

#define ROUTINE_INTERVAL   (10)
#define SELFSTREAM_MAX      (2)
#define SELFSTREAM_PREOPEN  (1)  // open stream so soon as created, keep the memory donot free

#define SELF_STREAM_SIZE   (0x400)
#define SELF_SENDBUF_SIZE  (0x100)
#define SELF_RECVBUF_SIZE  (0x100)
#define SELF_PACKET_MAX    (0x100)
#define SELF_EXTERNAL_SENDBUF_SIZE (0x100)
#define SELF_NVRAM_STA     "SELFSTA"
#define SELF_NVRAM_STASZ   "SELFSTASZ"

enum MFB_Status_e {
	MFBSTA_PLAY_PAUSE = 0x0,
	MFBSTA_VOICE_TRIGER,
};

#ifdef SPEC_ONE_TOUCH_MUSIC_BUTTON
typedef struct {
	u8_t action;		//0x01 long press, 0x02 double click
	u8_t button_id;		//0x01 lingting button, 0x02 play button
} one_touch_button_t;
#endif

#define NORMALEQ_BAND_COUNT  (3)
#define BAND_COUNT_MAX  (7)
#define EQ_DATA_SIZE (100)

enum EQ_Category_Id_e {
	EQCATEGORY_BALANCE = 0x00,
	EQCATEGORY_BASSBOOST1 = 0x01,
	EQCATEGORY_BASSBOOST2 = 0x02,
	EQCATEGORY_VOCAL = 0x03,
	EQCATEGORY_METAL = 0x04,
	EQCATEGORY_CLASSICAL = 0x05,
	EQCATEGORY_SIGNATURE = 0x06,
	EQCATEGORY_RELAXING = 0x07,
	EQCATEGORY_ENERGETIC = 0x08,
	EQCATEGORY_EXTREME = 0x09,

	EQCATEGORY_PLAYTIMEBOOST = 0x21,	// EQCATEGORY_PRESERVED_1
	EQCATEGORY_PRESERVED_2 = 0x22, // CHILL
	EQCATEGORY_PRESERVED_3,
	EQCATEGORY_PRESERVED_4,
	EQCATEGORY_PRESERVED_5,
	EQCATEGORY_PRESERVED_6,
	EQCATEGORY_PRESERVED_7,
	EQCATEGORY_PRESERVED_8,
	EQCATEGORY_PRESERVED_9,
	EQCATEGORY_PRESERVED_10,
	EQCATEGORY_PRESERVED_11,

	EQCATEGORY_CUSTOM_1 = 0xc1,
	EQCATEGORY_CUSTOM_2 = 0xc2,
};

#define	EQCATEGORY_DEFAULT EQCATEGORY_SIGNATURE

enum PresetEQ_Band_Type_e {
	IIRFilter_LowShelf = 0x00,
	IIRFilter_Peaking,
	IIRFilter_HighShelf,
	IIRFilter_LowPass,
	IIRFilter_HighPass,
};

enum CustomEQ_c1_Band_Type_e {
	Custom_c1_Band_1 = 0x01,	// Bass
	Custom_c1_Band_2,		// Mid
	Custom_c1_Band_3,		// Treble
	Custom_c1_Band_4,
	Custom_c1_Band_5,
	Custom_c1_Band_6,
	Custom_c1_Band_7,
};

enum CustomEQ_c1_Level_Scope_e {
	Custom_c1_Scope_n1p1 = 0x01,	//[-1, 1]
	Custom_c1_Scope_n2p2 = 0x02,	//[-2, 2]

	Custom_c1_Scope_n6p6 = 0x06,	//[-6, 6]
};
#define DEFAULT_SCOPE  (Custom_c1_Scope_n6p6 )

#define SFS_NONE    (0)
#define SFS_OPENED  (1)
#define SFS_CONCED  (2)

typedef struct {
	void  *handle;
	u8_t   connect_type;   // BT_Connect_Type_e
	u8_t   opened;         // SFS_NONE, SFS_OPENED, SFS_CONCED(can use)
	u8_t   index;
	u8_t   major;          // 1 mark that connection already exchange dev-info
} selfstream_t;

typedef struct {
	void *otadfu;
	void *spkeq;
	void *ledpkg;
#ifdef CONFIG_LOGSRV_SELF_APP
	void *logsrv;
#endif
	u8_t *sendbuf;
	u8_t *recvbuf;

	selfstream_t  sfstream[SELFSTREAM_MAX];
	void *current_hdl;  // handle of current handling sfstream
	u8_t  connect_num;  // amount of connecting sfstream

	u8_t stream_handle_suspend;
	u8_t pause_player:1;
	struct thread_timer timer;
#ifdef CONFIG_LOGSRV_SELF_APP
	p_logsrv_callback_t log_cb;
#endif
	struct AURACAST_GROUP creat_group;
	u32_t creat_group_time_out;
	u32_t indication_time_out;
	struct thread_timer creat_group_timer;
	struct thread_timer indication_timer;
	selfapp_device_info_t secondary_device;
	u32_t sendbuf_tid;
} selfapp_context_t;

extern selfapp_context_t *self_get_context(void);
extern u8_t *self_get_sendbuf(void);
extern int self_send_data(u8_t * buf, u16_t len);
extern int self_send_data_all_handle(u8_t *buf, u16_t len);
extern int self_send_data_major_handle(void *except_hdl, u8_t *buf, u16_t len);
extern int selfapp_stream_handle_suspend(uint8_t suspend_enable);
extern selfstream_t *self_find_sfstream_by_handle(void *stream_hdl);
extern selfstream_t *self_find_current_sfstream(void);
extern void *self_get_current_streamhdl(void);
extern void  self_set_sfstream_major(void);
extern u8_t  self_check_current_sfstream_major(void);

//cmd pack
u8_t selfapp_has_large_payload(u8_t cmdid);
int selfapp_playload_len_is_2(u8_t cmd);
int selfapp_check_status_response(u8_t cmd);
bool selfapp_cmd_check_id(u8_t val);
int selfapp_check_header(u8_t * buf, u16_t len, u8_t *cmdid, u8_t **payload, u16_t *paylen);
u16_t selfapp_get_header_len(u8_t command);
u16_t selfapp_pack_header(u8_t * buf, u8_t cmd, u16_t len);
u16_t selfapp_pack_bytes(u8_t * buf, u8_t * data, u16_t len);
u8_t selfapp_pack_int(u8_t * buf, u32_t data, u8_t len);
u16_t selfapp_pack_cmd_with_int(u8_t * buf, u8_t cmd, u32_t data, u8_t len);
u16_t selfapp_pack_cmd_with_bytes(u8_t * buf, u8_t cmd, u8_t * data, u16_t len);
u16_t selfapp_pack_devack(u8_t * buf, u8_t ackcmd, u8_t statuscode);
u16_t selfapp_pack_unsupported(u8_t * buf, u8_t ackcmd);

//cmd normal handler
int selfapp_cmd_handler(u8_t * buf, u16_t len);
u8_t cmdgroup_special(u8_t CmdID, u8_t * Payload, u16_t PayloadLen, int *result);
int cmdgroup_general(u8_t CmdID, u8_t * Payload, u16_t PayloadLen);
int cmdgroup_device_info(u8_t CmdID, u8_t * Payload, u16_t PayloadLen);
#ifdef SPEC_REMOTE_CONTROL
int cmdgroup_remote_control(u8_t CmdID, u8_t * Payload, u16_t PayloadLen);
#endif
int cmdgroup_speaker_settings(u8_t CmdID, u8_t * Payload, u16_t PayloadLen);
int cmdgroup_analytics(u8_t CmdID, u8_t * Payload, u16_t PayloadLen);

//cmd led handler
#ifdef SPEC_LED_PATTERN_CONTROL
extern int cmdgroup_led_pattern_control(u8_t CmdID, u8_t * Payload, u16_t PayloadLen);
extern int cmdgroup_led_package(u8_t CmdID, u8_t * Payload, u16_t PayloadLen);
extern int ledpkg_init(void);
extern int ledpkg_deinit(void);
#endif

//cmd EQ handler
u8_t get_active_eq(void);
int spkeq_RetNTIEQ(u8_t * buf, u8_t custom_type);
int spkeq_SetNTIEQ(u8_t * param, u16_t plen);
u8_t selfapp_eq_get_id_by_index(u8_t index);
const u8_t* selfapp_eq_get_default_data(void);

//cmd ota handler
extern u8_t otadfu_running(void);
extern int otadfu_NotifyDfuCancel(u8_t cansel_reason);
int cmdgroup_otadfu(u8_t CmdID, u8_t * Payload, u16_t PayloadLen);

extern void selfapp_command_process(void);

#ifdef CONFIG_LOGSRV_SELF_APP
// log service
int selfapp_logsrv_init(void *data_stream);
void selfapp_logsrv_exit(void);
void selfapp_logsrv_timer_routine(void);
int selfapp_logsrv_check_id(u8_t *buf,u16_t len);
int selfapp_logsrv_callback_register(p_logsrv_callback_t cb);
void *selfapp_logsrv_get_datastream(void);
#endif

/*
size - eq size in bytes.
Return:
	NULL - no eq command
	Others  - eq command address

*/
u8_t* selfapp_eq_cmd_get(u16_t * size);
void selfapp_eq_cmd_update(u8_t id, const u8_t* data, u16_t len);
int selfapp_eq_cmd_switch_eq(u8_t id, u8_t pre_id, const u8_t *data, u8_t len);
void selfapp_set_user_bat_cap(u8_t bat);
void selfapp_report_secondary_device_info(void);

void self_creat_group_handler(struct thread_timer *ttimer, void *expiry_fn_arg);

void selfapp_cmd_thread_timer_start(struct thread_timer *cur_timer);

int spk_ret_auracast_group(u8_t * Payload, u16_t PayloadLen);

struct bt_conn * selfapp_get_current_device_conn(void);

#endif

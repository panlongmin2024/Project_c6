#ifndef _SELFAPP_API_H
#define _SELFAPP_API_H

#include <stdbool.h>
#include <zephyr/types.h>

#define SELF_BTNAME_LEN    (32)
#define SELF_SN_LEN    (16)
#define SELF_DEFAULT_SN "Do Not-available"
#define SELF_DEFAULT_SN_LEN (sizeof(SELF_DEFAULT_SN))

struct AURACAST_GROUP {
	u8_t ch; // 0x00-full channel; 0x01-Left channel, 0x02-right channel.
	u8_t mode;// 0x01-stereo mode; 0x02-party mode.
	u8_t role;// 0x01-Primary device; 0x02-Secondary device.
	u32_t id;
	u8_t name[32];
	u8_t addr[6];
};

typedef struct{
	u8_t validate:1;
	u8_t bat_status;
	u8_t firmware_version[4];
	u8_t serial_num[SELF_SN_LEN+1];
	u8_t bt_name[SELF_BTNAME_LEN+1];
}selfapp_device_info_t;

typedef int (*p_logsrv_callback_t)(int log_type, int (*traverse_cb)(uint8_t *data, uint32_t max_len));

extern int selfapp_init(p_logsrv_callback_t cb);
extern int selfapp_deinit(void);

inline u8_t hex2dec_digitpos(u8_t number)
{
	u8_t ten, unit;
	if (number >= 100) {
		return 0xEE;
	}
	ten  = number / 10;
	unit = number - ten * 10;
	return (u8_t)((ten << 4) | unit);
}

/*
 Send message to selfapp thread (thread timer of system app thread)
*/
void selfapp_send_msg(u8_t type, u8_t cmd, u8_t param, int val);

/*
Mobile app asks for adv data of manuafacturer type.
*/
extern u8_t selfapp_get_manufacturer_data(u8_t *buf);

extern bool selfapp_otadfu_is_working(void);

/*
When enter/exit auracast(bmr/bms), it should notify mobile app.
*/
extern void selfapp_notify_role(void);

/*
Mobile app sets the playback channel mode in Stereo group setting.
return: output channel mode
	0 - Stereo
	1 - channel left
	2 - channel right
*/
u8_t selfapp_get_channel(void);

/*
Return: stereo group id
*/
u32_t selfapp_get_group_id(void);

/*
Return: stereo group name
*/
void selfapp_get_group_name(u8_t* name,int len);

/*
Selfapp connect event action.
*/
void selfapp_on_connect_event(u8_t connect, u8_t connect_type, u32_t handle);

/*
Return: [true, false].
	ture - playtime boost on
	false- playtime boost off
*/
bool selfapp_eq_is_playtimeboost(void);

/*
Return: [true, false].
	ture - Use default EQ. (Preset EQ)
	false- Use not default EQ. (Customer EQ)
*/
bool selfapp_eq_is_default(void);

/*
Return: EQ Category index, 4bits (0x0-0xF)
*/
u8_t selfapp_eq_get_index(void);

/*
Return:
	<0 - fail
	0  - Success
*/
int selfapp_report_bat(void);

/*
Reset nvram storage data of selfapp.
*/
void selfapp_config_reset(void);

/*
Switch on/off auracast eq mode.
mode:
	0 - auracast eq off
	others - auracast eq on
*/
void selfapp_eq_cmd_switch_auracast(int mode);

/*
Switch on/off dsp hardware test.
enable:
	false - normal mode
	true - hardware test mode.
*/
void selfapp_eq_cmd_switch_hw_test(bool enable);

/* Load selfapp configs. */
void selfapp_config_init(void);

/*
primary device sets the stereo group info to secondary device
*/
void selfapp_set_lasting_stereo_group_info(struct AURACAST_GROUP *group);

/*
Return:
	<0 - fail
	0  - Success
*/
void selfapp_notify_lasting_stereo_status(void);

void selfapp_set_device_info(selfapp_device_info_t *info);

int selfapp_update_bat_for_secondary_device(u8_t value);

u8_t selfapp_get_bat_power(void);

int selfapp_report_leaudio_status(void);

void selfapp_thread_timer_start(int value);

#endif

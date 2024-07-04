#include "selfapp_internal.h"
#include "selfapp_cmd_def.h"
#include "selfapp_config.h"

#include <mem_manager.h>
#include <property_manager.h>
#include <media_effect_param.h>
#include <media_player.h>

/*
Command List
ID_CMD_TUNING
	cmd: 0x20
	size: depending on tuning parameter
	buf: all the data sent by tunning tools.
ID_CMD_DSPMODE
	cmd: 0x21
	size: 1
	buf: 0: normal mode,  1: hardware test mode.
ID_USER_EQ
	cmd: 0x22
	size: 1+25, or 1+5
	buf: the first data is flag (0:preset eq; 1:user eq) and the reset is eq parameter or index based on flag value.
ID_EQMODE_SWITCH
	cmd: 0x23
	size: 1
	buf: 0:normal eq; 1: soundboost eq.
*/

//#define COMMAND_NUM (347)
#define COMMAND_NUM (EQ_DATA_SIZE/4+4)
typedef struct __attribute__((packed)) {
	u32_t update_flag;	// 0 is update
	u32_t cmd;
	u32_t size;
	u32_t buf[COMMAND_NUM];
} hm_dsp_command_t;


static bool g_hw_test_mode = false;
static hm_dsp_command_t g_eq_dsp_cmd;

u8_t* selfapp_eq_cmd_get(u16_t * size)
{
	*size = sizeof(hm_dsp_command_t);
	return (u8_t*)&g_eq_dsp_cmd;
}

static void selfapp_eq_cmd_pack(hm_dsp_command_t *cmd, u8_t id, const u8_t* data, u16_t len)
{
	/* 
	   0x06 + 100B - JBL Signature
	   0x22 + 100B - CHILL
	   0x08 + 100B - Energetic
	   0x03 + 100B - Vocal
	   0xC1 + 13B  - Custom  (c1 c1 06 05 01 06 02 05 03 00 04 FF 05 FE)
	   0x21 + 100B - PlaytimeBoost (the 100B are stuff data, not real EQ data)
	 */

	memset(cmd, 0, sizeof(hm_dsp_command_t));

	cmd->update_flag = 0;
	if (id == EQCATEGORY_PLAYTIMEBOOST) {
		cmd->cmd = 0x23;
		cmd->size = 1;
		cmd->buf[0] = 1;
	} else if (id == EQCATEGORY_CUSTOM_1) {
		u8_t num = data[2];
		if(num != BAND_COUNT_MAX) {
			selfapp_log_wrn("wrong custmoer size %d.", num);
		} else {
			cmd->cmd = 0x22;
			cmd->size = 1 + num;
			cmd->buf[0] = 1;
			for (int i = 0; i < num; i++) {
				cmd->buf[1+i] = 6 - (int8_t)(data[4 + i*2]);
			}
		}
	} else {
		if(len != EQ_DATA_SIZE) {
			selfapp_log_wrn("wrong eq size %d.", len);
		} else {
			cmd->cmd = 0x22;
			cmd->size = 1 + len/4;
			cmd->buf[0] = 0;
			memcpy((u8_t*)(cmd->buf+1), data, len);
		}
	}
}

void selfapp_eq_cmd_update(u8_t id, const u8_t* data, u16_t len)
{
	selfapp_log_inf("0x%x %d", id, len);

	if (g_hw_test_mode) {
		selfapp_log_wrn("In hw test mode.");
		return;
	}

	if(NULL != data) {
		selfapp_eq_cmd_pack(&g_eq_dsp_cmd, id, data, len);
	} else {
		selfapp_log_wrn("No data");
	}
}

int selfapp_eq_cmd_switch_eq(u8_t id, u8_t pre_id, const u8_t *data, u8_t len)
{
	media_player_t *media_handle = NULL;
	hm_dsp_command_t *cmd = NULL;

	media_handle = media_player_get_current_dumpable_player();
	if (NULL == media_handle) {
		selfapp_log_wrn("No player");
		return -1;
	}

	cmd = mem_malloc(sizeof(hm_dsp_command_t));
	if (cmd == NULL) {
		selfapp_log_err("malloc fails\n");
		return -1;
	}

	memset(cmd, 0, sizeof(hm_dsp_command_t));
	if (id == EQCATEGORY_PLAYTIMEBOOST) {
		selfapp_log_inf("Reset EQ.");
		cmd->update_flag = 0;
		cmd->cmd = 0x22;
		cmd->size = 1 + BAND_COUNT_MAX;
		cmd->buf[0] = 1;
		for (int i = 0; i < BAND_COUNT_MAX; i++) {
			cmd->buf[1+i] = 6;
		}
		selfapp_log_dump(cmd, sizeof(hm_dsp_command_t));
		media_player_update_effect_param(media_handle, cmd, sizeof(hm_dsp_command_t));
		//Interval of two dsp comnands is 2.6ms, the time of a data frame.
		os_sleep(10);
	} else if (pre_id == EQCATEGORY_PLAYTIMEBOOST) {
		selfapp_log_inf("Close PlaytimeBoost.");
		cmd->update_flag = 0;
		cmd->cmd = 0x23;
		cmd->size = 1;
		cmd->buf[0] = 0;
		selfapp_log_dump(cmd, sizeof(hm_dsp_command_t));
		media_player_update_effect_param(media_handle, cmd, sizeof(hm_dsp_command_t));
		//Interval of two dsp comnands is 2.6ms, the time of a data frame.
		os_sleep(10);
	}

	selfapp_eq_cmd_pack(cmd, id, data, len);

	selfapp_log_inf("Update EQ to dsp.");
	selfapp_log_dump(cmd, sizeof(hm_dsp_command_t));
	media_player_update_effect_param(media_handle, cmd, sizeof(hm_dsp_command_t));
	selfapp_log_inf("Free cmd.");
	mem_free(cmd);

	return 0;
}

void selfapp_eq_cmd_switch_auracast(int mode)
{
	if(0 == mode) {
		selfapp_eq_cmd_update(selfapp_config_get_eq_id(), selfapp_config_get_eq_data(), EQ_DATA_SIZE);
	} else {
		selfapp_eq_cmd_update(EQCATEGORY_DEFAULT, selfapp_eq_get_default_data(), EQ_DATA_SIZE);
	}
}

void selfapp_eq_cmd_switch_hw_test(bool enable)
{
	hm_dsp_command_t *cmd = &g_eq_dsp_cmd;
	media_player_t *media_handle = NULL;

	selfapp_log_inf("enable=%d", enable);
	g_hw_test_mode = enable;

	//pack cmd 0x21;
	memset(cmd, 0, sizeof(hm_dsp_command_t));
	cmd->update_flag = 0;
	cmd->cmd = 0x21;
	cmd->size = 1;
	if (enable) {
		cmd->buf[0] = 1;
	} else {
		cmd->buf[0] = 0;
	}

	media_handle = media_player_get_current_dumpable_player();
	if (NULL == media_handle) {
		selfapp_log_wrn("No player");
	} else {
		selfapp_log_inf("Update EQ to dsp.");
		selfapp_log_dump(cmd, sizeof(hm_dsp_command_t));
		media_player_update_effect_param(media_handle, cmd, sizeof(hm_dsp_command_t));
	}

	if (!enable) {
		os_sleep(10);
		selfapp_eq_cmd_update(EQCATEGORY_DEFAULT, selfapp_eq_get_default_data(), EQ_DATA_SIZE);
	}
}

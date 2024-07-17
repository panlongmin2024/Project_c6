#include "soft_version/soft_version.h"
#include <tts_manager.h>
#include <stdio.h>
#include <string.h>
#include <fw_version.h>
#include <os_common_api.h>
#include <wltmcu_manager_supply.h>

#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "soft_version"
#ifdef SYS_LOG_LEVEL
#undef SYS_LOG_LEVEL
#endif
#include <logging/sys_log.h>

static u32_t software_version_code;
static u8_t hardware_version_code;
static bool flag_version_alpha = false;

u8_t fw_version_get_hw_code(void)
{
	return hardware_version_code;
}

u32_t fw_version_get_sw_code(void)
{
	return software_version_code;
}

bool fw_version_get_alpha_flag(void)
{
	return flag_version_alpha;
}

int fw_version_format_check(void)
{
	int ret = 0;

	const char *fw_version_name = fw_version_get_name();
	
	const char name_format_str[] = "JBL_CHARGE6_vx.x.x.x_";
	int name_format_str_len = sizeof(name_format_str)-1;

	const char name_format_str_prefix[] = "JBL_CHARGE6_v";
	int name_format_str_prefix_len = sizeof(name_format_str_prefix)-1;

	SYS_LOG_INF("version_name:%s\n", fw_version_name);

	if (strlen(fw_version_name) < name_format_str_len)
	{
		ret = -1;
		__ASSERT(0, "fw version name format err,line:%d\n", __LINE__);
		goto __exit;
	}

	if (memcmp(fw_version_name, name_format_str_prefix, name_format_str_prefix_len) != 0)
	{
		ret = -1;
		__ASSERT(0, "fw version name format err,line:%d\n", __LINE__);
		goto __exit;
	}

	if (fw_version_name[name_format_str_prefix_len] < '0' || fw_version_name[name_format_str_prefix_len] > '9' ||
		fw_version_name[name_format_str_prefix_len+2] < '0' || fw_version_name[name_format_str_prefix_len+2] > '9' ||
		fw_version_name[name_format_str_prefix_len+4] < '0' || fw_version_name[name_format_str_prefix_len+4] > '9' ||
		fw_version_name[name_format_str_prefix_len+6] < '0' || fw_version_name[name_format_str_prefix_len+6] > '9')
	{
		ret = -1;
		__ASSERT(0, "fw version name format err,line:%d\n", __LINE__);
		goto __exit;
	}

	if (fw_version_name[name_format_str_prefix_len+1] != '.' || 
		fw_version_name[name_format_str_prefix_len+3] != '.' ||
		fw_version_name[name_format_str_prefix_len+5] != '.')
	{
		ret = -1;
		__ASSERT(0, "fw version name format err,line:%d\n", __LINE__);
		goto __exit;
	}

	if (fw_version_name[name_format_str_len-1] != name_format_str[name_format_str_len-1])
	{
		ret = -1;
		__ASSERT(0, "fw version name format err,line:%d\n", __LINE__);
		goto __exit;
	}

	software_version_code = ((fw_version_name[name_format_str_prefix_len] - '0') << 16) |
		   ((fw_version_name[name_format_str_prefix_len+2] - '0') << 8) |
		   (fw_version_name[name_format_str_prefix_len+4] - '0');

	hardware_version_code = fw_version_name[name_format_str_prefix_len+6] - '0';

	SYS_LOG_INF("software_version_code:0x%08X,hardware_version_code:0x%02X\n", software_version_code, hardware_version_code);

	if (!strncmp(&fw_version_name[name_format_str_len], "alpha_", 6))
	{
		flag_version_alpha = true;
	}

	SYS_LOG_INF("alpha version:%d\n", flag_version_alpha);

__exit:
	return ret;
}

int fw_version_play_by_tts(void)
{
	char file_name[] = "0.mp3";
	u32_t mode = UNBLOCK_UNINTERRUPTABLE;

	u8_t hw_ver =  fw_version_get_hw_code();
	u32_t sw_ver = fw_version_get_sw_code();
	u8_t pd_ver =  pd_get_pd_version();

/* 	if (tts_manager_is_enable_filter())
	{
		SYS_LOG_INF("tts filter,so don't play\n");
		return -1;
	} */

	os_sched_lock();

	SYS_LOG_INF("ok,ver:%x_%x_%x\n",sw_ver, hw_ver,pd_ver);

	file_name[0] = ((sw_ver >> 16) & 0xff) + '0';
	tts_manager_play(file_name, mode);

	file_name[0] = ((sw_ver >> 8) & 0xff) + '0';
	tts_manager_play(file_name, mode);

	file_name[0] = (sw_ver & 0xff) + '0';
	tts_manager_play(file_name, mode);

	file_name[0] = hw_ver + '0';
	tts_manager_play(file_name, mode);

	file_name[0] = ((pd_ver >> 4) & 0x0f) + '0';
	tts_manager_play(file_name, mode);
	
	file_name[0] = (pd_ver & 0x0f) + '0';
	tts_manager_play(file_name, mode);

	if (fw_version_get_alpha_flag() == true)
	{
		tts_manager_play("debug.mp3", mode);
	}

	os_sched_unlock();

	return 0;
}

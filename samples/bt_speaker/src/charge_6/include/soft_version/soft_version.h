#ifndef __SOFT_VERSION_H__
#define __SOFT_VERSION_H__

#include <kernel.h>
#include <fw_version.h>

int fw_version_format_check(void);
u8_t fw_version_get_hw_code(void);
u32_t fw_version_get_sw_code(void);
bool fw_version_get_alpha_flag(void);
int fw_version_play_by_tts(void);

#endif

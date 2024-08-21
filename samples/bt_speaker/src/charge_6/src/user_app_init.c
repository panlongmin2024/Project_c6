#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef CONFIG_C_PD_TPS65992
//#include "pd/tps65992/tps65992_head.h"
#endif

//#include "logic_mcu/MSPM0L/mspm0l_head.h"
//#include "logic_mcu/ls8a10049t/ls8a10049t.h"

#include "self_media_effect/self_media_effect.h"
#include "soft_version/soft_version.h"

#ifdef CONFIG_C_EXTERNAL_DSP_ATS3615
#include <external_dsp/ats3615/ats3615_head.h>
#endif


#include <audio_track.h>
#include "run_mode/run_mode.h"
#include "sys_manager.h"

#include <srv_manager.h>
#include <app_defines.h>
#include "power_supply.h"
#include "wltmcu_manager_supply.h"

#ifdef CONFIG_SYS_LOG
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "user_app_init"
#include <logging/sys_log.h>
#endif

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
extern void hm_ext_pa_deinit(void);
extern void hm_ext_pa_init(void);
extern void amp_tas5828m_clear_fault_timer_init(void);
extern void extern_dsp_3615_io_enable(int enable);
#endif

u16_t g_reboot_type = 0;
u8_t g_reboot_reason = 0;

#ifdef CONFIG_C_I2STX_KEEP_OPEN
void i2stx_first_open(void)
{
	struct audio_track_t *audio_track = audio_track_create(AUDIO_STREAM_MUSIC, 48, AUDIO_FORMAT_PCM_32_BIT, AUDIO_MODE_STEREO, NULL, NULL, NULL);
	
	if (audio_track != NULL)
	{
		SYS_LOG_INF("I2STX_KEEP_OPEN: poweron open i2stx ok\n");
		audio_track_destory(audio_track);
	}
	else 
	{
		SYS_LOG_ERR("I2STX_KEEP_OPEN: poweron open i2stx err\n");
	}

// #if 0
// 	i2stx_channel_test();
// #endif
}
#endif
void user_app_early_init(void)
{
    SYS_LOG_INF("\n");
	extern_dsp_3615_io_enable(0);
	fw_version_format_check();
	run_mode_init();
	pd_manager_v_sys_g1_g2(PD_V_BUS_G1_G2_INIT);
}

void user_app_later_init(void)
{
#ifdef CONFIG_C_I2STX_KEEP_OPEN
	i2stx_first_open();
#endif

#ifdef CONFIG_BT_CONTROLER_BQB
	extern bool main_system_is_enter_bqb(void);
	if(main_system_is_enter_bqb())
		return;
#endif	

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	soc_dvfs_set_level(SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE, "pa init");
	self_music_effect_ctrl_init();	
	external_dsp_ats3615_load(0);
	external_dsp_ats3615_timer_start();	
	hm_ext_pa_init();
	amp_tas5828m_clear_fault_timer_init();
	soc_dvfs_unset_level(SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE, "pa init");
#endif
}

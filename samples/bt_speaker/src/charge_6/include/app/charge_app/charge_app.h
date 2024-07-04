#ifndef _CHARGE_APP_MODE_H_
#define _CHARGE_APP_MODE_H_

#include <mem_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <msg_manager.h>

#include "app_defines.h"
#include "sys_manager.h"
#include "app_ui.h"

#include "app_switch.h"
#include "app_defines.h"
#include <app_launch.h>
#ifdef CONFIG_C_EXTERNAL_DSP_ATS3615
#include <external_dsp/ats3615/ats3615_head.h>
#endif
#ifdef CONFIG_C_AMP_TAS5828M
#include <driver/amp/tas5828m/tas5828m_head.h>
#endif

struct charge_app_t {
	int charge_state;
	int tts_start_time;
};

struct charge_app_t *charge_app_get_app(void);

void charge_app_input_event_proc(struct app_msg *msg);

void charge_app_view_init(void);
void charge_app_view_deinit(void);
extern int system_app_launch_for_charge_mode(u8_t mode);
extern int charge_app_enter_cmd(void);
extern int charge_app_exit_cmd(void);
extern int charge_app_real_exit_deal(void);
extern void charge_app_tts_event_proc(struct app_msg *msg);
#endif
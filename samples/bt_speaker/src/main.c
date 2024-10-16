/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <mem_manager.h>
#include <msg_manager.h>
#include <sys_manager.h>
#include <app_manager.h>
#include "app_defines.h"
#include "main/main_app.h"
#include "charger/charger.h"
#include <property_manager.h>

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#include <wltmcu_manager_supply.h>
extern void user_app_early_init(void);
extern void user_app_later_init(void);
#endif

extern int system_app_launch_init(void);
extern void system_app_init(void);
extern void main_app(void);

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
static bool enter_att_flag = false;
extern int get_autotest_connect_status(void);
extern void check_adfu_gpio_key(void);
extern void open_or_close_debug(void);
extern int property_get(const char *key, char *value, int value_len);
bool main_get_enter_att_state(void)
{
	return (enter_att_flag == true);
}

static void main_is_enter_att(void)
{
	printk("\n %s , enter ---\n",__func__);
    if(get_autotest_connect_status() == 0)
    {
        enter_att_flag = true;	
        return;
    }	

	check_adfu_gpio_key();
	open_or_close_debug();
	printk("\n %s , exit ---",__func__);
}
#endif
#ifdef CONFIG_WLT_ATS_ENABLE
extern void ats_wlt_enter(void);
extern bool ats_wlt_get_enter_state(void);
extern int ats_wlt_check_adfu(void);
#endif
static void main_pre_init(void)
{
	struct app_msg msg;
	int terminaltion = false;
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#ifdef CONFIG_ACTIONS_IMG_LOAD
	char buf[2]={0};
	int ret = property_get(CFG_USER_IN_OUT_NOSIGNAL_TEST_MODE,buf, 1);
	printf("------> ret %d nosignal_flag %d\n",ret,buf[0]);
	if(buf[0] == 6){
		/* clear enter nosignale flag! */
		u8_t buffer[1+1] = {0};
		int result = property_set(CFG_USER_IN_OUT_NOSIGNAL_TEST_MODE, buffer, 1);
		if(result!=0){
			result = property_set(CFG_USER_IN_OUT_NOSIGNAL_TEST_MODE, buffer, 1);
		}
		property_flush(CFG_USER_IN_OUT_NOSIGNAL_TEST_MODE);
		mcu_ui_power_hold_fn();
		extern int run_test_image(void);
		run_test_image();
	}
#endif
	enter_att_flag = false;
	main_is_enter_att();
#endif

#ifdef CONFIG_WLT_ATS_ENABLE
	/* check if need enter wlt factory test !! */
	ats_wlt_enter();
	ats_wlt_check_adfu();
#endif
	user_app_early_init();
	system_pre_init();
	app_manager_active_app(CONFIG_SYS_APP_NAME);

	while(!system_is_ready()){
		os_sleep(10);
	}
	os_sleep(10);//有概率SYS还没来得及发enter charge mode？
	/*clear all remain message */
	while ((!terminaltion) && receive_msg(&msg, OS_NO_WAIT)) {
		SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg.type, msg.cmd, msg.value);

		switch(msg.type){
#ifdef CONFIG_CHARGER_APP
			case MSG_CHARGER_MODE:
				if(msg.cmd){
					charger_mode_check();
				}
				break;
#endif
			case MSG_POWER_KEY:
				{
					terminaltion = true;
					SYS_LOG_INF("%d MSG_POWER_KEY\n",__LINE__);
					break;
				}
			default:
				break;
		}

		if (msg.callback) {
			msg.callback(&msg, 0, NULL);
		}
	}

	memset(&msg, 0, sizeof(msg));
	msg.type = MSG_CHARGER_MODE;
	msg.cmd = 0;
	send_async_msg(CONFIG_SYS_APP_NAME, &msg);
    pd_set_app_mode_state(BTMODE_APP_MODE);
	pd_srv_event_notify(PD_EVENT_SOURCE_REFREST,0);	
	msg.type = MSG_INIT_APP;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);

}

void main(void)
{
	main_pre_init();
	
	main_app();
}

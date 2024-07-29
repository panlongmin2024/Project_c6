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

bool main_get_enter_att_state(void)
{
	return (enter_att_flag == true);
}

static void main_is_enter_att(void)
{
	/*int property_get = -1;
	printk("\n %s , enter ---",__func__);

	property_get = property_get_int(CFG_AUTO_ENTER_ATS_MODULE, 0);
	printk("------> get_dat =  %d  att_status %d\n",property_get,get_autotest_connect_status());
    if(get_autotest_connect_status() == 0)
    {
        enter_att_flag = true;
		int ats_module_test_mode_write(uint8_t *buf, int size);
		char buffer[2] = "6";
		ats_module_test_mode_write(buffer,sizeof(buffer)-1);		
        return;
    }*/	

	check_adfu_gpio_key();
	printk("\n %s , exit ---",__func__);
}
#endif
//test2
static void main_pre_init(void)
{
	struct app_msg msg;
	int terminaltion = false;
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#ifdef CONFIG_ACTIONS_IMG_LOAD
	int property_nosignal_test_get = property_get_int(CFG_ATS_ENTER_NOSIGNAL_TEST_MODE, 0);
	printf("------> property_nosignal_test_get %d\n",property_nosignal_test_get);
	if(property_nosignal_test_get == 6){
		/* clear enter nosignale flag! */
		u8_t buffer[1+1] = "0";
		int result = property_set_factory(CFG_ATS_ENTER_NOSIGNAL_TEST_MODE, buffer, 1);
		if(result!=0){
			result = property_set_factory(CFG_ATS_ENTER_NOSIGNAL_TEST_MODE, buffer, 1);
		}

		mcu_ui_power_hold_fn();
		extern int run_test_image(void);
		run_test_image();
	}
#endif
	enter_att_flag = false;
	main_is_enter_att();
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
	msg.type = MSG_INIT_APP;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);

}

void main(void)
{
	main_pre_init();
	
	main_app();
}

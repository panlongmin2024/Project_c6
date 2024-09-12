#include "app/charge_app/charge_app.h"
#include "ui_manager.h"
#include <input_manager.h>
#ifdef CONFIG_LED_MANAGER
#include <led_manager.h>
#endif
#include <wltmcu_manager_supply.h>

static const ui_key_map_t charge_app_keymap[] = {
    {KEY_POWER, KEY_TYPE_ALL, 1, MSG_APP_INPUT_MESSAGE_CMD_START},
    {KEY_PAUSE_AND_RESUME, KEY_TYPE_ALL, 1, MSG_APP_INPUT_MESSAGE_CMD_START},
    {KEY_BT, KEY_TYPE_ALL, 1, MSG_APP_INPUT_MESSAGE_CMD_START},
    {KEY_VOLUMEDOWN, KEY_TYPE_ALL, 1, MSG_APP_INPUT_MESSAGE_CMD_START},
    {KEY_VOLUMEUP, KEY_TYPE_ALL, 1, MSG_APP_INPUT_MESSAGE_CMD_START},
    {KEY_BROADCAST, KEY_TYPE_ALL, 1, MSG_APP_INPUT_MESSAGE_CMD_START},
	// Combination buttons
    {KEY_MEDIA_EFFECT_BYPASS, KEY_TYPE_ALL, 1, MSG_APP_INPUT_MESSAGE_CMD_START},
    {KEY_FACTORY, KEY_TYPE_ALL, 1, MSG_APP_INPUT_MESSAGE_CMD_START},
    {KEY_SOFT_VERSION, KEY_TYPE_ALL, 1, MSG_APP_INPUT_MESSAGE_CMD_START},
    {KEY_ATS, KEY_TYPE_ALL, 1, MSG_APP_INPUT_MESSAGE_CMD_START},
    {KEY_RESERVED, 0, 0, 0}
};

static int _charge_app_view_proc(u8_t view_id, u8_t msg_id, u32_t msg_data)
{
	SYS_LOG_INF("view %d msg %d\n", view_id, msg_id);

	switch (msg_id) {
	case MSG_VIEW_CREATE:
	{
		SYS_LOG_INF("CREATE\n");
		break;
	}
	case MSG_VIEW_DELETE:
	{
		break;
	}
	case MSG_VIEW_PAINT:
	{
		break;
	}
	}
	return 0;
}

int charge_app_get_status(void)
{
	return 1;
}

void charge_app_view_init(void)
{
	ui_view_info_t  view_info;

	memset(&view_info, 0, sizeof(ui_view_info_t));

	view_info.view_proc = _charge_app_view_proc;
	view_info.view_key_map = charge_app_keymap;
	view_info.view_get_state = charge_app_get_status;
	view_info.order = 1;
	view_info.app_id = APP_ID_CHARGE_APP_NAME;

	ui_view_create(CHARGE_APP_VIEW, &view_info);

	//sys_event_notify(SYS_EVENT_ENTER_charge_app);
#ifdef CONFIG_LED_MANAGER
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	//led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);
#endif
#endif	
	SYS_LOG_INF(" ok\n");
}

void charge_app_view_deinit(void)
{
	ui_view_delete(CHARGE_APP_VIEW);
#ifdef CONFIG_LED_MANAGER
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);
	pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_ON_10S);
	pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(0)|AC_LED_STATE(0)|BAT_LED_STATE(0));		
#endif
#endif	
	SYS_LOG_INF("ok\n");
}

#include "app/fg_app_name/fg_app_name.h"
#include "ui_manager.h"
#include <input_manager.h>
#include <ui_manager.h>

static const ui_key_map_t fg_app_name_keymap[] = {
    {KEY_POWER, KEY_TYPE_ALL, 1, MSG_DEMO_NO_ACTION},
    {KEY_PAUSE_AND_RESUME, KEY_TYPE_ALL, 1, MSG_DEMO_NO_ACTION},
    {KEY_BT, KEY_TYPE_ALL, 1, MSG_DEMO_NO_ACTION},
    {KEY_VOLUMEDOWN, KEY_TYPE_ALL, 1, MSG_DEMO_NO_ACTION},
    {KEY_VOLUMEUP, KEY_TYPE_ALL, 1, MSG_DEMO_NO_ACTION},
    {KEY_BROADCAST, KEY_TYPE_ALL, 1, MSG_DEMO_NO_ACTION},
	// Combination buttons
    {KEY_MEDIA_EFFECT_BYPASS, KEY_TYPE_ALL, 1, MSG_DEMO_NO_ACTION},
    {KEY_FACTORY, KEY_TYPE_ALL, 1, MSG_DEMO_NO_ACTION},
    {KEY_SOFT_VERSION, KEY_TYPE_ALL, 1, MSG_DEMO_NO_ACTION},
    {KEY_ATS, KEY_TYPE_ALL, 1, MSG_DEMO_NO_ACTION},
    {KEY_RESERVED, 0, 0, 0}
};

static int _fg_app_name_view_proc(u8_t view_id, u8_t msg_id, u32_t msg_data)
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

int fg_app_name_get_status(void)
{
	return 1;
}

void fg_app_name_view_init(void)
{
	ui_view_info_t  view_info;

	memset(&view_info, 0, sizeof(ui_view_info_t));

	view_info.view_proc = _fg_app_name_view_proc;
	view_info.view_key_map = fg_app_name_keymap;
	view_info.view_get_state = fg_app_name_get_status;
	view_info.order = 1;
	view_info.app_id = APP_ID_FG_APP_NAME;

	ui_view_create(FG_APP_NAME_VIEW, &view_info);

	sys_event_notify(SYS_EVENT_ENTER_FG_APP_NAME);

	SYS_LOG_INF(" ok\n");
}

void fg_app_name_view_deinit(void)
{
	ui_view_delete(FG_APP_NAME_VIEW);

	SYS_LOG_INF("ok\n");
}

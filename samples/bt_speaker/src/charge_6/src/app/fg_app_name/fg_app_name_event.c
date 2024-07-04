#include "app/fg_app_name/fg_app_name.h"
#include <ui_manager.h>
#include <esd_manager.h>
#include "tts_manager.h"


void fg_app_name_input_event_proc(struct app_msg *msg)
{
	struct fg_app_name_app_t *fg_app_name_app = fg_app_name_get_app();

	if (!fg_app_name_app)
		return;

	switch (msg->cmd) {
	default:
		break;
	}
}


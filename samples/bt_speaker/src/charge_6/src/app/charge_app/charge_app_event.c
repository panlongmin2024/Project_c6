#include "app/charge_app/charge_app.h"
#include <ui_manager.h>
#include <esd_manager.h>
#include "tts_manager.h"

void charge_app_input_event_proc(struct app_msg *msg)
{
	struct charge_app_t *charge_app_app = charge_app_get_app();

	if (!charge_app_app)
		return;

	switch (msg->cmd) {
	default:
		break;
	}
}

void charge_app_tts_event_proc(struct app_msg *msg)
{
	switch (msg->value) {
	case TTS_EVENT_START_PLAY:
		printk("charge_app_tts_event_proc TTS_EVENT_START_PLAY\n");
		break;
	case TTS_EVENT_STOP_PLAY:
		printk("charge_app_tts_event_proc TTS_EVENT_STOP_PLAY \n");

		break;
	}
}
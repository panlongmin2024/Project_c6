#include "app/fg_app_name/fg_app_name.h"

#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif

#include <app_defines.h>
#include "tts_manager.h"
#include <thread_timer.h>
#include <global_mem.h>

#include <sys_wakelock.h>

#ifdef CONFIG_SYS_LOG
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "fg_app_name"
#include <logging/sys_log.h>
#endif

static struct fg_app_name_app_t *p_fg_app_name_app;

static int _fg_app_name_init(void)
{
	int ret = 0;

	p_fg_app_name_app = app_mem_malloc(sizeof(struct fg_app_name_app_t));
	if (!p_fg_app_name_app) {
		SYS_LOG_ERR("malloc fail!\n");
		return -ENOMEM;
	}

	sys_wake_lock(WAKELOCK_DEMO_MODE);

	fg_app_name_view_init();

	SYS_LOG_INF("init finished\n");

	return ret;
}

static void _fg_app_name_exit(void)
{
	if (!p_fg_app_name_app)
		goto exit;

	fg_app_name_view_deinit();

	app_mem_free(p_fg_app_name_app);
	p_fg_app_name_app = NULL;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif

 exit:
	app_manager_thread_exit(APP_ID_FG_APP_NAME);

	sys_wake_unlock(WAKELOCK_DEMO_MODE);

	SYS_LOG_INF("exit finished\n");
}

struct fg_app_name_app_t *fg_app_name_get_app(void)
{
	return p_fg_app_name_app;
}

static void fg_app_name_main_loop(void *parama1, void *parama2, void *parama3)
{
	struct app_msg msg = { 0 };
	int result = 0;
	bool terminaltion = false;

	if (_fg_app_name_init()) {
		SYS_LOG_ERR("init failed");
		_fg_app_name_exit();
		return;
	}

	while (!terminaltion) {
		if (receive_msg(&msg, thread_timer_next_timeout())) {
			SYS_LOG_INF("type %d, cmd %d, value %d\n", msg.type,
				    msg.cmd, msg.value);
			switch (msg.type) {
			case MSG_EXIT_APP:
				_fg_app_name_exit();
				terminaltion = true;
				break;
			case MSG_INPUT_EVENT:
				fg_app_name_input_event_proc(&msg);
				break;
			case MSG_TTS_EVENT:
				break;
			default:
				SYS_LOG_ERR("error: 0x%x!\n", msg.type);
				continue;
			}
			if (msg.callback != NULL)
				msg.callback(&msg, result, NULL);
		}
		thread_timer_handle_expired();
	}
}

APP_DEFINE(fg_app_name, share_stack_area, sizeof(share_stack_area),
	   CONFIG_APP_PRIORITY, FOREGROUND_APP, NULL, NULL, NULL,
	   fg_app_name_main_loop, NULL);

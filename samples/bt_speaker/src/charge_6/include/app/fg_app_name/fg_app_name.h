#include <mem_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <msg_manager.h>

#include "app_defines.h"
#include "sys_manager.h"
#include "app_ui.h"

enum {
	MSG_DEMO_NO_ACTION = 1,
};

struct fg_app_name_app_t {
	int flag;
};

struct fg_app_name_app_t *fg_app_name_get_app(void);

void fg_app_name_input_event_proc(struct app_msg *msg);

void fg_app_name_view_init(void);
void fg_app_name_view_deinit(void);

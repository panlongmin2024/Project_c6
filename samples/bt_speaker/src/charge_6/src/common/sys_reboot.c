#include "common/sys_reboot.h"
#include <app_manager.h>
#include <msg_manager.h>
#include <sys_manager.h>

void sys_reboot_by_user(uint8_t reason)
{
	struct app_msg msg = { 0 };
	msg.type = MSG_REBOOT;
	msg.cmd = reason;
	send_async_msg(CONFIG_SYS_APP_NAME, &msg);
}

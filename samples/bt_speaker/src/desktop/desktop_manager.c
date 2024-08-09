
#define SYS_LOG_DOMAIN "desktop"
#include <logging/sys_log.h>
#include <mem_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <msg_manager.h>
#include <input_manager.h>
#include <thread_timer.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bt_manager.h>
#include "app_defines.h"
#include "app_ui.h"
#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif
#include <app_launch.h>
#include "desktop_manager.h"
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

extern desktop_plugin_t __desktop_plugin_entry_table[];
extern desktop_plugin_t __desktop_plugin_entry_end[];


typedef struct{
	int def_plugin_id;
	int cur_plugin_id;
	int last_plugin_id;
	uint8_t plugin_num;
	uint8_t start:1;
	uint8_t locked : 1;
	uint32_t plugin_bits;
	desktop_plugin_t* cur_plugin;
}desktop_manager_ctx_t;

static desktop_manager_ctx_t desktop_manager;


OS_MUTEX_DEFINE(desktop_manager_mutex);


static desktop_plugin_t *get_plugin_by_id(int plugin_id)
{
	desktop_plugin_t *plugin_entry;

	if (plugin_id != DESKTOP_PLUGIN_ID_NONE) {
		for (plugin_entry = __desktop_plugin_entry_table;
		     plugin_entry < __desktop_plugin_entry_end;
		     plugin_entry++) {
			if (plugin_id == plugin_entry->plugin_id) {
				return plugin_entry;
			}
		}
	}
	return NULL;
}

static inline desktop_manager_ctx_t *_desktop_manager_get_ctx(void)
{
	return &desktop_manager;
}


int desktop_manager_init(int default_plugin_id)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	desktop_plugin_t* plugin = NULL;

	memset(ctx , 0, sizeof(desktop_manager_ctx_t));
	ctx->cur_plugin_id = DESKTOP_PLUGIN_ID_NONE;
	ctx->last_plugin_id = DESKTOP_PLUGIN_ID_NONE;
	ctx->plugin_num = 0;
	ctx->plugin_bits = 0;

	plugin = get_plugin_by_id(default_plugin_id);
	if (!plugin){
		SYS_LOG_INF("plugin %d no Exist.", default_plugin_id);
		return -1;
	}
	ctx->def_plugin_id = default_plugin_id;
	ctx->plugin_bits |=  (1<<default_plugin_id);
	ctx->plugin_num++;
	ctx->start = 1;

	return 0;
}

int desktop_manager_add(int plugin_id)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	desktop_plugin_t* plugin = NULL;
	int ret = 0;
	os_mutex_lock(&desktop_manager_mutex, OS_FOREVER);
	if(!ctx->start){
		os_mutex_unlock(&desktop_manager_mutex);
		return -EINVAL;
	}

	plugin = get_plugin_by_id(plugin_id);
	if (!plugin){
		SYS_LOG_INF("plugin %d no Exist.\n", plugin_id);
		ret = -1;
		goto Exit;
	}

	if (ctx->plugin_bits & (1<<plugin_id)){
		SYS_LOG_INF("plugin %d Already Exist.\n", plugin_id);
		goto Exit;
	}

	ctx->plugin_bits |= (1<<plugin_id);
	ctx->plugin_num ++;
	SYS_LOG_INF("plugin %d add sucess.\n", plugin_id);

Exit:
	os_mutex_unlock(&desktop_manager_mutex);
	return ret;

}

int desktop_manager_del(int plugin_id)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	desktop_plugin_t* plugin = NULL;

	os_mutex_lock(&desktop_manager_mutex, OS_FOREVER);
	if(!ctx->start){
		os_mutex_unlock(&desktop_manager_mutex);
		return -EINVAL;
	}

	plugin = get_plugin_by_id(plugin_id);
	if (!plugin){
		SYS_LOG_INF("plugin %d no Exist.", plugin_id);
		goto Exit;
	}

	if ((ctx->plugin_bits & (1<<plugin_id)) == 0){
		goto Exit;
	}

	ctx->plugin_bits &= (~(1<<plugin_id));
	ctx->plugin_num --;

	SYS_LOG_INF("plugin %d delete sucess.\n", plugin_id);

Exit:
	os_mutex_unlock(&desktop_manager_mutex);
	return 0;

}

int desktop_manager_switch(int plugin_id, uint32_t switch_mode)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	int ret = 0;
	os_mutex_lock(&desktop_manager_mutex, OS_FOREVER);
	if(!ctx->start){
		ret = -1;
		goto Exit;
	}

	if (switch_mode == DESKTOP_SWITCH_LAST){
		plugin_id = ctx->last_plugin_id;
	}else if(switch_mode == DESKTOP_SWITCH_NEXT){
		if(ctx->plugin_num > 1){
			plugin_id = (ctx->cur_plugin_id + 1) % 32;
			while(!((ctx->plugin_bits)&(1<<plugin_id))){
				plugin_id = (plugin_id + 1)% 32;
			}
		}
	}

	if(plugin_id == ctx->cur_plugin_id){
		SYS_LOG_INF("plugin %d again.", ctx->cur_plugin_id);
		goto Exit;
	}

	if ((ctx->plugin_bits & (1<<plugin_id)) == 0){
		SYS_LOG_ERR("plugin %d no Exist.", plugin_id);
		ret = -1;
		goto Exit;
	}

	ctx->last_plugin_id = ctx->cur_plugin_id;

	if (ctx->cur_plugin){
		ctx->cur_plugin->exit();
		SYS_LOG_INF("plugin %d EXIT.", ctx->cur_plugin_id);
	}

	ctx->cur_plugin_id = plugin_id;

	ctx->cur_plugin =  get_plugin_by_id(plugin_id);
	if (!ctx->cur_plugin){
		SYS_LOG_ERR("plugin %d no Exist.", plugin_id);
		ret = -1;
		goto Exit;
	}

	SYS_LOG_INF("plugin %d Enter.", plugin_id);

	ctx->cur_plugin->enter(ctx->cur_plugin->p1, ctx->cur_plugin->p2, ctx->cur_plugin->p3);

Exit:
	os_mutex_unlock(&desktop_manager_mutex);

	return ret;
}

int desktop_manager_dump_state(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();

	printk("Desktop plugins:\n");
	printk("  plugin_bits: 0x%x\n", ctx->plugin_bits);
	printk("  plugin_num: %d\n", ctx->plugin_num);
	printk("  def_plugin_id: %d\n", ctx->def_plugin_id);
	printk("  cur_plugin_id: %d\n", ctx->cur_plugin_id);
	printk("  last_plugin_id: %d\n", ctx->last_plugin_id);
	printk("  start: %d\n", ctx->start);

	printk("\napp status:\n");
	printk("  auracast: %d\n", system_app_get_auracast_mode());
#ifdef CONFIG_BT_SELF_APP
	printk("  ch: %d\n", selfapp_get_channel());
#endif

	if(!ctx->start){
		return 0;
	}

	if (ctx->cur_plugin_id == 0xff){
		return 0;
	}

	if(ctx->cur_plugin && ctx->cur_plugin->dump_state)
		return ctx->cur_plugin->dump_state();
	else
		return 0;

}

int desktop_manager_get_plugin_id(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	if(!ctx->start){
		return DESKTOP_PLUGIN_ID_NONE;
	}

	return ctx->cur_plugin_id;
}

int desktop_manager_get_last_plugin_id(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	if(!ctx->start){
		return DESKTOP_PLUGIN_ID_NONE;
	}

	return ctx->last_plugin_id;
}

void desktop_manager_enter(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	if(!ctx->start){
		return;
	}

	struct app_msg  msg = {0};

	msg.type = MSG_SWITCH_APP;
	msg.cmd = DESKTOP_SWITCH_CURR;
	msg.value = ctx->def_plugin_id;

	if(send_async_msg(CONFIG_FRONT_APP_NAME, &msg) == false){
		SYS_LOG_INF("send msg failed \n");
		return;
	}

	SYS_LOG_INF("init ok\n");
}

void desktop_manager_exit(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	if(!ctx->start){
		return;
	}

	if (ctx->cur_plugin){
		ctx->cur_plugin->exit();
		ctx->cur_plugin = NULL;
	}
	ctx->cur_plugin_id = DESKTOP_PLUGIN_ID_NONE;
	ctx->start = 0;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif

	SYS_LOG_INF("exit ok\n");
}

int desktop_manager_app_switch(struct app_msg *msg)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	int new_plugin_id;
	int old_plugin_id;

	SYS_LOG_INF("cmd: %d value %d\n", msg->cmd, msg->value);
	if(!ctx->start){
		return -EINVAL;
	}

	if (ctx->locked){
		SYS_LOG_WRN("app switch locked %d->%d\n", desktop_manager_get_last_plugin_id(), msg->value);
		#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
		//factory in ota after exit ota need enter to charge app
		if(msg->value == DESKTOP_PLUGIN_ID_CHARGER){
			ctx->last_plugin_id = DESKTOP_PLUGIN_ID_CHARGER;
			desktop_manager_add(msg->value);
		}
		#ifdef CONFIG_BT_SELF_APP
		extern void otadfu_SetForce_exit_ota(void);
		otadfu_SetForce_exit_ota();
		#endif
		#endif
		return -EINVAL;
	}

	switch (msg->cmd) {
	case DESKTOP_SWITCH_CURR:
		new_plugin_id = msg->value;
		int app = desktop_manager_get_plugin_id();
		if(desktop_manager_add(new_plugin_id)){
			SYS_LOG_INF("add plugin failed %d\n", new_plugin_id);
			SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,-1);
			break;
		}
#ifdef CONFIG_AURACAST
		if (app == DESKTOP_PLUGIN_ID_BMR)
		{
			if(!desktop_manager_switch(new_plugin_id,DESKTOP_SWITCH_CURR)){
				desktop_manager_del(app);
				SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,0);
			}else{
				SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,-2);
			}
			break;
		}
#endif
		if(!desktop_manager_switch(new_plugin_id,DESKTOP_SWITCH_CURR)){
			SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,0);
		}else{
			SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,-3);
		}
		break;

	case DESKTOP_SWITCH_LAST:
		old_plugin_id = msg->value;
		app = desktop_manager_get_plugin_id();
#if 0
		if ((app == DESKTOP_PLUGIN_ID_BR_MUSIC) && old_plugin_id == DESKTOP_PLUGIN_ID_BR_MUSIC
			&& system_app_get_auracast_mode())
		{
			if(desktop_manager_add(DESKTOP_PLUGIN_ID_BMR)){
				SYS_LOG_INF("add plugin failed %d\n", DESKTOP_PLUGIN_ID_BMR);
				SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,-1);
				break;
			}

			if(!desktop_manager_switch(DESKTOP_PLUGIN_ID_BMR,DESKTOP_SWITCH_CURR)){
				SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,0);
			}else{
				SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,-2);
			}
			SYS_LOG_INF("switched to bmr");
			break;
		}
#endif

		if (old_plugin_id == app) {
			if(!desktop_manager_switch(0, DESKTOP_SWITCH_LAST)){
				SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,0);
			}else{
				SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,-3);
			}
		}
		//do not remove default APP from switch list.
		if (ctx->def_plugin_id != old_plugin_id) {
			desktop_manager_del(old_plugin_id);
		}
		break;

	case DESKTOP_SWITCH_NEXT:
		if(!desktop_manager_switch(0,DESKTOP_SWITCH_NEXT)){
			SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,0);
		}else{
			SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,-1);
		}
		break;

	case DESKTOP_SWITCH_ADD_ONLY:
		new_plugin_id = msg->value;
		if(desktop_manager_add(new_plugin_id)){
			SYS_LOG_INF("add plugin failed %d\n", new_plugin_id);
			SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,-1);
		}
		break;

	case DESKTOP_SWITCH_CURR_AND_DEL:
		new_plugin_id = (msg->value)&0xff;
		old_plugin_id = (msg->value)>>8;
		if(desktop_manager_add(new_plugin_id)){
			SYS_LOG_INF("add plugin failed %d\n", new_plugin_id);
			SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,-1);
			break;
		}
		if(!desktop_manager_switch(new_plugin_id,DESKTOP_SWITCH_CURR)){
			SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,0);
			if (ctx->def_plugin_id != old_plugin_id){
				if(desktop_manager_del(old_plugin_id)){
					SYS_LOG_INF("del plugin failed %d\n", old_plugin_id);
				}
			}
		}else{
			SYS_EVENT_INF(EVENT_DESKTOP_APP_SWITCH, msg->cmd,msg->value,-2);
		}
		break;

	default:
		break;
	}

	return 0;
}


int desktop_manager_proc_app_msg(struct app_msg *msg)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();

	if(ctx->start){
		if (ctx->cur_plugin){
			return ctx->cur_plugin->proc_msg(msg);
		}
	}

	return 0;
}

int desktop_manager_lock(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();

	os_mutex_lock(&desktop_manager_mutex, OS_FOREVER);
	if(!ctx->start){
		os_mutex_unlock(&desktop_manager_mutex);
		return -EINVAL;
	}

	ctx->locked++;

	os_mutex_unlock(&desktop_manager_mutex);
	return 0;
}

int desktop_manager_unlock(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();

	os_mutex_lock(&desktop_manager_mutex, OS_FOREVER);
	if(!ctx->start){
		os_mutex_unlock(&desktop_manager_mutex);
		return -EINVAL;
	}

	if(ctx->locked > 0) {
		ctx->locked--;
	}

	os_mutex_unlock(&desktop_manager_mutex);
	return 0;

}



#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <shell/shell.h>
#include <soc.h>
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
#include <soc_dvfs.h>
#endif

#include <msg_manager.h>
#include <app_ui.h>
#include <sys_wakelock.h>

#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "user_shell"
#include <logging/sys_log.h>
#include <ats_cmd/ats.h>

int dolphin_set_vol(int argc, char *argv[]);
int dolphin_read_frames_start(int argc, char *argv[]);
int dolphin_read_frames_stop(int argc,char *argv[]);

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
static int shell_dvfs_info(int argc, char *argv[])
{
	soc_dvfs_dump_tbl();

	return 0;
}
#endif

static int shell_demo_mode_switch(int argc, char *argv[])
{
	struct app_msg msg = { 0 };
    msg.type = MSG_INPUT_EVENT;
    msg.cmd = MSG_DEMO_SWITCH;
    send_async_msg("main", &msg);

	return 0;
}

static int shell_factory_reset(int argc, char *argv[])
{
	struct app_msg msg = { 0 };
    msg.type = MSG_INPUT_EVENT;
    msg.cmd = MSG_FACTORY_RESET;
    send_async_msg("main", &msg);

	return 0;
}

static int shell_enter_adfu(int argc, char *argv[])
{
	sys_pm_reboot(REBOOT_TYPE_GOTO_ADFU);
	return 0;
}


static int shell_extern_dsp_switch(int argc, char *argv[])
{
	struct app_msg msg = { 0 };
    msg.type = MSG_INPUT_EVENT;
    msg.cmd = MSG_DSP_EFFECT_SWITCH;
    send_async_msg("main", &msg);
	return 0;
}

static struct k_delayed_work wakelocks_dump_delayed_work;

static void wakelocks_dump_delayed_work_cb(struct k_work *work)
{
	ARG_UNUSED(work);

	SYS_LOG_INF("sys_wakelocks_get_free_time:%u\n", sys_wakelocks_get_free_time());

	k_delayed_work_submit(&wakelocks_dump_delayed_work, 1000);
}

static int shell_wakelocks_dump(int argc, char *argv[])
{
	static uint8_t wakelocks_dump_delayed_work_start_flag;

	if (argc > 2)
	{
		SYS_LOG_ERR("argc num err\n");
		return -1;
	}

	if (argc == 1)
	{
		SYS_LOG_INF("sys_wakelocks_get_free_time:%u\n", sys_wakelocks_get_free_time());
	}
	else if (argc == 2)
	{
		if (!strcmp(argv[1], "1"))
		{
			if (wakelocks_dump_delayed_work_start_flag == 0)
			{
				wakelocks_dump_delayed_work_start_flag = 1;

				SYS_LOG_INF("loop start\n");
				k_delayed_work_init(&wakelocks_dump_delayed_work, wakelocks_dump_delayed_work_cb);
				k_delayed_work_submit(&wakelocks_dump_delayed_work, 0);
			}
		}
		else if (!strcmp(argv[1], "0"))
		{
			if (wakelocks_dump_delayed_work_start_flag == 1)
			{
				wakelocks_dump_delayed_work_start_flag = 0;

				SYS_LOG_INF("loop end\n");
				k_delayed_work_cancel(&wakelocks_dump_delayed_work);
			}
		}
	}
	
	return 0;
}
extern int btdrv_set_bqb_mode(void);
static int shell_enter_bqb(int argc, char *argv[])
{
	btdrv_set_bqb_mode();
	return 0;
}
static int shell_uart_test(int argc, char *argv[])
{
	 ats_init();
     ats_usb_cdc_acm_init();
	return 0;
}

extern void hm_ext_pa_deinit(void);
extern void hm_ext_pa_init(void);
static int shell_reset_pa_test(int argc, char *argv[])
{
	 printk("pa restart\n");
	 hm_ext_pa_deinit();
     hm_ext_pa_init();
	return 0;
}
static int shell_user_set_mac(int argc, char *argv[])
{
	for(int i=0;i<6;i++){
		printk("------> %s argc %d len %d %s\n",__func__,argc,sizeof(argv[i]),argv[i]);
	}

	if(sizeof(argv[1])==12){
		
	}
	
	return 0;
}
static int shell_user_set_name(int argc, char *argv[])
{
	for(int i=0;i<6;i++){
		printk("------> %s argc %d len %d %s\n",__func__,argc,sizeof(argv[i]),argv[i]);
	}

	return 0;
}
static int shell_user_set_mac_name(int argc, char *argv[])
{
	printk("------> %s argc %d\n",__func__,argc);

	return 0;
}

static const struct shell_cmd commands[] = {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
    { "dvfs_info", shell_dvfs_info, "dump DVFS info" },
#endif
	{ "demo_mode_switch", shell_demo_mode_switch, "demo mode switch" },
	{ "factory_reset", shell_factory_reset, "factory reset"},
	{ "enter_adfu", shell_enter_adfu, "enter adfu" },
#if 0	
    { "dolphin_set_vol", dolphin_set_vol, "dolphin set vol" },
    { "dolphin_read_frames_start", dolphin_read_frames_start, "dolphin read frames" },
    { "dolphin_read_frames_stop", dolphin_read_frames_stop, "dolphin read frames stop" },
#endif	
	{ "extern_dsp_effect_switch", shell_extern_dsp_switch, "shell_extern_dsp_switch" },
	{ "wakelocks_dump", shell_wakelocks_dump, "wakelocks dump"},
	{ "enter_bqb", shell_enter_bqb, "enter bqb"},
	{ "TL_ATS_IN", shell_uart_test, "enter uart test"},
	{ "reset_pa", shell_reset_pa_test, "test reset pa"},
	{ "set_mac", shell_user_set_mac, "user set mac"},
	{ "set_name", shell_user_set_name, "user set name"},
	{ "set_mac_name", shell_user_set_mac_name, "user set mac name"},
	{ NULL, NULL, NULL }
};

SHELL_REGISTER("user", commands);

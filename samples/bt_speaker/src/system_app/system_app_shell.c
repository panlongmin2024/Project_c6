/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief system app shell.
 */
#include <os_common_api.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <stdio.h>
#include <init.h>
#include <string.h>
#include <kernel.h>
#include <shell/shell.h>
#include <stdlib.h>
#include <gpio.h>
#include <property_manager.h>
#include <logging/sys_log.h>
#include <sys_event.h>
#include <bt_manager.h>
#include <input_manager.h>
#include <app_manager.h>
#include <media_player.h>
#include <audio_policy.h>
#include <media_effect_param.h>
#include "app_defines.h"
#include "app_common.h"
#include "desktop_manager.h"
#include <hex_str.h>

#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif

#ifdef CONFIG_DATA_ANALY
#include <data_analy.h>
#endif
#include "system_app.h"

#define APP_SHELL "app"

#ifdef CONFIG_CONSOLE_SHELL
static int shell_input_key_event(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		u32_t key_event;
		key_event = strtoul(argv[1], (char **)NULL, 0);
		sys_event_report_input(key_event);
	}
	return 0;
}

#ifdef CONFIG_PLAYTTS
void keytone_proc(os_timer * timer)
{
#ifdef CONFIG_BT_ENGINE_ACTS_ANDES
	app_message_post_async("main", MSG_VIEW_PAINT, MSG_UI_EVENT, &evt, 4);
#endif
}

static int shell_keytone_test(int argc, char *argv[])
{
	static os_timer timer;

	if (argv[1] != NULL) {
		if (memcmp(argv[1], "start", sizeof("start")) == 0) {
			os_timer_init(&timer, keytone_proc, NULL);
			os_timer_start(&timer, K_MSEC(10),
				       K_MSEC(strtoul(argv[2], NULL, 0)));
		} else if ((memcmp(argv[1], "stop", sizeof("stop")) == 0)) {
			os_timer_stop(&timer);
		}
	}
	return 0;
}

static int shell_tts_test(int argc, char *argv[])
{
	sys_event_notify(SYS_EVENT_ENTER_PAIR_MODE);

	return 0;
}


static int shell_tts_switch(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		if (memcmp(argv[1], "on", sizeof("on")) == 0) {
			tts_manager_lock();
		} else if ((memcmp(argv[1], "off", sizeof("off")) == 0)) {
			tts_manager_unlock();
		}
	}

	return 0;
}
#endif

static int shell_dump_bt_info(int argc, char *argv[])
{
#ifdef CONFIG_BT_MANAGER
	bt_manager_dump_info();
#endif
	return 0;
}

static int shell_read_bt_rssi(int argc, char *argv[])
{
    int rssi = 0;
#ifdef CONFIG_BT_MANAGER
    rssi = bt_manager_bt_read_rssi(0);
    SYS_LOG_INF("RSSI %d",rssi);
#endif
	return 0;
}

static int shell_read_bt_link_quality(int argc, char *argv[])
{
    int rssi = 0;
#ifdef CONFIG_BT_MANAGER
    rssi = bt_manager_bt_read_link_quality(0);
    SYS_LOG_INF("LINK QUALITY %d",rssi);
#endif
	return 0;
}

static os_delayed_work test_menu_work;
static int test_menu_period;

static void test_menu_work_handler(os_work * work)
{
	sys_event_report_input(KEY_MENU | KEY_TYPE_SHORT_UP);

	os_delayed_work_submit(&test_menu_work, OS_SECONDS(test_menu_period));
}

static int shell_test_menu_key(int argc, char *argv[])
{
	if (argc == 2) {
		test_menu_period = strtoul(argv[1], (char **)NULL, 10);
	} else {
		test_menu_period = 60;
	}

	SYS_LOG_INF("period: %d", test_menu_period);

	os_delayed_work_init(&test_menu_work, test_menu_work_handler);
	os_delayed_work_submit(&test_menu_work, OS_SECONDS(test_menu_period));

	return 0;
}

static int shell_dump_app_data(int argc, char *argv[])
{
	desktop_manager_dump_state();

	return 0;
}

static int shell_charge_enable(int argc, char *argv[])
{
	u8_t en;

	if (argc == 2) {
		en = (u8_t)strtoul(argv[1], (char **)NULL, 10);
	} else {
		en = 1;
	}

	if (en != 0) {
		en = 1;
	}
	SYS_LOG_INF("charge enable=%d", en);

	struct device *dev;
	dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (NULL == dev) {
		SYS_LOG_ERR("gpio Device not found\n");
		return -1;
	}
	gpio_pin_configure(dev, 42, GPIO_DIR_OUT);
	gpio_pin_write(dev, 42, en);

	return 0;
}

#ifdef CONFIG_TOOL
extern int tool_init(void);
static int shell_open_stub(int argc, char *argv[])
{
	tool_init();
	return 0;
}
#endif

static int shell_set_da(int argc, char *argv[])
{
	extern void audio_system_volume_set_music_pada(u32_t v);
	u32_t en;

	if (argc == 2) {
		en = strtoul(argv[1], (char **)NULL, 10);
	} else {
		en = 1;
	}
	printk("Set music da to 0x%x", en);
	audio_system_volume_set_music_pada(en);

	return 0;

}

static int shell_dump_update_peq(int argc, char *argv[])
{

	eq_band_t eq;
	eq.cutoff = strtoul(argv[1], (char**)NULL, 10);
	eq.q = strtoul(argv[2], (char**)NULL, 10);
	eq.gain = strtoul(argv[3], (char**)NULL, 10);
	eq.type = strtoul(argv[4], (char**)NULL, 10);

	audio_policy_set_dynamic_peq_info(&eq);
	void *dump_player =  media_player_get_current_dumpable_player();
	if(dump_player)
	{
		media_player_dynamic_update_peq(dump_player, NULL, 0);
	}
	return 0;

}

static int shell_dump_volume_table(int argc, char *argv[])
{
	extern void audio_system_volume_dump(void);

	audio_system_volume_dump();

	return 0;

}


static int shell_pa_switch(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		if (memcmp(argv[1], "on", sizeof("on")) == 0) {
		#ifdef CONFIG_AUDIO_POWERON_OPEN_PA
			hal_aout_open_pa(true);
		#endif
		} else if ((memcmp(argv[1], "off", sizeof("off")) == 0)) {
			/**disable pa */
		#ifdef CONFIG_AUDIO_POWERON_OPEN_PA
			hal_aout_close_pa(true);
		#endif
		}
	}

	return 0;
}

uint8_t pawr_enable_flag = 0;
uint8_t get_pawr_enable(void)
{
    return pawr_enable_flag;
}

void enable_disable_pawr(uint8_t enable)
{
    pawr_enable_flag = enable;
	SYS_LOG_INF("pawr_enable_flag=%d", pawr_enable_flag);
}

#ifdef CONFIG_BT_PAWR
static int shell_pawr_enable(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		if (memcmp(argv[1], "enable", sizeof("enable")) == 0) {
            enable_disable_pawr(1);
		} else if ((memcmp(argv[1], "disable", sizeof("disable")) == 0)) {
			enable_disable_pawr(0);
		}
	}

	return 0;
}
#endif

static void set_phone_controller_role(int role)
{
    int ret;
	ret = property_set_int("PHONE_CONTROLER_ROLE", role);
	if (ret != 0) {
       SYS_LOG_ERR("err:\n");
	}
	property_flush(NULL);
	SYS_LOG_INF("%d", role);
}

static int shell_phone_controller_role(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		if (memcmp(argv[1], "master", sizeof("master")) == 0) {
            set_phone_controller_role(CONTROLER_ROLE_MASTER);
		} else if ((memcmp(argv[1], "slave", sizeof("slave")) == 0)) {
			set_phone_controller_role(CONTROLER_ROLE_SLAVE);
		}
	}

	return 0;
}

#ifdef CONFIG_DATA_ANALY
static int shell_dump_data_analy_data_info(int argc, char *argv[])
{
	data_analy_dump_data();
	data_analy_dump_play();
	return 0;
}

static int shell_dump_data_analy_play_all_record(int argc, char *argv[])
{
	data_analy_dump_play_all_record();
	return 0;
}


static int shell_dump_data_analy_clear_all_record(int argc, char *argv[])
{
	data_analy_data_clear();
	data_analy_play_clear();
	return 0;
}


#endif

#ifdef CONFIG_BT_LETWS

int bt_mamager_set_remote_ble_addr(bt_addr_le_t *addr);
void bt_manager_letws_start_pair_search(uint8_t role,int time_out_s);

static int str2bt_addr(const char *str, bt_addr_t *addr)
{
	int i, j;
	uint8_t tmp;

	if (strlen(str) != 17) {
		return -EINVAL;
	}

	for (i = 5, j = 1; *str != '\0'; str++, j++) {
		if (!(j % 3) && (*str != ':')) {
			return -EINVAL;
		} else if (*str == ':') {
			i--;
			continue;
		}

		addr->val[i] = addr->val[i] << 4;

		if (char2hex(*str, &tmp) < 0) {
			return -EINVAL;
		}

		addr->val[i] |= tmp;
	}

	return 0;
}

static int shell_ble_scan_enable(int argc, char *argv[])
{
    bt_addr_le_t remote_ble_addr;
    uint8 i = 0;

	int err;
	remote_ble_addr.type = 0;
	err = str2bt_addr(argv[1], &remote_ble_addr.a);
	if (err) {
		printk("Invalid peer address (err %d)\n", err);
		return 0;
	}
	for (i=0;i<=5;i++) {
		printk("%x",remote_ble_addr.a.val[i]);
	}
	printk("\n");

    bt_mamager_set_remote_ble_addr(&remote_ble_addr);

	//bt_manager_audio_le_resume_scan();
    bt_manager_letws_start_pair_search(BTSRV_TWS_MASTER,60);
	return 0;
}

//int bt_manager_letws_adv_start(uint8 dir_flag);
static int shell_ble_adv_enable(int argc, char *argv[])
{
    bt_addr_le_t remote_ble_addr;
    uint8 i = 0;

	int err;
	remote_ble_addr.type = 0;
	err = str2bt_addr(argv[1], &remote_ble_addr.a);
	if (err) {
		printk("Invalid peer address (err %d)\n", err);
		return 0;
	}

	for (i=0;i<=5;i++) {
		printk("%x",remote_ble_addr.a.val[i]);
	}
	printk("\n");

    bt_mamager_set_remote_ble_addr(&remote_ble_addr);

   	//bt_manager_audio_le_resume_adv();
	//bt_manager_letws_adv_start(1);
	bt_manager_letws_start_pair_search(BTSRV_TWS_SLAVE,60);
	return 0;
}

static int shell_tws_reset(int argc, char *argv[])
{
	bt_manager_letws_reset();
	return 0;
}

#endif

static int shell_stereo_mode_enable(int argc, char *argv[])
{
	bool primary = false;
	if (argc > 1 && strtoul(argv[1], NULL, 0)) {
		primary = true;
	}

	bt_manager_lea_enter_stereo_mode(primary);
	return 0;
}

static int shell_stereo_mode_disable(int argc, char *argv[])
{
	bt_manager_lea_exit_stereo_mode();
	return 0;
}

#ifdef CONFIG_OTA_APP
#include "../ota/ota_app.h"
#endif

int trace_print_disable_set(unsigned int print_disable);

static int shell_uart0_switch(int argc, char *argv[])
{
	//disable uart0 tx dma print
	trace_print_disable_set(true);

#ifdef CONFIG_OTA_BACKEND_UART
	ota_app_init_uart();
#endif

	return 0;
}

static int shell_print_enable(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		if (memcmp(argv[1], "enable", sizeof("enable")) == 0) {
			trace_print_disable_set(false);
		} else if ((memcmp(argv[1], "disable", sizeof("disable")) == 0)) {
			trace_print_disable_set(true);
		}
	}

	return 0;
}

#ifdef CONFIG_ACTIONS_IMG_LOAD
static int shell_bin_test(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		u8_t id = strtoul(argv[1], (char**)NULL, 10);
		int ret = property_set_int(CFG_BIN_TEST_ID, id);
		if (ret == 0) {
			property_flush(CFG_BIN_TEST_ID);
			printk("bin test new id %d\n", id);
			sys_pm_reboot(REBOOT_TYPE_GOTO_WIFISYS);
		}

	}

	return 0;
}

#endif

extern int8_t broadcast_set_sq_mode(int8_t sq_mode);

static int shell_sq_enable(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		if (memcmp(argv[1], "enable", sizeof("enable")) == 0) {
			broadcast_set_sq_mode(true);
		} else if ((memcmp(argv[1], "disable", sizeof("disable")) == 0)) {
			broadcast_set_sq_mode(false);
		}
	}

	return 0;
}


static const struct shell_cmd app_commands[] = {
	{"input", shell_input_key_event, "input key event"},
	{"btinfo", shell_dump_bt_info, "dump bt info"},
    {"rssi",shell_read_bt_rssi,"read bt rssi"},
    {"quality",shell_read_bt_link_quality,"read bt link quality"},

#ifdef CONFIG_PLAYTTS
	{"tts", shell_tts_switch, "tts on/off"},
	{"keytone", shell_keytone_test, "keytone start or stop"},
	{"tts_test", shell_tts_test, "tts test start"},
#endif
	{"menu", shell_test_menu_key, "input menu key periodically"},
	{"dump", shell_dump_app_data, "do currect app data dump."},
#ifdef CONFIG_TOOL
	{"open_stub", shell_open_stub, "open_stub"},
#endif
	{"chgen", shell_charge_enable, "Enable/disable charge."},
	{"setda", shell_set_da, "set da(dsp) value."},
	{"dumpvol", shell_dump_volume_table, "dump volume table."},
	{"peq", shell_dump_update_peq, "dump volume table."},
	{"pa", shell_pa_switch, "enable/disable pa"},
#ifdef CONFIG_BT_PAWR
	{"pawr", shell_pawr_enable, "enable/disable pawr"},
#endif

#ifdef CONFIG_DATA_ANALY
	{"dump_da", shell_dump_data_analy_data_info, "dump analy data"},
	{"dump_pa", shell_dump_data_analy_play_all_record, "dump analy data play all record"},
	{"clear_da", shell_dump_data_analy_clear_all_record, "clear all_data_analy record"},
#endif

	{"br_phone_controller_role", shell_phone_controller_role, "master/slave"},
#ifdef CONFIG_BT_LETWS
	{"ble_scan", shell_ble_scan_enable, "xx:xx:xx:xx:xx:xx"},
	{"ble_adv", shell_ble_adv_enable, "xx:xx:xx:xx:xx:xx"},
	{"ble_tws_reset", shell_tws_reset, "reset ble tws"},
#endif
	{"stereo", shell_stereo_mode_enable, "1-primary,0-secondary"},
	{"stereo_exit", shell_stereo_mode_disable, "exit stereo mode"},

	{"uart0_switch", shell_uart0_switch, "uart0 disable print"},

	{"print", shell_print_enable, "print enable or disable"},

#ifdef CONFIG_ACTIONS_IMG_LOAD
	{"bin_test", shell_bin_test, "print enable or disable"},
#endif

	{"boardcast_sq", shell_sq_enable, "boardcast_sq enable or disable"},

	{NULL, NULL, NULL}};
#endif

SHELL_REGISTER(APP_SHELL, app_commands);

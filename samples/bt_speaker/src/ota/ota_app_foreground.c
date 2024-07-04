/*
 * Copyright (c) 2023 Actions Semiconductor Co, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <device.h>
#include <thread_timer.h>
#include <misc/util.h>
#include <logging/sys_log.h>
#include <mem_manager.h>
#include <global_mem.h>
#include <msg_manager.h>
#include <app_ui.h>
#include <bt_manager.h>
#include <app_manager.h>
#include <app_launch.h>
#include <srv_manager.h>
#include <sys_manager.h>
#include <sys_event.h>
#include <sys_wakelock.h>
#include <ota_upgrade.h>
#include <ota_backend.h>
#include <ota_backend_sdcard.h>
#include <ota_backend_bt.h>
#include <ota_backend_uart.h>
#include <ota_backend_selfapp.h>
#include <ota_app.h>
#include <ui_manager.h>
#include "flash.h"
#include "desktop_manager.h"
#include <input_manager.h>
#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif
#ifdef CONFIG_OTA_UNLOAD
#include <ota_unload.h>
#endif
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
#include <soc_dvfs.h>
#endif

#ifdef CONFIG_DVFS
#include <dvfs.h>
#endif

#ifdef CONFIG_DEBUG_RAMDUMP
#include <debug/ramdump.h>
#endif

#include "tts_manager.h"
#include <media_mem.h>
#include <audio_system.h>

#define CONFIG_OTA_APP_AUTO_START

#ifdef CONFIG_OTA_RECOVERY
#ifdef CONFIG_SPI_PSRAM_CACHE_DEV_NAME
#define OTA_STORAGE_DEVICE_NAME		CONFIG_SPI_PSRAM_CACHE_DEV_NAME
#else
#define OTA_STORAGE_DEVICE_NAME		CONFIG_OTA_TEMP_PART_DEV_NAME
#endif
#else
#define OTA_STORAGE_DEVICE_NAME		CONFIG_XSPI_NOR_ACTS_DEV_NAME
#endif

#define CONFIG_OTA_THREAD_STACK_SIZE (2048)

static struct ota_upgrade_info *g_ota;
#ifdef CONFIG_OTA_BACKEND_SDCARD
static struct ota_backend *backend_sdcard;
#endif
#ifdef CONFIG_OTA_BACKEND_BLUETOOTH
static struct ota_backend *backend_bt;
#endif
static bool is_sd_ota = false;
static bool is_ota_need_poweroff;

#ifdef CONFIG_OTA_BACKEND_UART
static struct ota_backend *backend_uart;
#endif

static void ota_app_start(void)
{
	SYS_LOG_INF("ota app start");

	struct device *flash_device =
	    device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);
	if (flash_device)
		flash_write_protection_set(flash_device, false);

#ifdef CONFIG_OTA_APP_AUTO_START
	system_app_launch_add(DESKTOP_PLUGIN_ID_OTA,true);
#else
	struct app_msg msg = { 0 };

	msg.type = MSG_OTA_APP_EVENT;
	msg.cmd = MSG_OTA_MESSAGE_CMD_INIT_APP;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
#endif
	sys_wake_lock(WAKELOCK_OTA);

#ifdef CONFIG_OTA_UNLOAD
	ota_unloading_set_state(OTA_UNLOADING_START);
#endif
}

#ifdef CONFIG_OTA_APP_AUTO_START
static void ota_app_stop(u8_t switch_app)
{

	SYS_LOG_INF("%d", switch_app);

	struct device *flash_device =
	    device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);
	if (flash_device)
		flash_write_protection_set(flash_device, true);

	if (switch_app) {
		system_app_launch_del(DESKTOP_PLUGIN_ID_OTA);
	}
	sys_wake_unlock(WAKELOCK_OTA);
}
#endif

#ifdef CONFIG_OTA_BACKEND_SDCARD
static bool check_sd_ota_restart(void)
{
#ifdef CONFIG_PROPERTY
	char sd_ota_flag[4] = { 0 };

	property_get("SD_OTA_FLAG", sd_ota_flag, 4);
	if (!memcmp(sd_ota_flag, "yes", strlen("yes"))) {
		property_set("SD_OTA_FLAG", "no", 4);
		property_flush(NULL);
		return true;
	}
#endif
	return false;
}
#endif

extern void ota_app_backend_callback(struct ota_backend *backend, int cmd,
				     int state)
{
	int err;
	uint8_t plugin_id = desktop_manager_get_plugin_id();

	SYS_LOG_INF("backend %p cmd %d state %d", backend, cmd, state);

	if (plugin_id == DESKTOP_PLUGIN_ID_BR_CALL) {
		SYS_LOG_WRN("btcall unsupport ota, skip...");
		return;
	}

	if (cmd == OTA_BACKEND_UPGRADE_STATE) {
		if (state > 0) {
#ifdef CONFIG_BT_HFP_HF
			SYS_LOG_INF("bt_manager_allow_sco_connect false\n");
			bt_manager_allow_sco_connect(false);
#endif
#ifdef CONFIG_OTA_BACKEND_SDCARD
			// no version control, don't enter power on sd_ota again after reboot
			if (backend == backend_sdcard) {  //if (backend != backend_bt) {
				if (check_sd_ota_restart()) {
					SYS_LOG_INF("sd ota restart\n");
					return;
				}
				is_sd_ota = true;
			} else {
				is_sd_ota = false;
			}
#endif
			err = ota_upgrade_attach_backend(g_ota, backend);
			if (err) {
				SYS_LOG_INF("unable attach backend");
				return;
			}

			ota_app_start();
		} else {
			ota_upgrade_detach_backend(g_ota, backend);
#ifdef CONFIG_BT_HFP_HF
			SYS_LOG_INF("bt_manager_allow_sco_connect true\n");
			bt_manager_allow_sco_connect(true);
#endif
		}
	} else if (cmd == OTA_BACKEND_UPGRADE_PROGRESS) {
		ota_view_show_upgrade_progress(state);
	}
}

#ifdef CONFIG_OTA_BACKEND_SDCARD
struct ota_backend_sdcard_init_param sdcard_init_param = {
	.fpath = "SD:ota.bin",
};

int ota_app_init_sdcard(void)
{
	backend_sdcard =
	    ota_backend_sdcard_init(ota_app_backend_callback,
				    &sdcard_init_param);
	if (!backend_sdcard) {
		SYS_LOG_INF("failed to init sdcard ota");
		return -ENODEV;
	}

	return 0;
}
#endif

#ifdef CONFIG_OTA_BACKEND_BLUETOOTH
#ifndef CONFIG_OTA_SELF_APP
/* UUID order using BLE format */
/*static const u8_t ota_spp_uuid[16] = {0x00,0x00,0x66,0x66, \
	0x00,0x00,0x10,0x00,0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB};*/

/* "00006666-0000-1000-8000-00805F9B34FB"  ota uuid spp */
static const u8_t ota_spp_uuid[16] = { 0xFB, 0x34, 0x9B, 0x5F, 0x80,
	0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x66, 0x66, 0x00, 0x00
};

/* BLE */
/*	"e49a25f8-f69a-11e8-8eb2-f2801f1b9fd1" reverse order  */
#define OTA_SERVICE_UUID BT_UUID_DECLARE_128( \
				0xd1, 0x9f, 0x1b, 0x1f, 0x80, 0xf2, 0xb2, 0x8e, \
				0xe8, 0x11, 0x9a, 0xf6, 0xf8, 0x25, 0x9a, 0xe4)

/* "e49a25e0-f69a-11e8-8eb2-f2801f1b9fd1" reverse order */
#define OTA_CHA_RX_UUID BT_UUID_DECLARE_128( \
				0xd1, 0x9f, 0x1b, 0x1f, 0x80, 0xf2, 0xb2, 0x8e, \
				0xe8, 0x11, 0x9a, 0xf6, 0xe0, 0x25, 0x9a, 0xe4)

/* "e49a28e1-f69a-11e8-8eb2-f2801f1b9fd1" reverse order */
#define OTA_CHA_TX_UUID BT_UUID_DECLARE_128( \
				0xd1, 0x9f, 0x1b, 0x1f, 0x80, 0xf2, 0xb2, 0x8e, \
				0xe8, 0x11, 0x9a, 0xf6, 0xe1, 0x28, 0x9a, 0xe4)

//static struct bt_gatt_ccc_cfg g_ota_ccc_cfg[1];

static struct bt_gatt_attr ota_gatt_attr[] = {
	BT_GATT_PRIMARY_SERVICE(OTA_SERVICE_UUID),

	BT_GATT_CHARACTERISTIC(OTA_CHA_RX_UUID,
			       BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE, NULL, NULL, NULL),

	BT_GATT_CHARACTERISTIC(OTA_CHA_TX_UUID, BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, NULL,
			       NULL, NULL),

	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
};

static const struct ota_backend_bt_init_param bt_init_param = {
	.spp_uuid = ota_spp_uuid,
	.gatt_attr = &ota_gatt_attr[0],
	.attr_size = ARRAY_SIZE(ota_gatt_attr),
	.tx_chrc_attr = &ota_gatt_attr[3],
	.tx_attr = &ota_gatt_attr[4],
	.tx_ccc_attr = &ota_gatt_attr[5],
	.rx_attr = &ota_gatt_attr[2],
	.read_timeout = OS_FOREVER,	/* K_FOREVER, K_NO_WAIT,  K_MSEC(ms) */
	.write_timeout = OS_FOREVER,
};
#endif //CONFIG_OTA_SELF_APP

int ota_app_init_bluetooth(void)
{
#ifndef CONFIG_OTA_SELF_APP
	SYS_LOG_INF("spp uuid\n");
	print_buffer(bt_init_param.spp_uuid, 1, 16, 16, 0);

	backend_bt = ota_backend_bt_init(ota_app_backend_callback,
					 (struct ota_backend_bt_init_param *)
					 &bt_init_param);
	if (!backend_bt) {
		SYS_LOG_INF("failed");
		return -ENODEV;
	}
#else
	backend_bt = NULL;
#endif
	return 0;
}
#endif

#ifdef CONFIG_OTA_SELF_APP
int ota_app_init_selfapp(void)
{
	selfapp_backend_init(ota_app_backend_callback);
	return 0;
}
#endif

#ifdef CONFIG_OTA_BACKEND_UART
int ota_app_init_uart(void)
{
	SYS_LOG_INF();

	backend_uart = ota_backend_uart_init(ota_app_backend_callback, NULL);
	if (backend_uart == NULL) {
		SYS_LOG_INF("failed");
		return -ENODEV;
	}

	return 0;
}
#endif

static void sys_reboot_by_ota(void)
{
	struct app_msg msg = { 0 };
	msg.type = MSG_REBOOT;
	msg.cmd = REBOOT_REASON_OTA_FINISHED;
	send_async_msg(CONFIG_SYS_APP_NAME, &msg);
}

static void sys_poweroff_by_ota(void)
{
	sys_event_send_message(MSG_POWER_OFF);
}

void ota_app_notify(int state, int old_state)
{
	SYS_LOG_INF("ota state: %d->%d", old_state, state);

	if (old_state != OTA_RUNNING && state == OTA_RUNNING) {
#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
		dvfs_set_level(DVFS_LEVEL_HIGH_PERFORMANCE, "ota");
#endif
	} else if (old_state == OTA_RUNNING && state != OTA_RUNNING) {
#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
		dvfs_unset_level(DVFS_LEVEL_HIGH_PERFORMANCE, "ota");
#endif
	}

#ifdef CONFIG_OTA_SELF_APP
	selfapp_backend_callback(state);
#endif

	if (state == OTA_DONE) {
		ota_view_show_upgrade_result("Succ", false);
		if (is_sd_ota) {
#ifdef CONFIG_PROPERTY
			property_set("SD_OTA_FLAG", "yes", 4);
			property_flush(NULL);
#endif
		}
		os_sleep(1000);

#ifdef CONFIG_OTA_SELF_APP
		if (ota_upgrade_get_backend_type(g_ota) == OTA_BACKEND_TYPE_SELFAPP) {
			printk("selfapp ota don't auto reboot\n");
			sys_event_notify(SYS_EVENT_OTA_FINISHED_REBOOT);
		} else
#endif
		{
			sys_event_notify(SYS_EVENT_OTA_FINISHED_REBOOT);
			//sys_reboot_by_ota();
		}
	}else if(state == OTA_FAIL){
		if(is_ota_need_poweroff){
			sys_event_notify(SYS_EVENT_OTA_FINISHED_REBOOT);
		}
	}
}

int ota_app_init(void)
{
	struct ota_upgrade_param param;

	SYS_LOG_INF("device name %s ", OTA_STORAGE_DEVICE_NAME);

#ifdef CONFIG_OTA_UNLOAD
	if(ota_unloading_get_state() != OTA_UNLOADING_FINISHED) {
		ota_unloading_clear_all();
        ota_unloading_set_state(OTA_UNLOADING_FINISHED);
	}
#endif

	memset(&param, 0x0, sizeof(struct ota_upgrade_param));

	param.storage_name = OTA_STORAGE_DEVICE_NAME;
	param.notify = ota_app_notify;
#ifdef CONFIG_OTA_RECOVERY
	param.flag_use_recovery = 1;
	param.flag_use_recovery_app = 0;
#endif
	param.no_version_control = 1;

#ifdef CONFIG_SOC_SECURE_BOOT
	param.flag_secure_boot = 1;
	param.public_key = ota_get_public_key();
#endif

#ifdef CONFIG_DEBUG_RAMDUMP
	if(ramdump_check() == 0){
		param.flag_skip_init_erase = true;
	}
#endif

	struct device *flash_device =
	    device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);
	if (flash_device)
		flash_write_protection_set(flash_device, false);

	g_ota = ota_upgrade_init(&param);
	if (!g_ota) {
		SYS_LOG_INF("init failed");
		if (flash_device)
			flash_write_protection_set(flash_device, true);
		return -1;
	}

	if (flash_device)
		flash_write_protection_set(flash_device, true);
	return 0;
}

#ifdef CONFIG_OTA_APP_AUTO_START
static void ota_app_start_ota_upgrade(void)
{
	struct app_msg msg = { 0 };

	msg.type = MSG_OTA_APP_EVENT;
	msg.cmd = MSG_OTA_MESSAGE_CMD_INIT_APP;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}
#endif

static void ota_app_stop_ota_upgrade(void)
{
	struct app_msg msg = { 0 };

	msg.type = MSG_OTA_APP_EVENT;
	msg.cmd = MSG_OTA_MESSAGE_CMD_EXIT_APP;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

static int _ota_app_init(void *p1, void *p2, void *p3)
{
	SYS_LOG_INF("enter");
	//sys_event_notify(SYS_EVENT_ENTER_OTA);
	ota_view_init();

	is_ota_need_poweroff = false;
	input_manager_lock();
#ifdef CONFIG_OTA_APP_AUTO_START
	//app_switch_lock(1);
	ota_app_start_ota_upgrade();
#endif

#ifdef CONFIG_ACT_EVENT
	act_event_runtime_enable(false);
#endif

	return 0;
}

static int _ota_exit(void)
{
	if (!g_ota)
		return 0;

	ota_view_deinit();
	input_manager_unlock();
	return 0;
}

static void ota_thread_deal(void *p1, void *p2, void *p3)
{
	uint32_t switch_last_app;
	struct ota_upgrade_check_param param;

	switch_last_app = 1;

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	/* to fix ble rx data error for low frequency */
	soc_dvfs_set_level(SOC_DVFS_LEVEL_ALL_PERFORMANCE, "ota");
#endif
#ifdef CONFIG_OTA_SELF_APP
	if (ota_upgrade_get_backend_type(g_ota) == OTA_BACKEND_TYPE_SELFAPP) {
		switch_last_app = 0;
	}
#endif

	param.data_buf = media_mem_get_cache_pool(OTA_UPGRADE_BUF,  AUDIO_STREAM_TTS);
	param.data_buf_size = media_mem_get_cache_pool_size(OTA_UPGRADE_BUF,  AUDIO_STREAM_TTS);

#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(true);
	tts_manager_lock();
#endif

	desktop_manager_lock();
	if (ota_upgrade_check(g_ota, &param)) {
		ota_view_show_upgrade_result("Fail", true);
		desktop_manager_unlock();
		if(!is_ota_need_poweroff){
			switch_last_app = 1;
		}
	} else {
		ota_view_show_upgrade_result("Succ", false);
	}

#ifdef CONFIG_PLAYTTS
	tts_manager_unlock();
#endif

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_unset_level(SOC_DVFS_LEVEL_ALL_PERFORMANCE, "ota");
#endif

#ifdef CONFIG_OTA_APP_AUTO_START
	//app_switch_unlock(1);
	ota_app_stop(switch_last_app);
#endif

	ota_app_stop_ota_upgrade();
}


static void _ota_create_sub_thread(void)
{
	uint8_t *thread_stack;
	uint32_t thread_stack_size;

	thread_stack = media_mem_get_cache_pool(OTA_THREAD_STACK,  AUDIO_STREAM_TTS);
	thread_stack_size = media_mem_get_cache_pool_size(OTA_THREAD_STACK,  AUDIO_STREAM_TTS);

	printk("create thread stack %p size %x\n", thread_stack, thread_stack_size);

	if (!thread_stack) {
		return;
	}

	os_thread_create(thread_stack, thread_stack_size,
			 ota_thread_deal, NULL, NULL, NULL, 5, 0, OS_NO_WAIT);

}

static void _ota_destory_sub_thread(void)
{
	printk("ota sub thread exit\n");
}

void ota_input_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case MSG_OTA_MESSAGE_CMD_POWEROFF:
		is_ota_need_poweroff = true;
		ota_upgrade_cancel_read(g_ota);
		break;

	default:
		break;
	}
}

static int _ota_proc_msg(struct app_msg *msg)
{

	SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg->type, msg->cmd, msg->value);

	switch (msg->type) {
		case MSG_OTA_APP_EVENT: {
			if(msg->cmd == MSG_OTA_MESSAGE_CMD_INIT_APP){
				_ota_create_sub_thread();
			}else if(msg->cmd == MSG_OTA_MESSAGE_CMD_EXIT_APP){
				_ota_destory_sub_thread();
			}
			break;
		}

		case MSG_INPUT_EVENT:
			ota_input_event_proc(msg);
			break;

		case MSG_EXIT_APP:
			_ota_exit();
			break;

		case MSG_REBOOT:
			sys_reboot_by_ota();
			break;

		case MSG_TTS_EVENT:
			if(msg->value == TTS_EVENT_POWER_OFF){
				if(!is_ota_need_poweroff){
					sys_reboot_by_ota();
				}else{
					sys_poweroff_by_ota();
				}
			}
			break;

		default:
			SYS_LOG_ERR("unknown: 0x%x!", msg->type);
			break;
	}
	return 0;
}

static int _ota_dump_app_state(void)
{
	return 0;
}

#ifdef CONFIG_DEBUG_RAMDUMP

#include <debug/ramdump.h>
#include <stack_backtrace.h>

static void crash_dump_ramdump(void)
{
	if(!ota_upgrade_is_ota_running()){
		if(ramdump_check() != 0){
			printk("start ramdump...\n");
			ramdump_save();
		}else{
			printk("ramdump data existed\n");
		}
	}else{
		printk("ota is running, skip ramdump!\n");
	}
}

CRASH_DUMP_REGISTER(ramdump_register, 1) =
{
    .dump = crash_dump_ramdump,
};

#endif

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_OTA, _ota_app_init, _ota_exit, _ota_proc_msg, \
	_ota_dump_app_state, NULL, NULL, NULL);

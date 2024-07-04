/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt ble manager.
 */
#define SYS_LOG_DOMAIN "btmgr_ble"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <msg_manager.h>
#include <mem_manager.h>
#include <acts_bluetooth/host_interface.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <hex_str.h>
#include "ctrl_interface.h"
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
//#include "ats/ats.h"
#include "gfp_convert_search_api/gfp_convert_search_api.h"
#endif
#ifdef CONFIG_ACT_EVENT
#include <bt_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_REGISTER(bt_ble, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

/* TODO: New only support one ble connection */

#define BLE_CONNECT_INTERVAL_CHECK		(5000)		/* 5s */
#define BLE_CONNECT_DELAY_UPDATE_PARAM	(4000)
#define BLE_UPDATE_PARAM_FINISH_TIME	(4000)
#define BLE_DELAY_UPDATE_PARAM_TIME		(50)
#define BLE_NOINIT_INTERVAL				(0xFFFF)
#define BLE_DEFAULT_IDLE_INTERVAL		(80)		/* 80*1.25 = 100ms */
#define BLE_TRANSFER_INTERVAL			(12)		/* 12*1.25 = 15ms */
#define BLE_DELAY_EXCHANGE_MTU_TIME		(200)		/* 200ms */

#ifdef CONFIG_BT_ADV_MANAGER
#define BLE_ADV_DATA_MAX_LEN			(31)
#define BLE_ADV_MAX_ARRAY_SIZE			(4)
#endif

#ifdef CONFIG_BT_DOUBLE_PHONE
#ifdef CONFIG_BT_DOUBLE_PHONE_TAKEOVER_MODE
#define BLE_DEV_MAX_CNT					(3)
#else
#define BLE_DEV_MAX_CNT					(2)
#endif
#else
#define BLE_DEV_MAX_CNT					(1)
#endif

enum {
	PARAM_UPDATE_EXCHANGE_MTU,
	PARAM_UPDATE_IDLE_STATE,
	PARAM_UPDATE_WAITO_UPDATE_STATE,
	PARAM_UPDATE_RUNING,
};

static OS_MUTEX_DEFINE(ble_mgr_lock);
static struct bt_gatt_indicate_params ble_ind_params __IN_BT_SECTION;
static sys_slist_t ble_list __IN_BT_SECTION;
static uint16_t ble_idle_interval __IN_BT_SECTION;

struct ble_mgr_dev {
	struct bt_conn *ble_conn;
	uint8_t device_mac[6];
	uint16_t ble_current_interval;
	uint8_t ble_transfer_state:1;
	uint8_t update_work_state:4;
	uint16_t ble_send_cnt;
	uint16_t ble_pre_send_cnt;
	os_delayed_work param_update_work;
	os_sem ble_ind_sem;
};

struct ble_mgr_info {
	uint8_t br_a2dp_runing:1;
	uint8_t br_hfp_runing:1;
	uint8_t initialized : 1;
	uint8_t adv_halt:1;
	uint8_t adv_enable:1;
    int8_t adv_power;
	struct ble_mgr_dev dev[BLE_DEV_MAX_CNT];

#ifdef CONFIG_BT_ADV_MANAGER
	uint8_t ad_data[BLE_ADV_DATA_MAX_LEN];
	uint8_t sd_data[BLE_ADV_DATA_MAX_LEN];
#endif
};

static struct ble_mgr_info ble_info;

static const struct bt_data ble_ad_discov[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static struct ble_mgr_dev* alloc_ble_dev_info(void)
{
	for (int i = 0; i < BLE_DEV_MAX_CNT; i++) {
		if (!ble_info.dev[i].ble_conn) {
			return &ble_info.dev[i];
		}
	}

	return NULL;
}

static struct ble_mgr_dev* get_ble_dev_info(struct bt_conn *conn)
{
	for (int i = 0; i < BLE_DEV_MAX_CNT; i++) {
		if (ble_info.dev[i].ble_conn == conn) {
			return &ble_info.dev[i];
		}
	}

	return NULL;
}

void ble_advertise(void)
{
	struct bt_le_adv_param param;
	const struct bt_data *ad;
	size_t ad_len;
	int err;

	if (!ble_info.initialized) {
		return;
	}

	memset(&param, 0, sizeof(param));
	param.id = BT_ID_DEFAULT;
	param.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
	param.interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
#ifdef CONFIG_BT_LETWS	
	param.options = (BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME  | BT_LE_ADV_OPT_USE_IDENTITY);
#else
	param.options = (BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME);
#endif
	ad = ble_ad_discov;
	ad_len = ARRAY_SIZE(ble_ad_discov);

	err = hostif_bt_le_adv_start(&param, ad, ad_len, NULL, 0);
	if (err < 0 && err != (-EALREADY)) {
		SYS_LOG_ERR("Failed to start advertising (err %d)", err);
	} else {
		SYS_LOG_INF("Advertising started");
	}
}

static uint16_t ble_param_interval_to_latency(uint16_t default_interval, uint16_t set_interval)
{
	uint16_t latency;

	latency = set_interval/default_interval;
	if (latency >= 1) {
		latency--;
	}

	return latency;
}

static void exchange_func(struct bt_conn *conn, uint8_t err,
			  struct bt_gatt_exchange_params *params)
{
	SYS_LOG_INF("%p, Exchange %s\n", conn, err == 0 ? "successful" : "failed");

#ifdef CONFIG_BT_LEATWS
	leatws_adv_enable_check();
#endif
}

static struct bt_gatt_exchange_params exchange_params = {
	.func = exchange_func,
};

static void param_update_work_callback(struct k_work *work)
{
	uint16_t req_interval;
	struct ble_mgr_dev *ble_dev =
		CONTAINER_OF(work, struct ble_mgr_dev, param_update_work);

#ifdef CONFIG_GFP_PROFILE
    extern uint8_t gfp_ble_key_pairing(void);
    if(gfp_ble_key_pairing()){
        return;
	}
#endif

	if (bt_manager_config_pts_test()) {
		return;
	}

	if (ble_dev->ble_conn) {
		struct bt_le_conn_param param;

		SYS_LOG_INF("update_work_state: %d", ble_dev->update_work_state);

		if (ble_dev->update_work_state == PARAM_UPDATE_EXCHANGE_MTU) {
			if (exchange_params.func) {
				hostif_bt_gatt_exchange_mtu(ble_dev->ble_conn, &exchange_params);
			}

			ble_dev->update_work_state = PARAM_UPDATE_IDLE_STATE;
			return;
		}
		if (ble_dev->update_work_state == PARAM_UPDATE_RUNING) {
			ble_dev->update_work_state = PARAM_UPDATE_IDLE_STATE;
			os_delayed_work_submit(&ble_dev->param_update_work, BLE_CONNECT_INTERVAL_CHECK);
			return;
		}

		if (ble_dev->update_work_state == PARAM_UPDATE_IDLE_STATE) {
			if (ble_dev->ble_transfer_state &&
				(ble_dev->ble_current_interval == BLE_TRANSFER_INTERVAL)) {
				if (ble_dev->ble_pre_send_cnt == ble_dev->ble_send_cnt) {
					ble_dev->ble_transfer_state = 0;
				} else {
					ble_dev->ble_pre_send_cnt = ble_dev->ble_send_cnt;
				}
			}
		}

		if (ble_info.br_a2dp_runing || ble_info.br_hfp_runing) {
			req_interval = ble_idle_interval;
		} else if (ble_dev->ble_transfer_state) {
			req_interval = BLE_TRANSFER_INTERVAL;
		} else {
			req_interval = ble_idle_interval;
		}

		if (req_interval == ble_dev->ble_current_interval) {
			ble_dev->update_work_state = PARAM_UPDATE_IDLE_STATE;
			os_delayed_work_submit(&ble_dev->param_update_work, BLE_CONNECT_INTERVAL_CHECK);
			return;
		}

		ble_dev->ble_current_interval = req_interval;

		/* interval time: x*1.25 */
		param.interval_min = BLE_TRANSFER_INTERVAL;
		param.interval_max = BLE_TRANSFER_INTERVAL;
		param.latency = ble_param_interval_to_latency(BLE_TRANSFER_INTERVAL, req_interval);
		param.timeout = 500;
		hostif_bt_conn_le_param_update(ble_dev->ble_conn, &param);
		SYS_LOG_INF("%d\n", req_interval);

		ble_dev->update_work_state = PARAM_UPDATE_RUNING;
		os_delayed_work_submit(&ble_dev->param_update_work, BLE_UPDATE_PARAM_FINISH_TIME);
	} else {
		ble_dev->update_work_state = PARAM_UPDATE_IDLE_STATE;
	}
}

static void ble_check_update_param(struct bt_conn *conn)
{
	struct ble_mgr_dev *ble_dev = get_ble_dev_info(conn);

	// SYS_LOG_INF("%d, %p, %p", ble_dev->ble_current_interval, conn, ble_dev->ble_conn);

	if(ble_info.initialized && ble_dev){
		if (ble_dev->update_work_state == PARAM_UPDATE_IDLE_STATE) {
			ble_dev->update_work_state = PARAM_UPDATE_WAITO_UPDATE_STATE;
			os_delayed_work_submit(&ble_dev->param_update_work, BLE_DELAY_UPDATE_PARAM_TIME);
		} else if (ble_dev->update_work_state == PARAM_UPDATE_RUNING) {
			ble_dev->update_work_state = PARAM_UPDATE_WAITO_UPDATE_STATE;
			os_delayed_work_submit(&ble_dev->param_update_work, BLE_UPDATE_PARAM_FINISH_TIME);
		} else {
			/* Already in PARAM_UPDATE_WAITO_UPDATE_STATE */
		}
	}
}

static void ble_send_data_check_interval(struct bt_conn *conn)
{
	struct ble_mgr_dev *ble_dev = get_ble_dev_info(conn);

	if (ble_dev) {
		ble_dev->ble_send_cnt++;
		// SYS_LOG_INF("%d, %p, %p", ble_dev->ble_current_interval, conn, ble_dev->ble_conn);
		if ((ble_dev->ble_current_interval != BLE_TRANSFER_INTERVAL) &&
			!ble_info.br_a2dp_runing && !ble_info.br_hfp_runing) {
			ble_dev->ble_transfer_state = 1;
			ble_check_update_param(conn);
		}
	}
}

static void notify_ble_connected(struct bt_conn *conn, uint8_t *mac, uint8_t connected)
{
	struct ble_reg_manager *le_mgr;

	if (!conn) {
		SYS_LOG_ERR("param err");
	}

	os_mutex_lock(&ble_mgr_lock, OS_FOREVER);

	SYS_SLIST_FOR_EACH_CONTAINER(&ble_list, le_mgr, node) {
		if (le_mgr->link_cb) {
			le_mgr->link_cb(conn, BT_CONN_TYPE_LE, mac, connected);
		}
	}

	os_mutex_unlock(&ble_mgr_lock);
}


static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[13];
	struct bt_conn_info info;
	struct ble_mgr_dev *dev = NULL;

	if ((hostif_bt_conn_get_info(conn, &info) < 0) ||
	    (info.type != BT_CONN_TYPE_LE) ||
	    (info.role != BT_CONN_ROLE_SLAVE)) {
		return;
	}

	os_sched_lock();
	dev = alloc_ble_dev_info();
	if ((dev) && (!dev->ble_conn)) {
		memcpy(dev->device_mac, info.le.dst->a.val, 6);
		memset(addr, 0, 13);
		hex_to_str(addr, dev->device_mac, 6);
		SYS_LOG_INF("MAC: %s", addr);
		SYS_EVENT_INF(EVENT_LE_CONNECTED, (uint32_t)conn,
						UINT8_ARRAY_TO_INT32(dev->device_mac), os_uptime_get_32());

		dev->ble_conn = hostif_bt_conn_ref(conn);
		notify_ble_connected(conn, dev->device_mac, true);

		dev->ble_current_interval = BLE_NOINIT_INTERVAL;
		dev->ble_transfer_state = 0;
		dev->ble_send_cnt = 0;
		dev->ble_pre_send_cnt = 0;
		dev->update_work_state = PARAM_UPDATE_EXCHANGE_MTU;
		os_delayed_work_submit(&dev->param_update_work, BLE_DELAY_EXCHANGE_MTU_TIME);
	}else {
		uint8_t device_mac[6];
		SYS_LOG_ERR("Already connected");
		memcpy(device_mac, info.le.dst->a.val, 6);
		memset(addr, 0, 13);
		hex_to_str(addr, device_mac, 6);
		SYS_LOG_INF("MAC: %s", addr);
		SYS_EVENT_INF(EVENT_LE_CONNECTED_ERR, (uint32_t)conn,
						UINT8_ARRAY_TO_INT32(device_mac), os_uptime_get_32());
	}

	os_sched_unlock();
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[13];
	struct bt_conn_info info;
	struct ble_mgr_dev *dev = NULL;

	if ((hostif_bt_conn_get_info(conn, &info) < 0) ||
	    (info.type != BT_CONN_TYPE_LE) ||
	    (info.role != BT_CONN_ROLE_SLAVE)) {
		return;
	}

	os_sched_lock();

	dev = get_ble_dev_info(conn);
	if (dev) {
		os_delayed_work_cancel(&dev->param_update_work);
		memset(addr, 0, sizeof(addr));
		hex_to_str(addr, dev->device_mac, 6);
		SYS_LOG_INF("MAC: %s, reason: %d", addr, reason);
		SYS_EVENT_INF(EVENT_LE_DISCONNECTED, (uint32_t)conn,
						UINT8_ARRAY_TO_INT32(dev->device_mac), reason, os_uptime_get_32());

		notify_ble_connected(conn, dev->device_mac, false);

		hostif_bt_conn_unref(dev->ble_conn);
		dev->ble_conn = NULL;
		os_sem_give(&dev->ble_ind_sem);

		/* param.options set BT_LE_ADV_OPT_ONE_TIME,
		 * need advertise again after disconnect.
		 */
#ifndef GONFIG_GMA_APP
#ifndef CONFIG_BT_ADV_MANAGER
		ble_advertise();
#endif		
#endif

	} else {
		SYS_LOG_ERR("Error conn %p", conn);
		SYS_EVENT_INF(EVENT_LE_DISCONNECTED_ERR, (uint32_t)conn, os_uptime_get_32());
	}

	os_sched_unlock();
}

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	SYS_LOG_INF("int (0x%04x, 0x%04x) lat %d to %d", param->interval_min,
		param->interval_max, param->latency, param->timeout);

	SYS_EVENT_INF(EVENT_LE_PARAM_REQ, (uint32_t)conn, param->interval_min,
					param->latency, param->timeout);

	return true;
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
			     uint16_t latency, uint16_t timeout)
{
	SYS_LOG_INF("int 0x%x lat %d to %d", interval, latency, timeout);
	SYS_EVENT_INF(EVENT_LE_PARAM_UPDATED, (uint32_t)conn, interval, latency, timeout);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.le_param_req = le_param_req,
	.le_param_updated = le_param_updated,
};

static int ble_notify_data(struct bt_conn *conn, struct bt_gatt_attr *attr, uint8_t *data, uint16_t len)
{
	int ret;

	if (!conn) {
		return -1;
	}

	// SYS_LOG_INF("conn: %p", conn);
	ret = hostif_bt_gatt_notify(conn, attr, data, len);
	if (ret < 0) {
		return ret;
	} else {
		return (int)len;
	}
}

static void ble_indicate_cb(struct bt_conn *conn, struct bt_gatt_indicate_params *attr,
			  uint8_t err)
{
	struct ble_mgr_dev *dev = NULL;

	// SYS_LOG_INF("conn: %p", conn);
	dev = get_ble_dev_info(conn);
	if (!dev) {
		SYS_LOG_ERR("param err");
		return;
	}
	os_sem_give(&dev->ble_ind_sem);
}

static int ble_indicate_data(struct bt_conn *conn, struct bt_gatt_attr *attr, uint8_t *data, uint16_t len)
{
	int ret;
	struct ble_mgr_dev *dev = NULL;

	// SYS_LOG_INF("conn: %p", conn);

	dev = get_ble_dev_info(conn);
	if (!dev) {
		SYS_LOG_ERR("param err");
		return -EIO;
	}

	if (os_sem_take(&dev->ble_ind_sem, OS_NO_WAIT) < 0) {
		return -EBUSY;
	}

	ble_ind_params.attr = attr;
	ble_ind_params.func = ble_indicate_cb;
	ble_ind_params.len = len;
	ble_ind_params.data = data;

	ret = hostif_bt_gatt_indicate(conn, &ble_ind_params);
	if (ret < 0) {
		return ret;
	} else {
		return (int)len;
	}
}

uint16_t bt_manager_get_ble_mtu(struct bt_conn *conn)
{
	return (conn) ? hostif_bt_gatt_get_mtu(conn) : 0;
}

int bt_manager_ble_send_data(struct bt_conn *conn, struct bt_gatt_attr *chrc_attr,
					struct bt_gatt_attr *des_attr, uint8_t *data, uint16_t len)
{
	struct bt_gatt_chrc *chrc = (struct bt_gatt_chrc *)(chrc_attr->user_data);

	if (!conn) {
		return -EIO;
	}

	if (len > (bt_manager_get_ble_mtu(conn) - 3)) {
		return -EFBIG;
	}

	ble_send_data_check_interval(conn);

	if (chrc->properties & BT_GATT_CHRC_NOTIFY) {
		return ble_notify_data(conn, des_attr, data, len);
	} else if (chrc->properties & BT_GATT_CHRC_INDICATE) {
		return ble_indicate_data(conn, des_attr, data, len);
	}

	/* Wait TODO */
	/* return ble_write_data(attr, data, len) */
	SYS_LOG_WRN("Wait todo");
	return -EIO;
}

void bt_manager_ble_disconnect(struct bt_conn *conn)
{
	int err;

	if (!conn) {
		SYS_LOG_ERR("param err");
	}

	// SYS_LOG_INF("conn: %p", conn);
	err = hostif_bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if (err) {
		SYS_LOG_INF("Disconnection failed (err %d)", err);
	}
}

void bt_manager_ble_disconnect_all_link(void)
{
	int err = 0;
	os_sched_lock();

	for (int i = 0; i < BLE_DEV_MAX_CNT; i++) {
		if (ble_info.dev[i].ble_conn) {
			err = hostif_bt_conn_disconnect(ble_info.dev[i].ble_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
			if (err) {
				SYS_LOG_INF("Disconnection failed (err %d)", err);
			}
		}
	}

	os_sched_unlock();
}

void bt_manager_ble_service_reg(struct ble_reg_manager *le_mgr)
{
	os_mutex_lock(&ble_mgr_lock, OS_FOREVER);

	sys_slist_append(&ble_list, &le_mgr->node);
	hostif_bt_gatt_service_register(&le_mgr->gatt_svc);

	os_mutex_unlock(&ble_mgr_lock);
}

int bt_manager_ble_set_idle_interval(uint16_t interval)
{
	if (!ble_info.initialized) {
		return -EIO;
	}
	/* Range: 0x0006 to 0x0C80 */
	if (interval < 0x0006 || interval > 0x0C80) {
		return -EIO;
	}

	ble_idle_interval = interval;

	for (int i = 0; i < BLE_DEV_MAX_CNT; i++) {
		if (ble_info.dev[i].update_work_state == PARAM_UPDATE_IDLE_STATE) {
			os_delayed_work_submit(&ble_info.dev[i].param_update_work, BLE_DELAY_UPDATE_PARAM_TIME);
		}
	}
	return 0;
}

#ifdef CONFIG_GFP_PROFILE
const uint8 MODEL_ID_ATS2875H[3] = {0x32,0xB2,0x6B};
const uint8 PRIVATE_ANTI_SPOOFING_KEY_ATS2875H[32] = {
    0x03, 0xb9, 0x3b, 0xc2, 0x21, 0x3c, 0x87, 0x40, 0x81, 0x77, 0xd2, 0x42, 0x56, 0xf2, 0x98, 0x30,
    0xb1, 0x79, 0xce, 0x54, 0xed, 0x20, 0x81, 0x27, 0x97, 0x5e, 0xb9, 0x0f, 0x2f, 0x18, 0xcd, 0x1f
};

const uint8 MODEL_ID_JBL_GO4_WHT[3] = {0xD5,0xCF,0x84};
const uint8 PRIVATE_ANTI_SPOOFING_KEY_JBL_GO4_WHT[32] = {
    0x7e, 0xd7, 0x70, 0x43, 0xa8, 0x8a, 0x73, 0xe0, 0xed, 0x37, 0x15, 0x10, 0x6d, 0xdb, 0x01, 0x39,
    0x4e, 0xea, 0xb6, 0xb3, 0x6c, 0xd4, 0x98, 0xd8, 0xbd, 0x6e, 0xa9, 0x8a, 0xa8, 0x60, 0xd5, 0xe5
};
static void _bt_manager_gfp_callback(uint16_t hdl, btsrv_gfp_event_e event, void *packet, int size)
{
	switch (event) {
	    case BTSRV_GFP_BLE_CONNECTED:
        bt_manager_event_notify(BT_GFP_BLE_CONNECTED, NULL, 0);
	    break;

	    case BTSRV_GFP_BLE_DISCONNECTED:	    
        bt_manager_event_notify(BT_GFP_BLE_DISCONNECTED, NULL, 0);
	    break;

        default:
	    break;
    }
}
#endif

void bt_manager_ble_init(void)
{
#ifdef CONFIG_GFP_PROFILE
	struct btsrv_gfp_start_param param;
#endif

	SYS_LOG_INF("\n");	
	memset(&ble_info, 0, sizeof(ble_info));
	ble_idle_interval = BLE_DEFAULT_IDLE_INTERVAL;

	ble_info.initialized = 1;

    ble_info.adv_power = CONFIG_BT_CONTROLER_BLE_MAX_TXPOWER;

	sys_slist_init(&ble_list);
	for (int i = 0; i < BLE_DEV_MAX_CNT; i++) {
		ble_info.dev[i].ble_current_interval = BLE_NOINIT_INTERVAL;
		os_delayed_work_init(&ble_info.dev[i].param_update_work, param_update_work_callback);
		os_sem_init(&ble_info.dev[i].ble_ind_sem, 1, 1);
	}

	hostif_bt_conn_cb_register(&conn_callbacks);

#ifdef CONFIG_GFP_PROFILE
	memset(&param, 0, sizeof(param));
	param.cb = &_bt_manager_gfp_callback;
#ifndef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	// param.module_id = (uint8_t *)MODEL_ID_ATS2875H;
	// param.private_anti_spoofing_key = (uint8_t *)PRIVATE_ANTI_SPOOFING_KEY_ATS2875H;
    param.module_id = (uint8_t *)MODEL_ID_JBL_GO4_WHT;
    param.private_anti_spoofing_key = (uint8_t *)PRIVATE_ANTI_SPOOFING_KEY_JBL_GO4_WHT;

    btif_gfp_start(&param);
#else
	int result = 0;
	int gfp_model_id_convert_index;
	u8_t model_id[3] = {0x49,0x32,0x7D};

	//result = get_gfp_model_id_from_ats(model_id);

	if (!result)
	{
		gfp_model_id_convert_index = gfp_model_id_convert_to_index((const u8_t *)model_id);

		if (gfp_model_id_convert_index == -1)
		{
			SYS_LOG_ERR("gfp:convert_index get fail\n");
			goto __default_gfp_param_init;
		}

		SYS_LOG_INF("gfp: param init ok\n");
		param.module_id = (uint8_t *)get_gfp_model_id_convert_search_table()[gfp_model_id_convert_index].model_id;
		param.private_anti_spoofing_key = (uint8_t *)get_gfp_model_id_convert_search_table()[gfp_model_id_convert_index].private_key_base64;

		SYS_LOG_INF("gfp:model_id:0x%02X_%02X_%02X\n", param.module_id[0], param.module_id[1], param.module_id[2]);
		SYS_LOG_INF("gfp:private_key_base64\n");
		SYS_LOG_INF("gfp:private_key_base64:0x%02X_%02X_%02X_%02X_%02X_%02X\n", 
			param.private_anti_spoofing_key[0], param.private_anti_spoofing_key[1], param.private_anti_spoofing_key[2],
			param.private_anti_spoofing_key[3], param.private_anti_spoofing_key[4], param.private_anti_spoofing_key[5]);
		
		// print_buffer(param.private_anti_spoofing_key, 1, 32, 16, 0);

		btif_gfp_start(&param);
	}
	else 
	{
		SYS_LOG_ERR("gfp:get model id err from nvram(ats)\n");

__default_gfp_param_init:
		SYS_LOG_INF("gfp:default param init\n");

		param.module_id = (uint8_t *)get_gfp_model_id_convert_search_table()[0].model_id;
		param.private_anti_spoofing_key = (uint8_t *)get_gfp_model_id_convert_search_table()[0].private_key_base64;
		btif_gfp_start(&param);
	}
#endif
#endif

#ifndef CONFIG_BT_ADV_MANAGER
#ifndef GONFIG_GMA_APP
	ble_advertise();
#endif
#endif
}

void bt_manager_ble_deinit(void)
{
	uint32_t time_out = 0;
	struct bt_conn *ble_conn = NULL;

	hostif_bt_le_adv_stop();

	ble_info.initialized = 0;

	for (int i = 0; i < BLE_DEV_MAX_CNT; i++) {
		ble_conn = ble_info.dev[i].ble_conn;
		if (ble_conn) {
			bt_manager_ble_disconnect(ble_conn);
			while (ble_conn && time_out++ < 500) {
				os_sleep(10);
			}
		}
		k_delayed_work_cancel_sync(&ble_info.dev[i].param_update_work, K_FOREVER);
	}
	hostif_bt_conn_cb_unregister(&conn_callbacks);

	SYS_LOG_INF("\n");
}

void bt_manager_ble_a2dp_play_notify(bool play)
{
	ble_info.br_a2dp_runing = (play) ? 1 : 0;

	for (int i = 0; i < BLE_DEV_MAX_CNT; i++) {
		if (ble_info.dev[i].ble_conn) {
			ble_check_update_param(ble_info.dev[i].ble_conn);
		}
	}
}

void bt_manager_ble_hfp_play_notify(bool play)
{
	ble_info.br_hfp_runing = (play) ? 1 : 0;

	for (int i = 0; i < BLE_DEV_MAX_CNT; i++) {
		if (ble_info.dev[i].ble_conn) {
			ble_check_update_param(ble_info.dev[i].ble_conn);
		}
	}
}

void bt_manager_halt_ble(void)
{
	/* If ble connected, what todo */

	if (!ble_info.initialized) {
		return;
	}

	ble_info.adv_halt = 1;
	hostif_bt_le_adv_stop();
	bt_manager_le_audio_adv_disable();
	ble_info.adv_enable = 0;
	SYS_LOG_INF("");

}

#ifdef CONFIG_BT_ADV_MANAGER
static uint8_t ble_format_adv_data(struct bt_data *ad, uint8_t *ad_data)
{
	uint8_t ad_len = 0;
	uint8_t i, pos = 0;

	for (i = 0; i < BLE_ADV_MAX_ARRAY_SIZE; i++) {
		if (ad_data[pos] == 0) {
			break;
		}

		if ((pos + ad_data[pos] + 1) > BLE_ADV_DATA_MAX_LEN) {
			break;
		}
		//SYS_LOG_INF("%d %d %d\n", ad_len,pos, ad_data[pos]);
		//SYS_LOG_INF("%d %d\n", pos, ad_data[pos+1]);
		//SYS_LOG_INF("%d %d\n", pos, ad_data[pos+2]);

		ad[ad_len].data_len = ad_data[pos] - 1;
		ad[ad_len].type = ad_data[pos + 1];
		ad[ad_len].data = &ad_data[pos + 2];
		ad_len++;

		pos += (ad_data[pos] + 1);
	}

	return ad_len;
}

#ifdef CONFIG_BT_LETWS
static bool gfp_ble_advertise_data(void)
{
	struct bt_le_adv_param *param;
	struct bt_data ad[BLE_ADV_MAX_ARRAY_SIZE], sd[BLE_ADV_MAX_ARRAY_SIZE];
	size_t ad_len, sd_len;
	int err = 0;

	ad_len = ble_format_adv_data((struct bt_data *)ad, ble_info.ad_data);
	sd_len = ble_format_adv_data((struct bt_data *)sd, ble_info.sd_data);

	struct bt_le_adv_param adv_param = {0};
	param = bt_manager_lea_policy_get_adv_param(ADV_TYPE_LEGENCY, &adv_param);
	if (param) {
		ctrl_set_le_legacy_adv_tx_pwr(ble_info.adv_power);
		err = hostif_bt_le_adv_start(param, (ad_len ? (const struct bt_data *)ad : NULL), ad_len,
									(sd_len ? (const struct bt_data *)sd : NULL), sd_len);
		if (err < 0 && err != (-EALREADY)) {
			SYS_LOG_ERR("Failed to start advertising (err %d)", err);
			return false;
		} else {
		//	SYS_LOG_INF("Advertising started");
		}

		return true;
	}

	return false;
}

static bool ble_advertise_data(void)
{
	struct bt_le_adv_param param;
	struct bt_data ad[BLE_ADV_MAX_ARRAY_SIZE], sd[BLE_ADV_MAX_ARRAY_SIZE];
	size_t ad_len, sd_len;
	int err = 0;

	ad_len = ble_format_adv_data((struct bt_data *)ad, ble_info.ad_data);
	sd_len = ble_format_adv_data((struct bt_data *)sd, ble_info.sd_data);

	memset(&param, 0, sizeof(param));
	param.id = BT_ID_DEFAULT;
	param.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
	param.interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
	param.options = (BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_IDENTITY);

	ctrl_set_le_legacy_adv_tx_pwr(ble_info.adv_power);
	err = hostif_bt_le_adv_start(&param, (ad_len ? (const struct bt_data *)ad : NULL), ad_len,
								(sd_len ? (const struct bt_data *)sd : NULL), sd_len);
	if (err < 0 && err != (-EALREADY)) {
		SYS_LOG_ERR("Failed to start advertising (err %d)", err);
		return false;
	}

	return true;
}
#else
static bool ble_advertise_data(void)
{
	struct bt_le_adv_param *param;
	struct bt_data ad[BLE_ADV_MAX_ARRAY_SIZE], sd[BLE_ADV_MAX_ARRAY_SIZE];
	size_t ad_len, sd_len;
	int err = 0;

	ad_len = ble_format_adv_data((struct bt_data *)ad, ble_info.ad_data);
	sd_len = ble_format_adv_data((struct bt_data *)sd, ble_info.sd_data);

	struct bt_le_adv_param adv_param = {0};
	param = bt_manager_lea_policy_get_adv_param(ADV_TYPE_LEGENCY, &adv_param);
	if (param) {
		ctrl_set_le_legacy_adv_tx_pwr(ble_info.adv_power);
		err = hostif_bt_le_adv_start(param, (ad_len ? (const struct bt_data *)ad : NULL), ad_len,
									(sd_len ? (const struct bt_data *)sd : NULL), sd_len);
		if (err < 0 && err != (-EALREADY)) {
			SYS_LOG_ERR("Failed to start advertising (err %d)", err);
			return false;
		} else {
		//	SYS_LOG_INF("Advertising started");
		}

		return true;
	}

	return false;
}
#endif

int bt_manager_ble_set_adv_data(uint8_t *ad_data, uint8_t ad_data_len, uint8_t *sd_data, uint8_t sd_data_len,int8_t power)
{
	if (!ble_info.initialized) {
		return -EIO;
	}
	
	if (ad_data && ad_data_len <= BLE_ADV_DATA_MAX_LEN) {
		memset(ble_info.ad_data, 0, BLE_ADV_DATA_MAX_LEN);
		memcpy(ble_info.ad_data, ad_data, ad_data_len);
	}

	if (0 == sd_data_len)
	{
		memset(ble_info.sd_data, 0, BLE_ADV_DATA_MAX_LEN);
	}

	
	if (sd_data && sd_data_len <= BLE_ADV_DATA_MAX_LEN) {
		memset(ble_info.sd_data, 0, BLE_ADV_DATA_MAX_LEN);
		memcpy(ble_info.sd_data, sd_data, sd_data_len);
	}

    ble_info.adv_power = power;

	return 0;
}

int bt_manager_ble_set_advertise_enable(uint8_t enable)
{
	if (!ble_info.initialized) {
		return -EIO;
	}

	if(ble_info.adv_halt) {
		return 0;
	}

	if (enable) {
		ble_info.adv_enable = ble_advertise_data();
	} else if (ble_info.adv_enable) {
		hostif_bt_le_adv_stop();
		ble_info.adv_enable = 0;
	}

	return 0;
}

int bt_manager_ble_gfp_set_advertise_enable(uint8_t enable)
{
	if (!ble_info.initialized) {
		return -EIO;
	}

	if(ble_info.adv_halt) {
		return 0;
	}

	if (enable) {
#ifdef CONFIG_BT_LETWS
		ble_info.adv_enable = gfp_ble_advertise_data();
#else
		ble_info.adv_enable = ble_advertise_data();
#endif
	} else if (ble_info.adv_enable) {
		hostif_bt_le_adv_stop();
		ble_info.adv_enable = 0;
	}

	return 0;
}

int bt_manager_ble_is_connected(void)
{
	uint8_t ble_connect = 0;

	for (int i = 0; i < BLE_DEV_MAX_CNT; i++) {
		if (ble_info.dev[i].ble_conn) {
			ble_connect = 1;
			break;
		}
	}
	return ble_connect;
}

int bt_manager_ble_max_connected(void)
{
	uint8_t ble_connect_cnt = 0;

	for (int i = 0; i < BLE_DEV_MAX_CNT; i++) {
		if (ble_info.dev[i].ble_conn) {
			ble_connect_cnt++;
		}
	}
	return (ble_connect_cnt >= BLE_DEV_MAX_CNT)? 1 : 0;
}
#endif

void bt_manager_resume_ble(void)
{
	/* If ble connected, what todo */
	if (!ble_info.initialized) {
		return;
	}

	ble_info.adv_halt = 0;

#ifndef CONFIG_BT_ADV_MANAGER
	if (!bt_manager_ble_is_connected()) {
		//#ifdef CONFIG_BT_ADV_MANAGER
		//ble_advertise_data();
		ble_advertise();
		bt_manager_le_audio_adv_enable();
		ble_info.adv_enable = 0;
	}
#endif
	SYS_LOG_INF("");

}

void bt_manager_check_per_adv_synced(void)
{
	int err;
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	
	SYS_LOG_INF(":%d,%d\n",bt_manager->per_synced_count,bt_manager->total_per_sync_count);
	if (bt_manager->total_per_sync_count == bt_manager->per_synced_count) {
        err = hostif_bt_le_scan_stop();
        if (err) {
	        printk("err: %d", err);
        }
	}
}


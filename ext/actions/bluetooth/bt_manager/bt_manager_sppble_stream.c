/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager genarate bt mac and name.
 */
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <msg_manager.h>
#include <mem_manager.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <stream.h>
#include <acts_bluetooth/host_interface.h>
#include <hex_str.h>

#ifdef CONFIG_ACT_EVENT
#include <bt_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_REGISTER(bt_sppble_stream, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#define MAX_SPPBLE_STREAM		(5)
#define SPPBLE_BUFF_SIZE		(1024*2)
#define SPPBLE_SEND_LEN_ONCE	(512)
#define SPPBLE_SEND_INTERVAL	(5)		/* 5ms */

struct sppble_info_t {
	uint8_t *spp_uuid;
	struct ble_reg_manager le_mgr;
	struct bt_gatt_attr	*tx_chrc_attr;
	struct bt_gatt_attr	*tx_attr;
	struct bt_gatt_attr	*tx_ccc_attr;
	struct bt_gatt_attr	*rx_attr;
	void (*connect_cb)(int connected, uint8_t connect_type, void *param);
	bool (*connect_filter_cb)(void *param, uint8_t connect_type);
	uint8_t connect_type;
	uint8_t spp_chl;
	uint8_t notify_ind_enable;
	int32_t read_timeout;
	int32_t write_timeout;
	int32_t read_buf_size;
	uint8_t *buff;
	os_mutex read_mutex;
	os_sem read_sem;
	os_mutex write_mutex;
	struct bt_conn *conn;
	void(*rxdata_cb)(void);
	uint8_t write_attr_enable_ccc;
};

static void sppble_rx_data(io_stream_t handle, uint8_t *buf, uint16_t len);
static ssize_t stream_ble_rx_set_notifyind(struct bt_conn *conn, uint8_t conn_type,
												const struct bt_gatt_attr *attr, uint16_t value);

static io_stream_t sppble_create_stream[MAX_SPPBLE_STREAM] __IN_BT_SECTION;
static OS_MUTEX_DEFINE(g_sppble_mutex);

static int sppble_add_stream(io_stream_t handle)
{
	int i;

	os_mutex_lock(&g_sppble_mutex, OS_FOREVER);
	for (i = 0; i < MAX_SPPBLE_STREAM; i++) {
		if (sppble_create_stream[i] == NULL) {
			sppble_create_stream[i] = handle;
			break;
		}
	}
	os_mutex_unlock(&g_sppble_mutex);

	if (i == MAX_SPPBLE_STREAM) {
		SYS_LOG_ERR("Failed to add stream handle %p", handle);
		return -EIO;
	}

	return 0;
}

static int sppble_remove_stream(io_stream_t handle)
{
	int i;

	os_mutex_lock(&g_sppble_mutex, OS_FOREVER);
	for (i = 0; i < MAX_SPPBLE_STREAM; i++) {
		if (sppble_create_stream[i] == handle) {
			sppble_create_stream[i] = NULL;
			break;
		}
	}
	os_mutex_unlock(&g_sppble_mutex);

	if (i == MAX_SPPBLE_STREAM) {
		SYS_LOG_ERR("Failed to remove stream handle %p", handle);
		return -EIO;
	}

	return 0;
}

static io_stream_t find_stream_by_spp_uuid(uint8_t *uuid)
{
	io_stream_t stream;
	struct sppble_info_t *info;
	int i;

	os_mutex_lock(&g_sppble_mutex, OS_FOREVER);

	for (i = 0; i < MAX_SPPBLE_STREAM; i++) {
		stream = sppble_create_stream[i];
		if (stream) {
			info = (struct sppble_info_t *)stream->data;
			if (!memcmp(info->spp_uuid, uuid, SPP_UUID_LEN)) {
				os_mutex_unlock(&g_sppble_mutex);
				return stream;
			}
		}
	}

	os_mutex_unlock(&g_sppble_mutex);
	return NULL;
}

static io_stream_t find_stream_by_spp_channel(uint8_t channel)
{
	io_stream_t stream;
	struct sppble_info_t *info;
	int i;

	os_mutex_lock(&g_sppble_mutex, OS_FOREVER);

	for (i = 0; i < MAX_SPPBLE_STREAM; i++) {
		stream = sppble_create_stream[i];
		if (stream) {
			info = (struct sppble_info_t *)stream->data;
			if (info->spp_chl == channel) {
				os_mutex_unlock(&g_sppble_mutex);
				return stream;
			}
		}
	}

	os_mutex_unlock(&g_sppble_mutex);
	return NULL;
}

static io_stream_t bond_stream_by_attr_and_conn(const struct bt_gatt_attr *attr, struct bt_conn *conn)
{
	io_stream_t stream;
	struct sppble_info_t *info;
	int i;

	os_mutex_lock(&g_sppble_mutex, OS_FOREVER);

	for (i = 0; i < MAX_SPPBLE_STREAM; i++) {
		stream = sppble_create_stream[i];
		if (stream) {
			info = (struct sppble_info_t *)stream->data;
			if ((info->conn == NULL) &&
				((info->tx_chrc_attr == attr) || (info->tx_attr == attr) ||
				 (info->tx_ccc_attr == attr) || (info->rx_attr == attr))) {
				os_mutex_unlock(&g_sppble_mutex);
				return stream;
			}
		}
	}

	os_mutex_unlock(&g_sppble_mutex);

	return NULL;
}


static io_stream_t find_stream_by_attr_and_conn(const struct bt_gatt_attr *attr, struct bt_conn *conn)
{
	io_stream_t stream;
	struct sppble_info_t *info;
	int i;

	os_mutex_lock(&g_sppble_mutex, OS_FOREVER);

	for (i = 0; i < MAX_SPPBLE_STREAM; i++) {
		stream = sppble_create_stream[i];
		if (stream) {
			info = (struct sppble_info_t *)stream->data;
			if ((info->conn == conn) &&
				((info->tx_chrc_attr == attr) || (info->tx_attr == attr) ||
				 (info->tx_ccc_attr == attr) || (info->rx_attr == attr))) {
				os_mutex_unlock(&g_sppble_mutex);
				return stream;
			}
		}
	}

	os_mutex_unlock(&g_sppble_mutex);

	return NULL;
}

static int stream_connect_filter_check(struct bt_conn *conn, uint8_t connect_type)
{
	io_stream_t stream;
	struct sppble_info_t *info;
	int i;
	int ret = -1;

	os_mutex_lock(&g_sppble_mutex, OS_FOREVER);

	for (i = 0; i < MAX_SPPBLE_STREAM; i++) {
		stream = sppble_create_stream[i];
		if(stream){
			info = (struct sppble_info_t *)stream->data;
			if (info->connect_filter_cb) {
				//ota running,disallowed new connect
				//gatt_over_br already connected, reject second
				if(info->connect_filter_cb((void *)conn, connect_type)){
					ret = 0;
					goto exit;
				}
			}
		}
	}

exit:
	os_mutex_unlock(&g_sppble_mutex);

	return ret;
}

static void stream_spp_connected_cb(uint8_t channel, uint8_t *uuid)
{
	struct sppble_info_t *info;
	io_stream_t handle = find_stream_by_spp_uuid(uuid);

	SYS_LOG_INF("channel:%d", channel);
	SYS_EVENT_INF(EVENT_STREAM_SPP_CONNECT_CB, channel, handle,
					(uuid[10]<<24)|(uuid[11]<<16)|(uuid[12]<<8)|(uuid[13]));

	if (handle && handle->data) {
		info = (struct sppble_info_t *)handle->data;
		info->spp_chl = channel;
		if (info->connect_type == NONE_CONNECT_TYPE) {
			info->connect_type = SPP_CONNECT_TYPE;
			if (info->connect_cb) {
				info->connect_cb(true, info->connect_type, (void *)((int)channel));
			}
		} else {
			SYS_LOG_WRN("Had connecte: %d", info->connect_type);
		}
	}
}

static void stream_spp_disconnected_cb(uint8_t channel)
{
	struct sppble_info_t *info;
	io_stream_t handle = find_stream_by_spp_channel(channel);

	SYS_LOG_INF("channel:%d", channel);
	SYS_EVENT_INF(EVENT_STREAM_SPP_DISCONNECT_CB, channel, handle);

	if (handle && handle->data) {
		info = (struct sppble_info_t *)handle->data;
		if (info->connect_type == SPP_CONNECT_TYPE) {
			info->connect_type = NONE_CONNECT_TYPE;
			os_sem_give(&info->read_sem);
			if (info->connect_cb) {
				info->connect_cb(false, info->connect_type, (void *)((int)channel));
			}
		} else {
			SYS_LOG_WRN("Not connecte spp: %d", info->connect_type);
		}
	}
}

static void stream_spp_receive_data_cb(uint8_t channel, uint8_t *data, uint32_t len)
{
	struct sppble_info_t *info;
	io_stream_t handle = find_stream_by_spp_channel(channel);

	SYS_LOG_DBG("channel:%d, len:%d", channel, len);
	if (handle && handle->data) {
		info = (struct sppble_info_t *)handle->data;
		if (info->connect_type == SPP_CONNECT_TYPE) {
			sppble_rx_data(handle, data, len);
		}
	}
}

static const struct btmgr_spp_cb stream_spp_cb = {
	.connected = stream_spp_connected_cb,
	.disconnected = stream_spp_disconnected_cb,
	.receive_data = stream_spp_receive_data_cb,
};

static ssize_t stream_ble_rx_data(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr,
			      const void *buf, uint16_t len, uint16_t offset,
			      uint8_t flags)
{
	uint8_t conn_type;
	struct sppble_info_t *info;
	io_stream_t stream = find_stream_by_attr_and_conn(attr, conn);

	if(!stream){
		stream = bond_stream_by_attr_and_conn(attr, conn);
		if(!stream){
			SYS_LOG_WRN("should not happen\n");
			return 0;
		}
	}

	SYS_LOG_DBG("att rx data len %d, %p", len, conn);
	if (stream && stream->data) {
		info = (struct sppble_info_t *)stream->data;

		if ((info->write_attr_enable_ccc) &&
			!(info->notify_ind_enable)) {
			conn_type = hostif_bt_conn_get_type(conn);
			stream_ble_rx_set_notifyind(conn, conn_type, attr, 1);
		}
		if ((info->connect_type == BLE_CONNECT_TYPE) ||
			(info->connect_type == GATT_OVER_BR_CONNECT_TYPE)) {
			sppble_rx_data(stream, (uint8_t *)buf, len);
		}
	}

	return len;
}

static const uint8_t * conn_type_str[4] =
{
    "NONE",
    "BLE",
    "BR",
    "OTHERS",
};

static const uint8_t * connect_type_str[4] =
{
	"NONE",
	"SPP",
	"BLE",
	"GATT_OVER_BR",
};

static ssize_t stream_ble_rx_set_notifyind(struct bt_conn *conn, uint8_t conn_type,
												const struct bt_gatt_attr *attr, uint16_t value)
{
	uint8_t connect_type;
	struct sppble_info_t *info;
	io_stream_t stream = NULL;
    uint8_t conn_type_index = conn_type;

	connect_type = (conn_type == BT_CONN_TYPE_BR) ? GATT_OVER_BR_CONNECT_TYPE : BLE_CONNECT_TYPE;
	if(stream_connect_filter_check(conn, connect_type) >=0) {
		SYS_LOG_INF("connect_filter\n");
		return sizeof(uint16_t);
	}else {
		stream = find_stream_by_attr_and_conn(attr, conn);
		if(!stream){
			stream = bond_stream_by_attr_and_conn(attr, conn);
			if(!stream){
				SYS_LOG_WRN("should not happen\n");
				return sizeof(uint16_t);
			}
		}
	}

	SYS_LOG_INF("conn: %p, attr:%p, enable:%d", conn, attr, value);

	info = (struct sppble_info_t *)stream->data;

    if(conn_type_index > BT_CONN_TYPE_BR){
        conn_type_index = 3;
    }

	SYS_LOG_INF("conn_type:%s,connect_type:%s", conn_type_str[conn_type_index],connect_type_str[info->connect_type]);

	if (stream && stream->data) {
		if (value) {
			if (info->connect_type == NONE_CONNECT_TYPE) {
				info->connect_type = connect_type;
				info->notify_ind_enable = 1;
				info->conn = conn;
				if (info->connect_cb) {
					info->connect_cb(true, info->connect_type, (void *)stream);
				}
			}else {
				SYS_LOG_WRN("Had connected:%s", connect_type_str[info->connect_type]);
			}
		}else {
			if ((info->connect_type == BLE_CONNECT_TYPE) ||
				(info->connect_type == GATT_OVER_BR_CONNECT_TYPE)) {
				info->notify_ind_enable = 0;
				info->connect_type = NONE_CONNECT_TYPE;
				info->conn = NULL;
				if (info->connect_cb) {
					info->connect_cb(false, info->connect_type, (void *)stream);
				}
				os_sem_give(&info->read_sem);
			}
		}
	}

	return sizeof(uint16_t);
}

void stream_ble_connect_cb(struct bt_conn *conn, uint8_t conn_type, uint8_t *mac, uint8_t connected)
{
	io_stream_t stream;
	struct sppble_info_t *info;
	int i;
    char addr[13];
    uint8_t conn_type_index;
    memset(addr, 0, 13);
    hex_to_str(addr, mac, 6);

    conn_type_index = conn_type;
    if(conn_type_index > BT_CONN_TYPE_BR){
        conn_type_index = 3;
    }

	SYS_LOG_INF("connected:%d addr:0x%s conn_type:%s", connected,addr,conn_type_str[conn_type_index]);

	SYS_EVENT_INF(EVENT_STREAM_LE_CONNECT_CB, (mac[3]<<16)|(mac[4]<<8)|mac[5], connected);

	os_mutex_lock(&g_sppble_mutex, OS_FOREVER);
	if (!connected) {
		for (i = 0; i < MAX_SPPBLE_STREAM; i++) {
			stream = sppble_create_stream[i];
			if (stream) {
				info = (struct sppble_info_t *)stream->data;
				if ((info->conn) && (info->conn == conn)) {
					if ((conn_type == BT_CONN_TYPE_LE) || (conn_type == BT_CONN_TYPE_BR)) {
						SYS_LOG_INF("conn_type:%s,connect_type:%s", conn_type_str[conn_type], connect_type_str[info->connect_type]);
						if (info->connect_cb) {
							info->connect_cb(false, info->connect_type, (void *)stream);
						}
					}
					info->notify_ind_enable = 0;
					info->connect_type = NONE_CONNECT_TYPE;
					info->conn = NULL;
					os_sem_give(&info->read_sem);
				}
			}
		}
	}else {
		if(stream_connect_filter_check(conn, NONE_CONNECT_TYPE) >=0) {
			SYS_LOG_INF("connect_filter\n");
			goto exit;
		}
    }

exit:
	os_mutex_unlock(&g_sppble_mutex);
}

static int sppble_register(struct sppble_info_t *info)
{
	struct _bt_gatt_ccc *ccc;
	int ret;

	ret = bt_manager_spp_reg_uuid(info->spp_uuid, (struct btmgr_spp_cb *)&stream_spp_cb);
	if (ret < 0) {
		if (ret == -EALREADY) {
			SYS_LOG_INF("Already register spp uuid");
			return 0;
		}else {
			SYS_LOG_ERR("Failed register spp uuid");
			return ret;
		}
	}

	if (info->tx_ccc_attr)
	{
		info->rx_attr->write = stream_ble_rx_data;
		ccc = (struct _bt_gatt_ccc *)info->tx_ccc_attr->user_data;
		ccc->cfg_write = stream_ble_rx_set_notifyind;
		info->le_mgr.link_cb = stream_ble_connect_cb;
#ifdef CONFIG_BT_BLE
		bt_manager_ble_service_reg(&info->le_mgr);
#endif
	}
	return 0;
}

static int sppble_init(io_stream_t handle, void *param)
{
	int ret = 0;
	struct sppble_info_t *info = NULL;
	struct sppble_stream_init_param *init_param = (struct sppble_stream_init_param *)param;

	if (sppble_add_stream(handle)) {
		ret = -EIO;
		goto err_exit;
	}

	info = mem_malloc(sizeof(struct sppble_info_t));
	if (!info) {
		SYS_LOG_ERR("cache stream info malloc failed\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	memset(info, 0, sizeof(struct sppble_info_t));
	info->spp_uuid = init_param->spp_uuid;
	info->le_mgr.gatt_svc.attrs = init_param->gatt_attr;
	info->le_mgr.gatt_svc.attr_count = init_param->attr_size;
	info->tx_chrc_attr = init_param->tx_chrc_attr;
	info->tx_attr = init_param->tx_attr;
	info->tx_ccc_attr = init_param->tx_ccc_attr;
	info->rx_attr = init_param->rx_attr;
	info->connect_cb = init_param->connect_cb;
	info->connect_filter_cb = init_param->connect_filter_cb;
	info->read_timeout = init_param->read_timeout;
	info->write_timeout = init_param->write_timeout;
	info->read_buf_size = init_param->read_buf_size;
	info->write_attr_enable_ccc = init_param->write_attr_enable_ccc;
	if (info->read_buf_size == 0) {
		info->read_buf_size = SPPBLE_BUFF_SIZE;
	}
	os_mutex_init(&info->read_mutex);
	os_sem_init(&info->read_sem, 0, 1);
	os_mutex_init(&info->write_mutex);

	handle->data = info;

	if (sppble_register(info)) {
		ret = -EIO;
		goto err_exit;
	}

	return 0;

err_exit:
	if (info) {
		mem_free(info);
	}

	sppble_remove_stream(handle);
	return ret;
}

static int sppble_open(io_stream_t handle, stream_mode mode)
{
	struct sppble_info_t *info = NULL;

	info = (struct sppble_info_t *)handle->data;
	if (info->connect_type == NONE_CONNECT_TYPE) {
		return -EIO;
	}

	os_mutex_lock(&info->read_mutex, OS_FOREVER);
	info->buff = mem_malloc(info->read_buf_size);
	if (!info->buff) {
		os_mutex_unlock(&info->read_mutex);
		SYS_LOG_ERR("Failed to malloc");
		return -ENOMEM;
	}

	handle->total_size = SPPBLE_BUFF_SIZE;
	handle->cache_size = 0;
	handle->rofs = 0;
	handle->wofs = 0;
	os_mutex_unlock(&info->read_mutex);

	return 0;
}

static void sppble_rx_data(io_stream_t handle, uint8_t *buf, uint16_t len)
{
	struct sppble_info_t *info = NULL;
	uint16_t w_len, r_len;

	info = (struct sppble_info_t *)handle->data;
	os_mutex_lock(&info->read_mutex, OS_FOREVER);
	if ((handle->cache_size + len) <= handle->total_size) {
		if ((handle->wofs + len) > handle->total_size) {
			w_len = handle->total_size - handle->wofs;
			memcpy(&info->buff[handle->wofs], &buf[0], w_len);
			r_len = len - w_len;
			memcpy(&info->buff[0], &buf[w_len], r_len);
			handle->wofs = r_len;
		} else {
			memcpy(&info->buff[handle->wofs], buf, len);
			handle->wofs += len;
		}

		handle->cache_size += len;
		os_sem_give(&info->read_sem);

		if(info->rxdata_cb){
			info->rxdata_cb();
		}
	} else {
		SYS_LOG_WRN("Not enough buffer: %d, %d, %d", handle->cache_size, len, handle->total_size);
	}
	os_mutex_unlock(&info->read_mutex);
}

static int sppble_read(io_stream_t handle, uint8_t *buf, int num)
{
	struct sppble_info_t *info = NULL;
	uint16_t r_len, rr_len;

	info = (struct sppble_info_t *)handle->data;

	os_mutex_lock(&info->read_mutex, OS_FOREVER);
	if ((info->connect_type == NONE_CONNECT_TYPE) ||
		(info->buff == NULL)) {
		os_mutex_unlock(&info->read_mutex);
		return -EIO;
	}

	if ((info->connect_type != NONE_CONNECT_TYPE) && (handle->cache_size == 0) &&
		(info->read_timeout != OS_NO_WAIT)) {
		os_sem_reset(&info->read_sem);
		os_mutex_unlock(&info->read_mutex);

		os_sem_take(&info->read_sem, info->read_timeout);
		os_mutex_lock(&info->read_mutex, OS_FOREVER);
	}

	if (handle->cache_size == 0) {
		os_mutex_unlock(&info->read_mutex);
		return 0;
	}

	r_len = (handle->cache_size > num) ? num : handle->cache_size;
	if ((handle->rofs + r_len) > handle->total_size) {
		rr_len = handle->total_size - handle->rofs;
		if (buf) {
			memcpy(&buf[0], &info->buff[handle->rofs], rr_len);
			memcpy(&buf[rr_len], &info->buff[0], (r_len - rr_len));
		}
		handle->cache_size -= r_len;
		handle->rofs = r_len - rr_len;
	} else {
		if (buf) {
			memcpy(&buf[0], &info->buff[handle->rofs], r_len);
		}
		handle->cache_size -= r_len;
		handle->rofs += r_len;
	}

	os_mutex_unlock(&info->read_mutex);
	return r_len;
}

static int sppble_tell(io_stream_t handle)
{
	int ret = 0;
	struct sppble_info_t *info = NULL;

	info = (struct sppble_info_t *)handle->data;
	if (info->connect_type == NONE_CONNECT_TYPE) {
		return -EIO;
	}

	os_mutex_lock(&info->read_mutex, OS_FOREVER);
	ret = handle->cache_size;
	os_mutex_unlock(&info->read_mutex);

	return ret;
}

static int sppble_spp_send_data(struct sppble_info_t *info, uint8_t *buf, int num)
{
	int send_len = 0, w_len;
	int32_t timeout = 0;

	while ((info->connect_type == SPP_CONNECT_TYPE) && (send_len < num)) {
		w_len = ((num - send_len) > SPPBLE_SEND_LEN_ONCE) ? SPPBLE_SEND_LEN_ONCE : (num - send_len);
		if (bt_manager_spp_send_data(info->spp_chl, &buf[send_len], w_len) > 0) {
			send_len += w_len;
		} else {
			if (info->write_timeout == OS_NO_WAIT) {
				break;
			} else if (info->write_timeout == OS_FOREVER) {
				os_sleep(SPPBLE_SEND_INTERVAL);
			} else {
				if (timeout >= info->write_timeout) {
					break;
				}

				timeout += SPPBLE_SEND_INTERVAL;
				os_sleep(SPPBLE_SEND_INTERVAL);
			}
		}
	}

	return send_len;
}
#ifdef CONFIG_BT_BLE

extern int bt_manager_gatt_over_br_send_data(struct bt_conn *conn, struct bt_gatt_attr *chrc_attr,
							struct bt_gatt_attr *des_attr, uint8_t *data, uint16_t len);

static int sppble_ble_send_data(struct sppble_info_t *info, uint8_t *buf, int num)
{
	uint16_t mtu;
	int send_len = 0, w_len, le_send, cur_len;
	int ret = 0;
	int32_t timeout = 0;

	while ( ((info->connect_type == BLE_CONNECT_TYPE) || (info->connect_type == GATT_OVER_BR_CONNECT_TYPE))
			&& (info->notify_ind_enable) && (send_len < num) ) {
		w_len = ((num - send_len) > SPPBLE_SEND_LEN_ONCE) ? SPPBLE_SEND_LEN_ONCE : (num - send_len);

		if(info->connect_type == GATT_OVER_BR_CONNECT_TYPE) {
			mtu = bt_manager_get_gatt_over_br_mtu(info->conn) - 3;
		}else {
			mtu = bt_manager_get_ble_mtu(info->conn) - 3;
		}
		le_send = 0;
		while (le_send < w_len) {
			cur_len = ((w_len - le_send) > mtu) ? mtu : (w_len - le_send);
			if(info->connect_type == GATT_OVER_BR_CONNECT_TYPE) {
				ret = bt_manager_gatt_over_br_send_data(info->conn, info->tx_chrc_attr, info->tx_attr, &buf[send_len], cur_len);
			}else {
				ret = bt_manager_ble_send_data(info->conn, info->tx_chrc_attr, info->tx_attr, &buf[send_len], cur_len);
			}
			if (ret < 0) {
				break;
			}
			send_len += cur_len;
			le_send += cur_len;
		}

		if (ret < 0) {
			if (info->write_timeout == OS_NO_WAIT) {
				break;
			} else if (info->write_timeout == OS_FOREVER) {
				os_sleep(SPPBLE_SEND_INTERVAL);
				continue;
			} else {
				if (timeout >= info->write_timeout) {
					break;
				}

				timeout += (int32_t)SPPBLE_SEND_INTERVAL;
				os_sleep(SPPBLE_SEND_INTERVAL);
				continue;
			}
		}
	}

	return send_len;
}
#endif

static int sppble_write(io_stream_t handle, uint8_t *buf, int num)
{
	int ret = 0;
	struct sppble_info_t *info = NULL;

	info = (struct sppble_info_t *)handle->data;
	if (info->connect_type == NONE_CONNECT_TYPE) {
		return -EIO;
	}

	os_mutex_lock(&info->write_mutex, OS_FOREVER);
	if (info->connect_type == SPP_CONNECT_TYPE) {
		ret = sppble_spp_send_data(info, buf, num);
	} else if((info->connect_type == BLE_CONNECT_TYPE) ||
			  (info->connect_type == GATT_OVER_BR_CONNECT_TYPE)) {
	#ifdef CONFIG_BT_BLE
		ret = sppble_ble_send_data(info, buf, num);
	#endif
	}
	os_mutex_unlock(&info->write_mutex);

	return ret;
}

static int sppble_close(io_stream_t handle)
{
	struct sppble_info_t *info = NULL;

	info = (struct sppble_info_t *)handle->data;
	if (info->connect_type != NONE_CONNECT_TYPE) {
		SYS_LOG_INF("Active do %d disconnect", info->connect_type);
		if (info->connect_type == SPP_CONNECT_TYPE) {
			bt_manager_spp_disconnect(info->spp_chl);
		} else if (info->connect_type == BLE_CONNECT_TYPE) {
		#ifdef CONFIG_BT_BLE
			bt_manager_ble_disconnect(info->conn);
		#endif
		} else if (info->connect_type == GATT_OVER_BR_CONNECT_TYPE) {
			hostif_bt_gatt_over_br_disconnect(info->conn);
		}
	}

	os_mutex_lock(&info->read_mutex, OS_FOREVER);
	if (info->buff) {
		mem_free(info->buff);
		info->buff = NULL;
		handle->rofs = 0;
		handle->wofs = 0;
		handle->cache_size = 0;
		handle->total_size = 0;
	}
	os_mutex_unlock(&info->read_mutex);

	return 0;
}

static int sppble_destroy(io_stream_t handle)
{
	SYS_LOG_WRN("Sppble stream not support destroy new!!");
	return -EIO;
}

static int sppble_stream_flush(io_stream_t handle)
{
	struct sppble_info_t *info = NULL;

	info = (struct sppble_info_t *)handle->data;

	os_mutex_lock(&info->read_mutex, OS_FOREVER);

	handle->total_size = SPPBLE_BUFF_SIZE;
	handle->cache_size = 0;
	handle->rofs = 0;
	handle->wofs = 0;
	os_mutex_unlock(&info->read_mutex);

	return 0;
}

const stream_ops_t sppble_stream_ops = {
	.init = sppble_init,
	.open = sppble_open,
	.read = sppble_read,
	.seek = NULL,
	.tell = sppble_tell,
	.write = sppble_write,
	.flush = sppble_stream_flush,
	.close = sppble_close,
	.destroy = sppble_destroy,
};

int sppble_stream_modify_write_timeout(io_stream_t handle,uint32_t write_timeout)
{
    struct sppble_info_t *info = NULL;

    info = (struct sppble_info_t *)handle->data;

    os_mutex_lock(&info->read_mutex, OS_FOREVER);

    info->write_timeout = write_timeout;

    os_mutex_unlock(&info->read_mutex);

    return 0;
}

int sppble_stream_set_rxdata_callback(io_stream_t handle, void (*callback)(void))
{
    struct sppble_info_t *info = NULL;

    info = (struct sppble_info_t *)handle->data;

    os_mutex_lock(&info->read_mutex, OS_FOREVER);

    info->rxdata_cb = callback;

    os_mutex_unlock(&info->read_mutex);

    return 0;
}


io_stream_t sppble_stream_create(void *param)
{
	return stream_create(&sppble_stream_ops, param);
}


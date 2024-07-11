/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief gatt for bqb test.
 */

#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN "bqb gatt"

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mem_manager.h>

#include <logging/sys_log.h>
#include <acts_bluetooth/uuid.h>
#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/host_interface.h>
#include <acts_bluetooth/addr.h>
#include "bqb_gatt.h"
#include "hex_str.h"
#include "gatt_internal.h"

#define TEST_BUF_SIZE                16
#define TEST_LONG_BUF_SIZE           512
#define BT_UUID_TEST_VAL             0xfff0
#define BT_UUID_TEST                 BT_UUID_DECLARE_16(BT_UUID_TEST_VAL)
#define BT_UUID_TEST_CTRL_POINT_VAL  0xfff1
#define BT_UUID_TEST_CTRL_POINT      BT_UUID_DECLARE_16(BT_UUID_TEST_CTRL_POINT_VAL)

#define BT_GATT_PERM_TEST_MASK	(BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
#define BT_GATT_PERM_AUT_TEST_MASK	(BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN)
#define BT_GATT_PERM_ENC_TEST_MASK	(BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT)
#define BT_GATT_PERM_AUT_ENC_TEST_MASK	(BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT | \
								BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN)

typedef void (* cmd_handler)(int argc, char *argv[]);
struct test_command {
	const char *cmd;
	cmd_handler handler;
};

struct bqb_gatt_t {
	struct bt_conn *conn;
	struct bt_gatt_discover_params discover_params;
	struct bt_gatt_write_params write_params;
	struct bt_gatt_read_params read_params;
	struct bt_gatt_indicate_params indicate_params;
	uint16_t offset;
	uint8_t buf_size;
	uint8_t *gatt_data;
	uint8_t start : 1;
	uint8_t perm_flag : 2;
	uint8_t read_descriptor : 1;
	uint8_t write_long_value : 1;
	uint8_t signed_cmd : 1;
};

static struct bqb_gatt_t gatt_ctx = {0};
static struct bt_uuid_16 uuid_16 = BT_UUID_INIT_16(0);
static struct bt_uuid_128 uuid_128 = BT_UUID_INIT_128(0);
static uint8_t bqb_gatt_recv_buf[TEST_BUF_SIZE] = {1,2,3,4,};

static void alloc_gatt_data(int size)
{
	gatt_ctx.gatt_data = mem_malloc(size);
	if (!gatt_ctx.gatt_data) {
		SYS_LOG_ERR("no mem\n");
	}
	memset(gatt_ctx.gatt_data, 'x', size);
	SYS_LOG_INF("alloc:%p\n",gatt_ctx.gatt_data);
}

static void free_gatt_data(void)
{
	if (gatt_ctx.gatt_data) {
		SYS_LOG_INF("free:%p\n",gatt_ctx.gatt_data);
		mem_free(gatt_ctx.gatt_data);
		gatt_ctx.gatt_data = NULL;
	}
}

static uint8_t *bqb_gatt_get_data(void)
{
	return gatt_ctx.gatt_data;
}

static ssize_t read_gatt_test(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, uint16_t len, uint16_t offset)
{
	switch(gatt_ctx.perm_flag) {
		case 1:
			return BT_GATT_ERR(BT_ATT_ERR_AUTHORIZATION);
		case 2:
			return BT_GATT_ERR(BT_ATT_ERR_AUTHENTICATION);
		case 3:
			return BT_GATT_ERR(BT_ATT_ERR_ENCRYPTION_KEY_SIZE);

		default:
			break;
	}

	if (gatt_ctx.write_long_value) {
		const char *value = bqb_gatt_get_data();
		gatt_ctx.offset = 0;
		return bt_gatt_attr_read(conn, attr, buf, len, offset, value, strlen(value));
	} else {
		const void *value = (const void *)bqb_gatt_recv_buf;
		return bt_gatt_attr_read(conn, attr, buf, len, offset, value, gatt_ctx.buf_size);
	}
}

static ssize_t write_gatt_test(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	switch(gatt_ctx.perm_flag) {
		case 1:
			return BT_GATT_ERR(BT_ATT_ERR_AUTHORIZATION);
		case 2:
			return BT_GATT_ERR(BT_ATT_ERR_AUTHENTICATION);
		case 3:
			return BT_GATT_ERR(BT_ATT_ERR_ENCRYPTION_KEY_SIZE);

		default:
			break;
	}

	if (BT_GATT_WRITE_FLAG_PREPARE == flags
		|| gatt_ctx.signed_cmd) {
		return 0;
	}

	if (gatt_ctx.write_long_value) {
		if (offset != gatt_ctx.offset) {
			gatt_ctx.offset = 0;
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
		}

		if (offset + len > strlen((char *)gatt_ctx.gatt_data)) {
			gatt_ctx.offset = 0;
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}

		memcpy(&gatt_ctx.gatt_data[offset], buf, len);
		gatt_ctx.offset += len;
	} else {
		if (len > gatt_ctx.buf_size) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		memcpy(bqb_gatt_recv_buf, buf, len);
	}
	return len;
}

static ssize_t read_cud(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	if (gatt_ctx.read_descriptor) {
		gatt_ctx.offset = 0;
		const char *value = (const char *)bqb_gatt_get_data();
		return bt_gatt_attr_read(conn, attr, buf, len, offset, value, strlen(value));
	} else {
		const void *value = (const void *)bqb_gatt_recv_buf;
		return bt_gatt_attr_read(conn, attr, buf, len, offset, value, gatt_ctx.buf_size);
	}
}

static ssize_t write_cud(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	if (gatt_ctx.read_descriptor) {
	   if (offset != gatt_ctx.offset) {
		   gatt_ctx.offset = 0;
		   return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	   }

	   if (offset + len > strlen((char *)gatt_ctx.gatt_data)) {
		   gatt_ctx.offset = 0;
		   return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	   }

	   memcpy(&gatt_ctx.gatt_data[offset], buf, len);
	   gatt_ctx.offset += len;
	} else {
	   if (len > gatt_ctx.buf_size) {
		   return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	   }
	   memcpy(bqb_gatt_recv_buf, buf, len);
	}
	return len;
}

static void test_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
	SYS_LOG_INF("DONE\n");
}

BT_GATT_SERVICE_DEFINE(gatt_test_svc,
BT_GATT_CHARACTERISTIC(BT_UUID_TEST, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_INDICATE,
			   BT_GATT_PERM_TEST_MASK|BT_GATT_PERM_PREPARE_WRITE, read_gatt_test, write_gatt_test, NULL),
BT_GATT_CCC(test_cfg_changed, BT_GATT_PERM_TEST_MASK),

BT_GATT_CHARACTERISTIC(BT_UUID_TEST_CTRL_POINT, BT_GATT_CHRC_READ|BT_GATT_CHRC_WRITE_WITHOUT_RESP|BT_GATT_CHRC_AUTH,
			  BT_GATT_PERM_TEST_MASK, read_gatt_test, write_gatt_test, NULL),
BT_GATT_DESCRIPTOR(BT_UUID_GATT_CUD, BT_GATT_PERM_TEST_MASK, read_cud, write_cud, NULL),
);


static void bqb_gatt_acl_connected_cb(struct bt_conn *conn, u8_t err)
{
	if (err) {
		return;
	}

	gatt_ctx.conn = conn;

	if (gatt_ctx.write_long_value
		|| gatt_ctx.read_descriptor) {
		alloc_gatt_data(gatt_ctx.offset);
		gatt_ctx.offset = 0;
	}
}

static void bqb_gatt_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    gatt_ctx.conn = NULL;

	if (gatt_ctx.read_descriptor
		|| gatt_ctx.write_long_value) {
		free_gatt_data();
		gatt_ctx.read_descriptor = 0;
		gatt_ctx.write_long_value = 0;
	}

}

static struct bt_conn_cb gatt_conn_cbs = {
	.connected = bqb_gatt_acl_connected_cb,
	.disconnected = bqb_gatt_acl_disconnected_cb,
};

static void bqb_gatt_start(int argc, char *argv[])
{
	if (gatt_ctx.start) {
		return;
	}
	gatt_ctx.start = 1;
#if defined(CONFIG_BT_PRIVACY)
	hostif_bt_le_address_resolution_enable(0);
#endif
    hostif_bt_conn_cb_register(&gatt_conn_cbs);
	gatt_ctx.buf_size = 4;
}

static void bqb_gatt_stop(int argc, char *argv[])
{
	if (!gatt_ctx.start) {
		return;
	}

	gatt_ctx.start = 0;
    hostif_bt_conn_cb_unregister(&gatt_conn_cbs);
	memset(&gatt_ctx, 0, sizeof(gatt_ctx));
}

static void exchange_func(struct bt_conn *conn, uint8_t err,
			  struct bt_gatt_exchange_params *params)
{
	SYS_LOG_INF("conn %p(%d) %s(%d)", conn, bt_conn_get_handle(conn),
		err == 0U ? "successful" : "failed", err);
}

static const struct bt_gatt_exchange_params mtu_params = {
	.func = exchange_func,
};

static void bqb_gatt_exchange_mtu(int argc, char *argv[])
{
	struct bt_gatt_exchange_params *params  = (struct bt_gatt_exchange_params *)&mtu_params;

	int ret = hostif_bt_gatt_exchange_mtu(gatt_ctx.conn, params);
	SYS_LOG_INF("ret:%d\n", ret);
}

static u8_t discover_func(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	struct bt_gatt_service_val *gatt_service;
	struct bt_gatt_chrc *gatt_chrc;
	struct bt_gatt_include *gatt_include;
	char uuid[37];

	if (!attr) {
		memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	switch (params->type) {
	case BT_GATT_DISCOVER_SECONDARY:
	case BT_GATT_DISCOVER_PRIMARY:
		gatt_service = attr->user_data;
		bt_uuid_to_str(gatt_service->uuid, uuid, sizeof(uuid));
		printk("Service %s found: start handle %x, end_handle %x\n",
		       uuid, attr->handle, gatt_service->end_handle);
		break;
	case BT_GATT_DISCOVER_CHARACTERISTIC:
		gatt_chrc = attr->user_data;
		bt_uuid_to_str(gatt_chrc->uuid, uuid, sizeof(uuid));
		printk("Characteristic %s found: handle %x\n", uuid,
		       attr->handle);
		break;
	case BT_GATT_DISCOVER_INCLUDE:
		gatt_include = attr->user_data;
		bt_uuid_to_str(gatt_include->uuid, uuid, sizeof(uuid));
		printk("Include %s found: handle %x, start %x, end %x\n",
		       uuid, attr->handle, gatt_include->start_handle,
		       gatt_include->end_handle);
		break;
	default:
		bt_uuid_to_str(attr->uuid, uuid, sizeof(uuid));
		printk("Descriptor %s found: handle %x\n", uuid, attr->handle);
		break;
	}

	return BT_GATT_ITER_CONTINUE;
}

static void bqb_gatt_discover(int argc, char *argv[])
{
	int err;

	gatt_ctx.discover_params.func = discover_func;
	gatt_ctx.discover_params.start_handle = 0x0001;
	gatt_ctx.discover_params.end_handle = 0xffff;

	uuid_16.val = BT_UUID_GATT_VAL;

	if (argc > 0) {
		gatt_ctx.discover_params.type = strtoul(argv[0], NULL, 0);
		if (gatt_ctx.discover_params.type > BT_GATT_DISCOVER_ATTRIBUTE) {
			SYS_LOG_ERR("type err:%d\n", gatt_ctx.discover_params.type);
			return;
		}
	}

	if (argc > 1) {
		/* Only set the UUID if the value is valid (non zero) */
		uuid_16.val = strtoul(argv[1], NULL, 16);
		if (uuid_16.val) {
			gatt_ctx.discover_params.uuid = &uuid_16.uuid;
		}
	}

	if (argc > 2) {
		gatt_ctx.discover_params.start_handle = strtoul(argv[2], NULL, 16);
		if (argc > 3) {
			gatt_ctx.discover_params.end_handle = strtoul(argv[3], NULL, 16);
		}
	}

	err = bt_gatt_discover(gatt_ctx.conn, &gatt_ctx.discover_params);
	if (err) {
		SYS_LOG_ERR("err:%d\n", err);
	}
}

static uint8_t bqb_gatt_read_cb(struct bt_conn *conn, uint8_t err,
					struct bt_gatt_read_params *params,
					const void *data, uint16_t length)
{
	SYS_LOG_INF("err:%d\n", err);

	if (!data) {
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_CONTINUE;
}

static void bqb_gatt_read(int argc, char *argv[])
{
	int err = 0;
	struct bt_gatt_read_params *read_params = &gatt_ctx.read_params;

	read_params->func = bqb_gatt_read_cb;
	read_params->handle_count = 1;
	read_params->single.handle = 0;
	read_params->single.offset = 0U;

	if (argc > 0) {
		read_params->single.handle = strtoul(argv[0], NULL, 16);
	}

	if (argc > 1) {
		read_params->single.offset = strtoul(argv[1], NULL, 0);
	}

	err = bt_gatt_read(gatt_ctx.conn, read_params);
	if (err) {
		SYS_LOG_ERR("err:%d\n", err);
	}
}

static void bqb_gatt_multi_read(int argc, char *argv[])
{
	int err = 0;
	uint16_t hdl[8] = {0};
	uint8_t num = ARRAY_SIZE(hdl);
	uint8_t idx = 0;
	struct bt_gatt_read_params *read_params = &gatt_ctx.read_params;

	if (argc > num) {
		SYS_LOG_INF("hdl num overflow:%d\n", argc);
		return;
	}

	for (idx = 0; idx < argc; idx++) {
		hdl[idx] = strtoul(argv[idx], NULL, 16);
	}

	read_params->func = bqb_gatt_read_cb;
	read_params->handle_count = idx;
	read_params->handles = hdl;
	err = bt_gatt_read(gatt_ctx.conn, read_params);
	if (err) {
		SYS_LOG_ERR("err:%d\n", err);
	}
}

static void bqb_gatt_uuid16_read(int argc, char *argv[])
{
	int err = 0;
	struct bt_gatt_read_params *read_params = &gatt_ctx.read_params;

	read_params->func = bqb_gatt_read_cb;
	read_params->handle_count = 0;
	read_params->by_uuid.start_handle = 0x0001;
	read_params->by_uuid.end_handle = 0xffff;

	if (argc > 0) {
		uuid_16.val = strtoul(argv[0], NULL, 16);
		if (uuid_16.val) {
			read_params->by_uuid.uuid = &uuid_16.uuid;
		}
	}

	if (argc > 1) {
		read_params->by_uuid.start_handle = strtoul(argv[1], NULL, 16);
		if (argc > 2) {
			read_params->by_uuid.end_handle = strtoul(argv[2], NULL, 16);
		}
	}

	err = bt_gatt_read(gatt_ctx.conn, read_params);
	if (err) {
		SYS_LOG_ERR("err:%d\n", err);
	}
}

static void bqb_gatt_uuid128_read(int argc, char *argv[])
{
	int err = 0;
	struct bt_gatt_read_params *read_params = &gatt_ctx.read_params;

	read_params->func = bqb_gatt_read_cb;
	read_params->handle_count = 0;
	read_params->by_uuid.start_handle = 0x0001;
	read_params->by_uuid.end_handle = 0xffff;

	if (argc > 0) {
		const char * uuid_str = argv[0];
		char bytes[33] = {0};
		int idx = 0;
		uint8_t idx2 = 0;
		uint8_t len = strlen(uuid_str);
		if (len != 39) {
			SYS_LOG_ERR("len:%d\n", len);
			return;
		}
		idx = len - 1;
		do {
			if (uuid_str[idx] == '-') {
				idx --;
			}
			bytes[idx2] = uuid_str[idx - 1];
			bytes[idx2 + 1] = uuid_str[idx];
			idx2 += 2;
			idx -= 2;
		} while(idx >= 0);

		str_to_hex(uuid_128.val, bytes, 16);
		read_params->by_uuid.uuid = &uuid_128.uuid;
	}

	if (argc > 1) {
		read_params->by_uuid.start_handle = strtoul(argv[1], NULL, 16);
		if (argc > 2) {
			read_params->by_uuid.end_handle = strtoul(argv[2], NULL, 16);
		}
	}

	err = bt_gatt_read(gatt_ctx.conn, read_params);
	if (err) {
		SYS_LOG_ERR("err:%d\n", err);
	}
}

static void gatt_write_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params)
{
	SYS_LOG_INF("err:%d,hdl:0x%04x,data:%p,len:%d\n", err, params->handle, params->data, params->length);

	if (gatt_ctx.gatt_data) {
		mem_free(gatt_ctx.gatt_data);
		SYS_LOG_INF("free:%p\n",gatt_ctx.gatt_data);
		gatt_ctx.gatt_data = NULL;
	}
	memset((void *)(&gatt_ctx.write_params),0,sizeof(gatt_ctx.write_params));
}

static void bqb_gatt_write(int argc, char *argv[])
{
	int err = 0;
	uint16_t handle = 0;
	uint16_t size = 0;

	if (argc < 2) {
		SYS_LOG_ERR("write 'handle' 'size' 'offset'\n");
		return;
	}

	handle = strtoul(argv[0], NULL, 16);
	size = strtoul(argv[1], NULL, 0);

	if (gatt_ctx.write_params.func) {
		SYS_LOG_ERR("busy\n");
		return;
	}

	if (gatt_ctx.gatt_data) {
		mem_free(gatt_ctx.gatt_data);
		SYS_LOG_INF("free:%p\n",gatt_ctx.gatt_data);
		gatt_ctx.gatt_data = NULL;
	}
	memset((void *)(&gatt_ctx.write_params),0,sizeof(gatt_ctx.write_params));

	gatt_ctx.gatt_data = mem_malloc(size);
	if (!gatt_ctx.gatt_data) {
		SYS_LOG_ERR("no mem\n");
		return;
	}
	SYS_LOG_INF("alloc:%p\n",gatt_ctx.gatt_data);
	memset(gatt_ctx.gatt_data, 0xab, size);


	gatt_ctx.write_params.offset = 0;
	if (argc == 3) {
		gatt_ctx.write_params.offset = strtoul(argv[2], NULL, 0);
	}

	SYS_LOG_INF("hdl:0x%04x,data:%p,len:%d,offset:%d\n",
		handle, gatt_ctx.gatt_data, size, gatt_ctx.write_params.offset);

    gatt_ctx.write_params.data = gatt_ctx.gatt_data;
    gatt_ctx.write_params.length = size;
    gatt_ctx.write_params.handle = handle;
    gatt_ctx.write_params.func = gatt_write_cb;

    err = bt_gatt_write(gatt_ctx.conn, &gatt_ctx.write_params);
    if (err) {
        SYS_LOG_ERR("err:%d\n", err);
		if (gatt_ctx.gatt_data) {
			mem_free(gatt_ctx.gatt_data);
			SYS_LOG_INF("free:%p\n",gatt_ctx.gatt_data);
			gatt_ctx.gatt_data = NULL;
		}
		memset((void *)(&gatt_ctx.write_params),0,sizeof(gatt_ctx.write_params));
    }
}

static void bqb_gatt_write_without_rsp(int argc, char *argv[])
{
	uint16_t handle = 0;
	uint8_t data = 0xcd;
	bool sign = false;

	if (argc < 1) {
		SYS_LOG_ERR("write_without_rsp 'handle'\n");
		return;
	}

	handle = strtoul(argv[0], NULL, 16);

	if (argc > 1) {
		sign = (strtoul(argv[1], NULL, 0)) ?  true : false;
	}

	int err = bt_gatt_write_without_response(gatt_ctx.conn, handle, (uint8_t *)&data, 1, sign);
    if (err) {
        SYS_LOG_ERR("err:%d\n", err);
    }
}

static uint8_t notify_handler(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
				   const void *data, uint16_t length)
{
	if (!data) {
		mem_free(params);
		SYS_LOG_INF("free:%p\n", params);
		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_CONTINUE;
}

static void bqb_gatt_config_ccc(int argc, char *argv[])
{
	uint16_t handle = 0;
	struct bt_gatt_subscribe_params *sub_params = NULL;

	if (argc < 1) {
		SYS_LOG_ERR("need handle\n");
		return;
	}

	sub_params = mem_malloc(sizeof(struct bt_gatt_subscribe_params));
	if (!sub_params) {
		SYS_LOG_ERR("nomem\n");
		return;
	}

	handle = strtoul(argv[0], NULL, 16);
	sub_params->value = BT_GATT_CCC_NOTIFY;
	sub_params->value_handle = handle - 1;
	sub_params->ccc_handle = handle;
	sub_params->notify = notify_handler;

	atomic_set_bit(sub_params->flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);

	if (argc > 1 && (strtoul(argv[1], NULL, 0))) {
		sub_params->value = BT_GATT_CCC_INDICATE;
	}

	int err = bt_gatt_subscribe(gatt_ctx.conn, sub_params);
	if (err) {
		SYS_LOG_ERR("hdl %04x, err:%d\n",
			sub_params->ccc_handle, err);
		mem_free(sub_params);
		return;
	}
	SYS_LOG_INF("alloc:%p\n", sub_params);
}

static void bqb_gatt_val_write(int argc, char *argv[])
{
	uint16_t handle = 0;
	uint16_t size = 0;
	uint8_t idx = 0;

	if (argc < 3) {
		SYS_LOG_ERR("write 'handle' 'size' 'data...'\n");
		return;
	}

	handle = strtoul(argv[0], NULL, 16);
	size = strtoul(argv[1], NULL, 0);

	if (gatt_ctx.write_params.func) {
		SYS_LOG_ERR("busy\n");
		return;
	}

	if (gatt_ctx.gatt_data) {
		mem_free(gatt_ctx.gatt_data);
		SYS_LOG_INF("free:%p\n",gatt_ctx.gatt_data);
		gatt_ctx.gatt_data = NULL;
	}
	memset((void *)(&gatt_ctx.write_params),0,sizeof(gatt_ctx.write_params));

	gatt_ctx.gatt_data = mem_malloc(size);
	if (!gatt_ctx.gatt_data) {
		SYS_LOG_ERR("no mem\n");
		return;
	}
	SYS_LOG_INF("alloc:%p\n",gatt_ctx.gatt_data);
	memset(gatt_ctx.gatt_data, 0, size);

	for (idx = 0; idx < size; idx++) {
		gatt_ctx.gatt_data[idx] = strtoul(argv[idx + 2], NULL, 16);
	}

	SYS_LOG_INF("hdl:0x%04x,data:%p,len:%d\n", handle, gatt_ctx.gatt_data, size);

    gatt_ctx.write_params.data = gatt_ctx.gatt_data;
    gatt_ctx.write_params.length = size;
    gatt_ctx.write_params.handle = handle;
    gatt_ctx.write_params.func = gatt_write_cb;

    int err = bt_gatt_write(gatt_ctx.conn, &gatt_ctx.write_params);
    if (err) {
        SYS_LOG_ERR("err:%d\n", err);
		if (gatt_ctx.gatt_data) {
			mem_free(gatt_ctx.gatt_data);
			SYS_LOG_INF("free:%p\n",gatt_ctx.gatt_data);
			gatt_ctx.gatt_data = NULL;
		}
		memset((void *)(&gatt_ctx.write_params),0,sizeof(gatt_ctx.write_params));
    }
}

static void bqb_gatt_change_test_attr_perm(int argc, char *argv[])
{
	/* 1-Authorization, 2-Authentication, 3-Encryption Key Size*/
	if (argc > 0) {
		gatt_ctx.perm_flag = strtoul(argv[0], NULL, 0);
	} else {
		gatt_ctx.perm_flag = 0;
	}
}

static void bqb_gatt_update_read_long_descriptor_flag(int argc, char *argv[])
{
	if (gatt_ctx.read_descriptor) {
		gatt_ctx.read_descriptor = 0;
	}
	gatt_ctx.read_descriptor = 1;
}

static void bqb_gatt_update_write_long_value_flag(int argc, char *argv[])
{
	if (gatt_ctx.write_long_value) {
		gatt_ctx.write_long_value = 0;
	}
	gatt_ctx.write_long_value = 1;
	gatt_ctx.offset = 55;
	if (argc > 0) {
		gatt_ctx.offset = strtoul(argv[0], NULL, 0);
	}
}

static void bqb_gatt_update_recv_buf_size(int argc, char *argv[])
{
	gatt_ctx.buf_size = 4;

	if (argc > 0) {
		gatt_ctx.buf_size = strtoul(argv[0], NULL, 0);
	}

	if (gatt_ctx.buf_size > TEST_BUF_SIZE) {
		SYS_LOG_ERR("size overflow, need use write_long_value\n");
		gatt_ctx.buf_size = 4;
	}
}

static void bqb_gatt_update_signed_cmd_flag(int argc, char *argv[])
{
	if (gatt_ctx.signed_cmd) {
		gatt_ctx.signed_cmd = 0;
		return;
	}
	gatt_ctx.signed_cmd = 1;
}

static const struct test_command cmd_list[] = {
	{"start", bqb_gatt_start},
	{"stop", bqb_gatt_stop},
	{"exchange_mtu", bqb_gatt_exchange_mtu},
	{"discover", bqb_gatt_discover},/*discover 'discover type' 'uuid16' 'start hdl' 'stop hdl'*/
	{"read", bqb_gatt_read},/*read 'hdl' 'offset'*/
	{"multi_read", bqb_gatt_multi_read},/*multi_read 'hdl' 'hdl' '...'*/
	{"uuid16_read", bqb_gatt_uuid16_read},/*uuid16_read 'uuid' 'start hdl' 'end hdl'*/
	{"uuid128_read", bqb_gatt_uuid128_read},/*uuid128_read 'uuid' 'start hdl' 'end hdl'*/
	{"write", bqb_gatt_write},/*write 'hdl' 'size' 'offset'*/
	{"write_without_rsp", bqb_gatt_write_without_rsp},/*write_without_rsp 'hdl' 'size' 'offset'*/
	{"config_ccc", bqb_gatt_config_ccc},/*config_ccc 'hdl' 'default-notify, 1-indicate'*/
	{"val_write", bqb_gatt_val_write},/*val_write 'hdl' 'size' 'data...'*/
	{"change_test_attr_perm", bqb_gatt_change_test_attr_perm},/*change_test_attr_perm 'test perm'*/
	{"read_long_descriptor", bqb_gatt_update_read_long_descriptor_flag},/*read_long_descriptor*/
	{"write_long_value", bqb_gatt_update_write_long_value_flag},/*write_long_value*/
	{"update_recv_buf_size", bqb_gatt_update_recv_buf_size},/*update_recv_buf_size 'size'*/
	{"update_signed_cmd_flag", bqb_gatt_update_signed_cmd_flag},/*update_signed_cmd_flag*/

	/*end*/
	{NULL, NULL}
};

int bqb_gatt_test_command_handler(int argc, char *argv[])
{
	char *cmd = NULL;

	if (argc < 2) {
		SYS_LOG_INF("Used: bqb gatt xx\n");
		return -EINVAL;
	}

	cmd = argv[1];
	SYS_LOG_INF("gatt cmd:%s\n", cmd);

	uint32_t num = ARRAY_SIZE(cmd_list);
	uint32_t idx = 0;

	for (idx = 0; idx < num; idx++) {
		if (cmd_list[idx].cmd && cmd_list[idx].handler
			&& (!strcmp(cmd, cmd_list[idx].cmd))) {
			cmd_list[idx].handler(argc -2, &argv[2]);
			SYS_LOG_INF("cmd:%s, num:%d,idx:%d\n", cmd_list[idx].cmd, num, idx);
			break;
		}
	}
	return 0;
}


/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief l2cap for bqb test.
 */

#include <acts_bluetooth/l2cap.h>
#include <acts_bluetooth/host_interface.h>
#include <acts_bluetooth/addr.h>

#include "bqb_utils.h"
#include "bqb_gap.h"
#include "bqb_l2cap.h"
#include "hci_core.h"
#include "conn_internal.h"
#include "l2cap_internal.h"
#include "bqb_gap_le.h"

static void bqb_l2cap_acl_connected_cb(struct bt_conn *conn, u8_t err);
static void bqb_l2cap_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason);
static void bqb_l2cap_connected_cb(struct bt_l2cap_chan *chan);
static void bqb_l2cap_disconnected_cb(struct bt_l2cap_chan *chan);
static void bqb_l2cap_encrypt_changed_cb(struct bt_l2cap_chan *chan, uint8_t status);
static int bqb_l2cap_recv_cb(struct bt_l2cap_chan *chan, struct net_buf *buf);
extern int atoi (const char *nptr);

struct bqb_l2cap_context {
    struct bt_conn *conn;
    struct bt_l2cap_br_chan br_chan;
    struct bt_l2cap_le_chan le_chan;
};

static struct bt_conn_cb bqb_l2cap_conn_cbs = {
    .connected = bqb_l2cap_acl_connected_cb,
    .disconnected = bqb_l2cap_acl_disconnected_cb,
};

static const struct bt_l2cap_chan_ops bqb_l2cap_ops = {
    .connected = bqb_l2cap_connected_cb,
    .disconnected = bqb_l2cap_disconnected_cb,
    .encrypt_change = bqb_l2cap_encrypt_changed_cb,
    .recv = bqb_l2cap_recv_cb
};

static struct bqb_l2cap_context bqb_l2cap_cntx = {0};

static void bqb_l2cap_acl_connected_cb(struct bt_conn *conn, u8_t err)
{
	BT_INFO("ACL Connected type: %d, err: %x", hostif_bt_conn_get_type(conn), err);
    if (0 == err) {
        bqb_l2cap_cntx.conn = conn;
        bqb_l2cap_cntx.br_chan.chan.ops = &bqb_l2cap_ops;
        //ret = bt_l2cap_chan_connect(bqb_l2cap_cntx.conn, &(bqb_l2cap_cntx.br_chan.chan), 0x01);
    } else {
        BT_INFO("br connected, err:%d\n", err);
    }
}

static void bqb_l2cap_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    BT_INFO("acl disconnected, reason:%d\n", reason);
    bqb_l2cap_cntx.conn = NULL;
}

static void bqb_l2cap_connected_cb(struct bt_l2cap_chan *chan)
{
    BT_INFO("bqb_l2cap_connected_cb\n");
}

static void bqb_l2cap_disconnected_cb(struct bt_l2cap_chan *chan)
{
    BT_INFO("bqb_l2cap_disconnected_cb\n");
    //bqb_gap_disconnect_acl(bqb_l2cap_cntx.conn);
}

static void bqb_l2cap_encrypt_changed_cb(struct bt_l2cap_chan *chan, uint8_t status)
{
    BT_INFO("bqb_l2cap_encrypt_changed_cb\n");
}

static int bqb_l2cap_recv_cb(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
    BT_INFO("bqb_l2cap_recv\n");
	return 0;
}

static void bqb_l2cap_start(void)
{
    hostif_bt_conn_cb_register(&bqb_l2cap_conn_cbs);
}

static void bqb_l2cap_stop(void)
{
    hostif_bt_conn_cb_unregister(&bqb_l2cap_conn_cbs);
    memset(&bqb_l2cap_cntx, 0, sizeof(bqb_l2cap_cntx));
}

static int bqb_l2cap_server_accept(struct bt_conn *conn, struct bt_l2cap_chan **chan)
{
	BT_INFO("conn: %p", conn);
	memset(&bqb_l2cap_cntx.le_chan, 0, sizeof(bqb_l2cap_cntx.le_chan));
	bqb_l2cap_cntx.conn = conn;
	bqb_l2cap_cntx.le_chan.chan.ops = &bqb_l2cap_ops;
	*chan = &bqb_l2cap_cntx.le_chan.chan;

	return 0;
}

struct bt_l2cap_server s_bqb_l2cap_server = {
	.psm = 0xF1,
	.sec_level = BT_SECURITY_L0,
	.accept = bqb_l2cap_server_accept,
	.node.next = NULL,
};

static int bqb_l2cap_send_data(uint32_t len)
{
	struct net_buf* buf = bt_l2cap_create_pdu(NULL, 0);
	net_buf_add_mem(buf, "abab", len);
	int  ret = bt_l2cap_chan_send(&bqb_l2cap_cntx.le_chan.chan, buf);
	BT_INFO(" send data %d", ret);
	if (ret < 0) {
		net_buf_destroy(buf);
	}
	return ret;
}

static int bqb_l2cap_req_update_conn_param(void)
{
    struct bt_le_conn_param param;
    param.interval_min = 0x28;
    param.interval_max = 0x50;
    param.latency = 0;
    param.timeout = 0xC8;
    extern int bt_l2cap_update_conn_param(struct bt_conn *conn, const struct bt_le_conn_param *param);
    return bt_l2cap_update_conn_param(bqb_l2cap_cntx.conn, &param);
}

static int bqb_l2cap_br_server_accept(struct bt_conn *conn, struct bt_l2cap_chan **chan)
{
	BT_INFO("conn: %p", conn);
	memset(&bqb_l2cap_cntx.br_chan, 0, sizeof(bqb_l2cap_cntx.br_chan));
	bqb_l2cap_cntx.conn = conn;
	bqb_l2cap_cntx.br_chan.chan.ops = &bqb_l2cap_ops;
	*chan = &bqb_l2cap_cntx.br_chan.chan;

	return 0;
}


struct bt_l2cap_server s_bqb_l2cap_br_server = {
	.psm = 0xF3,
	.sec_level = BT_SECURITY_L1,
	.accept = bqb_l2cap_br_server_accept,
	.node.next = NULL,
};

static int bqb_l2cap_br_send_data(uint32_t len)
{
	struct net_buf* buf = bt_l2cap_create_pdu(NULL, 0);
	net_buf_add(buf, len);
	int  ret = bt_l2cap_br_chan_send(&bqb_l2cap_cntx.br_chan.chan, buf);
	BT_INFO(" send data %d", ret);
	if (ret < 0) {
		net_buf_destroy(buf);
	}
	return ret;
}

static int bqb_l2cap_conn_le_acl(char* addr)
{
    char temp_addr[6] = {0};
    bt_addr_le_t le_addr = {0};
    bqb_utils_str2hex(temp_addr, addr, 6);
    bqb_utils_bytes_reverse(le_addr.a.val, temp_addr, 6);
    le_addr.type = BT_ADDR_LE_PUBLIC;
    return bqb_gap_le_connect_acl(&le_addr, &bqb_l2cap_cntx.conn);
}

int bqb_l2cap_test_command_handler(int argc, char *argv[])
{
    char *cmd = NULL;

    if (argc < 2) {
        return -EINVAL;
    }

    cmd = argv[1];

    if (!strcmp(cmd, "start")) {
        bqb_l2cap_start();
    } else if (!strcmp(cmd, "stop")) {
        bqb_l2cap_stop();
    } else if (!strcmp(cmd, "conn")) {
        cmd = argv[2]; /*addr format: 12:34:56:78:9a*/
        bqb_l2cap_cntx.conn= bqb_gap_connect_br_acl(cmd);
    } else if (!strcmp(cmd, "disc")) {
        bt_l2cap_chan_disconnect(&(bqb_l2cap_cntx.br_chan.chan));
    } else if (!strcmp(cmd, "disc_acl")) {
        hostif_bt_conn_disconnect(bqb_l2cap_cntx.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    } else if (!strcmp(cmd, "connle")){
        bqb_l2cap_conn_le_acl(argv[2]);
    } else if (!strcmp(cmd, "discle")) {
        bt_l2cap_chan_disconnect(&(bqb_l2cap_cntx.le_chan.chan));
    } else if (!strcmp(cmd, "update_param")) {
        bqb_l2cap_req_update_conn_param();
    } else if (!strcmp(cmd, "regsrv")) {
        bt_l2cap_server_register(&s_bqb_l2cap_server);
    } else if (!strcmp(cmd, "srvsd")){
        bqb_l2cap_send_data(atoi(argv[2]));
    } else if (!strcmp(cmd, "regbrsrv")) {
        bt_l2cap_br_server_register(&s_bqb_l2cap_br_server);
    } else if (!strcmp(cmd, "connbrsrv")) {
        bt_l2cap_br_chan_connect(bqb_l2cap_cntx.conn, &bqb_l2cap_cntx.br_chan.chan, s_bqb_l2cap_br_server.psm);
    }else if (!strcmp(cmd, "echo")) {
        static uint8_t echo_data[4] = {0, 1, 2, 3};
        bt_l2cap_br_chan_echo_req(&bqb_l2cap_cntx.br_chan.chan, echo_data, sizeof(echo_data));
    } else if (!strcmp(cmd, "setbrsrvparam")) {
        uint16_t psm = atoi(argv[2]);
        s_bqb_l2cap_br_server.psm = psm;
        s_bqb_l2cap_br_server.sec_level = atoi(argv[3]);
        BT_INFO("set BR server PSM: %x, SEC_LE: %x", s_bqb_l2cap_br_server.psm, s_bqb_l2cap_br_server.sec_level);
    } else if (!strcmp(cmd, "brsrvsd")) {
         bqb_l2cap_br_send_data(atoi(argv[2]));
    } else if (!strcmp(cmd, "adv")) {
         uint8_t enable = atoi(argv[2]);
         if (enable) {
            bqb_gap_le_adv_start(3);
         } else {
            bqb_gap_le_adv_stop();
         }
    } else if (!strcmp(cmd, "scan")) {
         uint8_t enable = atoi(argv[2]);
         if (enable) {
            bqb_gap_write_scan_enable(3);
         } else {
            bqb_gap_write_scan_enable(0);
         }
    } else {
        printk("The cmd is not existed!\n");
    }

    return 0;
}


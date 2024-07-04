/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief rfcomm for bqb smp test.
 */

#include <acts_bluetooth/host_interface.h>
#include <acts_bluetooth/addr.h>
#include <acts_bluetooth/gatt.h>

#include "smp.h"

#include "bqb_utils.h"
#include "bqb_gap_le.h"
#include "bqb_gap.h"

extern uint16_t bt_gatt_get_attr_by_uuid(uint16_t uuid_val);

static void bqb_smp_acl_connected_cb(struct bt_conn *conn, u8_t err);
static void bqb_smp_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason);

static void bqb_smp_io_passkey_display_cb(struct bt_conn *conn, unsigned int passkey);
static void bqb_smp_io_passkey_entry_cb(struct bt_conn *conn);
static void bqb_smp_io_passkey_confirm_cb(struct bt_conn *conn, unsigned int passkey);
static void bqb_smp_io_oob_data_request_cb(struct bt_conn *conn, struct bt_conn_oob_info *oob_info);
static void bqb_smp_io_cancel_cb(struct bt_conn *conn);
static void bqb_smp_io_pairing_confirm_cb(struct bt_conn *conn);
static void bqb_smp_io_pincode_entry_cb(struct bt_conn *conn, bool highsec);

static ssize_t bqb_smp_gatt_server_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
        void *buf, u16_t len, u16_t offset);
static ssize_t bqb_smp_gatt_server_write_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
        const void *buf, u16_t len, u16_t offset, u8_t flags);
static uint8_t bqb_smp_conn_gatt_read_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params,
        const void *data, uint16_t length);

typedef struct bqb_smp_context {
    bool init;
    struct bt_conn *conn;
    struct bt_le_oob local_oob;
    struct bt_le_oob rem_oob;
    struct bt_gatt_subscribe_params sub_params;
    struct bt_gatt_read_params read_params;
    bt_security_t default_lev;
} bqb_smp_context_t;

static struct bt_conn_cb bqb_smp_conn_cbs = {
    .connected = bqb_smp_acl_connected_cb,
    .disconnected = bqb_smp_acl_disconnected_cb,
};

//keyboard&display
static struct bt_conn_auth_cb bqb_smp_io_default_cbs = {
    .passkey_display = bqb_smp_io_passkey_display_cb,
    .passkey_entry  = bqb_smp_io_passkey_entry_cb,
    .passkey_confirm = bqb_smp_io_passkey_confirm_cb,
    .cancel = bqb_smp_io_cancel_cb,
    .pairing_confirm = bqb_smp_io_pairing_confirm_cb,
    .pincode_entry = bqb_smp_io_pincode_entry_cb,
};

static struct bt_conn_auth_cb bqb_smp_io_displayonly_cbs = {
    .passkey_display = bqb_smp_io_passkey_display_cb,
    .passkey_entry = NULL,
    .passkey_confirm = NULL,
    .cancel = bqb_smp_io_cancel_cb,
    .pairing_confirm = NULL,
    .pincode_entry = NULL,
};

static struct bt_conn_auth_cb bqb_smp_io_oob_dponly_cbs = {
    .passkey_display = bqb_smp_io_passkey_display_cb,
    .passkey_entry = NULL,
    .passkey_confirm = NULL,
    .oob_data_request = bqb_smp_io_oob_data_request_cb,
    .cancel = bqb_smp_io_cancel_cb,
    .pairing_confirm = NULL,
    .pincode_entry = NULL,
};

static struct bt_conn_auth_cb bqb_smp_io_oob_jw_cbs = {
    .passkey_display = NULL,
    .passkey_entry = NULL,
    .passkey_confirm = NULL,
    .oob_data_request = bqb_smp_io_oob_data_request_cb,
    .cancel = bqb_smp_io_cancel_cb,
    .pairing_confirm = NULL,
    .pincode_entry = NULL,
};

//GATT test service
#define BQB_SMP_GAP_TEST_SERVICE_UUID	     BT_UUID_DECLARE_16(0xEEE0)
#define BQB_SMP_GAP_TEST_CHRC_UUID_1	     BT_UUID_DECLARE_16(0xEEE1)
#define BQB_SMP_GAP_TEST_CHRC_UUID_2	     BT_UUID_DECLARE_16(0xEEE2)
#define BQB_SMP_GAP_TEST_CHRC_UUID_3	     BT_UUID_DECLARE_16(0xEEE3)

static struct bt_gatt_attr bqb_smp_gatt_attrs[] = {
    /* bqb smp test Primary Service Declaration */
    BT_GATT_PRIMARY_SERVICE(BQB_SMP_GAP_TEST_SERVICE_UUID),
    //Need auth
    BT_GATT_CHARACTERISTIC(BQB_SMP_GAP_TEST_CHRC_UUID_1,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN,
        bqb_smp_gatt_server_read_cb, bqb_smp_gatt_server_write_cb, NULL),
    //No need auth
    BT_GATT_CHARACTERISTIC(BQB_SMP_GAP_TEST_CHRC_UUID_2,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
        bqb_smp_gatt_server_read_cb, bqb_smp_gatt_server_write_cb, NULL),
    #if 0
    //No need auth&encrypt
    BT_GATT_CHARACTERISTIC(BQB_SMP_GAP_TEST_CHRC_UUID_3,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        bqb_smp_gatt_read_cb, bqb_smp_gatt_write_cb, NULL),
    #endif
};

static struct bt_gatt_service bqb_smp_gatt = BT_GATT_SERVICE(bqb_smp_gatt_attrs);

static bqb_smp_context_t g_bqb_smp_cntx = {0};

static void bqb_smp_acl_connected_cb(struct bt_conn *conn, u8_t err)
{
    BT_INFO("acl connected, conn:(%p,%p), err:%d\n", g_bqb_smp_cntx.conn, conn, err);
    if (err == 0) {
        if (g_bqb_smp_cntx.conn == NULL) {
            struct bt_conn_info info;
            bt_security_t lev = g_bqb_smp_cntx.default_lev;
            g_bqb_smp_cntx.conn = conn;
            BT_INFO("sec lev:(%d,%d)\n", g_bqb_smp_cntx.default_lev, bt_conn_get_security(conn));
            if (bt_conn_get_security(conn) < lev) {
                bt_conn_set_security(conn, lev);
            }
            // for oob case
            if (0 == bt_conn_get_info(conn, &info)) {
                //bt_addr_le_copy(&g_bqb_smp_cntx.local_oob.addr, info.le.local);
                bt_addr_le_copy(&g_bqb_smp_cntx.rem_oob.addr, info.le.remote);
            }
        } else if (g_bqb_smp_cntx.conn == conn) {

        } else {
            //BT_INFO("acl connect fail, existed conn:%p\n", g_bqb_smp_cntx.conn);
        }
    }
}

static void bqb_smp_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    BT_INFO("acl disconnected, reason:%d\n", reason);
    g_bqb_smp_cntx.conn = NULL;
}

static void bqb_smp_io_passkey_display_cb(struct bt_conn *conn, unsigned int passkey)
{
    printk("---------------------------------------\n");
    printk("|              Passkey[%d]            |\n", passkey);
    printk("---------------------------------------\n");
    if (conn != g_bqb_smp_cntx.conn) {
       BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_smp_cntx.conn, conn);
    }
}

static void bqb_smp_io_passkey_entry_cb(struct bt_conn *conn)
{
    printk("---------------------------------------------------------\n");
    printk("| Please entry passkey, use: bqb smp pk_input <Passkey> |\n");
    printk("---------------------------------------------------------\n");
    if (conn != g_bqb_smp_cntx.conn) {
       BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_smp_cntx.conn, conn);
    }
}

static void bqb_smp_io_passkey_confirm_cb(struct bt_conn *conn, unsigned int passkey)
{
    printk("----------------------------------------------------------\n");
    printk("|    Please Confirm Passkey[%d],use: bqb smp pk_cfm   |\n", passkey);
    printk("----------------------------------------------------------\n");
    if (conn != g_bqb_smp_cntx.conn) {
       BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_smp_cntx.conn, conn);
    }
}

static void bqb_smp_io_oob_data_request_cb(struct bt_conn *conn, struct bt_conn_oob_info *oob_info)
{
    struct bt_conn_info info = {0};
    bt_conn_get_info(conn, &info);

    if (conn != g_bqb_smp_cntx.conn) {
        BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_smp_cntx.conn, conn);
        return;
    }

    if (oob_info->type == BT_CONN_OOB_LE_SC) {
        BT_INFO("oob congig: %d\n", oob_info->lesc.oob_config);

        if (oob_info->lesc.oob_config == BT_CONN_OOB_LOCAL_ONLY) { //0
            bt_smp_le_oob_set_sc_data(g_bqb_smp_cntx.conn, &g_bqb_smp_cntx.local_oob.le_sc_data, NULL);
        } else if(oob_info->lesc.oob_config == BT_CONN_OOB_REMOTE_ONLY) { //1
            bt_smp_le_oob_set_sc_data(g_bqb_smp_cntx.conn, NULL, &g_bqb_smp_cntx.rem_oob.le_sc_data);
        } else if (oob_info->lesc.oob_config == BT_CONN_OOB_BOTH_PEERS) {  //2
            bt_smp_le_oob_set_sc_data(g_bqb_smp_cntx.conn, &g_bqb_smp_cntx.local_oob.le_sc_data, &g_bqb_smp_cntx.rem_oob.le_sc_data);
        } else {  //3
            BT_ERR("no req oob data: %d\n", oob_info->lesc.oob_config);
        }
    } else if (oob_info->type == BT_CONN_OOB_LE_LEGACY) {
        printk("--------------------------------------------------------------------\n");
        printk("| Please input <TSPX_OOB_Data> from PTS IXIT, use command:\n");
        printk("| bqb smp oob_tk xxxxxxxxxxxxxxxxxxxxxxxxxx   |\n");
        printk("--------------------------------------------------------------------\n");
    }
}

static void bqb_smp_io_cancel_cb(struct bt_conn *conn)
{
    printk("---------------------------------------\n");
    printk("|            cancel                   |\n");
    printk("---------------------------------------\n");
    if (conn != g_bqb_smp_cntx.conn) {
       BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_smp_cntx.conn, conn);
    }
}

static void bqb_smp_io_pairing_confirm_cb(struct bt_conn *conn)
{
    printk("---------------------------------------------------\n");
    printk("|  Please Confirm Pairing, use: bqb smp pair_cfm  |\n");
    printk("---------------------------------------------------\n");
    if (conn != g_bqb_smp_cntx.conn) {
       BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_smp_cntx.conn, conn);
    }
}

static void bqb_smp_io_pincode_entry_cb(struct bt_conn *conn, bool highsec)
{
    printk("---------------------------------------\n");
    printk("|     Please Entry Pincode[%d]        |\n", highsec);
    printk("---------------------------------------\n");
    if (conn != g_bqb_smp_cntx.conn) {
        BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_smp_cntx.conn, conn);
    }
}

static ssize_t bqb_smp_gatt_server_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
        void *buf, u16_t len, u16_t offset)
{
    BT_INFO("read len:%d\n", len);
    return 0;
}

static ssize_t bqb_smp_gatt_server_write_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
        const void *buf, u16_t len, u16_t offset, u8_t flags)
{
    BT_INFO("write len:%d\n", len);
    //return BT_GATT_ERR(BT_ATT_ERR_AUTHENTICATION);
    return len;
}

static uint8_t bqb_smp_conn_gatt_read_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params,
    const void *data, uint16_t length)
{
    BT_INFO("err:%d, length:%d\n", err, length);

    if (!data) {
        (void)memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_CONTINUE;
}

static void bqb_smp_le_adv_enable(uint8 enable)
{
    if (enable == 1) {
        bqb_gap_le_adv_start(BQB_GAP_LE_ADV_CONN);
    } else if (enable == 0) {
        bqb_gap_le_adv_stop();
    } else {
        BT_ERR("error inputr: %d\n", enable);
	}
}

static void bqb_smp_connect_le_acl(char *str_addr)
{
    uint8_t temp_addr[6] = {0};
    bt_addr_le_t addr = {0};
    int ret;

    bqb_utils_str2hex(temp_addr, str_addr, 6);
    bqb_utils_bytes_reverse(addr.a.val, temp_addr, 6);
    addr.type = BT_ADDR_LE_PUBLIC;
    BT_INFO("conn_p: %p\n", &g_bqb_smp_cntx.conn);
    //bqb_utils_print_addr(addr.a.val, true);
    ret = bqb_gap_le_connect_acl(&addr, &g_bqb_smp_cntx.conn);
    //BT_INFO("ret: %d\n", ret);
}

static void bqb_smp_connect_br_acl(char *str_addr)
{
    g_bqb_smp_cntx.conn = bqb_gap_connect_br_acl(str_addr);
}

static void bqb_smp_conn_start_encrypt(void)
{
    BT_INFO("start enc\n");
    bt_smp_start_security(g_bqb_smp_cntx.conn);
}

static void bqb_smp_start(void)
{
    if (!g_bqb_smp_cntx.init) {
        BT_INFO("enter...\n");
        g_bqb_smp_cntx.init = true;
        g_bqb_smp_cntx.default_lev = BT_SECURITY_L1;
        hostif_bt_conn_cb_register(&bqb_smp_conn_cbs);
        hostif_bt_conn_le_auth_cb_register(&bqb_smp_io_default_cbs);
    } else {
        //BT_WARN("smp have been started!\n");
    }
}

static void bqb_smp_stop(void)
{
    if (g_bqb_smp_cntx.init) {
        BT_INFO("exit...\n");
        g_bqb_smp_cntx.init = false;
        hostif_bt_conn_cb_unregister(&bqb_smp_conn_cbs);
        hostif_bt_conn_le_auth_cb_register(NULL);
        memset((void*)&g_bqb_smp_cntx, 0x0, sizeof(bqb_smp_context_t));
    } else {
        //BT_WARN("smp have been stoped!\n");
    }
}

static void bqb_smp_set_security_level(bt_security_t sec)
{
    BT_INFO("\n");
    int ret = hostif_bt_conn_set_security(g_bqb_smp_cntx.conn, sec);
    BT_INFO("Sec level: %d, ret: %d\n", sec, ret);
}

static void bqb_smp_conn_auth_pairing_confirm(void)
{
    int ret = bt_conn_auth_pairing_confirm(g_bqb_smp_cntx.conn);
    BT_INFO("pair cfm ret: %d\n", ret);
}

static void bqb_smp_conn_auth_passkey_confirm(void)
{
    int ret = bt_conn_auth_passkey_confirm(g_bqb_smp_cntx.conn);
    BT_INFO("passkey cfm ret: %d\n", ret);
}

static void bqb_smp_conn_auth_passkey_entry(unsigned int passkey)
{
    int ret = bt_conn_auth_passkey_entry(g_bqb_smp_cntx.conn, passkey);
    BT_INFO("passkey entry:%d, ret: %d\n", passkey, ret);
}

static void bqb_smp_conn_enter_justwork(uint8_t enable)
{
    BT_INFO("justwork en:%d\n", enable);

    hostif_bt_conn_le_auth_cb_register(NULL);
    if (!enable) {
        hostif_bt_conn_le_auth_cb_register(&bqb_smp_io_default_cbs);
    }
}

static void bqb_smp_conn_displayonly_en(uint8_t enable)
{
    BT_INFO("diaplay en:%d\n", enable);

    hostif_bt_conn_le_auth_cb_register(NULL);
    if (enable) {
        hostif_bt_conn_le_auth_cb_register(&bqb_smp_io_displayonly_cbs);
    } else {
        hostif_bt_conn_le_auth_cb_register(&bqb_smp_io_default_cbs);
    }
}

static void bqb_smp_conn_oob_en(uint8_t enable)
{
    BT_INFO("oob en:%d\n", enable);
    bool en = enable ? true:false;
    bt_set_oob_data_flag(en);
    hostif_bt_conn_le_auth_cb_register(NULL);
    if (en) {
        hostif_bt_conn_le_auth_cb_register(&bqb_smp_io_oob_jw_cbs);
    } else {
        hostif_bt_conn_le_auth_cb_register(&bqb_smp_io_default_cbs);
    }
}

static void bqb_smp_conn_sc_oob_en(uint8_t enable)
{
    BT_INFO("oob en:%d\n", enable);
    char oob_sc_conf_str[33] = {0};
    char oob_sc_rand_str[33] = {0};
    uint8_t oob_sc_conf_data[16] = {0};
    uint8_t oob_sc_rand_data[16] = {0};

    bool en = enable ? true:false;
    hostif_bt_conn_le_auth_cb_register(NULL);
    if (en) {
        hostif_bt_conn_le_auth_cb_register(&bqb_smp_io_oob_jw_cbs);
        bt_le_oob_get_local(BT_ID_DEFAULT, &g_bqb_smp_cntx.local_oob);

        bqb_utils_bytes_reverse(oob_sc_conf_data, g_bqb_smp_cntx.local_oob.le_sc_data.c, 16);
        bqb_utils_bytes_reverse(oob_sc_rand_data, g_bqb_smp_cntx.local_oob.le_sc_data.r, 16);
        bqb_utils_hex2str(oob_sc_conf_str, (char*)oob_sc_conf_data, 16);
        bqb_utils_hex2str(oob_sc_rand_str, (char*)oob_sc_rand_data, 16);

        printk("---------------------------------------------------------|\n");
        printk("| IUT OOB confirmation [%s] \n", oob_sc_conf_str);
        printk("| IUT OOB random       [%s] \n", oob_sc_rand_str);
        printk("---------------------------------------------------------|\n");
        printk("| Please input the OOB SC data in the PTS UI!            |\n");
    } else {
        hostif_bt_conn_le_auth_cb_register(&bqb_smp_io_default_cbs);
    }
}

static void bqb_smp_conn_oob_set_tk(char *tk_str)
{
    uint8_t tk[16] = {0};
    uint8_t input_tk[16] = {0};
    int err;
    BT_INFO("%s", tk_str);

    bqb_utils_str2hex(tk, tk_str, 16);
    bqb_utils_bytes_reverse(input_tk, tk, 16);
    err = bt_smp_le_oob_set_tk(g_bqb_smp_cntx.conn, (const uint8_t *)input_tk);
    BT_INFO("set tk ret:%d\n", err);
}

static void bqb_smp_conn_sc_oob_set_remote_data(char *cfm, char* random)
{
    uint8_t remote_cfm[16] = {0};
    uint8_t remote_rand[16] = {0};
    printk("-----------------------------------------------------------|\n");
    printk("Remote OOB confirmation [%s] \n", cfm);
    printk("Remote OOB rand         [%s] \n", random);
    printk("-----------------------------------------------------------|\n");

    bqb_utils_str2hex(remote_cfm, cfm, 16);
    bqb_utils_str2hex(remote_rand, random, 16);

    bqb_utils_bytes_reverse(g_bqb_smp_cntx.rem_oob.le_sc_data.c, remote_cfm, 16);
    bqb_utils_bytes_reverse(g_bqb_smp_cntx.rem_oob.le_sc_data.r, remote_rand, 16);
}

static void bqb_smp_conn_sc_oob_get_local_data(void)
{
    uint8_t local_cfm[16] = {0};
    uint8_t local_rand[16] = {0};
    char local_cfm_str[33] = {0};
    char local_rand_str[33] = {0};

    bqb_utils_bytes_reverse(local_cfm, g_bqb_smp_cntx.local_oob.le_sc_data.c, 16);
    bqb_utils_bytes_reverse(local_rand, g_bqb_smp_cntx.local_oob.le_sc_data.r, 16);
    bqb_utils_hex2str(local_cfm_str, local_cfm, 16);
    bqb_utils_hex2str(local_rand_str, local_rand, 16);
    printk("-----------------------------------------------------------|\n");
    printk("IUT OOB confirmation [%s] \n", local_cfm_str);
    printk("IUT OOB rand         [%s] \n", local_rand_str);
    printk("-----------------------------------------------------------|\n");
}

static void bqb_smp_conn_sc_oob_set_dponly(bool en)
{
    hostif_bt_conn_le_auth_cb_register(NULL);
    if (en) {
        hostif_bt_conn_le_auth_cb_register(&bqb_smp_io_oob_dponly_cbs);
    } else {
        hostif_bt_conn_le_auth_cb_register(&bqb_smp_io_oob_jw_cbs);
    }
}

static void bqb_smp_conn_send_gatt_sign_cmd(void)
{
    uint8_t data[4] = {1,2,3,4};
    bt_gatt_write_without_response(g_bqb_smp_cntx.conn, 5, (const void *)data, 4, true);
}

static void bqb_smp_clear_all_device(void)
{
    BT_INFO("clear LTK.\n");
    hostif_bt_le_clear_list();
}

static void bqb_smp_conn_wscan_enable(uint8_t enable)
{
    BT_INFO("write scan :%d\n", enable);

    bool en = enable ? true:false;
    if (en) {
        bqb_gap_write_scan_enable(BQB_GAP_PAGE_SCAN_ONLY);
    } else {
        bqb_gap_write_scan_enable(BQB_GAP_NO_SCANS);
    }
}

static void bqb_smp_conn_gatt_write_cmd(uint16_t handle)
{
    uint8_t data[4] = {1,2,3,4};
    BT_INFO("write, handle:0x%04x\n", handle);
    bt_gatt_write_without_response(g_bqb_smp_cntx.conn, handle, (const void *)data, 4, false);
}

static void bqb_smp_conn_gatt_read_cmd(uint16_t handle)
{
    BT_INFO("read, handle:0x%04x\n", handle);
    struct bt_gatt_read_params *read_params = &g_bqb_smp_cntx.read_params;

    read_params->func = bqb_smp_conn_gatt_read_cb;

    read_params->handle_count = 1;
    read_params->single.handle = handle;
    read_params->single.offset = 0U;

    bt_gatt_read(g_bqb_smp_cntx.conn, read_params);
}

static void bqb_smp_gatt_get_attr(uint16_t uuid)
{
    uint16_t att_handle = bt_gatt_get_attr_by_uuid(uuid);

    //BT_UUID_GAP:0x1800
    //BT_UUID_GATT:0x1801
    printk("\n--------------------------------------------|");
    printk("\n  UUID: [%04x],  Attr handle: [%04x]        |", uuid, att_handle);
    printk("\n  Please input to the PTS UI......          |");
    printk("\n--------------------------------------------|\n");
}

static uint8_t bqb_smp_gatt_notify_handler(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
        const void *data, uint16_t length)
{
    if (!data) {
        BT_INFO("end notify\n");
        memset(&(g_bqb_smp_cntx.sub_params), 0x00, sizeof(struct bt_gatt_subscribe_params));
        return BT_GATT_ITER_STOP;
    }

    BT_INFO("received notify: data%p, len:%d\n", data, length);
    return BT_GATT_ITER_CONTINUE;
}

static void bqb_smp_gatt_config_ccc(uint16_t handle, uint16_t ccc_op)
{
    struct bt_gatt_subscribe_params *sub_params = &g_bqb_smp_cntx.sub_params;

    BT_INFO("config ccc handle:0x%04x, notify:0x%04x\n", handle, ccc_op);

    if ((ccc_op != BT_GATT_CCC_NOTIFY) && (ccc_op != BT_GATT_CCC_INDICATE)) {
        BT_ERR("ccc type is wrong!\n");
        return;
    }

    sub_params->value = ccc_op;
    sub_params->value_handle = handle - 1;
    sub_params->ccc_handle = handle;
    sub_params->notify = bqb_smp_gatt_notify_handler;
    atomic_set_bit(sub_params->flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);

    int err = bt_gatt_subscribe(g_bqb_smp_cntx.conn, sub_params);
    if (err) {
        BT_ERR("hdl %04x, err:%d\n", sub_params->ccc_handle, err);
    }
}

static void bqb_smp_config_sec_level(uint8_t level)
{
    BT_INFO("sec level: %d\n", level);
    if (level <= BT_SECURITY_L4) {
        g_bqb_smp_cntx.default_lev = (bt_security_t)level;
    } else {
        BT_ERR("Unsupport sec level: %d\n", level);
    }
}

static void bqb_smp_config_reset(void)
{
    BT_INFO("reset\n");

    bqb_smp_conn_wscan_enable(0);
    bqb_gap_le_adv_stop();

    hostif_bt_le_clear_list();
    g_bqb_smp_cntx.default_lev = BT_SECURITY_L1;
    hostif_bt_conn_le_auth_cb_register(NULL);
    hostif_bt_conn_le_auth_cb_register(&bqb_smp_io_default_cbs);
}

int bqb_smp_test_command_handler(int argc, char *argv[])
{
    char *cmd = NULL;

    if (argc < 2) {
        return -EINVAL;
    }

    cmd = argv[1];

    if (!g_bqb_smp_cntx.init) {
        if (!strcmp(cmd, "enter")) {
            bqb_smp_start();
        } else {
            printk("Reject! Please enter SMP test firstly.\n");
        }
        return 0;
    }

    if (!strcmp(cmd, "enter")) {
        BT_WARN("has entered!\n");
    } else if (!strcmp(cmd, "exit")) {
        bqb_smp_stop();
    } else if (!strcmp(cmd, "adv")) {
        uint8 enable = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &enable);
        bqb_smp_le_adv_enable(enable);
    } else if (!strcmp(cmd, "con_le")) {
        cmd = argv[2];
        bqb_smp_connect_le_acl(cmd);
    } else if (!strcmp(cmd, "con_br")) {
        cmd = argv[2];
        bqb_smp_connect_br_acl(cmd);
    } else if (!strcmp(cmd, "disc")) {
        bqb_gap_le_disconnect_acl(g_bqb_smp_cntx.conn);
    } else if (!strcmp(cmd, "pair")) {
        bt_smp_start_security(g_bqb_smp_cntx.conn);
    } else if (!strcmp(cmd, "enc")) {
        bqb_smp_conn_start_encrypt();
    } else if (!strcmp(cmd, "sec")) {
        bt_security_t level = BT_SECURITY_L4;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &level);
        bqb_smp_set_security_level(level);
    } else if (!strcmp(cmd, "pair_cfm")) {
        bqb_smp_conn_auth_pairing_confirm();
    } else if (!strcmp(cmd, "pk_cfm")) {
        bqb_smp_conn_auth_passkey_confirm();
    } else if (!strcmp(cmd, "pk_input")) {
        unsigned int passkey = (unsigned int)bqb_utils_atoi(argv[2]);
        bqb_smp_conn_auth_passkey_entry(passkey);
    } else if (!strcmp(cmd, "jw")) {
        uint8_t en = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &en);
        bqb_smp_conn_enter_justwork(en);
    } else if (!strcmp(cmd, "dponly")) {
        uint8_t en = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &en);
        bqb_smp_conn_displayonly_en(en);
    } else if (!strcmp(cmd, "oob_flag")) {
        uint8_t enable = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &enable);
        bt_set_oob_data_flag(enable ? true:false);
    } else if (!strcmp(cmd, "oob")) {
        uint8_t en = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &en);
        bqb_smp_conn_oob_en(en);
    } else if (!strcmp(cmd, "oob_tk")) {
        cmd = argv[2];
        bqb_smp_conn_oob_set_tk(cmd);
    } else if (!strcmp(cmd, "oob_sc")) {
        uint8_t en = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &en);
        bqb_smp_conn_sc_oob_en(en);
    } else if (!strcmp(cmd, "oob_dponly")) {
        uint8_t en = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &en);
        bqb_smp_conn_sc_oob_set_dponly(en ? true:false);
    } else if (!strcmp(cmd, "oob_rem")) {
        char *conf = argv[2];
        char *random = argv[3];
        bqb_smp_conn_sc_oob_set_remote_data(conf, random);
    } else if (!strcmp(cmd, "oob_local")) {
        bqb_smp_conn_sc_oob_get_local_data();
    } else if (!strcmp(cmd, "send_sign")) {
        bqb_smp_conn_send_gatt_sign_cmd();
    } else if (!strcmp(cmd, "clear")) {
        bqb_smp_clear_all_device();
    } else if (!strcmp(cmd, "wscan")) {
        uint8_t en = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &en);
        bqb_smp_conn_wscan_enable(en);
    } else if (!strcmp(cmd, "gattw")) {
        uint8_t handle[2] = {0};
        bqb_utils_str2hex(handle, argv[2], 2);
        bqb_smp_conn_gatt_write_cmd(handle[0]<<8|handle[1]);
    } else if (!strcmp(cmd, "gattr")) {
        uint16_t handle = bqb_utils_strtoul(argv[2], NULL, 16);
        bqb_smp_conn_gatt_read_cmd(handle);
    } else if (!strcmp(cmd, "gattreg")) {
        bt_gatt_service_register(&bqb_smp_gatt);
    } else if (!strcmp(cmd, "attr")) {
        uint16_t uuid = bqb_utils_strtoul(argv[2], NULL, 16);
        bqb_smp_gatt_get_attr(uuid);
    } else if (!strcmp(cmd, "ccc")) {
        uint16_t handle = bqb_utils_strtoul(argv[2], NULL, 16);
        uint16_t ccc_op = bqb_utils_strtoul(argv[3], NULL, 16);
        bqb_smp_gatt_config_ccc(handle, ccc_op);
    } else if (!strcmp(cmd, "cfgsec")) {
        uint8_t lev = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &lev);
        bqb_smp_config_sec_level(lev);
    } else if (!strcmp(cmd, "reset")) {
        bqb_smp_config_reset();
    } else {
        printk("invalid parameters\n");
    }

    return 0;
}


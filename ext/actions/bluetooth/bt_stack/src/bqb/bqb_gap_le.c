/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief rfcomm for bqb gap le test.
 */

#include <acts_bluetooth/host_interface.h>
#include <acts_bluetooth/conn.h>

#include <atomic.h>
#include "hci_core.h"
#include "conn_internal.h"

#include "bqb_utils.h"
#include "bqb_gap_le.h"

static void bqb_gap_le_adv_sent_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info);
static void bqb_gap_le_adv_connected_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_connected_info *info);
static void bqb_gap_le_adv_scanned_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_scanned_info *info);

static void bqb_gap_le_adv_scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *ad);
static void bqb_gap_le_adv_scan_timeout(void);

static void bqb_gap_le_acl_connected_cb(struct bt_conn *conn, u8_t err);
static void bqb_gap_le_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason);

static void bqb_gap_le_io_passkey_display_cb(struct bt_conn *conn, unsigned int passkey);
//static void bqb_gap_le_io_passkey_entry_cb(struct bt_conn *conn);
static void bqb_gap_le_io_passkey_confirm_cb(struct bt_conn *conn, unsigned int passkey);
static void bqb_gap_le_io_cancel_cb(struct bt_conn *conn);
static void bqb_gap_le_io_pairing_confirm_cb(struct bt_conn *conn);

#if defined(BQB_GAPLE_TEST_PA_PAWR_BIS)
extern bqb_gap_le_bis_tx_cb bis_send_host_cbk;
extern bqb_gap_le_bis_rx_cb bis_recv_host_cbk;
static void bqb_gap_le_recv_biginfo_cb(struct bt_le_per_adv_sync *sync, const struct bt_iso_biginfo *biginfo);
static void bqb_gap_le_pasync_synced_cb(struct bt_le_per_adv_sync *sync, struct bt_le_per_adv_sync_synced_info *info);
static void bqb_gap_le_pasync_data_recv_cb(struct bt_le_per_adv_sync *sync, const struct bt_le_per_adv_sync_recv_info *info, struct net_buf_simple *buf);
static void bqb_gap_le_pasync_term_cb(struct bt_le_per_adv_sync *sync, const struct bt_le_per_adv_sync_term_info *info);
static void bqb_gap_le_adv_pawr_data_request_cb(struct bt_le_ext_adv *adv, const struct bt_le_per_adv_data_request *request);
static void bqb_gap_le_adv_pawr_response_cb(struct bt_le_ext_adv *adv, struct bt_le_per_adv_response_info *info, struct net_buf_simple *buf);
static void bqb_gap_le_pa_create(bool is_pawr);
static void bqb_gap_le_iso_recv_cb(struct bt_iso_chan *chan, const struct bt_iso_recv_info *info, struct net_buf *buf);
static void bqb_gap_le_iso_connected_cb(struct bt_iso_chan *chan);
static void bqb_gap_le_iso_disconnected_cb(struct bt_iso_chan *chan, uint8_t reason);
static int bqb_gap_le_bis_direct_send_cb(uint16_t handle, uint8_t *buf, uint16_t *len);
static int bqb_gap_le_bis_direct_recv_cb(uint16_t handle, uint8_t *buf, uint16_t len,uint32_t data_rx_flag);
#endif /*BQB_GAPLE_TEST_PA_PAWR_BIS*/

typedef struct bqb_gap_le_context {
    // adv
    struct bt_le_ext_adv *adv;
    uint8_t adv_index;
    struct bt_data adv_data;
    uint8_t adv_payload[20];
    // pa
    //struct bt_le_per_adv_param per_adv_param;
    struct bt_data pa_data;
    uint8_t pa_payload[10];

    // scan
    bt_addr_t scan_addr;
    //pa sync
    bool pasync_trigger;
    struct bt_le_per_adv_sync *pa_sync;
    //struct bt_le_per_adv_sync_cb pa_sync_cb;
    struct net_buf_simple *pa_sync_resp_buf;

    // connection
    bool init;
    bt_security_t sec_level;
    struct bt_conn *conn;
    //gatt
    struct bt_gatt_read_params params;
    struct bt_uuid_16 gatt_uuid_16;

    //big
    struct bt_iso_big *big;
} bqb_gap_le_context_t;

static bqb_gap_le_context_t g_bqb_gap_le_cntx = {0};

static struct bt_le_ext_adv_cb bqb_gap_le_adv_callbacks = {
    .sent = bqb_gap_le_adv_sent_cb,
    .connected = bqb_gap_le_adv_connected_cb,
    .scanned = bqb_gap_le_adv_scanned_cb,
#if defined(BQB_GAPLE_TEST_PA_PAWR_BIS)
    .pawr_data_request = bqb_gap_le_adv_pawr_data_request_cb,
    .pawr_response = bqb_gap_le_adv_pawr_response_cb,
#endif
};

static struct bt_conn_cb bqb_gap_le_conn_cbs = {
    .connected = bqb_gap_le_acl_connected_cb,
    .disconnected = bqb_gap_le_acl_disconnected_cb,
};

//keyboard&display
static struct bt_conn_auth_cb bqb_gap_le_io_default_cbs = {
    .passkey_display = bqb_gap_le_io_passkey_display_cb,
   // .passkey_entry  = bqb_gap_le_io_passkey_entry_cb,
    .passkey_confirm = bqb_gap_le_io_passkey_confirm_cb,
    .cancel = bqb_gap_le_io_cancel_cb,
    .pairing_confirm = bqb_gap_le_io_pairing_confirm_cb,
};

struct bt_le_scan_cb bqb_gap_le_scan_callbacks = {
    .recv = bqb_gap_le_adv_scan_recv,
    .timeout = bqb_gap_le_adv_scan_timeout,
};

#if defined(BQB_GAPLE_TEST_PA_PAWR_BIS)
struct bt_le_per_adv_sync_cb bqb_gap_le_pasync_cb = {
    .synced = bqb_gap_le_pasync_synced_cb,
    .recv = bqb_gap_le_pasync_data_recv_cb,
    .term = bqb_gap_le_pasync_term_cb,
    .state_changed = NULL,
    .biginfo = bqb_gap_le_recv_biginfo_cb,
};

static struct bt_iso_chan_ops bqb_gap_le_iso_ops = {
    .recv         = bqb_gap_le_iso_recv_cb,
    .connected    = bqb_gap_le_iso_connected_cb,
    .disconnected = bqb_gap_le_iso_disconnected_cb,
};

static struct bt_iso_bis_params bqb_gap_le_bis_params = {0};

static struct bt_iso_chan_path bqb_gap_le_io_chan_rx_path = {
    .pid = BT_ISO_DATA_PATH_CB,//BT_ISO_DATA_PATH_HCI,/* over callback */
    .format = 0x03, //Transparent
    .cid = 0x0000,
    .vid = 0x0000,
    .delay = 0x0,
    .cc_len = 0x0,
};

static struct bt_iso_chan_path bqb_gap_le_io_chan_tx_path = {
    .pid = BT_ISO_DATA_PATH_CB, /* over callback */
    .format = 0x03, //Transparent
    .cid = 0x0000,
    .vid = 0x0000,
    .delay = 0x0,
    .cc_len = 0x0,
};

static struct bt_iso_chan_io_qos bqb_gap_le_io_qos_rx = {
    .sdu = 100,
    .phy = BT_GAP_LE_PHY_1M,
    .path = &bqb_gap_le_io_chan_rx_path,
};

static struct bt_iso_chan_io_qos bqb_gap_le_io_qos_tx = {
    .sdu = 100,
    .phy = BT_GAP_LE_PHY_1M,
    .path = &bqb_gap_le_io_chan_tx_path,
};

static struct bt_iso_chan_qos bqb_gap_le_bis_iso_qos = {
    .rx = &bqb_gap_le_io_qos_rx,
    .tx = &bqb_gap_le_io_qos_tx,
};

static struct bt_iso_chan bqb_gap_le_bis_iso_chan = {
    .ops = &bqb_gap_le_iso_ops,
    .qos = &bqb_gap_le_bis_iso_qos,
    .bis_params = &bqb_gap_le_bis_params,
};
static struct bt_iso_chan *bqb_gap_le_bis_channels[1] = {&bqb_gap_le_bis_iso_chan };
#endif /*BQB_GAPLE_TEST_PA_PAWR_BIS*/

/**************************************************************************/
/*                          Static functions                              */
/**************************************************************************/
static void bqb_gap_le_io_passkey_display_cb(struct bt_conn *conn, unsigned int passkey)
{
    printk("---------------------------------------\n");
    printk("|              Passkey[%d]            |\n", passkey);
    printk("---------------------------------------\n");
    if (conn != g_bqb_gap_le_cntx.conn) {
       BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_gap_le_cntx.conn, conn);
    }
}

static void bqb_gap_le_io_passkey_confirm_cb(struct bt_conn *conn, unsigned int passkey)
{
    printk("----------------------------------------------------------\n");
    printk("|    Please Confirm Passkey[%d],use: bqb smp pk_cfm   |\n", passkey);
    printk("----------------------------------------------------------\n");
    if (conn != g_bqb_gap_le_cntx.conn) {
        BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_gap_le_cntx.conn, conn);
        return;
    }
    bt_conn_auth_passkey_confirm(g_bqb_gap_le_cntx.conn);
}

static void bqb_gap_le_io_cancel_cb(struct bt_conn *conn)
{
    printk("---------------------------------------\n");
    printk("|            cancel                   |\n");
    printk("---------------------------------------\n");
    if (conn != g_bqb_gap_le_cntx.conn) {
       BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_gap_le_cntx.conn, conn);
    }
}

static void bqb_gap_le_io_pairing_confirm_cb(struct bt_conn *conn)
{
    printk("---------------------------------------------------\n");
    printk("|  Please Confirm Pairing, use: bqb smp pair_cfm  |\n");
    printk("---------------------------------------------------\n");
    if (conn != g_bqb_gap_le_cntx.conn) {
        BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_gap_le_cntx.conn, conn);
        return;
    }
    bt_conn_auth_pairing_confirm(g_bqb_gap_le_cntx.conn);
}

static void bqb_gap_le_adv_sent_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info)
{
    BT_INFO("adv: %p\n", adv);
    return;
}

static void bqb_gap_le_adv_connected_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_connected_info *info)
{
    BT_INFO("adv(%p,%p), con(%p,%p)\n", adv, g_bqb_gap_le_cntx.adv, info->conn, g_bqb_gap_le_cntx.conn);
    hostif_bt_le_ext_adv_delete(g_bqb_gap_le_cntx.adv);
    g_bqb_gap_le_cntx.adv = NULL;

    return;
}

static void bqb_gap_le_adv_scanned_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_scanned_info *info)
{
    BT_INFO("adv: %p\n", adv);
    return;
}

static bool bqb_gap_le_adv_data_flags_parse(struct bt_data *data, void *user_data)
{
    bool ret = true;
    uint8_t type = data->type;

    switch (type) {
        case BT_DATA_FLAGS:
        {
            uint8_t flags = data->data[0];
            // Discoverable Mode
            if (flags & BT_LE_AD_LIMITED) {
                printk("| PTS is in Limited Discoverable Mode...         |\n");
            }
            if (flags & BT_LE_AD_GENERAL) {
                printk("| PTS is in General Discoverable Mode...         |\n");
            }
            if (!(flags & (BT_LE_AD_LIMITED|BT_LE_AD_GENERAL))) {
                printk("| PTS is in Non Discoverable Mode...             |\n");
            }
            // BR EDR support
            if (flags & BT_LE_AD_NO_BREDR) {
                printk("| BR/EDR not supported...\n");
            }
            printk("|------------------------------------------------|\n");
            ret = false;
        }
        break;

        default:
        {

        }
        break;
    }

    BT_INFO("adv type: %d, len: %d, ret:%d\n",type, data->data_len, ret);

    return ret;
}

static void bqb_gap_le_adv_scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *ad)
{
    bt_addr_t zero_addr = {0};
    bt_addr_t *pts_addr = &g_bqb_gap_le_cntx.scan_addr;

    if (!memcmp(zero_addr.val, pts_addr->val, 6)) {
        BT_ERR("Not Set Scan address\n");
        return;
    }

    //Only print ADV info from PTS Dongle.
    if (!memcmp(pts_addr->val, info->addr->a.val, 6)) {

        printk("\n|-----------------------------------------------------|");
        printk("\n|Recv ADV from PTS dongle,(%d)addr:0x%02x%02x%02x%02x%02x%02x|",
            info->addr->type,
            info->addr->a.val[5], info->addr->a.val[4], info->addr->a.val[3],
            info->addr->a.val[2], info->addr->a.val[1], info->addr->a.val[0]);
        printk("\n|type:%d, props:0x%x, int:%d, rssi:%d, phy:%d, %d          |",
            info->adv_type, info->adv_props, info->interval, info->rssi, info->primary_phy, info->secondary_phy);
        printk("\n|-----------------------------------------------------|\n");
        if (ad) {
            bt_data_parse(ad, bqb_gap_le_adv_data_flags_parse, NULL);
        }

        if (g_bqb_gap_le_cntx.pasync_trigger && !g_bqb_gap_le_cntx.pa_sync) {
            int err = 0;
            struct bt_le_per_adv_sync_param sync_param = {0};

            bt_addr_le_copy(&sync_param.addr, info->addr);
            sync_param.options = (BT_LE_PER_ADV_SYNC_OPT_FILTER_DUPLICATE|BT_LE_PER_ADV_SYNC_OPT_REPORTING_INITIALLY_DISABLED);
            sync_param.sid = info->sid;
            sync_param.skip = 5;
            sync_param.timeout = 400;//interval_to_sync_timeout(info->interval);
            err = bt_le_per_adv_sync_create(&sync_param, &g_bqb_gap_le_cntx.pa_sync);
            if (err != 0) {
                g_bqb_gap_le_cntx.pasync_trigger = false;
                BT_ERR("err: %d\n", err);
                return;
            }
            BT_INFO("pa_sync:%p\n", g_bqb_gap_le_cntx.pa_sync);
        }
    }
}

static void bqb_gap_le_adv_scan_timeout(void)
{
    BT_INFO("\n");
}

static void bqb_gap_le_acl_connected_cb(struct bt_conn *conn, u8_t err)
{
    int ret = err;
    BT_INFO("acl connected, conn:%p, err:%d\n", conn, ret);
    if (ret == 0) {
        uint8_t type = hostif_bt_conn_get_type(conn);
        if (type != BT_CONN_TYPE_LE) {
            BT_ERR("Not support Link type:0x%x\n", type);
            return;
        }

        if (g_bqb_gap_le_cntx.conn == NULL) {
            g_bqb_gap_le_cntx.conn = conn;
        } else if (g_bqb_gap_le_cntx.conn == conn) {
            bt_security_t current_lev = bt_conn_get_security(conn);
            BT_INFO("orig lev:%d, target lev:%d\n", current_lev, g_bqb_gap_le_cntx.sec_level);
            if (current_lev < g_bqb_gap_le_cntx.sec_level) {
                ret = bt_conn_set_security(conn, g_bqb_gap_le_cntx.sec_level);
                BT_INFO("sec err:%d\n", ret);
            }
        } else {
            BT_INFO("acl other connection:%p\n", g_bqb_gap_le_cntx.conn);
        }
    }
}

static void bqb_gap_le_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    BT_INFO("acl disconnected, (%p, %p) reason:%d\n\n", conn, g_bqb_gap_le_cntx.conn, reason);
    if (conn == g_bqb_gap_le_cntx.conn) {
        g_bqb_gap_le_cntx.conn = NULL;
    }
}

static void bqb_gap_le_enter(void)
{
    if (!g_bqb_gap_le_cntx.init) {
        printk("\n|------------------------------|");
        printk("\n|->>>>>>Enter gaple test>>>>>>-|");
        printk("\n|------------------------------|\n");
        g_bqb_gap_le_cntx.init = true;
        g_bqb_gap_le_cntx.sec_level = BT_SECURITY_L2;
        hostif_bt_conn_cb_register(&bqb_gap_le_conn_cbs);

    #ifdef BQB_GAPLE_TEST_PA_PAWR_BIS
        bt_le_per_adv_sync_cb_register(&bqb_gap_le_pasync_cb);
    #endif /*BQB_GAPLE_TEST_PA_PAWR_BIS*/

    } else {
        //BT_WARN("gap le have been started!\n");
    }
}

static void bqb_gap_le_exit(void)
{
    if (g_bqb_gap_le_cntx.init) {
        printk("\n|-----------------------------|");
        printk("\n|-<<<<<<Exit gaple test<<<<<<-|");
        printk("\n|-----------------------------|\n");
        g_bqb_gap_le_cntx.init = false;
        hostif_bt_conn_cb_unregister(&bqb_gap_le_conn_cbs);
        g_bqb_gap_le_cntx.conn = NULL;

    #ifdef BQB_GAPLE_TEST_PA_PAWR_BIS
        bt_le_per_adv_sync_cb_unregister(&bqb_gap_le_pasync_cb);
    #endif /*BQB_GAPLE_TEST_PA_PAWR_BIS*/
        //memset((void*)&g_bqb_le_cntx, 0x0, sizeof(g_bqb_le_cntx.));
    } else {
        //BT_WARN("gap le have been stoped!\n");
    }
}

static void bqb_gap_le_adv_enable(bqb_gap_le_adv_type_t type, bqb_gap_le_adv_param_t param)
{
    struct bt_le_ext_adv_start_param start_param = {0};
    int err;
    bool direct_adv = false;

    BT_INFO("Send ADV in: type:%d, data_type:%d, (%p,%d)\n", type, param.data_type, g_bqb_gap_le_cntx.adv, g_bqb_gap_le_cntx.adv_index);

    if (!g_bqb_gap_le_cntx.adv) {
        struct bt_le_adv_param adv_params = {0};

        //ADV parameters
        adv_params.id = BT_ID_PUBLIC;
        adv_params.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
        adv_params.interval_max = BT_GAP_ADV_FAST_INT_MAX_2;

        switch (type) {
            case BQB_GAP_LE_ADV_NCONN:
            {
                adv_params.options = BT_LE_ADV_OPT_USE_IDENTITY;
            }
            break;

            case BQB_GAP_LE_ADV_SCAN:
            {
                adv_params.options = (BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_SCANNABLE);
            }
            break;

            case BQB_GAP_LE_ADV_CONN:
            {
                adv_params.options = (BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_CONNECTABLE);
            }
            break;

            case BQB_GAP_LE_ADV_NCONN_RPA:
            {
                adv_params.options = BT_LE_ADV_OPT_ADDR_RPA;
                bqb_utils_print_addr(param.peer->a.val, true);
                adv_params.peer = (bt_addr_le_t *)param.peer;
            }
            break;

            case BQB_GAP_LE_ADV_CONN_RPA:
            {
                adv_params.options = (BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_ADDR_RPA | BT_LE_ADV_OPT_CONNECTABLE);
                bqb_utils_print_addr(param.peer->a.val, true);
                adv_params.peer = (bt_addr_le_t *)param.peer;
            }
            break;

            case BQB_GAP_LE_ADV_DIR_CONN:
            {
                adv_params.options = (BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY);
                bqb_utils_print_addr(param.peer->a.val, true);
                adv_params.peer = (bt_addr_le_t *)param.peer;
                direct_adv = true;
            }
            break;

            //EXT ADV TYPE
            case BQB_GAP_LE_ADV_NCONN_PA:
            case BQB_GAP_LE_ADV_NCONN_PAWR:
            {
                adv_params.options = (BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_EXT_ADV);
            }
            break;

            default:
            {
                BT_ERR("No support adv type: %d!\n", type);
                return;
            }
            break;
        }

        err = bt_le_ext_adv_create(&adv_params, &bqb_gap_le_adv_callbacks, &g_bqb_gap_le_cntx.adv);
        if (err) {
            BT_INFO("Failed to create advertiser set (%d)\n", err);
            return;
        }

        g_bqb_gap_le_cntx.adv_index = bt_le_ext_adv_get_index(g_bqb_gap_le_cntx.adv);

        if (!direct_adv) {
            //ADV data
            start_param.timeout = 0;

            switch (param.data_type) {
                case BQB_GAP_LE_ADV_DATA_NON_DISC:
                {
                    g_bqb_gap_le_cntx.adv_data.type = BT_DATA_FLAGS;
                    g_bqb_gap_le_cntx.adv_payload[0] = BT_LE_AD_NO_BREDR;
                    g_bqb_gap_le_cntx.adv_data.data_len = 1;
               }
               break;

               case BQB_GAP_LE_ADV_DATA_LIMITED_DISC:
               {
                    g_bqb_gap_le_cntx.adv_data.type = BT_DATA_FLAGS;
                    g_bqb_gap_le_cntx.adv_payload[0] = BT_LE_AD_LIMITED;
                    g_bqb_gap_le_cntx.adv_data.data_len = 1;
                    start_param.timeout = 1000; //10s
               }
               break;

               case BQB_GAP_LE_ADV_DATA_GENERAL_DISC:
               {
                    g_bqb_gap_le_cntx.adv_data.type = BT_DATA_FLAGS;
                    g_bqb_gap_le_cntx.adv_payload[0] = BT_LE_AD_GENERAL;
                    g_bqb_gap_le_cntx.adv_data.data_len = 1;
               }
               break;

               case BQB_GAP_LE_ADV_DATA_UUID:
               {
                    g_bqb_gap_le_cntx.adv_data.type = BT_DATA_UUID16_SOME;
                    //BT_UUID_PACS_VAL 0x1850
                    g_bqb_gap_le_cntx.adv_payload[0] = 0x50;
                    g_bqb_gap_le_cntx.adv_payload[1] = 0x18;
                    g_bqb_gap_le_cntx.adv_data.data_len = 2;
               }
               break;

               case BQB_GAP_LE_ADV_DATA_MANUFACTURER:
               {
                    g_bqb_gap_le_cntx.adv_data.type = BT_DATA_MANUFACTURER_DATA;
                    g_bqb_gap_le_cntx.adv_payload[0] = 0xee;
                    g_bqb_gap_le_cntx.adv_payload[1] = 0xff;
                    g_bqb_gap_le_cntx.adv_data.data_len = 2;
               }
               break;

               case BQB_GAP_LE_ADV_DATA_NAME:
               {
                    char name[15] ="Act_host_bqb";
                    uint8_t len  = bqb_utils_strlen(name);
                    g_bqb_gap_le_cntx.adv_data.type = BT_DATA_NAME_COMPLETE;
                    memcpy(g_bqb_gap_le_cntx.adv_payload, name, len);
                    g_bqb_gap_le_cntx.adv_data.data_len = len;
               }
               break;

               case BQB_GAP_LE_ADV_DATA_TX_POWER:
               {
                    g_bqb_gap_le_cntx.adv_data.type = BT_DATA_TX_POWER;
                    g_bqb_gap_le_cntx.adv_payload[0] = 0xCC;
                    g_bqb_gap_le_cntx.adv_data.data_len = 1;
               }
               break;

               case BQB_GAP_LE_ADV_DATA_PERIPHERAL_INT_RANGE:
               {
                    g_bqb_gap_le_cntx.adv_data.type = BT_DATA_PERIPHERAL_INT_RANGE;
                    g_bqb_gap_le_cntx.adv_payload[0] = 0x02;
                    g_bqb_gap_le_cntx.adv_payload[1] = 0x01;
                    g_bqb_gap_le_cntx.adv_payload[2] = 0x04;
                    g_bqb_gap_le_cntx.adv_payload[3] = 0x03;
                    g_bqb_gap_le_cntx.adv_data.data_len = 4;
               }
               break;

               case BQB_GAP_LE_ADV_DATA_SOLICIT16:
               {
                    g_bqb_gap_le_cntx.adv_data.type = BT_DATA_SOLICIT16;
                    //BT_UUID_PACS_VAL 0x1850
                    g_bqb_gap_le_cntx.adv_payload[0] = 0x50;
                    g_bqb_gap_le_cntx.adv_payload[1] = 0x18;
                    g_bqb_gap_le_cntx.adv_data.data_len = 2;
               }
               break;

               case BQB_GAP_LE_ADV_DATA_SVC_DATA16:
               {
                    g_bqb_gap_le_cntx.adv_data.type = BT_DATA_SVC_DATA16;
                    g_bqb_gap_le_cntx.adv_payload[0] = 0x50;
                    g_bqb_gap_le_cntx.adv_payload[1] = 0x18;
                    g_bqb_gap_le_cntx.adv_data.data_len = 2;
               }
               break;

               case BQB_GAP_LE_ADV_DATA_GAP_APPEARANCE:
               {
                    g_bqb_gap_le_cntx.adv_data.type = BT_DATA_GAP_APPEARANCE;
                    //0x0841:Standalone Speaker
                    g_bqb_gap_le_cntx.adv_payload[0] = 0x41;
                    g_bqb_gap_le_cntx.adv_payload[1] = 0x08;
                    g_bqb_gap_le_cntx.adv_data.data_len = 2;
               }
               break;

               default:
               {
                    BT_ERR("No support ADV data type: 0x%2x\n", param.data_type);
                    hostif_bt_le_ext_adv_delete(g_bqb_gap_le_cntx.adv);
                    g_bqb_gap_le_cntx.adv = NULL;
                    return;
               }
            }

            g_bqb_gap_le_cntx.adv_data.data = g_bqb_gap_le_cntx.adv_payload;
            err = hostif_bt_le_ext_adv_set_data(g_bqb_gap_le_cntx.adv, &g_bqb_gap_le_cntx.adv_data, 1, NULL, 0);
            if (err) {
                BT_ERR("set adv data fail: %d\n",err);
                return;
             }
        #ifdef BQB_GAPLE_TEST_PA_PAWR_BIS
            if (BQB_GAP_LE_ADV_NCONN_PA == type) {
                bqb_gap_le_pa_create(false);
            } else if (BQB_GAP_LE_ADV_NCONN_PAWR == type) {
                bqb_gap_le_pa_create(true);
            }
        #endif /*BQB_GAPLE_TEST_PA_PAWR_BIS*/
        }
    }

    err = hostif_bt_le_ext_adv_start(g_bqb_gap_le_cntx.adv, &start_param);
    if (err) {
        hostif_bt_le_ext_adv_delete(g_bqb_gap_le_cntx.adv);
        g_bqb_gap_le_cntx.adv = NULL;
    }

    BT_INFO("Send ADV out: ret:%d, (%p,%d)\n", err , g_bqb_gap_le_cntx.adv, g_bqb_gap_le_cntx.adv_index);
}

static void bqb_gap_le_adv_command_handler(char *argv[])
{
    bqb_gap_le_adv_type_t type = BQB_GAP_LE_ADV_INVALID;
    char *cmd = argv[2];
    bqb_utils_char2hex(cmd[0], &type);
    if (BQB_GAP_LE_ADV_STOP == type) {
        bqb_gap_le_adv_stop();
    } else if (BQB_GAP_LE_ADV_DIR_CONN == type) {
        uint8_t temp_addr[6] = {0};
        bqb_gap_le_adv_param_t param = {0};
        bt_addr_le_t peer = {0};
        bqb_utils_str2hex(temp_addr, argv[3], 6);
        bqb_utils_bytes_reverse(peer.a.val, temp_addr, 6);
        peer.type = BT_ADDR_LE_PUBLIC;
        param.peer = &peer;
        bqb_gap_le_adv_enable(type, param);
    } else if (BQB_GAP_LE_ADV_NCONN_RPA == type || BQB_GAP_LE_ADV_CONN_RPA == type) {
        bqb_gap_le_adv_data_type_t data_type = BQB_GAP_LE_ADV_DATA_INVALID;
        uint8_t temp_addr[6] = {0};
        bqb_gap_le_adv_param_t param = {0};
        bt_addr_le_t peer = {0};

        //get adv data type
        cmd = argv[3];
        bqb_utils_char2hex(cmd[0], &data_type);
        param.data_type = data_type;
        //get peer addr
        cmd = argv[4];
        bqb_utils_str2hex(temp_addr, cmd, 6);
        bqb_utils_bytes_reverse(peer.a.val, temp_addr, 6);
        peer.type = BT_ADDR_LE_PUBLIC;
        param.peer = &peer;

        bqb_gap_le_adv_enable(type, param);
    } else {
        bqb_gap_le_adv_data_type_t data_type = BQB_GAP_LE_ADV_DATA_INVALID;
        bqb_gap_le_adv_param_t param = {0};
        cmd = argv[3];
        bqb_utils_char2hex(cmd[0], &data_type);
        param.data_type = data_type;
        bqb_gap_le_adv_enable(type, param);
    }
}

static void bqb_gap_le_scan_start(bqb_gap_le_scan_type_t type)
{
    int err;
    struct bt_le_scan_param scan_param = {0};

    //Default scan paramters.
    scan_param.options = BT_HCI_LE_SCAN_FILTER_DUP_ENABLE;
    scan_param.interval = BT_GAP_SCAN_FAST_INTERVAL;
    scan_param.window = BT_GAP_SCAN_FAST_WINDOW;
    scan_param.timeout = 0;

    if (BQB_GAP_LE_SCAN_PASSIVE == type) {
        scan_param.options = BT_HCI_LE_SCAN_PASSIVE;
    } else if (BQB_GAP_LE_SCAN_ACTIVE == type) {
        scan_param.options = BT_HCI_LE_SCAN_ACTIVE;
    } else {
        BT_ERR("Not support scan type: %d", type);
        return;
    }

    hostif_bt_le_scan_cb_register(&bqb_gap_le_scan_callbacks);

    err = hostif_bt_le_scan_start((const struct bt_le_scan_param *)&scan_param, NULL);
    if (err) {
        BT_ERR("fail: %d", err);
        if(err != -EALREADY) {
            hostif_bt_le_scan_cb_unregister(&bqb_gap_le_scan_callbacks);
        }
    }
}

static void bqb_gap_le_scan_command_handler(char *argv[])
{
    bqb_gap_le_adv_type_t type = BQB_GAP_LE_SCAN_INVALID;
    char *cmd = argv[2];

    bqb_utils_char2hex(cmd[0], &type);
    BT_INFO("scan type: %d\n", type);

    if (BQB_GAP_LE_SCAN_STOP == type) {
        hostif_bt_le_scan_stop();
        memset(g_bqb_gap_le_cntx.scan_addr.val, 0x0, 6);
    } else {
        cmd = argv[3];
        uint8_t temp_addr[6] = {0};

        //Store scan address.
        bqb_utils_str2hex(temp_addr, cmd, 6);
        bqb_utils_bytes_reverse(g_bqb_gap_le_cntx.scan_addr.val, temp_addr, 6);

        bqb_gap_le_scan_start(type);
    }
}

static void bqb_gap_le_connect(char *type_str, char *str_addr)
{
    uint8_t temp_addr[6] = {0};
    uint8_t add_type = 0;
    bt_addr_le_t addr = {0};
    int ret;

    bqb_utils_char2hex(type_str[0], &add_type);

    bqb_utils_str2hex(temp_addr, str_addr, 6);
    bqb_utils_bytes_reverse(addr.a.val, temp_addr, 6);

    if (0 == add_type) {
        addr.type = BT_ADDR_LE_PUBLIC;
    } else if(1 == add_type) {
        addr.type = BT_ADDR_LE_RANDOM;
    } else {
        BT_ERR("Not support type: %d\n", add_type);
    }

    bqb_utils_print_addr(addr.a.val, true);
    ret = bqb_gap_le_connect_acl(&addr, &g_bqb_gap_le_cntx.conn);
    BT_INFO("ret: %d, addr type:%d\n", ret, add_type);
}

static u8_t bqb_gap_le_get_remote_name_cb(struct bt_conn *conn, u8_t err,
    struct bt_gatt_read_params *params, const void *data, u16_t length)
{
    if ((g_bqb_gap_le_cntx.conn == conn) && !err && length) {
        char name_str[30] = {0} ;
        int len = 30 < length ? 30 : length;
        bqb_utils_hex2str(name_str,(char*) data, len);
        printk("\n|------------------------------------------------|");
        printk("\n|Get remote name: %s|", name_str);
        printk("\n|------------------------------------------------|\n");
    } else {
        BT_ERR("ret: %d, (conn:%p,%p)\n", err, g_bqb_gap_le_cntx.conn, conn);
    }

    return BT_GATT_ITER_STOP;
}

static void bqb_gap_le_get_remote_name(void)
{
    int ret = 0;

    g_bqb_gap_le_cntx.params.func = bqb_gap_le_get_remote_name_cb;
    g_bqb_gap_le_cntx.params.handle_count = 0;

    g_bqb_gap_le_cntx.gatt_uuid_16.uuid.type = BT_UUID_TYPE_16;
    g_bqb_gap_le_cntx.gatt_uuid_16.val = 0x2A00;

    g_bqb_gap_le_cntx.params.by_uuid.start_handle = 0;
    g_bqb_gap_le_cntx.params.by_uuid.end_handle = 0xFFFF;
    g_bqb_gap_le_cntx.params.by_uuid.uuid = &g_bqb_gap_le_cntx.gatt_uuid_16.uuid;

    ret = bt_gatt_read(g_bqb_gap_le_cntx.conn, &g_bqb_gap_le_cntx.params);

    BT_INFO("ret: %d\n", ret);
}

static void bqb_gap_le_get_rpa_timeout(char *timeout_str)
{
    int ret = 0;
    uint8_t to_array[2] = {0};
    uint16_t timeout = 0;
    bqb_utils_str2hex(to_array, timeout_str, 2);
    timeout = to_array[0]<<8|to_array[1];

    BT_INFO("rpa timeout: %d\n", timeout);
    ret = bt_le_set_rpa_timeout(timeout);

    BT_INFO("ret: %d\n", ret);
}

static void bqb_gap_le_clear_list(void)
{
    hostif_bt_le_clear_list();
}

static void bqb_gap_conn_update(char *min, char *max, char *latency, char *timeout)
{
    int ret = 0;
    struct bt_le_conn_param param = {0};
    uint8_t value[2] = {0};

    //interval_min
    bqb_utils_str2hex(value, min, 2);
    param.interval_min = (value[0]<<8|value[1]);
    //interval_max
    bqb_utils_str2hex(value, max, 2);
    param.interval_max = (value[0]<<8|value[1]);
    //latency
    bqb_utils_str2hex(value, latency, 2);
    param.latency = (value[0]<<8|value[1]);
    //timeout
    bqb_utils_str2hex(value, timeout, 2);
    param.timeout = (value[0]<<8|value[1]);

    BT_INFO("min:0x%2x, max:0x%2x,latency:0x%2x,timeout:0x%2x\n",
        param.interval_min, param.interval_max, param.latency, param.timeout);

    ret = bt_conn_le_param_update(g_bqb_gap_le_cntx.conn, &param);
}

extern int bt_l2cap_update_conn_param(struct bt_conn *conn, const struct bt_le_conn_param *param);
static void bqb_gap_l2cap_conn_update(char *min, char *max, char *latency, char *timeout)
{
    int ret = 0;
    struct bt_le_conn_param param = {0};
    uint8_t value[2] = {0};

    //interval_min
    bqb_utils_str2hex(value, min, 2);
    param.interval_min = (value[0]<<8|value[1]);
    //interval_max
    bqb_utils_str2hex(value, max, 2);
    param.interval_max = (value[0]<<8|value[1]);
    //latency
    bqb_utils_str2hex(value, latency, 2);
    param.latency = (value[0]<<8|value[1]);
    //timeout
    bqb_utils_str2hex(value, timeout, 2);
    param.timeout = (value[0]<<8|value[1]);

    BT_INFO("min:0x%2x, max:0x%2x,latency:0x%2x,timeout:0x%2x\n",
        param.interval_min, param.interval_max, param.latency, param.timeout);

    ret = bt_l2cap_update_conn_param(g_bqb_gap_le_cntx.conn, &param);
}


static void bqb_gap_le_config_sec_level(uint8_t level)
{
    BT_INFO("sec level: %d\n", level);
    if (level <= BT_SECURITY_L4) {
        g_bqb_gap_le_cntx.sec_level = (bt_security_t)level;
    } else {
        BT_ERR("Unsupport sec level: %d\n", level);
    }
}

static void bqb_gap_le_set_bondable(bool en)
{
    BT_INFO("set bondable: %d\n", en);
    bt_set_bondable(en);
}


static void bqb_gap_le_register_sec(bool en)
{
    BT_INFO("register sec: %d\n", en);
    if (en) {
        hostif_bt_conn_le_auth_cb_register(&bqb_gap_le_io_default_cbs);
    } else {
        hostif_bt_conn_le_auth_cb_register(NULL);
    }
}

#if defined(BQB_GAPLE_TEST_PA_PAWR_BIS)
static void bqb_gap_le_iso_recv_cb(struct bt_iso_chan *chan, const struct bt_iso_recv_info *info, struct net_buf *buf)
{
    char data_str[41] = {0};
    printk("\n|---------------------------------------------------|");
    printk("\n|Recv iso data, flag:%d,ssn:%d, ts:%d  len:%d, len:%d |", info->flags, info->sn, info->ts, buf->len, chan->iso->rx_len);
    if (buf && buf->len) {
        uint16_t len = buf->len;
        if (len*2 > 20) {
            len = 20;
        }
        bqb_utils_hex2str(data_str, buf->data, len);
        printk("\n|Recv data: %s|",data_str);
    }
    printk("\n|---------------------------------------------------|\n");
}

static void bqb_gap_le_iso_connected_cb(struct bt_iso_chan *chan)
{
    BT_INFO("ISO Channel %p connected\n", chan);
}

static void bqb_gap_le_iso_disconnected_cb(struct bt_iso_chan *chan, uint8_t reason)
{
    BT_INFO("ISO Channel %p disconnected with reason 0x%02x\n", chan, reason);
}

static int bqb_gap_le_bis_direct_send_cb(uint16_t handle, uint8_t *buf, uint16_t *len)
{
    uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    char data_str[17] = {0};
    memcpy(buf, data, 8);

    bqb_utils_hex2str(data_str, data, 8);
    printk("\n|------------------------------------------------|");
    printk("\n| Send BIS data:[%s]", data_str);
    printk("\n|------------------------------------------------|\n");

    memcpy(buf, data, 8);
    *len = 8;
    return 0;
}

static int bqb_gap_le_bis_direct_recv_cb(uint16_t handle, uint8_t *buf, uint16_t len,uint32_t data_rx_flag)
{
    printk("Recv BIS rx data: %d, flag%d\n", len, data_rx_flag);
    return 0;
}

static void bqb_gap_le_recv_biginfo_cb(struct bt_le_per_adv_sync *sync, const struct bt_iso_biginfo *biginfo)
{
    int err = 0;
    struct bt_iso_big_sync_param big_params = {0};
    printk("\n|---------------------------------------------------------------|");
    printk("\n|Recv BIG info with PTS dongle,(%d)addr:0x%02x%02x%02x%02x%02x%02x |",
        biginfo->addr->type,
        biginfo->addr->a.val[5], biginfo->addr->a.val[4], biginfo->addr->a.val[3],
        biginfo->addr->a.val[2], biginfo->addr->a.val[1], biginfo->addr->a.val[0]);
    printk("\n|sid: %d, num: %d, sub: %d, iso: %d, bn: %d, irc: %d, pdu: %d, sdu: %d, %d, phy: %d, f: %d, e: %d",
        biginfo->sid, biginfo->num_bis, biginfo->sub_evt_count, biginfo->iso_interval,
        biginfo->burst_number, biginfo->rep_count, biginfo->max_pdu, biginfo->sdu_interval,
        biginfo->max_sdu, biginfo->phy, biginfo->framing, biginfo->encryption);
    printk("\n|---------------------------------------------------------------|\n");

    if (!g_bqb_gap_le_cntx.big) {

        bis_recv_host_cbk = bqb_gap_le_bis_direct_recv_cb;

        big_params.bis_channels = bqb_gap_le_bis_channels;
        big_params.num_bis = biginfo->num_bis;
        big_params.mse = 0; /* Let controller decide */
        big_params.bis_bitfield = BIT(1);
        big_params.sync_timeout = BT_ISO_SYNC_MSE_MAX;
        big_params.encryption = biginfo->encryption;

        err = bt_iso_big_sync(sync, &big_params, &g_bqb_gap_le_cntx.big);
        if (err) {
            BT_ERR("Unable to sync to BIG (err %d)\n", err);
        }
    } else {
        BT_ERR("BIG has been created!\n");
    }
}

static void bqb_gap_le_create_big(bool enc, uint8_t *bcode)
{
    int err = 0;
    struct bt_iso_big_create_test_param param = {0};

    param.num_bis = 1;
    param.bis_channels = bqb_gap_le_bis_channels;
    param.framing = BT_ISO_FRAMING_UNFRAMED;
    param.packing = BT_ISO_PACKING_SEQUENTIAL;
    param.interval = 10000;

    param.iso_interval = 2*BT_ISO_INTERVAL_MIN; //10ms
    param.nse = 6;
    param.max_pdu = 100;
    param.bn = 1;
    param.irc = 3;
    param.pto = 0;
    param.encryption = false;

    if (enc && bcode) {
        param.encryption = true;
        memcpy(param.bcode, bcode, BT_ISO_BROADCAST_CODE_SIZE);
    }

    err = hostif_bt_iso_big_create_test(g_bqb_gap_le_cntx.adv, &param, &g_bqb_gap_le_cntx.big);

    if (!err) {
        bis_send_host_cbk = bqb_gap_le_bis_direct_send_cb;
    }
    BT_INFO("create big ret:%d\n",err);
}

static void bqb_gap_le_terminate_big(void)
{
    int err = 0;
    BT_INFO("terminate big:%p\n",g_bqb_gap_le_cntx.big);

    if (g_bqb_gap_le_cntx.big) {
        err = hostif_bt_iso_big_terminate(g_bqb_gap_le_cntx.big);
    }

    BT_INFO("terminate big ret:%d\n",err);
}

static void bqb_gap_le_big_command_handler(char *argv[])
{
    bqb_gap_le_big_op_t op = BQB_GAP_LE_BIG_OP_INVALID;

    char *cmd = argv[2];
    bqb_utils_char2hex(cmd[0], &op );

    if (BQB_GAP_LE_BIG_OP_STOP == op) {
        bqb_gap_le_terminate_big();
    } else if (BQB_GAP_LE_BIG_OP_START == op) {
        bqb_gap_le_create_big(false, NULL);
    } else if (BQB_GAP_LE_BIG_OP_START_ENC == op) {
        uint8_t bcode[BT_ISO_BROADCAST_CODE_SIZE] = {0};
        cmd = argv[3];
        bqb_utils_str2hex(bcode, cmd, BT_ISO_BROADCAST_CODE_SIZE);
        bqb_gap_le_create_big(true, bcode);
    } else {
        BT_ERR("not support big op: %d\n", op);
    }
}

static void bqb_gaple_pa_create_synced(char *type_str, char *str_addr)
{
    uint8_t temp_addr[6] = {0};
    uint8_t add_type = 0;
    bt_addr_le_t addr = {0};
    int ret = 0;
    struct bt_conn_le_create_synced_param synced_param;
    struct bt_le_conn_param conn_param;

    bqb_utils_char2hex(type_str[0], &add_type);

    bqb_utils_str2hex(temp_addr, str_addr, 6);
    bqb_utils_bytes_reverse(addr.a.val, temp_addr, 6);

    if (0 == add_type) {
        addr.type = BT_ADDR_LE_PUBLIC;
    } else if(1 == add_type) {
        addr.type = BT_ADDR_LE_RANDOM;
    } else {
        BT_ERR("Not support type: %d\n", add_type);
    }

    synced_param.peer = &addr;
    synced_param.subevent = 0;

    conn_param.interval_min = 32;
    conn_param.interval_max = 32;

    conn_param.latency = 0;
    conn_param.timeout = 400;

    ret = bt_conn_le_create_synced(g_bqb_gap_le_cntx.adv, &synced_param, &conn_param, &g_bqb_gap_le_cntx.conn);
	if (ret) {
		BT_ERR("err:%d\n", ret);
	}
}

static void bqb_gaple_pa_sync_transfer(void)
{
    int err = 0;
    if (!g_bqb_gap_le_cntx.conn || !g_bqb_gap_le_cntx.pa_sync) {
        BT_ERR("not ready pa_sync:%p,conn:%p\n", g_bqb_gap_le_cntx.pa_sync, g_bqb_gap_le_cntx.conn );
        return;
    }
    err = bt_le_per_adv_sync_transfer(g_bqb_gap_le_cntx.pa_sync, g_bqb_gap_le_cntx.conn, 0);
    BT_INFO("error: %d\n", err);
}

static void bqb_gaple_pa_set_info_transfer(void)
{
    int err = 0;
    if (!g_bqb_gap_le_cntx.conn || !g_bqb_gap_le_cntx.adv) {
        BT_ERR("not ready adv:%p,conn:%p\n", g_bqb_gap_le_cntx.adv, g_bqb_gap_le_cntx.conn);
        return;
    }

    err = bt_le_per_adv_set_info_transfer(g_bqb_gap_le_cntx.adv, g_bqb_gap_le_cntx.conn, 0);
    if (err) {
        BT_INFO("err: %d\n", err);
    }
}

static void bqb_gaple_pa_sync_past(bool report_en)
{
    int err;
    uint8_t report_flag = 1;
    struct bt_le_per_adv_sync_transfer_param param = {0};

    if (!g_bqb_gap_le_cntx.conn) {
        BT_ERR("not ready conn:%p\n", g_bqb_gap_le_cntx.conn);
        return;
    }

    if (!report_en) {
        report_flag = 0;
        BT_INFO("just sync, do not report\n");
    } else {
        BT_INFO("sync and report\n");
	}

    param.skip = 5;
    param.timeout = 100;//1s

    if (report_flag) {
        err = bt_le_per_adv_sync_transfer_subscribe(g_bqb_gap_le_cntx.conn, &param);
    } else {
        err = bt_le_per_adv_sync_transfer_unsubscribe(g_bqb_gap_le_cntx.conn);
    }
    BT_INFO("err:%d\n", err);
}

static struct net_buf_simple *bqb_gap_le_pa_data_buf_init(void)
{
    bt_addr_le_t local_addr = {0};
    NET_BUF_SIMPLE_DEFINE_STATIC(rsp_buf, sizeof(bt_addr_le_t) + 2 * sizeof(uint8_t));

    bqb_utils_get_local_public_addr(&local_addr);

    /* Respond with own address for the advertiser to connect to */
    net_buf_simple_reset(&rsp_buf);
    net_buf_simple_add_u8(&rsp_buf, sizeof(bt_addr_le_t) + 1);
    net_buf_simple_add_u8(&rsp_buf, BT_DATA_LE_BT_DEVICE_ADDRESS);
    net_buf_simple_add_mem(&rsp_buf, &local_addr.a, sizeof(local_addr.a));
    net_buf_simple_add_u8(&rsp_buf, local_addr.type);

    return &rsp_buf;
}

static void bqb_gap_le_adv_pawr_data_request_cb(struct bt_le_ext_adv *adv, const struct bt_le_per_adv_data_request *request)
{
    int err = 0;
    uint8_t num_subevts = 0;
    struct bt_le_per_adv_subevent_data_params params = {0};

    printk("[pawr data req]start:%d, cnt:%d\n", request->start, request->count);

    params.subevent = request->start;
    params.response_slot_start = 0;
    params.response_slot_count = 2;
    params.data = bqb_gap_le_pa_data_buf_init();
    num_subevts = 1;

    err = bt_le_per_adv_set_subevent_data(g_bqb_gap_le_cntx.adv, num_subevts, &params);
    if (err != 0) {
        BT_ERR("err: %d\n", err);
    }

}

static void bqb_gap_le_adv_pawr_response_cb(struct bt_le_ext_adv *adv, struct bt_le_per_adv_response_info *info, struct net_buf_simple *buf)
{

    BT_INFO("[pawr resp]subevent:%d, status:%d, resp_slot:%d\n", info->subevent, info->tx_status, info->response_slot);
}

static void bqb_gap_le_pa_create(bool is_pawr)
{
    struct bt_le_per_adv_param pa_param = {0};
    int err = 0;
    struct bt_le_ext_adv *ea = g_bqb_gap_le_cntx.adv;

    BT_INFO("create pa, ea:%p\n", ea);

    if (ea == NULL) {
        BT_WARN("please start EXT ADV firstly!");
        return;
    }

    /* 180ms by default */
    pa_param.interval_min = 80;
    pa_param.interval_max = 120;
    pa_param.options = BT_LE_PER_ADV_OPT_NONE;

    if (is_pawr) {
        pa_param.num_subevents = 2;
    }

    if (pa_param.num_subevents) {
        pa_param.subevent_interval = 32;//SUBEVENT_INTERVAL;
        pa_param.response_slot_delay = 1;
        pa_param.response_slot_spacing = 10;
        pa_param.num_response_slots = 2;
    }

    err = bt_le_per_adv_set_param(ea, &pa_param);
    if (err != 0) {
        BT_ERR("pa param ret: %d\n", err);
        return;
    }

    g_bqb_gap_le_cntx.pa_data.type = BT_DATA_FLAGS;
    g_bqb_gap_le_cntx.pa_payload[0] = BT_LE_AD_GENERAL;
    g_bqb_gap_le_cntx.pa_data.data_len = 1;

    err = bt_le_per_adv_set_data(ea, &(g_bqb_gap_le_cntx.pa_data), 1);
    if (err != 0) {
        BT_ERR("set pa data ret: %d\n", err);
        return;
     }

    err = bt_le_per_adv_start(ea);
    if (err != 0) {
        BT_ERR("pa start ret: %d\n", err);
    }
}

static void bqb_gap_le_pa_start(void)
{
    int err = 0;
    struct bt_le_ext_adv *ea = g_bqb_gap_le_cntx.adv;

    BT_INFO("start pa, ea:%p\n", ea);

    if (ea == NULL) {
        BT_WARN("please start EXT ADV firstly!");
        return;
    }

    err = bt_le_per_adv_start(ea);

    if (err != 0) {
        BT_ERR("pa start ret: %d\n", err);
    }
}

static void bqb_gap_le_pa_stop(void)
{
    int err = 0;
    struct bt_le_ext_adv *ea = g_bqb_gap_le_cntx.adv;

    BT_INFO("stop pa, ea:%p\n", ea);

    if (ea == NULL) {
        BT_WARN("No ext adv start!");
        return;
    }

    err = bt_le_per_adv_stop(ea);
    if (err != 0) {
        BT_ERR("pa stop ret: %d\n", err);
    }
}

static void bqb_gap_le_pa_command_handler(char *argv[])
{
    bqb_gap_le_pa_type_t type = BQB_GAP_LE_PA_INVALID;
    char *cmd = argv[2];
    bqb_utils_char2hex(cmd[0], &type);

    if (BQB_GAP_LE_PA_EA_START == type) {
        bqb_gap_le_adv_param_t param = {0};
        param.data_type = BQB_GAP_LE_ADV_DATA_GENERAL_DISC;
        bqb_gap_le_adv_enable(BQB_GAP_LE_ADV_NCONN_PA, param);
    } else if (BQB_GAP_LE_PAWR_EA_START == type) {
        bqb_gap_le_adv_param_t param = {0};
        param.data_type = BQB_GAP_LE_ADV_DATA_GENERAL_DISC;
        bqb_gap_le_adv_enable(BQB_GAP_LE_ADV_NCONN_PAWR, param);
    } else if (BQB_GAP_LE_PA_ONLY_START == type) {
        bqb_gap_le_pa_start();
    } else if (BQB_GAP_LE_PA_EA_STOP == type) {
        bqb_gap_le_pa_stop();
        bqb_gap_le_adv_stop();
    } else if (BQB_GAP_LE_PA_ONLY_STOP == type) {
        bqb_gap_le_pa_stop();
    } else {
        BT_ERR("not support pa type: %d\n", type);
    }
}

static void  bqb_gap_le_pasync_synced_cb(struct bt_le_per_adv_sync *sync, struct bt_le_per_adv_sync_synced_info *info)
{
    printk("\n|---------------------------------------------------------------|");
    printk("\n|Recv PA Sync sucess evt with PTS dongle,(%d)addr:0x%02x%02x%02x%02x%02x%02x |",
        info->addr->type,
        info->addr->a.val[5], info->addr->a.val[4], info->addr->a.val[3],
        info->addr->a.val[2], info->addr->a.val[1], info->addr->a.val[0]);
    printk("\n|SID:%d, interval:%d, recv_enabled:%d                         |",
        info->sid, info->interval, info->recv_enabled);

    printk("\n|num_sub:%d,sub intv:%d, rsp_slot delay:%d, spacing:%d         |",
        info->num_subevents,info->subevent_interval, info->response_slot_delay,info->response_slot_spacing);

    printk("\n|---------------------------------------------------------------|\n");

    if (info->num_subevents > 0) {
        int err = 0;
        struct bt_le_per_adv_sync_subevent_params params = {0};
        uint8_t subevents[2] = {0,1};

        //resp_buf_init();
        params.properties = 0;
        params.num_subevents = info->num_subevents;
        params.subevents = subevents;
        err = bt_le_per_adv_sync_subevent(sync, &params);
        if (err) {
            BT_ERR("sync subevent err:%d\n", err);
            return;
        }

        err = bt_le_per_adv_sync_recv_enable(sync);
        if (err) {
            BT_ERR("recv enable err:%d\n", err);
            return;
        }
    }
}

static void bqb_gap_le_pasync_data_recv_cb(struct bt_le_per_adv_sync *sync, const struct bt_le_per_adv_sync_recv_info *info, struct net_buf_simple *buf)
{
    char data_str[81] = {0};

    printk("\n|----------------------------------------------------|");
    printk("\n|Recv PA data from PTS dongle,(%d)addr:0x%02x%02x%02x%02x%02x%02x |",
        info->addr->type,
        info->addr->a.val[5], info->addr->a.val[4], info->addr->a.val[3],
        info->addr->a.val[2], info->addr->a.val[1], info->addr->a.val[0]);
    printk("\n|SID:%d, tx_power:%d, rssi:%d                        |",
        info->sid, info->tx_power, info->rssi);

    printk("\n|evt counter:%d,sub evt:%d, len:%d                    |",
        info->periodic_event_counter, info->subevent, buf->len);

     if (buf && buf->len) {
        uint16_t len = buf->len;
        if (len*2 > 80) {
            len = 80;
        }
        bqb_utils_hex2str(data_str, buf->data, len);
        printk("\n|Recv data: %s|",data_str);
    }
    printk("\n|----------------------------------------------------|\n");

    if (info->subevent !=0) {
        int err = 0;
        static struct bt_le_per_adv_response_params rsp_params = {0};
        rsp_params.request_event = info->periodic_event_counter;
        rsp_params.request_subevent = info->subevent;
        rsp_params.response_subevent = info->subevent;
        rsp_params.response_slot = 1;

        g_bqb_gap_le_cntx.pa_sync_resp_buf = bqb_gap_le_pa_data_buf_init();
		
        err = bt_le_per_adv_set_response_data(sync, &rsp_params, g_bqb_gap_le_cntx.pa_sync_resp_buf);
        if (err) {
           BT_ERR("Failed to send response (err %d)\n", err);
		}
	}
}

static void bqb_gap_le_pasync_term_cb(struct bt_le_per_adv_sync *sync, const struct bt_le_per_adv_sync_term_info *info)
{
    BT_INFO("sid:%d,reason:%d\n", info->sid, info->reason);
    g_bqb_gap_le_cntx.pa_sync = NULL;
}

static void bqb_gap_le_pasync_start(void)
{
    g_bqb_gap_le_cntx.pasync_trigger = true;
    //bt_le_per_adv_sync_cb_register(&bqb_gap_le_pasync_cb);
}

static void bqb_gap_le_pasync_stop(void)
{
    int err = 0;
    struct bt_le_per_adv_sync *pasync = g_bqb_gap_le_cntx.pa_sync;

    BT_INFO("stop pasync, ea:%p\n", pasync);

    if (pasync == NULL) {
        BT_WARN("No pasync start!");
        return;
    }
    g_bqb_gap_le_cntx.pasync_trigger = false;
    //bt_le_per_adv_sync_cb_unregister(&bqb_gap_le_pasync_cb);
    err = bt_le_per_adv_sync_delete(pasync);
    if (err != 0) {
        BT_ERR("pasync stop ret: %d\n", err);
    }
}

static void bqb_gap_le_pasync_recv_start(void)
{
    bt_le_per_adv_sync_recv_enable(g_bqb_gap_le_cntx.pa_sync);
}

static void bqb_gap_le_pasync_recv_stop(void)
{
    bt_le_per_adv_sync_recv_disable(g_bqb_gap_le_cntx.pa_sync);
}

static void bqb_gap_le_pasync_command_handler(char *argv[])
{
    bqb_gap_le_pasync_type_t type = BQB_GAP_LE_PASYNC_INVALID;
    char *cmd = argv[2];
    bqb_utils_char2hex(cmd[0], &type);

    if (BQB_GAP_LE_PASYNC_SYNC_START == type) {
        bqb_gap_le_pasync_start();
    } else if (BQB_GAP_LE_PASYNC_SYNC_STOP == type) {
        bqb_gap_le_pasync_stop();
    } else if (BQB_GAP_LE_PASYNC_RECV_START == type) {
        bqb_gap_le_pasync_recv_start();
    } else if (BQB_GAP_LE_PASYNC_RECV_STOP == type) {
        bqb_gap_le_pasync_recv_stop();
    } else {
        BT_ERR("not support pa type: %d\n", type);
    }
}
#endif /*BQB_GAPLE_TEST_PA_PAWR*/

/****************************************************************************/
/*                          External functions                              */
/****************************************************************************/
void bqb_gap_le_adv_start(bqb_gap_le_adv_type_t type)
{
    if (type <= BQB_GAP_LE_ADV_CONN_RPA) {
        bqb_gap_le_adv_param_t param = {0};
        param.data_type = BQB_GAP_LE_ADV_DATA_NON_DISC;
        bqb_gap_le_adv_enable(type, param);
    } else {
        BT_ERR("Unsupport adv type:%d\n", type);
    }
}

void bqb_gap_le_adv_stop(void)
{
    BT_INFO("adv stop:%p\n", g_bqb_gap_le_cntx.adv);
    if (g_bqb_gap_le_cntx.adv) {
        hostif_bt_le_ext_adv_stop(g_bqb_gap_le_cntx.adv);
        hostif_bt_le_ext_adv_delete(g_bqb_gap_le_cntx.adv);
        g_bqb_gap_le_cntx.adv = NULL;
    }
}

int bqb_gap_le_connect_acl(bt_addr_le_t *peer, struct bt_conn **conn)
{
    int err = 0;

    const struct bt_conn_le_create_param create_param = {
        .options = BT_CONN_LE_OPT_NONE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
        .interval_coded = 0,
        .window_coded = 0,
        .timeout = 3000,       //Connection initiation timeout (N * 10 MS)
    };

    const struct bt_le_conn_param conn_param = {
        .interval_min = BT_GAP_INIT_CONN_INT_MIN, // Minimum Connection Interval
        .interval_max = BT_GAP_INIT_CONN_INT_MAX, // Maximum Connection Interval
        .latency = 0,                             //onnection Latency
        .timeout = 500,                           // Supervision Timeout (N * 10 ms)
    };

    err = bt_conn_le_create((const bt_addr_le_t *)peer, &create_param, &conn_param, conn);

    BT_INFO("le create ret:%d, addr type:%d\n", err, peer->type);

    if (err) {
    } else {
        /* unref connection obj in advance as app user */
        bt_conn_unref(*conn);
    }

    return err;
}

int bqb_gap_le_disconnect_acl(struct bt_conn *conn)
{
    BT_INFO("le disconnect:%p\n", conn);
    return hostif_bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

/****************************************************************************/
/*                          Shell Command Handler                           */
/****************************************************************************/

int bqb_gap_le_test_command_handler(int argc, char *argv[])
{
    char *cmd = NULL;

    if (argc < 2) {
        return -EINVAL;
    }

    cmd = argv[1];

    if (!strcmp(cmd, "enter")) {
        bqb_gap_le_enter();
    } else if (!strcmp(cmd, "exit")) {
        bqb_gap_le_exit();
    } else if (!strcmp(cmd, "adv")) {
        bqb_gap_le_adv_command_handler(argv);
    } else if (!strcmp(cmd, "scan")) {
        bqb_gap_le_scan_command_handler(argv);
    } else if (!strcmp(cmd, "con")) {
        bqb_gap_le_connect(argv[2], argv[3]);
    } else if (!strcmp(cmd, "disc")) {
        bqb_gap_le_disconnect_acl(g_bqb_gap_le_cntx.conn);
    } else if (!strcmp(cmd, "getname")) {
        bqb_gap_le_get_remote_name();
    } else if (!strcmp(cmd, "rpato")) {
        cmd = argv[2];
        bqb_gap_le_get_rpa_timeout(cmd);
    } else if (!strcmp(cmd, "clear")) {
         bqb_gap_le_clear_list();
    } else if (!strcmp(cmd, "upd")) {
        bqb_gap_conn_update(argv[2], argv[3], argv[4], argv[5]);
    } else if (!strcmp(cmd, "l2capupd")) {
        bqb_gap_l2cap_conn_update(argv[2], argv[3], argv[4], argv[5]);
    } else if (!strcmp(cmd, "cfgsec")) {
        uint8_t lev = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &lev);
        bqb_gap_le_config_sec_level(lev);
    } else if (!strcmp(cmd, "bond")) {
        uint8_t bond = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &bond);
        bqb_gap_le_set_bondable(bond ? true:false);
    } else if (!strcmp(cmd, "regsec")) {
        uint8_t en = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &en);
        bqb_gap_le_register_sec(en ? true:false);
#ifdef BQB_GAPLE_TEST_PA_PAWR_BIS
    } else if (!strcmp(cmd, "pa")) {
        bqb_gap_le_pa_command_handler(argv);
    } else if (!strcmp(cmd, "pasync")) {
        bqb_gap_le_pasync_command_handler(argv);
    } else if (!strcmp(cmd, "con_pa")) {
        bqb_gaple_pa_create_synced(argv[2], argv[3]);
    } else if (!strcmp(cmd, "pasynctransfer")) {
        bqb_gaple_pa_sync_transfer();
    } else if (!strcmp(cmd, "sendpainfo")) {
        bqb_gaple_pa_set_info_transfer();
    } else if (!strcmp(cmd, "transferset")) {
        uint8_t enable = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &enable);
        bqb_gaple_pa_sync_past((bool)enable);
    } else if (!strcmp(cmd, "big")) {
        bqb_gap_le_big_command_handler(argv);
#endif /*BQB_GAPLE_TEST_PA_PAWR_BIS*/
    } else {
        printk("The cmd is not existed!\n");
    }

    return 0;
}


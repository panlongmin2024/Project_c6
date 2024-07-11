/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief rfcomm for bqb gap test.
 */

#include <acts_bluetooth/host_interface.h>

#include "bqb_utils.h"
#include "bqb_gap.h"
#include "keys_br_store.h"


extern void bt_ssp_set_bondable(bool enable);

static void bqb_gap_write_scan_enable_cb(uint8_t status, uint8_t *data, uint16_t len);
static void bqb_gap_inquiry_cb(struct bt_br_discovery_result *result);

static void bqb_gap_acl_connected_cb(struct bt_conn *conn, u8_t err);
static void bqb_gap_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason);

static void bqb_gap_auth_cancel_cb(struct bt_conn *conn);
static void bqb_gap_auth_pairing_auto_confirm_cb(struct bt_conn *conn);

static void bqb_gap_auth_passkey_display_cb(struct bt_conn *conn, unsigned int passkey);
static void bqb_gap_auth_passkey_confirm_cb(struct bt_conn *conn, unsigned int passkey);
static void bqb_gap_auth_passkey_entry_cb(struct bt_conn *conn);
//static void bqb_gap_auth_pincode_entry_cb(struct bt_conn *conn, bool highsec);
static void bqb_gap_auth_pairing_confirm_cb(struct bt_conn *conn);

static void bqb_gap_l2cap_connected_cb(struct bt_l2cap_chan *chan);
static void bqb_gap_l2cap_disconnected_cb(struct bt_l2cap_chan *chan);
static void bqb_gap_l2cap_encrypt_changed_cb(struct bt_l2cap_chan *chan, uint8_t status);
static int bqb_gap_l2cap_recv_cb(struct bt_l2cap_chan *chan, struct net_buf *buf);
static int bqb_gap_l2cap_accept_cb(struct bt_conn *conn, struct bt_l2cap_chan **chan);

typedef struct bqb_gap_context {
    // connection
    bool init;
    struct bt_conn *conn;
    struct bt_conn_auth_cb *auth_default_cbs;
    struct bt_l2cap_br_chan br_chan;
    bt_security_t sec_level;
} bqb_gap_context_t;

static bqb_gap_context_t g_bqb_gap_cntx = {0};

static struct bt_conn_cb bqb_gap_conn_cbs = {
    .connected = bqb_gap_acl_connected_cb,
    .disconnected = bqb_gap_acl_disconnected_cb,
};

//No input&No output
static const struct bt_conn_auth_cb bqb_gap_conn_auth_noio_cbs = {
    .passkey_display = NULL,
    .passkey_entry = NULL,
    .passkey_confirm = NULL,
    .pincode_entry = NULL,
    .cancel = bqb_gap_auth_cancel_cb,
    .pairing_confirm = bqb_gap_auth_pairing_auto_confirm_cb,
};

//Display YesNo
static const struct bt_conn_auth_cb bqb_gap_conn_auth_display_yesno_cbs = {
    .passkey_display = bqb_gap_auth_passkey_display_cb,
    .passkey_entry = NULL,
    .passkey_confirm = bqb_gap_auth_passkey_confirm_cb,
    .pincode_entry = NULL,
    .cancel = bqb_gap_auth_cancel_cb,
    .pairing_confirm = bqb_gap_auth_pairing_confirm_cb,
};

//Keyborad only
static const struct bt_conn_auth_cb bqb_gap_conn_auth_kb_only_cbs = {
    .passkey_display = NULL,
    .passkey_entry = bqb_gap_auth_passkey_entry_cb,
    .passkey_confirm = NULL,
    .pincode_entry = NULL,
    .cancel = bqb_gap_auth_cancel_cb,
    .pairing_confirm = bqb_gap_auth_pairing_confirm_cb,
};

//Display only
static const struct bt_conn_auth_cb bqb_gap_conn_auth_display_only_cbs = {
    .passkey_display = bqb_gap_auth_passkey_display_cb,
    .passkey_entry = NULL,
    .passkey_confirm = NULL,
    .pincode_entry = NULL,
    .cancel = bqb_gap_auth_cancel_cb,
    .pairing_confirm = bqb_gap_auth_pairing_confirm_cb,
};

static const struct bt_l2cap_chan_ops bqb_gap_l2cap_ops_cbs = {
    .connected = bqb_gap_l2cap_connected_cb,
    .disconnected = bqb_gap_l2cap_disconnected_cb,
    .encrypt_change = bqb_gap_l2cap_encrypt_changed_cb,
    .recv = bqb_gap_l2cap_recv_cb
};

static struct bt_l2cap_server bqb_gap_l2cap_server = {
    .psm = 0x1001,
    .accept = bqb_gap_l2cap_accept_cb,
    .sec_level = BT_SECURITY_L0,
};


/**************************************************************************/
/*                          Static functions                              */
/**************************************************************************/

static void bqb_gap_write_scan_enable_cb(uint8_t status, uint8_t *data, uint16_t len)
{
    BT_INFO("%d\n",status);
}

static void bqb_gap_inquiry_cb(struct bt_br_discovery_result *result)
{
    if (result) {
        char name_str[128] = {0};
        if (result->len) {
            int len = result->len < 128 ? result->len : 128;
            bqb_utils_hex2str(name_str, (char *)result->name, len);
        }
        printk("\n|---------------------------------------|");
        printk("\n|Inquiry result, addr:0x%02x%02x%02x%02x%02x%02x   |",
                result->addr.val[5], result->addr.val[4], result->addr.val[3],
                result->addr.val[2], result->addr.val[1], result->addr.val[0]);
        if (result->len) {
            printk("\n|remote name:%s                      |", name_str);
        }
        printk("\n|cod:0x%2x%2x%2x, rssi:%d                |", result->cod[0], result->cod[1], result->cod[2], result->rssi);
        printk("\n|---------------------------------------|\n");
    }
}

static void bqb_gap_acl_connected_cb(struct bt_conn *conn, u8_t err)
{
    BT_INFO("acl connected, (conn:%p,%p), err:%d\n", conn, g_bqb_gap_cntx.conn, err);
    if (err == 0) {
        if (g_bqb_gap_cntx.conn == NULL) {
            g_bqb_gap_cntx.conn = conn;
        } else if (g_bqb_gap_cntx.conn == conn) {
            bt_security_t current_lev = bt_conn_get_security(conn);
            BT_INFO("orig:%d, target:%d\n", current_lev, g_bqb_gap_cntx.sec_level);
            if (current_lev < g_bqb_gap_cntx.sec_level) {
                int ret = bt_conn_set_security(conn, g_bqb_gap_cntx.sec_level);
                BT_INFO("sec ret:%d\n", ret);
            }
        } else {
            BT_INFO("acl other connection:%p\n", g_bqb_gap_cntx.conn);
        }
    }
}

static void bqb_gap_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    BT_INFO("acl disconnected, reason:%d\n", reason);
    g_bqb_gap_cntx.conn = NULL;
}

static void bqb_gap_auth_pairing_auto_confirm_cb(struct bt_conn *conn)
{
    BT_INFO(" Just work auto confirm.\n");
    int ret = bt_conn_auth_pairing_confirm(conn);
    BT_INFO("pair cfm ret: %d\n", ret);
}

static void bqb_gap_auth_passkey_display_cb(struct bt_conn *conn, unsigned int passkey)
{
    printk("\n---------------------------------------");
    printk("\n|              Passkey[%d]            |", passkey);
    printk("\n---------------------------------------\n");
    if (conn != g_bqb_gap_cntx.conn) {
        BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_gap_cntx.conn, conn);
    }
}

static void bqb_gap_auth_passkey_confirm_cb(struct bt_conn *conn, unsigned int passkey)
{
    printk("\n-----------------------------------------------------");
    printk("\n|  Please Confirm Passkey[%d],use: bqb gap pk_cfm |", passkey);
    printk("\n-----------------------------------------------------\n");
    if (conn != g_bqb_gap_cntx.conn) {
        BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_gap_cntx.conn, conn);
        return;
    }
}

static void bqb_gap_auth_passkey_entry_cb(struct bt_conn *conn)
{
    printk("\n---------------------------------------------------------");
    printk("\n| Please entry passkey, use: bqb gap pk <Passkey>    |");
    printk("\n---------------------------------------------------------\n");
    if (conn != g_bqb_gap_cntx.conn) {
       BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_gap_cntx.conn, conn);
    }
}


static void bqb_gap_auth_pairing_confirm_cb(struct bt_conn *conn)
{
    printk("\n--------------------------------------------------");
    printk("\n|  Please Confirm Pairing, use: bqb smp pair_cfm  |");
    printk("\n--------------------------------------------------\n");
    if (conn != g_bqb_gap_cntx.conn) {
       BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_gap_cntx.conn, conn);
    }
}

#if 0
static void bqb_gap_auth_pincode_entry_cb(struct bt_conn *conn, bool highsec)
{

}
#endif

static void bqb_gap_auth_cancel_cb(struct bt_conn *conn)
{
    printk("---------------------------------------\n");
    printk("|            cancel                   |\n");
    printk("---------------------------------------\n");
    if (conn != g_bqb_gap_cntx.conn) {
        BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_gap_cntx.conn, conn);
    }
}

static void bqb_gap_l2cap_connected_cb(struct bt_l2cap_chan *chan)
{
    BT_INFO("l2cap connected\n");
}

static void bqb_gap_l2cap_disconnected_cb(struct bt_l2cap_chan *chan)
{
    BT_INFO("l2cap disconnected to disconnect acl\n");
    //bqb_gap_disconnect_acl(g_bqb_gap_cntx.conn);
}

static void bqb_gap_l2cap_encrypt_changed_cb(struct bt_l2cap_chan *chan, uint8_t status)
{
    BT_INFO("enrypt changed: %d\n", status);
}

static int bqb_gap_l2cap_recv_cb(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
    BT_INFO("data recv, buf:%p\n", buf);
    return 0;
}

static int bqb_gap_l2cap_accept_cb(struct bt_conn *conn, struct bt_l2cap_chan **chan)
{
    BT_INFO("conn: %p\n", conn);

    g_bqb_gap_cntx.br_chan.chan.ops = &bqb_gap_l2cap_ops_cbs;
    g_bqb_gap_cntx.br_chan.rx.mtu = 672;
    *chan = &(g_bqb_gap_cntx.br_chan.chan);
    return 0;
}

static void bqb_gap_enter(void)
{
    if (!g_bqb_gap_cntx.init) {
        printk("\n|----------------------------|");
        printk("\n|->>>>>>Enter gap test>>>>>>-|");
        printk("\n|----------------------------|\n");
        g_bqb_gap_cntx.init = true;
        g_bqb_gap_cntx.sec_level = BT_SECURITY_L1;
        g_bqb_gap_cntx.auth_default_cbs = (struct bt_conn_auth_cb *)&bqb_gap_conn_auth_display_yesno_cbs;
        hostif_bt_conn_cb_register(&bqb_gap_conn_cbs);
        hostif_bt_conn_auth_cb_register(g_bqb_gap_cntx.auth_default_cbs);
    } else {
        BT_WARN("gap have been started!\n");
    }
}

static void bqb_gap_exit(void)
{
    if (g_bqb_gap_cntx.init) {
        printk("\n|---------------------------|");
        printk("\n|-<<<<<<Exit gap test<<<<<<-|");
        printk("\n|---------------------------|\n");
        g_bqb_gap_cntx.init = false;
        hostif_bt_conn_cb_unregister(&bqb_gap_conn_cbs);
        hostif_bt_conn_auth_cb_register(NULL);
        g_bqb_gap_cntx.conn = NULL;
        g_bqb_gap_cntx.auth_default_cbs = NULL;
        g_bqb_gap_cntx.sec_level = BT_SECURITY_L0;
    } else {
        BT_WARN("gap have been stoped!\n");
    }
}

static void bqb_gap_inquiry_enable(bqb_gap_inquiry_mode_t mode)
{
    int ret;
    BT_INFO("inquiry mode:0x%x\n", mode);

    if (BQB_GAP_INQUIRY_MODE_NONE == mode) {
        ret = hostif_bt_br_discovery_stop();
    } else if (BQB_GAP_INQUIRY_MODE_LIMITED == mode || BQB_GAP_INQUIRY_MODE_GENERAL == mode) {
        struct bt_br_discovery_param param = {0};
        param.length = 4;
        param.num_responses = 0;
        param.limited = false;
        if (BQB_GAP_INQUIRY_MODE_LIMITED == mode) {
            param.limited = true;
        }
        ret = hostif_bt_br_discovery_start((const struct bt_br_discovery_param *)&param, bqb_gap_inquiry_cb);
    } else {
        BT_ERR("Not supported inquiry mode:0x%x\n", mode);
    }
}

static void bqb_gap_set_scan_mode(bqb_gap_scan_mode_t scan_mode, bqb_gap_discovery_mode_t disc_mode)
{
    BT_INFO(" Set, scan_mode:0x%x, disc_mode:%d\n", scan_mode, disc_mode);

    if (BQB_GAP_DISC_MODE_GENERAL == disc_mode) {
        hostif_bt_br_write_iac(false);
    } else if (BQB_GAP_DISC_MODE_LIMITED == disc_mode) {
        hostif_bt_br_write_iac(true);
    } else if (BQB_GAP_DISC_MODE_NONE == disc_mode) {
        //Do nothing.
    } else {
        BT_ERR("Not supported disc mode:0x%x\n", disc_mode);
        return;
    }

    hostif_bt_br_write_scan_enable(scan_mode, bqb_gap_write_scan_enable_cb);
}

static void bqb_gap_set_scan_mode_commmand_handler(char *argv[])
{
    bqb_gap_scan_mode_t scan_mode = BQB_GAP_SCAN_MODE_INVALID;
    bqb_gap_discovery_mode_t disc_mode = BQB_GAP_DISC_MODE_INVALID;

    char* cmd = argv[2];
    bqb_utils_char2hex(cmd[0], &scan_mode);
    if (BQB_GAP_NO_SCANS == scan_mode || BQB_GAP_PAGE_SCAN_ONLY == scan_mode) {
        disc_mode = BQB_GAP_DISC_MODE_NONE;
    } else if (BQB_GAP_INQUIRY_SCAN_ONLY == scan_mode || BQB_GAP_BOTH_INQUIRY_PAGE_SCAN == scan_mode) {
        cmd = argv[3];
        bqb_utils_char2hex(cmd[0], &disc_mode);
    } else {
        BT_ERR("Not supported scan_mode:0x%x\n", scan_mode);
        return;
    }

    bqb_gap_set_scan_mode(scan_mode, disc_mode);
}
static void bqb_gap_connect(char *str_addr)
{
    g_bqb_gap_cntx.conn = bqb_gap_connect_br_acl(str_addr);
}

static void bqb_gap_connect_l2cap_chan(uint16_t psm, bt_security_t sec)
{
    int ret = 0;
    BT_INFO("connect PSM: 0x%x, lev:%d\n", psm, sec);
    g_bqb_gap_cntx.br_chan.chan.ops = &bqb_gap_l2cap_ops_cbs;
    g_bqb_gap_cntx.br_chan.chan.required_sec_level = sec;
    ret = bt_l2cap_chan_connect(g_bqb_gap_cntx.conn, &(g_bqb_gap_cntx.br_chan.chan), psm);
    BT_INFO("connect ret:%d\n", ret);
}

static void bqb_gap_auth_set_io(uint8_t io)
{
    BT_INFO("enable: %d\n", io);
    hostif_bt_conn_auth_cb_register(NULL);
    if (BT_IO_DISPLAY_ONLY == io) {
        hostif_bt_conn_auth_cb_register(&bqb_gap_conn_auth_display_only_cbs);
    } else if (BT_IO_DISPLAY_YESNO == io) {
        hostif_bt_conn_auth_cb_register(&bqb_gap_conn_auth_display_yesno_cbs);
    } else if (BT_IO_KEYBOARD_ONLY == io) {
        hostif_bt_conn_auth_cb_register(&bqb_gap_conn_auth_kb_only_cbs);
    } else if (BT_IO_NO_INPUT_OUTPUT == io) {
        hostif_bt_conn_auth_cb_register(&bqb_gap_conn_auth_noio_cbs);
    } else if (0x09 == io) { //Special case to set the default io.
        hostif_bt_conn_auth_cb_register(g_bqb_gap_cntx.auth_default_cbs);
    } else {
        BT_INFO("Not supported io: %d\n", io);
    }
}

static void bqb_gap_conn_auth_passkey_confirm(void)
{
    int ret = bt_conn_auth_passkey_confirm(g_bqb_gap_cntx.conn);
    BT_INFO("passkey cfm ret: %d\n", ret);
}

static void bqb_gap_conn_auth_passkey_entry(unsigned int passkey)
{
    int ret = bt_conn_auth_passkey_entry(g_bqb_gap_cntx.conn, passkey);
    BT_INFO("passkey entry:%d, ret: %d\n", passkey, ret);
}

static void bqb_gap_conn_auth_pairing_confirm(void)
{
    int ret = bt_conn_auth_pairing_confirm(g_bqb_gap_cntx.conn);
    BT_INFO("pair cfm ret: %d\n", ret);
}

static void bqb_gap_config_sec_level(uint8_t level)
{
    BT_INFO("sec level: %d\n", level);
    if (level <= BT_SECURITY_L4) {
        g_bqb_gap_cntx.sec_level = (bt_security_t)level;
    } else {
        BT_ERR("Unsupport sec level: %d\n", level);
    }
}

static void bqb_gap_set_security(bt_security_t sec)
{
    BT_INFO("set sec:0x%x\n", sec);
    //g_bqb_gap_cntx.sec_level = sec;
    bt_conn_set_security(g_bqb_gap_cntx.conn, sec);
}

static void bqb_gap_register_l2cap_server(char *psm_str, char seclev_str)
{
    uint8_t temp_psm[2] = {0};
    bt_security_t seclev = 0;
    int res;

    bqb_utils_str2hex(temp_psm, psm_str, 2);
    bqb_gap_l2cap_server.psm = (temp_psm[0]<<8 | temp_psm[1]);

    bqb_utils_char2hex(seclev_str, &seclev);
    bqb_gap_l2cap_server.sec_level = seclev;

    BT_INFO("psm: 0x%x, sec level: %d\n", bqb_gap_l2cap_server.psm, bqb_gap_l2cap_server.sec_level);

    res = bt_l2cap_br_server_register(&bqb_gap_l2cap_server);
    if (res) {
        BT_ERR("L2CAP server register fail: %d", res);
    }
}

static void bqb_gap_set_spp_bondable(bool en)
{
    BT_INFO("set ssp bondable: %d\n", en);
    bt_ssp_set_bondable(en);
}

static void bqb_gap_config_reset(void)
{
    BT_INFO("reset cfg params....\n");

    bt_ssp_set_bondable(true);
    hostif_bt_conn_auth_cb_register(NULL);
    hostif_bt_conn_auth_cb_register(g_bqb_gap_cntx.auth_default_cbs);
    g_bqb_gap_cntx.sec_level = BT_SECURITY_L1;

    bt_store_clear_linkkey(NULL);
    hostif_bt_br_write_iac(false);
    hostif_bt_br_write_scan_enable(BQB_GAP_NO_SCANS, bqb_gap_write_scan_enable_cb);
}

/****************************************************************************/
/*                          External functions                              */
/****************************************************************************/
void bqb_gap_auth_register(void)
{
    hostif_bt_conn_auth_cb_register(&bqb_gap_conn_auth_noio_cbs);
}

void bqb_gap_clean_linkkey(void)
{
    bt_store_clear_linkkey(NULL);
}

struct bt_conn *bqb_gap_connect_br_acl(char *str_addr)
{
    uint8_t temp_addr[6] = {0};
    struct bt_conn *conn = NULL;
    bt_addr_t addr = {0};

    bqb_utils_str2hex(temp_addr, str_addr, 6);
    bqb_utils_bytes_reverse(addr.val, temp_addr, 6);
    bqb_utils_print_addr(addr.val, true);

    conn = hostif_bt_conn_create_br((const bt_addr_t*)&addr, BT_BR_CONN_PARAM_DEFAULT);
    if (!conn) {
        BT_ERR("Connection failed\n");
    } else {
        BT_INFO("Connection pending, %p\n", conn);
        /* unref connection obj in advance as app user */
        hostif_bt_conn_unref(conn);
    }
    return conn;
}

int bqb_gap_disconnect_acl(struct bt_conn *conn)
{
    return hostif_bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

void bqb_gap_write_scan_enable(bqb_gap_scan_mode_t mode)
{
    BT_INFO(" write scan mode:0x%x\n", mode);
    bqb_gap_set_scan_mode(mode, BQB_GAP_DISC_MODE_GENERAL);
}

/****************************************************************************/
/*                          Shell Command Handler                           */
/****************************************************************************/
int bqb_gap_test_command_handler(int argc, char *argv[])
{
    char *cmd = NULL;

    if (argc < 2) {
        return -EINVAL;
    }

    cmd = argv[1];

    if (!strcmp(cmd, "enter")) {
        bqb_gap_enter();
    } else if (!strcmp(cmd, "exit")) {
        bqb_gap_exit();
    } else if (!strcmp(cmd, "scan")) {
        bqb_gap_set_scan_mode_commmand_handler(argv);
    } else if (!strcmp(cmd, "inquiry")) {
        bqb_gap_inquiry_mode_t mode = BQB_GAP_INQUIRY_MODE_INVALID;
        char* mode_str = argv[2];
        bqb_utils_char2hex(mode_str[0], &mode);
        bqb_gap_inquiry_enable(mode);
    } else if (!strcmp(cmd, "con")) {
        bqb_gap_connect(argv[2]);
    } else if (!strcmp(cmd, "disc")) {
        bqb_gap_disconnect_acl(g_bqb_gap_cntx.conn);
    } else if (!strcmp(cmd, "con_l2cap")) {
        uint8_t temp_psm[2] = {0};
        uint8_t lev = 0;
        bqb_utils_str2hex(temp_psm, argv[2], 2);
        cmd = argv[3];
        bqb_utils_char2hex(cmd[0], &lev);
        bqb_gap_connect_l2cap_chan(temp_psm[0]<<8 |temp_psm[1], lev);
    } else if (!strcmp(cmd, "io")) {
        uint8_t io = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &io);
        bqb_gap_auth_set_io(io);
    } else if (!strcmp(cmd, "pk_cfm")) {
        bqb_gap_conn_auth_passkey_confirm();
    } else if (!strcmp(cmd, "pk")) {
        unsigned int passkey = (unsigned int)bqb_utils_atoi(argv[2]);
        bqb_gap_conn_auth_passkey_entry(passkey);
    } else if (!strcmp(cmd, "pair_cfm")) {
        bqb_gap_conn_auth_pairing_confirm();
    } else if (!strcmp(cmd, "cfgsec")) {
        uint8_t lev = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &lev);
        bqb_gap_config_sec_level(lev);
    } else if (!strcmp(cmd, "clear")) {
        bqb_gap_clean_linkkey();
    } else if (!strcmp(cmd, "pair_start")) {
        uint8_t sec = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &sec);
        bqb_gap_set_security(sec);
    } else if (!strcmp(cmd, "regl2cap")) {
        cmd = argv[3];
        bqb_gap_register_l2cap_server(argv[2], cmd[0]);
    } else if (!strcmp(cmd, "bond")) {
        uint8_t bond = 0;
        cmd = argv[2];
        bqb_utils_char2hex(cmd[0], &bond);
        bqb_gap_set_spp_bondable(bond ? true:false);
    } else if (!strcmp(cmd, "cfgreset")) {
        bqb_gap_config_reset();
    } else {
        printk("The cmd is not existed!\n");
    }

    return 0;
}


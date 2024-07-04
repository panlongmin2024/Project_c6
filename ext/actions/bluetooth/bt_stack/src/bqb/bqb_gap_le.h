/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bqb gap le test
 */

#include <os_common_api.h>
#include <acts_bluetooth/host_interface.h>

#if defined(CONFIG_BT_PAWR) && defined(CONFIG_BT_PAST) && defined(CONFIG_BT_PER_ADV_SYNC_TRANSFER_SENDER)  && defined(CONFIG_BT_ISO_BROADCASTER)
#define BQB_GAPLE_TEST_PA_PAWR_BIS
#endif

#define BQB_GAP_LE_ADV_DATA_NON_DISC              0x00
#define BQB_GAP_LE_ADV_DATA_LIMITED_DISC          0x01
#define BQB_GAP_LE_ADV_DATA_GENERAL_DISC          0x02
#define BQB_GAP_LE_ADV_DATA_UUID                  0x03
#define BQB_GAP_LE_ADV_DATA_MANUFACTURER          0x04
#define BQB_GAP_LE_ADV_DATA_NAME                  0x05
#define BQB_GAP_LE_ADV_DATA_TX_POWER              0x06
#define BQB_GAP_LE_ADV_DATA_PERIPHERAL_INT_RANGE  0x07
#define BQB_GAP_LE_ADV_DATA_SOLICIT16             0x08
#define BQB_GAP_LE_ADV_DATA_SVC_DATA16            0x09
#define BQB_GAP_LE_ADV_DATA_GAP_APPEARANCE        0x0A
#define BQB_GAP_LE_ADV_DATA_INVALID               0xFF
typedef uint8_t bqb_gap_le_adv_data_type_t;

#define BQB_GAP_LE_ADV_STOP          0x00
#define BQB_GAP_LE_ADV_NCONN         0x01
#define BQB_GAP_LE_ADV_SCAN          0x02
#define BQB_GAP_LE_ADV_CONN          0x03
#define BQB_GAP_LE_ADV_NCONN_RPA     0x04
#define BQB_GAP_LE_ADV_CONN_RPA      0x05
#define BQB_GAP_LE_ADV_DIR_CONN      0x06
//EXT ADV
#define BQB_GAP_LE_ADV_NCONN_PA       0x07
#define BQB_GAP_LE_ADV_NCONN_PAWR     0x08
//END
#define BQB_GAP_LE_ADV_INVALID       0xFF
typedef uint8_t bqb_gap_le_adv_type_t;

#define BQB_GAP_LE_SCAN_STOP          0x00
#define BQB_GAP_LE_SCAN_PASSIVE       0x01
#define BQB_GAP_LE_SCAN_ACTIVE        0x02
#define BQB_GAP_LE_SCAN_INVALID       0xFF
typedef uint8_t bqb_gap_le_scan_type_t;

#define BQB_GAP_LE_PA_EA_STOP          0x00
#define BQB_GAP_LE_PA_ONLY_STOP        0x01
#define BQB_GAP_LE_PA_EA_START         0x02
#define BQB_GAP_LE_PAWR_EA_START       0x03
#define BQB_GAP_LE_PA_ONLY_START       0x04
#define BQB_GAP_LE_PA_INVALID          0xFF
typedef uint8_t bqb_gap_le_pa_type_t;

#define BQB_GAP_LE_PASYNC_SYNC_STOP     0x00
#define BQB_GAP_LE_PASYNC_SYNC_START      0x01
#define BQB_GAP_LE_PASYNC_RECV_START       0x02
#define BQB_GAP_LE_PASYNC_RECV_STOP     0x03
#define BQB_GAP_LE_PASYNC_INVALID        0xFF
typedef uint8_t bqb_gap_le_pasync_type_t;

#define BQB_GAP_LE_BIG_OP_STOP              0x00
#define BQB_GAP_LE_BIG_OP_START             0x01
#define BQB_GAP_LE_BIG_OP_START_ENC         0x02
#define BQB_GAP_LE_BIG_OP_INVALID           0xFF
typedef uint8_t bqb_gap_le_big_op_t;

typedef struct bqb_gap_le_adv_param {
    bqb_gap_le_adv_data_type_t data_type;
    bt_addr_le_t *peer;
} bqb_gap_le_adv_param_t;

int bqb_gap_le_test_command_handler(int argc, char *argv[]);
void bqb_gap_le_adv_start(bqb_gap_le_adv_type_t type);
void bqb_gap_le_adv_stop(void);
int bqb_gap_le_connect_acl(bt_addr_le_t *peer, struct bt_conn **conn);
int bqb_gap_le_disconnect_acl(struct bt_conn *conn);

typedef int (*bqb_gap_le_bis_tx_cb)(uint16_t handle, uint8_t *buf, uint16_t *len);
typedef int (*bqb_gap_le_bis_rx_cb)(uint16_t handle, uint8_t *buf, uint16_t len,uint32_t data_rx_flag);



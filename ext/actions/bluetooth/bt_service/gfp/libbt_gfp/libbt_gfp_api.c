#include <stdio.h>
#include <string.h>

#include <acts_bluetooth/hci.h>
#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/rfcomm.h>
#include <acts_bluetooth/sdp.h>
#include <acts_bluetooth/hci.h>
#include <acts_bluetooth/hci.h>
#include <acts_bluetooth/host_interface.h>

#include <btservice_api.h>
#include <btservice_gfp_api.h>


/////////////////////////////////////////////////////////////////
// Bluetooth Provider.
//
// Provides stubs for Bluetooth functionality, this
// implementation will vary significatly based on chipset.
/////////////////////////////////////////////////////////////////


static btsrv_gfp_pairing_request_callback created_pairing_request_callback;
//static fp_is_fastpair_start is_fastpair_started;
static struct bt_conn *gfp_conn = NULL;

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
    bd_address_t addr;
    btif_br_get_local_mac(&addr);
    //bytes_reverse(addr.val, addr.val, MAC_ADDRESS_LENGTH);
    SYS_LOG_INF("%x",passkey);
    gfp_conn = conn;
    if(created_pairing_request_callback){
        created_pairing_request_callback(addr.val,passkey);
    }
}

static void auth_pincode_entry(struct bt_conn *conn, bool highsec)
{
}

static void auth_cancel(struct bt_conn *conn)
{
	gfp_conn = NULL;
}

static void auth_pairing_confirm(struct bt_conn *conn)
{
	SYS_LOG_INF("auth_pairing_confirm");
}

static const struct bt_conn_auth_cb auth_cb_display_yes_no = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.passkey_confirm = auth_passkey_confirm,
	.pincode_entry = auth_pincode_entry,
	.cancel = auth_cancel,
	.pairing_confirm = auth_pairing_confirm,
};

static int register_auth_cb(bool reg_auth)
{
    SYS_LOG_INF("setting device capabilities %d",reg_auth);

	if (reg_auth) {
		hostif_bt_conn_auth_cb_register(&auth_cb_display_yes_no);
	} else {
		hostif_bt_conn_auth_cb_register(NULL);
	}
	return 0;
}

void btsrv_gfp_pairing_request_reg(btsrv_gfp_pairing_request_callback callback)
{
    created_pairing_request_callback = callback;
}

void btsrv_gfp_cap_io_set(bool enable)
{
    if (enable){
        register_auth_cb(true);
    }
    else{
        register_auth_cb(false);
    }
}

void btsrv_gfp_confirm_pairing_reply(bool success)
{
	if (!gfp_conn)
	{
		SYS_LOG_ERR("gfp_conn is null");
		return;
	}

    if (success){
        hostif_bt_conn_ssp_confirm_reply(gfp_conn);
    }
    else{
        hostif_bt_conn_ssp_confirm_neg_reply(gfp_conn);
    }
}


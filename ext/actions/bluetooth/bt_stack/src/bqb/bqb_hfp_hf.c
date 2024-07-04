#include "bqb_hfp_hf.h"
#include "hfp_internal.h"
#include "common/log.h"
#include "bqb_gap.h"
#include "bqb_utils.h"
#include <acts_bluetooth/host_interface.h>
#include <os_common_api.h>

typedef struct {
	struct bt_conn* conn;
	struct bt_conn* sco_conn;
	bool auto_conn_hf;
} bqb_hfp_hf_ctx_t;

static bqb_hfp_hf_ctx_t s_bqb_hfp_hf_ctx;

static void bqb_hf_acl_connected(struct bt_conn *conn, u8_t err)
{
	BT_INFO("type: %d, error: 0x%x",hostif_bt_conn_get_type(conn), err);
	if (BT_CONN_TYPE_SCO == hostif_bt_conn_get_type(conn)) {
		BT_INFO("Sco connected");
		s_bqb_hfp_hf_ctx.sco_conn = conn;
		return;
	}

	if (err) {
		hostif_bt_conn_unref(s_bqb_hfp_hf_ctx.conn);
		s_bqb_hfp_hf_ctx.conn = NULL;
	} else {
		s_bqb_hfp_hf_ctx.conn = conn;
		if (s_bqb_hfp_hf_ctx.auto_conn_hf) {
			hostif_bt_hfp_hf_connect(conn);
		}
	}
}

static bool bqb_hf_acl_connect_req(bt_addr_t *peer,uint8_t *cod)
{
	BT_INFO("cod: 0x%x", *cod);
	return true;
}

static void bqb_hf_acl_disconnected(struct bt_conn *conn, uint8_t reason)
{
	BT_INFO("disconnected : 0x%x", reason);
	if (conn == s_bqb_hfp_hf_ctx.conn) {
		s_bqb_hfp_hf_ctx.conn = NULL;
	}

	s_bqb_hfp_hf_ctx.sco_conn = NULL;
}

static struct bt_conn_cb s_bqb_hfp_bt_conn_cb = {
	.connect_req = bqb_hf_acl_connect_req,
	.connected = bqb_hf_acl_connected,
	.disconnected = bqb_hf_acl_disconnected,
};

static void bqb_hf_connected(struct bt_conn *conn)
{
	BT_INFO("HF connect success %p", conn);
}

static void bqb_hf_disconnected(struct bt_conn *conn)
{
	BT_INFO("HF disconnect success %p", conn);
}


static struct bt_hfp_hf_cb s_hf_cb = {
	.connected = bqb_hf_connected,
	.disconnected = bqb_hf_disconnected,
};


static void bqb_hfp_hf_init()
{
	memset(&s_bqb_hfp_hf_ctx, 0, sizeof(s_bqb_hfp_hf_ctx));
	s_bqb_hfp_hf_ctx.auto_conn_hf = true;
	hostif_bt_conn_cb_register(&s_bqb_hfp_bt_conn_cb);
	hostif_bt_hfp_hf_register_cb(&s_hf_cb);
	bqb_gap_auth_register();
}

static void bqb_hfp_hf_deinit()
{
	if (s_bqb_hfp_hf_ctx.conn) {
		hostif_bt_hfp_ag_disconnect(s_bqb_hfp_hf_ctx.conn);
		bqb_gap_disconnect_acl(s_bqb_hfp_hf_ctx.conn);
	}

	memset(&s_bqb_hfp_hf_ctx, 0, sizeof(s_bqb_hfp_hf_ctx));
}

static int bqb_hfp_hf_create_conn()
{
	bt_addr_t addr = {0};
	bqb_utils_read_pts_addr(&addr);
	s_bqb_hfp_hf_ctx.conn = hostif_bt_conn_create_br(&addr, BT_BR_CONN_PARAM_DEFAULT);
	if (!s_bqb_hfp_hf_ctx.conn) {
		BT_ERR("create acl failed");
		return -1;
	}
	hostif_bt_conn_unref(s_bqb_hfp_hf_ctx.conn);
	return 0;
}


int bqb_hfp_hf_test_command_handler(int argc, char *argv[])
{
	int ret = 0;
	char* cmd = argv[1];
	if (!strcmp(cmd, "start")) {
		bqb_hfp_hf_init();
	} else if (!strcmp(cmd, "stop")) {
		bqb_hfp_hf_deinit();
	} else if (!strcmp(cmd, "conn")) {
		ret = bqb_hfp_hf_create_conn();
	} else if (!strcmp(cmd, "disconn_acl")) {
		if (s_bqb_hfp_hf_ctx.conn) {
			ret = hostif_bt_conn_disconnect(s_bqb_hfp_hf_ctx.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		} else {
			ret = -1;
		}
	} else {
		BT_ERR("No This Cmd");
	}
	return ret;
}


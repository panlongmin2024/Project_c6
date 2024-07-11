#include "bqb_hfp_ag.h"
#include "hfp_internal.h"
#include "common/log.h"
#include "bqb_gap.h"
#include "bqb_utils.h"
#include <acts_bluetooth/host_interface.h>
#include <os_common_api.h>

#define AG_EVENT_RING  "RING"
#define AG_EVENT_CLIP  "+CLIP:\"1234567\",129"

#define AG_EVENT_NEGOTIATION_LC3_SWB  "+BCS:3"
#define AG_EVENT_NEGOTIATION_SBC  "+BCS:2"
#define AG_EVENT_NEGOTIATION_CVSD  "+BCS:1"
#define AG_EVENY_SETUP_AUDIO_CONN  "AT+BCC"
#define AG_EVENY_OK  "OK"
#define AG_EVENT_ERROR  "ERROR"

#define AG_EVENT_CALL_LIST_CALL1  "+CLCC: 1,0,0,0,0"
#define AG_EVENT_CALL_LIST_CALL2  "+CLCC: 2,1,1,0,0"


//static uint8_t s_hfp_ag_delay_work_stack[2048] __aligned(4);

typedef enum {
	BT_HFP_AG_CODEC_TYPE_CVSD = 0x1,
	BT_HFP_AG_CODEC_TYPE_MSBC = 0x2,
	BT_HFP_AG_CODEC_TYPE_LC3_SWB = 0x03
} bt_hfp_ag_codec_e;

typedef enum{
	BQB_CALL_STATE_IDLE,
	BQB_CALL_STATE_INCOMING,
	BQB_CALL_STATE_RING,
	BQB_CALL_STATE_SETUP,
	BQB_CALL_STATE_CODEC_NEGO,
	BQB_CALL_STATE_OUT_GOING_CALL,
	BQB_CALL_STATE_CREATE_AUDIO_CONN,
	BQB_CALL_STATE_AUDIO_CONNECTED,
} bqb_call_state_e;

typedef enum{
	BQB_AG_STATE_IDLE,
	BQB_AG_STATE_CONNECTING_ACL,
	BQB_AG_STATE_CONNECTED_ACL,
	BQB_AG_STATE_CONNECTING_AG,
	BQB_AG_STATE_CONNECTED_AG
} bqb_ag_state_e;

typedef enum {
	BQB_AG_FLAG_IGNOR_HF_ACCEPT_CALL = 0x1,
	BQB_AG_FLAG_IGNOR_HF_END_CALL    = 0x2,
	BQB_AG_FLAG_RETURN_ERROR         = 0x4,
	BQB_AF_FLAG_CALL_ACTIVE_PRE      = 0x8,
	BQB_AF_FLAG_CALL_LIST            = 0x10,
	BQB_AF_FLAG_IND_INIT_BY_USER     = 0x20,
} bqb_ag_flag_e;

typedef struct {
	struct bt_conn* conn;
	struct bt_conn* sco_conn;

	os_delayed_work work;

	uint32_t flags;
	uint8_t call1_status[16];
	uint8_t call2_status[16];
	uint8_t call_status_idx;

	bqb_call_state_e call_state;
	bqb_ag_state_e ag_state;
	bt_hfp_ag_codec_e codec;

	uint16_t ring_counter;
	bool auto_conn_ag;
} bqb_hfp_ag_ctx_t;

extern int atoi(const char* str);

static bqb_hfp_ag_ctx_t s_bqb_hfp_ag_ctx;
uint16_t s_ag_set_content_format = 0x0060;
static int bqb_hfp_ag_conn_ag();


static void bqb_ag_acl_connected(struct bt_conn *conn, u8_t err)
{
	BT_INFO("type: %d, error: 0x%x",hostif_bt_conn_get_type(conn), err);
	if (BT_CONN_TYPE_SCO == hostif_bt_conn_get_type(conn)) {
		BT_INFO("Sco connected");
		s_bqb_hfp_ag_ctx.sco_conn = conn;
		return;
	}

	if (err) {
		//hostif_bt_conn_unref(s_bqb_hfp_ag_ctx.conn);
		s_bqb_hfp_ag_ctx.conn = NULL;
	} else {
		s_bqb_hfp_ag_ctx.conn = conn;
		//hostif_bt_conn_ref(s_bqb_hfp_ag_ctx.conn);
		if (!(BQB_AF_FLAG_IND_INIT_BY_USER & s_bqb_hfp_ag_ctx.flags)) {
			hostif_bt_hfp_ag_update_indicator(CIEV_SERVICE_IND, 1);
			if (BQB_AF_FLAG_CALL_ACTIVE_PRE & s_bqb_hfp_ag_ctx.flags ) {
				hostif_bt_hfp_ag_update_indicator(CIEV_CALL_IND, 1);
			} else {
				hostif_bt_hfp_ag_update_indicator(CIEV_CALL_IND, 0);
			}
			hostif_bt_hfp_ag_update_indicator(CIEV_CALL_SETUP_IND, 0);
			hostif_bt_hfp_ag_update_indicator(CIEV_CALL_HELD_IND, 0);
			hostif_bt_hfp_ag_update_indicator(CIEV_SINGNAL_IND, 5);
			hostif_bt_hfp_ag_update_indicator(CIEV_ROAM_IND, 0);
			hostif_bt_hfp_ag_update_indicator(CIEV_BATTERY_IND, 5);
		}

		if (s_bqb_hfp_ag_ctx.auto_conn_ag) {
			bqb_hfp_ag_conn_ag();
		}
	}
}

static bool bqb_ag_acl_connect_req(bt_addr_t *peer,uint8_t *cod)
{
	BT_INFO("cod: 0x%x", *cod);
	return true;
}

static void bqb_ag_acl_disconnected(struct bt_conn *conn, uint8_t reason)
{
	BT_INFO("disconnected : 0x%x", reason);
	if (conn == s_bqb_hfp_ag_ctx.conn) {
		s_bqb_hfp_ag_ctx.conn = NULL;
		s_bqb_hfp_ag_ctx.ag_state = BQB_AG_STATE_IDLE;
		s_bqb_hfp_ag_ctx.call_state = BQB_CALL_STATE_IDLE;
		s_bqb_hfp_ag_ctx.auto_conn_ag = false;
	}

	s_bqb_hfp_ag_ctx.sco_conn = NULL;
}

static struct bt_conn_cb s_bqb_hfp_bt_conn_cb = {
	.connect_req = bqb_ag_acl_connect_req,
	.connected = bqb_ag_acl_connected,
	.disconnected = bqb_ag_acl_disconnected,
};

static void bqb_hfp_ag_callin_work_handler(os_work * work)
{
	BT_INFO("state: %d", s_bqb_hfp_ag_ctx.call_state);
	switch (s_bqb_hfp_ag_ctx.call_state) {
		case BQB_CALL_STATE_INCOMING:
		{
			s_bqb_hfp_ag_ctx.call_state = BQB_CALL_STATE_RING;
		}
		case BQB_CALL_STATE_RING:
		{
			s_bqb_hfp_ag_ctx.ring_counter++;
			hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, AG_EVENT_RING, strlen(AG_EVENT_RING));
			hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, AG_EVENT_CLIP, strlen(AG_EVENT_CLIP));

			if (s_bqb_hfp_ag_ctx.ring_counter >= 80) {
				s_bqb_hfp_ag_ctx.ring_counter = 0;
				BT_WARN("Ring Timeout....");
				hostif_bt_hfp_ag_update_indicator(CIEV_CALL_SETUP_IND, 0);
				k_sleep(500);
				hostif_bt_hfp_ag_update_indicator(CIEV_CALL_IND, 0);
			} else {
				os_delayed_work_submit(&s_bqb_hfp_ag_ctx.work, 2000);
			}
			break;
		}
		case BQB_CALL_STATE_OUT_GOING_CALL:
		{
			hostif_bt_hfp_ag_update_indicator(CIEV_CALL_SETUP_IND, 2);
			break;
		}
		default:
			break;
	}
}

static int bqb_hfp_ag_create_acl_conn(char* str_addr)
{
	s_bqb_hfp_ag_ctx.conn = bqb_gap_connect_br_acl(str_addr);
	if (!s_bqb_hfp_ag_ctx.conn) {
		BT_ERR("create acl failed");
		return -1;
	}
	return 0;
}

static int bqb_hfp_ag_create_acl_conn_by_stored_addr()
{
	bt_addr_t addr = {0};
	bqb_utils_read_pts_addr(&addr);
	s_bqb_hfp_ag_ctx.conn = hostif_bt_conn_create_br(&addr, BT_BR_CONN_PARAM_DEFAULT);
	if (!s_bqb_hfp_ag_ctx.conn) {
		BT_ERR("create acl failed");
		return -1;
	}
	hostif_bt_conn_unref(s_bqb_hfp_ag_ctx.conn);
	return 0;
}

static int bqb_hfp_ag_conn_ag()
{
	if (!s_bqb_hfp_ag_ctx.conn) {
		BT_ERR("No Acl connected, Please use conn_acl create acl");
		return -1;
	}
	return hostif_bt_hfp_ag_connect(s_bqb_hfp_ag_ctx.conn);
}

static int bqb_hfp_ag_callin(char* codec)
{
	if (!strcmp(codec, "msbc")){
		s_bqb_hfp_ag_ctx.codec = 2;
	} else if (!strcmp(codec, "lc3")) {
		s_bqb_hfp_ag_ctx.codec = 3;
	} else {
		s_bqb_hfp_ag_ctx.codec = 1;
	}

	hostif_bt_hfp_ag_update_indicator(CIEV_CALL_SETUP_IND, 1);
	hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, AG_EVENT_CLIP, strlen(AG_EVENT_CLIP));
	s_bqb_hfp_ag_ctx.call_state = BQB_CALL_STATE_INCOMING;

	os_delayed_work_submit(&s_bqb_hfp_ag_ctx.work, 2);
	return 0;
}

static int bqb_hfp_ag_setcodec(bt_hfp_ag_codec_e codec)
{
	if (codec == 3) {
		hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, AG_EVENT_NEGOTIATION_LC3_SWB, strlen(AG_EVENT_NEGOTIATION_SBC));
	} else if (codec == 2){
		hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, AG_EVENT_NEGOTIATION_SBC, strlen(AG_EVENT_NEGOTIATION_SBC));
	} else {
		hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, AG_EVENT_NEGOTIATION_CVSD, strlen(AG_EVENT_NEGOTIATION_CVSD));
	}
	s_bqb_hfp_ag_ctx.ag_state = BQB_CALL_STATE_CODEC_NEGO;
	return 0;
}

static void bqb_ag_connect_failed(struct bt_conn *conn)
{
	BT_INFO("AG connect failed %p", conn);
}

static void bqb_ag_connected(struct bt_conn *conn)
{
	BT_INFO("AG connect success %p", conn);
}

static void bqb_ag_disconnected(struct bt_conn *conn)
{
	BT_INFO("AG disconnect success %p", conn);
	s_bqb_hfp_ag_ctx.ag_state = BQB_AG_STATE_IDLE;
}

static int bqb_ag_ag_at_cmd(struct bt_conn *conn, uint8_t at_type, void *param)
{
	BT_INFO("AG AT CMD %p, type: %d", conn, at_type);

	switch (at_type)
	{
		case AT_CMD_ATA:
		{
			//hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, AG_EVENY_OK, strlen(AG_EVENY_OK));
			if (!(BQB_AG_FLAG_IGNOR_HF_ACCEPT_CALL & s_bqb_hfp_ag_ctx.flags))
			{
				BT_INFO("Remote Accept the Call");
				s_bqb_hfp_ag_ctx.call_state = BQB_CALL_STATE_SETUP;

				hostif_bt_hfp_ag_update_indicator(CIEV_CALL_IND, 1);
				hostif_bt_hfp_ag_update_indicator(CIEV_CALL_SETUP_IND, 0);
				bqb_hfp_ag_setcodec(s_bqb_hfp_ag_ctx.codec);
			}
			break;
		}

		case AT_CMD_BCS:
		{
			struct at_cmd_param* at_param = (struct at_cmd_param*)param;
			BT_INFO("Remote Codec: %d", at_param->val_u32t);
			//hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, AG_EVENY_SETUP_AUDIO_CONN, strlen(AG_EVENY_SETUP_AUDIO_CONN));
			//k_sleep(200);
			if (at_param->val_u32t == s_bqb_hfp_ag_ctx.codec) {
				hostif_bt_conn_create_sco(s_bqb_hfp_ag_ctx.conn);
				s_bqb_hfp_ag_ctx.call_state = BQB_CALL_STATE_CREATE_AUDIO_CONN;
			}
			break;
		}

		case AT_CMD_CHUP:
		{
			BT_INFO("Remote Terminate the call...");

			if (!(s_bqb_hfp_ag_ctx.flags & BQB_AG_FLAG_IGNOR_HF_END_CALL)) {
				if (BQB_CALL_STATE_OUT_GOING_CALL == s_bqb_hfp_ag_ctx.call_state) {
					hostif_bt_hfp_ag_update_indicator(CIEV_CALL_SETUP_IND, 0);
				} else {
					hostif_bt_hfp_ag_update_indicator(CIEV_CALL_IND, 0);
				}
			}
			break;
		}

		case AT_CMD_BLDN:
		{
			s_bqb_hfp_ag_ctx.call_state = BQB_CALL_STATE_OUT_GOING_CALL;
			os_delayed_work_submit(&s_bqb_hfp_ag_ctx.work, 20);
			break;
		}

		case AT_CMD_ATD:
		{
			struct at_cmd_param* at_param = (struct at_cmd_param*)param;
			if (at_param->is_index) {
				BT_INFO("HF Setup a call by index: %s  ag: 0x%x", at_param->s_val_char, s_bqb_hfp_ag_ctx.flags);
			} else {
				BT_INFO("HF Setup a call by phone number: %s  ag: 0x%x", at_param->s_val_char, s_bqb_hfp_ag_ctx.flags);
			}

			if (s_bqb_hfp_ag_ctx.flags & BQB_AG_FLAG_RETURN_ERROR) {
				return -1;
			}
			if (!(s_bqb_hfp_ag_ctx.flags & BQB_AG_FLAG_IGNOR_HF_END_CALL)) {
				s_bqb_hfp_ag_ctx.call_state = BQB_CALL_STATE_OUT_GOING_CALL;
				os_delayed_work_submit(&s_bqb_hfp_ag_ctx.work, 20);
			}
			break;
		}
		case AT_CMD_BIA:
		{
			uint32_t indicators = *((uint32_t*)param);
			BT_INFO("indicators: %u", indicators);
			break;
		}

		case AT_CMD_CLCC:
		{
			if (s_bqb_hfp_ag_ctx.flags & BQB_AF_FLAG_CALL_LIST) {
				hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, s_bqb_hfp_ag_ctx.call1_status, 16);
				hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, s_bqb_hfp_ag_ctx.call2_status, 16);
			} else {
				hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, AG_EVENT_CALL_LIST_CALL1, strlen(AG_EVENT_CALL_LIST_CALL1));
				hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, AG_EVENT_CALL_LIST_CALL2, strlen(AG_EVENT_CALL_LIST_CALL2));
			}
			break;
		}
		default:
			break;
	}
	return 0;
}

int bqb_ag_get_status(struct bt_conn *conn, uint8_t at_type, void *param)
{
	BT_INFO("at_type: %d, flags: %d", at_type, s_bqb_hfp_ag_ctx.flags);
	switch(at_type)
	{
		case AT_CMD_BLDN:
		case AT_CMD_ATD:
		{
			if (s_bqb_hfp_ag_ctx.flags & BQB_AG_FLAG_RETURN_ERROR) {
				BT_INFO("clean the number stored");
				return -1;
			}
			break;
		}
		default:
			break;
	}

	BT_INFO("Status is OK");
	return 0;
}


static struct bt_hfp_ag_cb s_ag_cb = {
	.connect_failed = bqb_ag_connect_failed,
	.connected = bqb_ag_connected,
	.disconnected = bqb_ag_disconnected,
	.ag_at_cmd = bqb_ag_ag_at_cmd,
	.ag_get_status = bqb_ag_get_status,
};

static int bqb_hfp_ag_set_ind(char* name, char* value)
{
	BT_INFO("name: %s, val: %s", name, value);
	uint8_t ind_value = value[0] - '0';
	if (!strcmp(name, "srv") || name[0] == '1') {
		hostif_bt_hfp_ag_update_indicator(CIEV_SERVICE_IND, ind_value);
	} else if (!strcmp(name, "call") || name[0] == '2') {
		os_delayed_work_cancel(&s_bqb_hfp_ag_ctx.work);
		hostif_bt_hfp_ag_update_indicator(CIEV_CALL_IND, ind_value);
		if (ind_value) {
			if (BQB_CALL_STATE_RING == s_bqb_hfp_ag_ctx.call_state) {
				s_bqb_hfp_ag_ctx.call_state = BQB_CALL_STATE_SETUP;
			}
		} else {
			s_bqb_hfp_ag_ctx.call_state = BQB_CALL_STATE_IDLE;
		}

	} else if (!strcmp(name, "callsetup") || name[0] == '3') {
		hostif_bt_hfp_ag_update_indicator(CIEV_CALL_SETUP_IND, ind_value);
		if (ind_value == 0 && BQB_CALL_STATE_RING == s_bqb_hfp_ag_ctx.call_state) {
			s_bqb_hfp_ag_ctx.call_state = BQB_CALL_STATE_IDLE;
			os_delayed_work_cancel(&s_bqb_hfp_ag_ctx.work);
		}
	} else if (!strcmp(name, "hold") || name[0] == '4') {
		hostif_bt_hfp_ag_update_indicator(CIEV_CALL_HELD_IND, ind_value);
	} else if (!strcmp(name, "signal") || name[0] == '5') {
		hostif_bt_hfp_ag_update_indicator(CIEV_SINGNAL_IND, ind_value);
	} else if (!strcmp(name, "roam") || name[0] == '6') {
		hostif_bt_hfp_ag_update_indicator(CIEV_ROAM_IND, ind_value);
	} else if (!strcmp(name, "batt") || name[0] == '7') {
		hostif_bt_hfp_ag_update_indicator(CIEV_BATTERY_IND, ind_value);
	}

	return 0;
}

static void bqb_hfp_ag_init()
{
	memset(&s_bqb_hfp_ag_ctx, 0, sizeof(s_bqb_hfp_ag_ctx));
	hostif_bt_conn_cb_register(&s_bqb_hfp_bt_conn_cb);
	hostif_bt_hfp_ag_register_cb(&s_ag_cb);
	bqb_gap_auth_register();

	//os_work_q_start(&s_bqb_hfp_ag_ctx.work_q, s_hfp_ag_delay_work_stack, 1500, 8);
	os_delayed_work_init(&s_bqb_hfp_ag_ctx.work, bqb_hfp_ag_callin_work_handler);
}

static void bqb_hfp_ag_deinit()
{
	if (s_bqb_hfp_ag_ctx.conn) {
		hostif_bt_hfp_ag_disconnect(s_bqb_hfp_ag_ctx.conn);
		bqb_gap_disconnect_acl(s_bqb_hfp_ag_ctx.conn);
	}

	memset(&s_bqb_hfp_ag_ctx, 0, sizeof(s_bqb_hfp_ag_ctx));
}

int bqb_hfp_ag_test_command_handler(int argc, char *argv[])
{
	int ret = 0;
	char* cmd = argv[1];
	if (!strcmp(cmd, "start")) {
		bqb_hfp_ag_init();
	} else if (!strcmp(cmd, "stop")) {
		bqb_hfp_ag_deinit();
	} else if (!strcmp(cmd, "conn")) {
		s_bqb_hfp_ag_ctx.auto_conn_ag = true;
		bqb_gap_clean_linkkey();
		if (s_bqb_hfp_ag_ctx.conn) {
			ret = bqb_hfp_ag_conn_ag();
		} else {
			if (3 == argc) {
				ret = bqb_hfp_ag_create_acl_conn(argv[2]);
			} else {
				ret = bqb_hfp_ag_create_acl_conn_by_stored_addr();
			}
		}
	} else if (!strcmp(cmd, "conn_acl")) {
		bqb_gap_clean_linkkey();
		if (3 == argc) {
			ret = bqb_hfp_ag_create_acl_conn(argv[2]);
		} else {
			ret = bqb_hfp_ag_create_acl_conn_by_stored_addr();
		}
	} else if (!strcmp(cmd, "disconn_acl")) {
		if (s_bqb_hfp_ag_ctx.conn) {
			ret = hostif_bt_conn_disconnect(s_bqb_hfp_ag_ctx.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		} else {
			ret = -1;
		}
	} else if (!strcmp(cmd, "conn_ag")) {
		ret = bqb_hfp_ag_conn_ag();
	} else if (!strcmp(cmd, "disconn_ag")) {
		ret = hostif_bt_hfp_ag_disconnect(s_bqb_hfp_ag_ctx.conn);
	} else if (!strcmp(cmd, "callin")) {
		ret = bqb_hfp_ag_callin(argv[2]);
	} else if (!strcmp(cmd, "setind")) {
		if (argc < 4) {
			BT_ERR("param err:  indname  value");
			ret = -1;
		} else {
			ret = bqb_hfp_ag_set_ind(argv[2], argv[3]);
		}
	} else if (!strcmp(cmd, "sendevent")) {
		if (argc < 3) {
			BT_ERR("param err:  event");
			ret = -1;
		} else {
			ret = hostif_bt_hfp_ag_send_event(s_bqb_hfp_ag_ctx.conn, argv[2], strlen(argv[2]));
		}
	} else if (!strcmp(cmd, "setflag")) {
		if (!strcmp(argv[2], "help") || argc < 3) {
			printk("	formate: setflag  xx\n");
			printk("	xx = 0   clean all the flags\n");
			printk("	xx = 1   AG wile ignore the accept call cmd(ATA)\n");
			printk("	xx = 2   AG wile ignore the hf end call cmd(AT+CHUP)\n");
			printk("	xx = 4   AG no number stored for the memory location\n");
			printk("	xx = 0X10   USE input call status\n");
		} else {
			s_bqb_hfp_ag_ctx.flags = atoi(argv[2]);
		}
	} else if (!strcmp(cmd, "conn_audio")) {
		hostif_bt_conn_create_sco(s_bqb_hfp_ag_ctx.conn);
	} else if (!strcmp(cmd, "disconn_audio")) {
		hostif_bt_conn_disconnect(s_bqb_hfp_ag_ctx.sco_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	} else if (!strcmp(cmd, "callstatus")) {
		if (s_bqb_hfp_ag_ctx.call_status_idx % 2 == 0) {
			strcpy(s_bqb_hfp_ag_ctx.call1_status, argv[2]);
		} else {
			strcpy(s_bqb_hfp_ag_ctx.call2_status, argv[2]);
		}
		s_bqb_hfp_ag_ctx.call_status_idx++;
	} else if (!strcmp(cmd, "setcnt")) {
		s_ag_set_content_format = atoi(argv[2]);
	} else if (!strcmp(cmd, "setcodec")) {
		s_bqb_hfp_ag_ctx.codec = atoi(argv[2]);
	} else {
		BT_ERR("Error Cmd...");
	}
	return ret;
}


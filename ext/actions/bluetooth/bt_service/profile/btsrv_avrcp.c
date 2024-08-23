/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief btsrvice avrcp
 */
#define SYS_LOG_DOMAIN "btsrv_avrcp"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

#define AVRCP_SYNC_TIMER_INTERVAL				50		/* 50ms */

#define AVRCP_CTRL_PASS_CHECK_INTERVAL			5		/* 5ms */
#define AVRCP_WAIT_ACCEPT_PUSH_TIMEOUT			500		/* 500ms */
#define AVRCP_CTRL_PASS_DELAY_RELEASE_TIME		5		/* 5ms */

enum {
	AVRCP_PASS_STATE_IDLE,
	AVRCP_PASS_STATE_WAIT_ACCEPT_PUSH,
	AVRCP_PASS_STATE_WAIT_DELAY_RELEASE,
	AVRCP_PASS_STATE_CONTINUE_START,
};

static btsrv_avrcp_callback_t *avrcp_user_callback;
static struct thread_timer sync_volume_timer;
static struct thread_timer ctrl_pass_timer;

struct btsrv_avrcp_stack_cb {
	struct bt_conn *conn;
	union {
		uint8_t event_id;
		uint8_t op_id;
	};
	uint8_t status;
};

static void _btsrv_avrcp_ctrl_connected_cb(struct bt_conn *conn)
{
	char addr_str[BT_ADDR_STR_LEN];
	bt_addr_t *bt_addr = NULL;

	btsrv_adapter_callback(BTSRV_REQ_HIGH_PERFORMANCE, NULL);
	bt_addr = (bt_addr_t *)GET_CONN_BT_ADDR(conn);
	hostif_bt_addr_to_str(bt_addr, addr_str, BT_ADDR_STR_LEN);
	SYS_LOG_INF("Avrcp connected %s", addr_str);

	btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_AVRCP_CONNECTED, conn);
}

static void _btsrv_avrcp_ctrl_disconnected_cb(struct bt_conn *conn)
{
	char addr_str[BT_ADDR_STR_LEN];
	bt_addr_t *bt_addr = NULL;

	bt_addr = (bt_addr_t *)GET_CONN_BT_ADDR(conn);
	hostif_bt_addr_to_str(bt_addr, addr_str, BT_ADDR_STR_LEN);
	SYS_LOG_INF("Avrcp disconnected %s", addr_str);
	btsrv_adapter_callback(BTSRV_REQ_HIGH_PERFORMANCE, NULL);

	btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_AVRCP_DISCONNECTED, conn);
}

void btsrv_avrcp_user_callback_ev(struct bt_conn *conn, btsrv_avrcp_event_e event, void *param)
{
	uint16_t hdl = hostif_bt_conn_get_handle(conn);

	switch (event) {
	case BTSRV_AVRCP_UPDATE_ID3_INFO:
	case BTSRV_AVRCP_TRACK_CHANGE:		/* Track change trigger get ID3 info, if need, just sync ID3 info */
		break;
	default:
		btsrv_tws_sync_info_type(conn, BTSRV_SYNC_TYPE_AVRCP, event);
		break;
	}

	avrcp_user_callback->event_cb(hdl, event, param);
}

static uint8_t avrcp_tx_buffer_match(struct bt_conn *conn,uint8_t *data,int len){
	if(!conn || !data){
		return 0;
	}
	//BT_AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME
	if(len > 12 && data[len - 5] == 0x50 && 
		data[len - 6] == 0x58 &&
		data[len - 7] == 0x19 &&
		data[len - 8] == 0x00){
		return 1;
	}
	return 0;
}

static void _btsrv_avrcp_ctrl_event_notify_cb(struct bt_conn *conn, uint8_t event_id, uint8_t status)
{
	struct btsrv_avrcp_stack_cb param;
	uint8_t host_pending,controler_pending,ret = 0;

	param.conn = conn;
	param.event_id = event_id;
	param.status = status;
	if (event_id == BT_AVRCP_EVENT_VOLUME_CHANGED) {
		if(btsrv_rdm_a2dp_get_active_dev() != conn){
			hostif_bt_br_conn_pending_pkt(conn, &host_pending, &controler_pending);
			if(host_pending >= bt_dev.br.pkts_num - 2){
				ret = hostif_bt_conn_drop_tx_buffer(conn,avrcp_tx_buffer_match,host_pending/2);
				SYS_LOG_INF("pending %d %d drop %d\n",host_pending,controler_pending,ret);
			}
		}
		btsrv_rdm_avrcp_delay_set_absolute_volume(conn, status);
	}else {
		btsrv_event_notify_malloc(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_NOTIFY_CB, (uint8_t *)&param, sizeof(param), 0);
	}
}

static void btsrv_avrcp_ctrl_event_notify_proc(void *in_param)
{
	int cb_ev = -1;
	struct btsrv_avrcp_stack_cb *param = in_param;
	struct bt_conn *conn = param->conn;
	uint8_t event_id = param->event_id;
	uint8_t status = param->status;

	if (event_id != BT_AVRCP_EVENT_PLAYBACK_STATUS_CHANGED &&
		event_id != BT_AVRCP_EVENT_VOLUME_CHANGED &&
		event_id != BT_AVRCP_EVENT_TRACK_CHANGED) {
		SYS_LOG_WRN("avrcp is NULL or event_id 0x%x not care", event_id);
		return;
	}

	uint16_t hdl = hostif_bt_conn_get_handle(conn);

#if 1
	if (event_id == BT_AVRCP_EVENT_VOLUME_CHANGED) {
		SYS_LOG_INF("BT_AVRCP_EVENT_VOLUME_CHANGED");
        avrcp_user_callback->set_volume_cb(hdl, status, btsrv_rdm_get_avrcp_sync_volume_wait(conn));
		return;
	}
#else
	uint32_t start_time = 0;
	uint32_t delay_time = btsrv_volume_sync_delay_ms();

	if (event_id == BT_AVRCP_EVENT_VOLUME_CHANGED &&
		btsrv_rdm_a2dp_get_active_dev() == conn &&
		btsrv_rdm_is_a2dp_stream_open(conn)) {
		start_time = btsrv_rdm_get_avrcp_sync_vol_start_time(conn);
		if (start_time && (os_uptime_get_32() - start_time) > delay_time
			 && !btsrv_rdm_get_avrcp_sync_volume_wait(conn)) {
			avrcp_user_callback->set_volume_cb(hdl, status, false);
		}
		return;
	}
#endif
	else if (event_id == BT_AVRCP_EVENT_TRACK_CHANGED){
		cb_ev = BTSRV_AVRCP_TRACK_CHANGE;
	} else if(event_id == BT_AVRCP_EVENT_PLAYBACK_STATUS_CHANGED) {
		switch (status) {
		case BT_AVRCP_PLAYBACK_STATUS_STOPPED:
			cb_ev = BTSRV_AVRCP_STOPED;
			btsrv_rdm_set_avrcp_state(conn,0);
			break;
		case BT_AVRCP_PLAYBACK_STATUS_PLAYING:
			cb_ev = BTSRV_AVRCP_PLAYING;
			btsrv_rdm_set_avrcp_state(conn,1);
			break;
		case BT_AVRCP_PLAYBACK_STATUS_PAUSED:
			cb_ev = BTSRV_AVRCP_PAUSED;
			btsrv_rdm_set_avrcp_state(conn,0);
			break;
		case BT_AVRCP_PLAYBACK_STATUS_FWD_SEEK:
		case BT_AVRCP_PLAYBACK_STATUS_REV_SEEK:
		case BT_AVRCP_PLAYBACK_STATUS_ERROR:
			break;
		}
	}

#if 1
	if (cb_ev > 0) {
		btsrv_avrcp_user_callback_ev(conn, cb_ev, NULL);
	}
#else
	if (cb_ev > 0 && btsrv_rdm_a2dp_get_active_dev() == conn) {
		btsrv_avrcp_user_callback_ev(conn, cb_ev, NULL);
	}
#endif
}

static void _btsrv_avrcp_ctrl_get_volume_cb(struct bt_conn *conn, uint8_t *volume)
{
	uint16_t hdl = hostif_bt_conn_get_handle(conn);

	avrcp_user_callback->get_volume_cb(hdl, volume, true);
	SYS_LOG_INF("volume %d\n",*volume);
}

static void _btsrv_avrcp_ctrl_setvolume_cb(struct bt_conn *conn, uint8_t volume)
{
	uint8_t host_pending,controler_pending,ret = 0;

    btsrv_volume_sync_delay_ms() = 0;

    if(btsrv_rdm_a2dp_get_active_dev() != conn){
        hostif_bt_br_conn_pending_pkt(conn, &host_pending, &controler_pending);
        if(host_pending >= bt_dev.br.pkts_num - 2){
            ret = hostif_bt_conn_drop_tx_buffer(conn,avrcp_tx_buffer_match,host_pending/2);
            SYS_LOG_INF("pending %d %d drop %d\n",host_pending,controler_pending,ret);
        }
    }
   
    btsrv_rdm_avrcp_delay_set_absolute_volume(conn, volume);
}

static void _btsrv_avrcp_ctrl_pass_ctrl_cb(struct bt_conn *conn, uint8_t op_id, uint8_t state)
{
	struct btsrv_avrcp_stack_cb param;

	param.conn = conn;
	param.op_id = op_id;
	param.status = state;
	btsrv_event_notify_malloc(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_PASS_CTRL_CB, (uint8_t *)&param, sizeof(param), 0);
}

static void btsrv_avrcp_ctrl_rsp_pass_proc(struct bt_conn *conn, uint8_t op_id, uint8_t state)
{
	struct btsrv_rdm_avrcp_pass_info *info;

	if (state != BT_AVRCP_RSP_STATE_PASS_THROUGH_PUSHED) {
		return;
	}

	info = btsrv_rdm_avrcp_get_pass_info(conn);
	if (!info) {
		return;
	}

	if (info->op_id == op_id && info->pass_state == AVRCP_PASS_STATE_WAIT_ACCEPT_PUSH) {
		info->pass_state = AVRCP_PASS_STATE_WAIT_DELAY_RELEASE;
		info->op_time = os_uptime_get_32();
	}
}

static void btsrv_avrcp_ctrl_pass_ctrl_proc(void *in_param)
{
	struct btsrv_avrcp_stack_cb *param = in_param;
	struct bt_conn *conn = param->conn;
	uint8_t op_id = param->op_id;
	uint8_t state = param->status;
	uint16_t hdl = hostif_bt_conn_get_handle(conn);
    uint8_t cmd;

	SYS_LOG_INF("op_id 0x%x state 0x%x", op_id,state);

	if (state == BT_AVRCP_RSP_STATE_PASS_THROUGH_PUSHED ||
		state == BT_AVRCP_RSP_STATE_PASS_THROUGH_RELEASED) {
		btsrv_avrcp_ctrl_rsp_pass_proc(conn, op_id, state);

        if ((state == BT_AVRCP_RSP_STATE_PASS_THROUGH_RELEASED )
            && ((op_id == AVRCP_OPERATION_ID_PLAY) || (op_id == AVRCP_OPERATION_ID_PAUSE))) {
            if(op_id == AVRCP_OPERATION_ID_PLAY){
                cmd = BTSRV_AVRCP_CMD_PLAY;
            }
            else{
                cmd = BTSRV_AVRCP_CMD_PAUSE;
            }
            state &= 0x7F;
            avrcp_user_callback->pass_ctrl_cb(hdl, cmd, state);
            btsrv_avrcp_user_callback_ev(conn, BTSRV_AVRCP_PASS_THROUGH_RELEASED, (void *)&op_id);
        }
	} else {
		if (btsrv_rdm_a2dp_get_active_dev() == conn){
		switch(op_id){
		case AVRCP_OPERATION_ID_PLAY:
			cmd = BTSRV_AVRCP_CMD_PLAY;
		break;
		case AVRCP_OPERATION_ID_PAUSE:
			cmd = BTSRV_AVRCP_CMD_PAUSE;
		break;
		case AVRCP_OPERATION_ID_VOLUME_UP:
			cmd = BTSRV_AVRCP_CMD_VOLUMEUP;
		break;
		case AVRCP_OPERATION_ID_VOLUME_DOWN:
			cmd = BTSRV_AVRCP_CMD_VOLUMEDOWN;
		break;
		case AVRCP_OPERATION_ID_MUTE:
			cmd = BTSRV_AVRCP_CMD_MUTE;
		break;
		default:
		SYS_LOG_ERR("op_id %d not support\n", op_id);
		return;
		}
			avrcp_user_callback->pass_ctrl_cb(hdl, cmd, state);
		}
	}
}

static void _btsrv_avrcp_ctrl_update_id3_info_cb(struct bt_conn *conn, struct id3_info * info)
{
	if (btsrv_rdm_a2dp_get_active_dev() == conn){
		btsrv_avrcp_user_callback_ev(conn, BTSRV_AVRCP_UPDATE_ID3_INFO, info);
	}
}

static void _btsrv_avrcp_ctrl_get_play_status_cb(struct bt_conn *conn, uint32_t *song_len, uint32_t *song_pos, uint8_t *play_state)
{
	SYS_LOG_INF("len:0x%x, pos:0x%x, status:%d", *song_len, *song_pos, *play_state);

	btsrv_rdm_set_avrcp_playstatus_info(conn, song_len, song_pos, play_state);
}

static const struct bt_avrcp_app_cb btsrv_avrcp_ctrl_cb = {
	.connected = _btsrv_avrcp_ctrl_connected_cb,
	.disconnected = _btsrv_avrcp_ctrl_disconnected_cb,
	.notify = _btsrv_avrcp_ctrl_event_notify_cb,
	.pass_ctrl = _btsrv_avrcp_ctrl_pass_ctrl_cb,
	.get_play_status = _btsrv_avrcp_ctrl_get_play_status_cb,
	.get_volume = _btsrv_avrcp_ctrl_get_volume_cb,
	.update_id3_info = _btsrv_avrcp_ctrl_update_id3_info_cb,
	.setvolume = _btsrv_avrcp_ctrl_setvolume_cb,
};

static void btsrv_avrcp_ctrl_pass_timer_start_stop(bool start)
{
	if (start) {
		if (!thread_timer_is_running(&ctrl_pass_timer)) {
			thread_timer_start(&ctrl_pass_timer, AVRCP_CTRL_PASS_CHECK_INTERVAL, AVRCP_CTRL_PASS_CHECK_INTERVAL);
		}
	} else {
		if (thread_timer_is_running(&ctrl_pass_timer)) {
			thread_timer_stop(&ctrl_pass_timer);
		}
	}
}

static void connected_dev_cb_check_ctrl_pass(struct bt_conn *base_conn, uint8_t tws_dev, void *cb_param)
{
	int *need_timer = cb_param;
	struct btsrv_rdm_avrcp_pass_info *info;
	uint32_t time, check_timeout = 0;

	if (tws_dev) {
		return;
	}

	info = btsrv_rdm_avrcp_get_pass_info(base_conn);
	if (!info){
		return;
	}

	if (info->op_id == 0 ||
		info->pass_state == AVRCP_PASS_STATE_IDLE ||
		info->pass_state == AVRCP_PASS_STATE_CONTINUE_START) {
		return;
	}

	*need_timer = 1;
	time = os_uptime_get_32();

	if (info->pass_state == AVRCP_PASS_STATE_WAIT_ACCEPT_PUSH) {
		check_timeout = AVRCP_WAIT_ACCEPT_PUSH_TIMEOUT;
	} else if (info->pass_state == AVRCP_PASS_STATE_WAIT_DELAY_RELEASE) {
		check_timeout = AVRCP_CTRL_PASS_DELAY_RELEASE_TIME;
	}

	if ((time - info->op_time) > check_timeout) {
		hostif_bt_avrcp_ct_pass_through_cmd(base_conn, info->op_id, false);
		info->pass_state = AVRCP_PASS_STATE_IDLE;
		info->op_id = 0;
	}
}

static void btsrv_avrcp_ctrl_pass_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	int need_timer = 0;

	btsrv_rdm_get_connected_dev(connected_dev_cb_check_ctrl_pass, &need_timer);
	if (!need_timer) {
		btsrv_avrcp_ctrl_pass_timer_start_stop(false);
	}
}

static void connected_dev_cb_sync_volume(struct bt_conn *base_conn, uint8_t tws_dev, void *cb_param)
{
	uint32_t *wait_sync = cb_param;
	uint32_t start_time, curr_time;
	uint8_t volume;
	uint16_t hdl;

	if (tws_dev || btsrv_rdm_get_dev_role() == BTSRV_TWS_SLAVE) {
		return;
	}

	if (!btsrv_rdm_get_avrcp_sync_volume_wait(base_conn)) {
		return;
	}

	*wait_sync = 1;

    hdl = hostif_bt_conn_get_handle(base_conn);
    avrcp_user_callback->get_volume_cb(hdl, &volume, false);

	curr_time = os_uptime_get_32();
	start_time = btsrv_rdm_get_avrcp_sync_vol_start_time(base_conn);
	if (start_time == 0) {
		start_time = curr_time;
		btsrv_rdm_set_avrcp_sync_vol_start_time(base_conn, start_time);
	}
    else{
        if(btsrv_rdm_is_ios_dev(base_conn)){
            if((volume == 0) || (volume == 0x7F)){
                btsrv_volume_sync_delay_ms() = 1500;
            }
            else{
                btsrv_volume_sync_delay_ms() = 1000;
            }
        }
    }
	if ((curr_time - start_time) > btsrv_volume_sync_delay_ms()) {
		btsrv_rdm_set_avrcp_sync_volume_wait(base_conn, 0);
		SYS_LOG_DBG("hdl 0x%x, volume %d", hdl, volume);

		if (!btsrv_is_pts_test()) {
			hostif_bt_avrcp_tg_notify_change(base_conn, volume);
			if(btsrv_rdm_is_ios_dev(base_conn)){
                btsrv_rdm_set_avrcp_sync_vol_start_time(base_conn, curr_time);
            }
		}
	}
}

static void btsrv_avrcp_sync_volume_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	uint32_t wait_sync = 0;

	btsrv_rdm_get_connected_dev(connected_dev_cb_sync_volume, &wait_sync);
	if (!wait_sync) {
		thread_timer_stop(&sync_volume_timer);
	}
}

int btsrv_avrcp_init(btsrv_avrcp_callback_t *cb)
{
	hostif_bt_avrcp_cttg_register_cb((struct bt_avrcp_app_cb *)&btsrv_avrcp_ctrl_cb);
	avrcp_user_callback = cb;

	thread_timer_init(&sync_volume_timer, btsrv_avrcp_sync_volume_timer_handler, NULL);
	thread_timer_init(&ctrl_pass_timer, btsrv_avrcp_ctrl_pass_timer_handler, NULL);

	return 0;
}

int btsrv_avrcp_deinit(void)
{
	btsrv_avrcp_ctrl_pass_timer_start_stop(false);
	if (thread_timer_is_running(&sync_volume_timer)) {
		thread_timer_stop(&sync_volume_timer);
	}
	return 0;
}

int btsrv_avrcp_disconnect(struct bt_conn *conn)
{
	if (conn && btsrv_rdm_is_avrcp_connected(conn)) {
		SYS_LOG_INF("avrcp_disconnect\n");
		hostif_bt_avrcp_cttg_disconnect(conn);
	}
	return 0;
}

int btsrv_avrcp_connect(struct bt_conn *conn)
{
	int ret = 0;

	if (!hostif_bt_avrcp_cttg_connect(conn)) {
		SYS_LOG_INF("Connect avrcp\n");
		ret = 0;
	} else {
		SYS_LOG_ERR("Connect avrcp failed\n");
		ret = -1;
	}

	return ret;
}

static void btsrv_avrcp_sync_vol(void *param)
{
	uint16_t hdl = (uint16_t)(uint32_t)param;
	struct bt_conn *conn;

	if (!hdl) {
		conn = btsrv_rdm_a2dp_get_active_dev();
	} else {
		conn = btsrv_rdm_find_conn_by_hdl(hdl);
	}

	if (!conn) {
		return;
	}

#if 1
    uint16_t delay_ms = ((uint32_t)param >> 16);

    SYS_LOG_DBG("delay_ms %d", delay_ms);
    btsrv_volume_sync_delay_ms() = delay_ms;
#endif

	btsrv_rdm_set_avrcp_sync_volume_wait(conn, 1);
	if (!thread_timer_is_running(&sync_volume_timer)) {
		thread_timer_start(&sync_volume_timer, AVRCP_SYNC_TIMER_INTERVAL, AVRCP_SYNC_TIMER_INTERVAL);
	}
}

static int btsrv_avrcp_ct_pass_through_cmd(struct bt_conn *conn,
										uint8_t opid, bool continue_cmd, bool push)
{
	int ret = 0;
	struct btsrv_rdm_avrcp_pass_info *info;

	info = btsrv_rdm_avrcp_get_pass_info(conn);
	if (!info) {
		return -EIO;
	}

	if (continue_cmd) {
		if ((push && info->pass_state != AVRCP_PASS_STATE_IDLE) ||
			(!push && info->pass_state != AVRCP_PASS_STATE_CONTINUE_START)) {
			SYS_LOG_INF("Pass busy %d op_id 0x%x", info->pass_state, info->op_id);
			ret = -EBUSY;
			goto pass_exit;
		}

		if (push) {
			info->pass_state = AVRCP_PASS_STATE_CONTINUE_START;
			info->op_id = opid;
		} else {
			info->pass_state = AVRCP_PASS_STATE_IDLE;
			info->op_id = 0;
		}

		ret = hostif_bt_avrcp_ct_pass_through_cmd(conn, opid, push);
	} else {
		if (info->pass_state != AVRCP_PASS_STATE_IDLE) {
			SYS_LOG_INF("Pass busy %d op_id 0x%x", info->pass_state, info->op_id);
			ret = -EBUSY;
			goto pass_exit;
		}

		info->pass_state = AVRCP_PASS_STATE_WAIT_ACCEPT_PUSH;
		info->op_id = opid;
		info->op_time = os_uptime_get_32();
		btsrv_avrcp_ctrl_pass_timer_start_stop(true);

		ret = hostif_bt_avrcp_ct_pass_through_cmd(conn, opid, push);
	}

pass_exit:
	return ret;
}

static int _btsrv_avrcp_controller_process(uint16_t hdl, btsrv_avrcp_cmd_e cmd)
{
	uint8_t op_id = 0;
	bool push = true, continue_cmd = false;
	int status = 0;
	struct bt_conn *avrcp_conn;

	if (!hdl) {
		avrcp_conn = btsrv_rdm_a2dp_get_active_dev();
	} else {
		avrcp_conn = btsrv_rdm_find_conn_by_hdl(hdl);
	}

    if(!avrcp_conn && btsrv_info->cfg.pts_test_mode){
        avrcp_conn = btsrv_rdm_avrcp_get_connected_dev();
    }
	if (!avrcp_conn) {
		return -EINVAL;
	}

	switch (cmd) {
	case BTSRV_AVRCP_CMD_PLAY:
		op_id = AVRCP_OPERATION_ID_PLAY;
	break;
	case BTSRV_AVRCP_CMD_PAUSE:
		op_id = AVRCP_OPERATION_ID_PAUSE;
	break;
	case BTSRV_AVRCP_CMD_STOP:
		op_id = AVRCP_OPERATION_ID_STOP;
	break;
	case BTSRV_AVRCP_CMD_FORWARD:
		op_id = AVRCP_OPERATION_ID_FORWARD;
	break;
	case BTSRV_AVRCP_CMD_BACKWARD:
		op_id = AVRCP_OPERATION_ID_BACKWARD;
	break;
	case BTSRV_AVRCP_CMD_VOLUMEUP:
		op_id = AVRCP_OPERATION_ID_VOLUME_UP;
	break;
	case BTSRV_AVRCP_CMD_VOLUMEDOWN:
		op_id = AVRCP_OPERATION_ID_VOLUME_DOWN;
	break;
	case BTSRV_AVRCP_CMD_MUTE:
		op_id = AVRCP_OPERATION_ID_MUTE;
	break;
	case BTSRV_AVRCP_CMD_FAST_FORWARD_START:
		op_id = AVRCP_OPERATION_ID_FAST_FORWARD;
		continue_cmd = true;
		push = true;
	break;
	case BTSRV_AVRCP_CMD_FAST_FORWARD_STOP:
		op_id = AVRCP_OPERATION_ID_FAST_FORWARD;
		continue_cmd = true;
		push = false;
	break;
	case BTSRV_AVRCP_CMD_FAST_BACKWARD_START:
		op_id = AVRCP_OPERATION_ID_REWIND;
		continue_cmd = true;
		push = true;
	break;
	case BTSRV_AVRCP_CMD_FAST_BACKWARD_STOP:
		op_id = AVRCP_OPERATION_ID_REWIND;
		continue_cmd = true;
		push = false;
	break;
	case BTSRV_AVRCP_CMD_REPEAT_SINGLE:
	case BTSRV_AVRCP_CMD_REPEAT_ALL_TRACK:
	case BTSRV_AVRCP_CMD_REPEAT_OFF:
	case BTSRV_AVRCP_CMD_SHUFFLE_ON:
	case BTSRV_AVRCP_CMD_SHUFFLE_OFF:
	default:
	SYS_LOG_ERR("cmd 0x%02x not support\n", cmd);
	return -EINVAL;
	}

	status = btsrv_avrcp_ct_pass_through_cmd(avrcp_conn, op_id, continue_cmd, push);
	if (status < 0) {
		SYS_LOG_ERR("0x%x failed status %d conn 0x%x",
					cmd, status, hostif_bt_conn_get_handle(avrcp_conn));
	} else {
		SYS_LOG_INF("0x%x ok conn 0x%x", cmd, hostif_bt_conn_get_handle(avrcp_conn));
	}

	return status;
}

static int  _btsrv_avrcp_get_id3_info()
{
	int status = 0;
	struct bt_conn *avrcp_conn = btsrv_rdm_a2dp_get_active_dev();
	if(!avrcp_conn)
		return -EINVAL;
	status = hostif_bt_avrcp_ct_get_id3_info(avrcp_conn);

	return status;
}

static int btsrv_avrcp_get_play_status(struct bt_conn *avrcp_conn)
{
	if(!avrcp_conn)
		return -EINVAL;

	return hostif_bt_avrcp_ct_get_play_status(avrcp_conn);
}

static int btsrv_avrcp_set_absolute_volume(uint32_t param)
{
	struct bt_conn *avrcp_conn = btsrv_rdm_a2dp_get_active_dev();

	if (!avrcp_conn && btsrv_info->cfg.pts_test_mode) {
		avrcp_conn = btsrv_rdm_avrcp_get_connected_dev();
	}

	if(!avrcp_conn) {
		return -EIO;
	}

	return hostif_bt_avrcp_ct_set_absolute_volume(avrcp_conn, param);
}

static int btsrv_avrcp_notify_volume_change(uint8_t volume)
{
	if (!btsrv_info->cfg.pts_test_mode) {
		return -EIO;
	}
	struct bt_conn * avrcp_conn = btsrv_rdm_avrcp_get_connected_dev();
	if(!avrcp_conn) {
		return -EIO;
	}
	return hostif_bt_avrcp_tg_notify_change(avrcp_conn, volume);
}

int btsrv_avrcp_process(struct app_msg *msg)
{
	struct bt_conn *conn;

	switch (_btsrv_get_msg_param_cmd(msg)) {
	case MSG_BTSRV_AVRCP_START:
		SYS_LOG_INF("MSG_BTSRV_AVRCP_START\n");
		btsrv_avrcp_init(msg->ptr);
		break;
	case MSG_BTSRV_AVRCP_STOP:
		SYS_LOG_INF("MSG_BTSRV_AVRCP_STOP\n");
		btsrv_avrcp_deinit();
		break;
	case MSG_BTSRV_AVRCP_CONNECT_TO:
		SYS_LOG_INF("MSG_BTSRV_AVRCP_CONNECT_TO\n");
		conn = btsrv_rdm_find_conn_by_addr(msg->ptr);
		if (conn) {
			btsrv_avrcp_connect(conn);
		}
		break;
	case MSG_BTSRV_AVRCP_DISCONNECT:
		SYS_LOG_INF("MSG_BTSRV_AVRCP_DISCONNECT\n");
		conn = btsrv_rdm_find_conn_by_addr(msg->ptr);
		if (conn) {
			btsrv_avrcp_disconnect(conn);
		}
		break;

	case MSG_BTSRV_AVRCP_SEND_CMD:
	{
		uint16_t hdl = (msg->value) & 0xffff;
		int avrcp_cmd = (msg->value >>16) & 0xffff;
		SYS_LOG_INF("MSG_BTSRV_AVRCP_SEND_CMD hdl 0x%x, cmd %d", hdl,avrcp_cmd);
		_btsrv_avrcp_controller_process(hdl, avrcp_cmd);
	}
		break;
	case MSG_BTSRV_AVRCP_GET_ID3_INFO:
		SYS_LOG_INF("MSG_BTSRV_AVRCP_GET_ID3_INFO\n");
		_btsrv_avrcp_get_id3_info();
		break;
	case MSG_BTSRV_AVRCP_SET_ABSOLUTE_VOLUME:
		SYS_LOG_INF("MSG_BTSRV_AVRCP_SET_ABSOLUTE_VOLUME 0x%x", msg->value);
		btsrv_avrcp_set_absolute_volume((uint32_t)msg->value);
		break;
	case MSG_BTSRV_AVRCP_CONNECTED:
		btsrv_avrcp_user_callback_ev(msg->ptr, BTSRV_AVRCP_CONNECTED, NULL);
		btsrv_adapter_callback(BTSRV_RELEASE_HIGH_PERFORMANCE, NULL);
		break;
	case MSG_BTSRV_AVRCP_DISCONNECTED:
		btsrv_avrcp_user_callback_ev(msg->ptr, BTSRV_AVRCP_DISCONNECTED, NULL);
		break;
	case MSG_BTSRV_AVRCP_NOTIFY_CB:
		btsrv_avrcp_ctrl_event_notify_proc(msg->ptr);
		break;
	case MSG_BTSRV_AVRCP_PASS_CTRL_CB:
		btsrv_avrcp_ctrl_pass_ctrl_proc(msg->ptr);
		break;
	case MSG_BTSRV_AVRCP_SYNC_VOLUME:
		SYS_LOG_DBG("MSG_BTSRV_AVRCP_SYNC_VOLUME\n");
		if (btsrv_rdm_get_dev_role() != BTSRV_TWS_SLAVE) {
			btsrv_avrcp_sync_vol(msg->ptr);
		}
		break;
	case MSG_BTSRV_AVRCP_GET_PLAY_STATUS:
		SYS_LOG_INF("MSG_BTSRV_AVRCP_GET_PLAY_STATUS\n");
		btsrv_avrcp_get_play_status(msg->ptr);
		break;
	case MSG_BTSRV_AVRCP_NOTIFY_VOLUME_CHANGE:
		SYS_LOG_INF("MSG_BTSRV_AVRCP_NOTIFY_VOLUME_CHANGE\n");
		btsrv_avrcp_notify_volume_change(msg->value);
	default:
		break;
	}

	return 0;
}

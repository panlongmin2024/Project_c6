#include "selfapp_internal.h"
#include "selfapp_config.h"
#include "../bt_manager/bt_manager_inner.h"

#include <mem_manager.h>
#include <app_ui.h>

static selfapp_context_t *ptr_BT_SelfApp = NULL;

selfapp_context_t *self_get_context(void)
{
	return ptr_BT_SelfApp;
}

void self_set_context(selfapp_context_t * ctx)
{
	if (ptr_BT_SelfApp && ctx && ptr_BT_SelfApp != ctx) {
		selfapp_log_err("[BUG] selfctx existed!\n");
		return;
	}
	ptr_BT_SelfApp = ctx;
}

void *self_get_streamhdl(void)
{
	selfapp_context_t *selfctx = self_get_context();
	if (NULL == selfctx) {
		return NULL;
	}
	if (selfctx && selfctx->stream_hdl) {
		return selfctx->stream_hdl;
	}
	return NULL;
}

u8_t *self_get_sendbuf(void)
{
	selfapp_context_t *selfctx = self_get_context();

	if (NULL == selfctx) {
		return NULL;
	}

	if (selfctx->sendbuf) {
		return selfctx->sendbuf;
	} else {
		return NULL;
	}
}

int self_send_data(u8_t * buf, u16_t len)
{
	void *stream_hdl = self_get_streamhdl();
	if (stream_hdl == NULL || len == 0) {
		return -1;
	}

	if (len > SELF_SENDBUF_SIZE) {
		selfapp_log_err("[BUG] sendbuf exceed %d_%d\n", len,
				SELF_SENDBUF_SIZE);
		len = SELF_SENDBUF_SIZE;
	}

	stream_write(stream_hdl, buf, len);

#ifdef SELFAPP_DEBUG
	selfapp_log_inf("TX: %2x %2x", buf[0], buf[1]);
	selfapp_log_dump(buf, len);
#endif
	return 0;
}

int selfapp_handler_by_stream(void *handle)
{
	selfapp_context_t *selfctx = self_get_context();
	int recv_len = 0, try_count = 0;

	u8_t CmdID = 0;
	u16_t PayloadLen = 0;
	u8_t *Payload = NULL, *buf = NULL;
	void *stream_handle = NULL;
	u8_t tmp_buf[10];

	if (selfctx == NULL || selfctx->recvbuf == NULL) {
		selfapp_log_inf("not init %p\n", handle);
		return -1;
	}

	stream_handle = handle ? handle : selfctx->stream_hdl;
	if (stream_handle == NULL) {
		selfapp_log_inf("no stream\n");
		return -1;
	}

	// if (stream_get_length(stream_handle) == 0) {
	if (stream_tell(stream_handle) <= 0) {
		return -16;
	}

	buf = selfctx->recvbuf;
	memset(buf, 0, SELF_RECVBUF_SIZE);

	// readout the IDENTIFIER
	do {
		recv_len = stream_read(stream_handle, buf, 1);
		if (selfapp_cmd_check_id(buf[0]) || try_count >= 10) {
			break;
		}
		tmp_buf[try_count++] = buf[0];

#ifdef CONFIG_LOGSRV_SELF_APP
        if(try_count == 10){
            if(selfapp_logsrv_check_id((u8_t *)tmp_buf,try_count) == 0){
                selfapp_logsrv_init(selfctx->stream_hdl);
				selfapp_logsrv_callback_register(selfctx->log_cb);
                if(selfctx->logsrv){
                    return 0;
                }
            }
        }
#endif
	} while (stream_tell(stream_handle) > 0);

	if (try_count > 0) {
		selfapp_log_inf("skipped %d:\n", try_count);
		selfapp_log_dump(tmp_buf, try_count);
	}
	if (!selfapp_cmd_check_id(buf[0])) {
		selfapp_log_inf("no Identifier 0x%x,%d\n", buf[0], try_count);
		return -1;
	}
	// readout the command
	recv_len += stream_read(stream_handle, &(buf[recv_len]), 2);
	if (selfapp_playload_len_is_2(buf[1])) {
		recv_len += stream_read(stream_handle, &(buf[recv_len]), 1);
	}
	//Payload might be an illegal address, do NOT use it
	if (selfapp_check_header(buf, recv_len, &CmdID, &Payload, &PayloadLen) != 0) {
		selfapp_log_inf("header wrong %d\n", recv_len);
		return -1;
	}
	if (PayloadLen == 0) {
		//selfapp_log_inf("no payload 0x%x\n", CmdID);
	} else if (selfapp_has_large_payload(CmdID)) {
		// don't read the large payload here and command should read by itself
		//selfapp_log_inf("large payload 0x%x\n", CmdID);
		PayloadLen = 0;
	} else {
		u8_t *payload_buf = &(buf[recv_len]);
		if (recv_len + PayloadLen > SELF_RECVBUF_SIZE) {
			payload_buf = NULL;
		}

		if (payload_buf) {
			recv_len +=
			    stream_read(stream_handle, payload_buf, PayloadLen);
		} else {
			stream_read(stream_handle, NULL, PayloadLen);	//indicate and drop it
		}
	}

	//+3(+4): the length of package header
	if (recv_len != PayloadLen + 3 && recv_len != PayloadLen + 4) {
		selfapp_log_inf("stream fail 0x%x:%d,%d\n", CmdID, recv_len,
				PayloadLen);
		return -1;
	}

	return selfapp_cmd_handler(buf, recv_len);
}

static int selfapp_set_ble_connect(u8_t conc)
{
	u8_t conc_type;
	selfapp_context_t *selfctx = self_get_context();
	if (selfctx == NULL) {
		selfapp_log_inf("not init %d\n", conc);
		return -1;
	}

	conc_type = selfctx->connect_type;
	selfctx->connect_type = conc ? CONCTYPE_BLE : CONCTYPE_BR_EDR;
	selfapp_log_inf("conc_type %d, %d->%d\n", conc, conc_type,
			selfctx->connect_type);
	return 0;
}

static int selfapp_connect_init(void)
{
	selfapp_context_t *selfctx = self_get_context();
	if (selfctx == NULL || selfctx->stream_hdl == NULL) {
		return -1;
	}

	if (selfctx->stream_opened == 0) {
		if (stream_open(selfctx->stream_hdl, MODE_IN_OUT) != 0) {
			goto Label_conc_fail;
		}
		selfctx->stream_opened = 1;
	} else {
		selfapp_log_wrn("to flush %d bytes", stream_tell(selfctx->stream_hdl));
		stream_flush(selfctx->stream_hdl);
	}

	if (selfctx->sendbuf == NULL) {
		selfctx->sendbuf =
		    mem_malloc(SELF_SENDBUF_SIZE + SELF_RECVBUF_SIZE);
	}
	if (selfctx->sendbuf == NULL) {
		goto Label_conc_fail;
	}
	selfctx->recvbuf = &(selfctx->sendbuf[SELF_SENDBUF_SIZE]);

	selfctx->connect_type = CONCTYPE_BR_EDR;

#ifdef SPEC_LED_PATTERN_CONTROL
	ledpkg_init();
#endif

	selfapp_log_inf("conc: %d\n", selfctx->connect_type);
	return 0;

 Label_conc_fail:
	selfapp_log_inf("concfail %d_%p\n", selfctx->stream_opened,
			selfctx->sendbuf);
	if (selfctx->sendbuf) {
		mem_free(selfctx->sendbuf);
		selfctx->sendbuf = NULL;
	}
	if (selfctx->stream_opened) {
		stream_close(selfctx->stream_hdl);
		selfctx->stream_opened = 0;
	}
	return -1;
}

static int selfapp_connect_deinit(void)
{
	selfapp_context_t *selfctx = self_get_context();
	if (selfctx == NULL) {
		return -1;
	}

#ifdef CONFIG_LOGSRV_SELF_APP
	if(selfctx->logsrv){
	    selfapp_logsrv_exit();
	    selfctx->logsrv = NULL;
	}
#endif

	if (selfctx->otadfu) {
		otadfu_NotifyDfuCancel(0);
	}
#ifdef SPEC_LED_PATTERN_CONTROL
	if (selfctx->ledpkg) {
		ledpkg_deinit();
	}
#endif
	if (selfctx->sendbuf) {
		mem_free(selfctx->sendbuf);
		selfctx->sendbuf = NULL;
		selfapp_log_inf("disc\n");
	}

	if (selfctx->stream_opened) {
		stream_close(selfctx->stream_hdl);
		selfctx->stream_opened = 0;
	}

	if(selfctx->mute_player){
		selfapp_mute_player(0);
	}
	return 0;
}

/* - - - BT Selfapp Basic Support - - - */

/* "00006666-0000-1000-8000-00805F9B34FB"  uuid spp */
static const u8_t app_spp_uuid[16] = { 0xFB, 0x34, 0x9B, 0x5F, 0x80,
	0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x66, 0x66, 0x00, 0x00
};

#ifdef CONFIG_OTA_SELF_APP
/* "00006d6f-632e-746e-696f-706c65637865"  uuid service */
#define APP_SERVICE_UUID  BT_UUID_DECLARE_128( \
	0x00, 0x00, 0x6d, 0x6f, 0x63, 0x2e, 0x74, 0x6e, 0x69, 0x6f,\
	0x70, 0x6c, 0x65, 0x63, 0x78, 0x65)

#define APP_CHA_RX_UUID  BT_UUID_DECLARE_128( \
	0x02, 0x00, 0x6d, 0x6f, 0x63, 0x2e, 0x74, 0x6e, 0x69, 0x6f,\
	0x70, 0x6c, 0x65, 0x63, 0x78, 0x65)

#define APP_CHA_TX_UUID  BT_UUID_DECLARE_128( \
	0x01, 0x00, 0x6d, 0x6f, 0x63, 0x2e, 0x74, 0x6e, 0x69, 0x6f,\
	0x70, 0x6c, 0x65, 0x63, 0x78, 0x65)

#else
/* "e49a25f8-f69a-11e8-8eb2-f2801f1b9fd1"  uuid service */
#define APP_SERVICE_UUID  BT_UUID_DECLARE_128( \
	0xd1, 0x9f, 0x1b, 0x1f, 0x80, 0xf2, 0xb2,0x8e, 0xe8, 0x11,\
	0x9a, 0xf6, 0xf8, 0x25, 0x9a, 0xe4)

#define APP_CHA_RX_UUID  BT_UUID_DECLARE_128( \
	0xd1, 0x9f, 0x1b, 0x1f, 0x80, 0xf2, 0xb2,0x8e, 0xe8, 0x11,\
	0x9a, 0xf6, 0xe0, 0x25, 0x9a, 0xe4)

#define APP_CHA_TX_UUID  BT_UUID_DECLARE_128( \
	0xd1, 0x9f, 0x1b, 0x1f, 0x80, 0xf2, 0xb2,0x8e, 0xe8, 0x11,\
	0x9a, 0xf6, 0xe1, 0x28, 0x9a, 0xe4)

#endif

static struct bt_gatt_attr app_gatt_attr[] = {
	BT_GATT_PRIMARY_SERVICE(APP_SERVICE_UUID),

	BT_GATT_CHARACTERISTIC(APP_CHA_RX_UUID,
			       BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE, NULL, NULL, NULL),

	BT_GATT_CHARACTERISTIC(APP_CHA_TX_UUID,
			       BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, NULL,
			       NULL, NULL),

	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
};

void selfapp_routine_start_stop(u8_t start)
{
	selfapp_context_t *selfctx = self_get_context();
	selfapp_log_inf("%d %p", start, selfctx);
	if (selfctx) {
		if (start) {
			thread_timer_start(&selfctx->timer, 0,
					   ROUTINE_INTERVAL);
		} else {
			thread_timer_stop(&selfctx->timer);
		}
	}
}

void selfapp_send_msg(u8_t type, u8_t cmd, u8_t param, int val)
{
	struct app_msg msg = { 0 };
	msg.type = type;
	msg.cmd = cmd;
	msg.reserve = param;
	msg.value = val;
	send_async_msg(CONFIG_SYS_APP_NAME, &msg);
}

static void selfapp_connect_callback(int connected, uint8_t connect_type, u32_t handle)
{
	selfapp_context_t *selfctx = self_get_context();

	selfapp_log_inf("%d<%d> %x", connected, connect_type, handle);

	if (selfctx == NULL) {
		selfapp_log_wrn("Not inited.");
		return;
	}

	selfapp_send_msg(MSG_SELFAPP_APP_EVENT, SELFAPP_CMD_CALLBACK, (connected<<4)|(connect_type&0xF), handle);
}

/*
 Do not put data from temporary actived source device's to stream.
 Return "Dev Ack" to let app got to main menu.
*/
static void selfapp_handle_old_device_data(const u8_t* data, u16_t len)
{
	u8_t buf[5];

	selfapp_context_t *selfctx = self_get_context();

	if (selfctx == NULL) {
		selfapp_log_wrn("Not inited.");
		return;
	}

	selfapp_log_inf("Got %d data", len);
	selfapp_log_dump(data, len);

	buf[0] = 0xaa;
	buf[1] = 0x00; // dev ack
	buf[2] = 2;
	buf[3] = 3; // APP byebye
	buf[4] = 0;

	// send data
	self_send_data(buf, 5);
}

static bool connect_filter_cb(u32_t handle,uint8_t connect_type)
{
#ifdef CONFIG_OTA_SELF_APP
    return selfapp_otadfu_is_working();
#endif
}

static void selfapp_timer_handler(struct thread_timer *timer, void *pdata)
{
	selfapp_context_t *selfctx = self_get_context();

	// ota_app thread would deal with App command during OTA
	if (selfctx == NULL || otadfu_running()) {
		return;
	}

#ifdef CONFIG_LOGSRV_SELF_APP
    if(selfctx->logsrv){
       selfapp_logsrv_timer_routine();
       return;
    }
#endif

	if (selfctx->stream_hdl && !selfctx->stream_handle_suspend && stream_tell(selfctx->stream_hdl) > 0) {
		selfapp_handler_by_stream(selfctx->stream_hdl);
	}
}

int selfapp_init(p_logsrv_callback_t cb)
{
	selfapp_context_t *selfctx = self_get_context();
	struct sppble_stream_init_param stream_param;
	u8_t color_id = 1;
	if (selfctx) {
		selfapp_log_inf("already inited\n");
		return 0;
	}

	selfctx = malloc(sizeof(selfapp_context_t));
	if (selfctx == NULL) {
		selfapp_log_inf("init nomem\n");
		goto Label_init_fail;
	}
	memset(selfctx, 0, sizeof(selfapp_context_t));

	memset(&stream_param, 0, sizeof(struct sppble_stream_init_param));
	stream_param.spp_uuid = (uint8_t *) app_spp_uuid;
	stream_param.gatt_attr = &app_gatt_attr[0];
	stream_param.attr_size =
	    sizeof(app_gatt_attr) / sizeof(app_gatt_attr[0]);
	stream_param.tx_chrc_attr = &app_gatt_attr[3];
	stream_param.tx_attr = &app_gatt_attr[4];
	stream_param.tx_ccc_attr = &app_gatt_attr[5];
	stream_param.rx_attr = &app_gatt_attr[2];
	stream_param.connect_cb = selfapp_connect_callback;
	stream_param.rx_data_on_changed_con_cb = selfapp_handle_old_device_data;
	stream_param.connect_filter_cb = connect_filter_cb;
	stream_param.read_timeout = OS_NO_WAIT;
	stream_param.write_timeout = OS_NO_WAIT;

	selfctx->stream_hdl = sppble_stream_create(&stream_param);
	if (selfctx->stream_hdl == NULL) {
		selfapp_log_inf("no stream\n");
		goto Label_init_fail;
	}

	thread_timer_init(&(selfctx->timer), selfapp_timer_handler, NULL);

#ifdef CONFIG_LOGSRV_SELF_APP
	selfctx->log_cb = cb;
#endif
	extern int get_color_id_from_ats(u8_t *color_id);
	get_color_id_from_ats(&color_id);
	selfapp_set_model_id(color_id);
	self_set_context(selfctx);
	selfapp_log_inf("inited\n");

	return 0;

 Label_init_fail:
	selfapp_deinit();
	return -1;
}

int selfapp_deinit(void)
{
	selfapp_context_t *selfctx = self_get_context();

	if (selfctx) {
		selfctx->connect_handle = 0;
		selfapp_routine_start_stop(0);
		selfapp_connect_deinit();
		self_set_context(NULL);
		if (thread_timer_is_running(&selfctx->mute_timer)){
			thread_timer_stop(&selfctx->mute_timer);
		}
		mem_free(selfctx);
		selfapp_log_inf("deinited\n");
		return 0;
	}

	return -1;
}

void selfapp_on_connect_event(u8_t connect, u8_t connect_type, u32_t handle)
{
	selfapp_context_t *selfctx = self_get_context();

	if (NULL == selfctx) {
		return;
	}

	/*
	  When first device's connect coming, init sppble stream and timer.

	  When second device's connect coming, flush the sppble stream, and
	  use old sppble stream and timer.

	  When a old device's discconect coming, do not close sppblem stream and
	  timer.

	  When currect device's discconect coming, close sppblem stream and timer.
	*/

	selfapp_log_inf("%d<%d> %x->%x", connect, connect_type, selfctx->connect_handle, handle);
	if (0 != connect) {
		//connect
		if(handle != selfctx->connect_handle){
			// When a new handle conneted, clear stream data.
			self_stamem_save(1);
			selfctx->connect_handle = handle;
			if (selfapp_connect_init() != 0) {
				return;
			}

			if (connect_type == BLE_CONNECT_TYPE) {
				// BLE connected
				selfapp_set_ble_connect(1);
			}

			selfapp_routine_start_stop(1);
		}
	} else {
		//disconnect
		if(selfctx->connect_handle == handle) {
			selfctx->connect_handle = 0;
			selfapp_routine_start_stop(0);
			self_stamem_save(1);
			selfapp_connect_deinit();
		}
	}
}

int selfapp_stream_handle_suspend(uint8_t suspend_enable)
{
	selfapp_context_t *selfctx = self_get_context();

	if(selfctx){
		selfctx->stream_handle_suspend = suspend_enable;
		return 0;
	}else{
		return -EIO;
	}
}

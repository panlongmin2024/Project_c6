#include "selfapp_internal.h"
#include "selfapp_config.h"
#include <acts_bluetooth/host_interface.h>

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

void self_show_sfstream_status(char *tag)
{
	selfapp_context_t *selfctx = self_get_context();
	selfstream_t *sfstream;
	u8_t  i;
	if (selfctx == NULL) {
		return ;
	}

	printk("[Self] sfstream_status %s: 0x%x\n", tag ? tag : "", (u32_t)(selfctx->current_hdl));
	for (i = 0; i < SELFSTREAM_MAX; i++) {
		sfstream = &(selfctx->sfstream[i]);
		if (sfstream->handle) {
			printk("%d: %d_0x%x, open=%d, type=%d, major=%d\n", i+1, sfstream->index, (u32_t)(sfstream->handle), \
				sfstream->opened, sfstream->connect_type, sfstream->major);
		}
	}
}

selfstream_t *self_find_sfstream_by_conctype(u8_t type)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t  i;
	if (selfctx == NULL) {
		return NULL;
	}

	for (i = 0; i < SELFSTREAM_MAX; i++) {
		if (selfctx->sfstream[i].handle && selfctx->sfstream[i].connect_type == type) {
			return &(selfctx->sfstream[i]);
		}
	}

	return NULL;
}

selfstream_t *self_find_sfstream_by_handle(void *stream_hdl)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t  i;
	if (selfctx == NULL || stream_hdl == NULL) {
		return NULL;
	}
	
	for (i = 0; i < SELFSTREAM_MAX; i++) {
		if (selfctx->sfstream[i].handle == stream_hdl) {
			return &(selfctx->sfstream[i]);
		}
	}
	return NULL;
}

selfstream_t *self_find_current_sfstream(void)
{
	selfapp_context_t *selfctx = self_get_context();
	if (selfctx && selfctx->current_hdl) {
		return self_find_sfstream_by_handle(selfctx->current_hdl);
	}
	return NULL;
}

void *self_get_current_streamhdl(void)
{
	// find stream and return stream's handle, to avoid current_hdl not updated invalid
	selfstream_t *cur_sfstream = self_find_current_sfstream();
	if (cur_sfstream) {
		return cur_sfstream->handle;
	}
	return NULL;
}

void self_set_sfstream_major(void)
{
	selfstream_t *sfstream = self_find_current_sfstream();
	if (sfstream) {
		sfstream->major = 1;
	}
	self_show_sfstream_status("major");
}

u8_t self_check_current_sfstream_major(void)
{
	selfstream_t *sfstream = self_find_current_sfstream();
	if (sfstream) {
		return sfstream->major;
	}
	return 0;
}

u8_t *self_get_sendbuf(void)
{
	selfapp_context_t *selfctx = self_get_context();

	if (NULL == selfctx) {
		return NULL;
	}

	if (selfctx->sendbuf) {

		if (selfctx->sendbuf_tid && selfctx->sendbuf_tid != (uint32_t)k_current_get()){
			printk("ERROR:sendbuf tid err %x expected %x\n", (uint32_t)k_current_get(), selfctx->sendbuf_tid);
			panic(NULL);
		}
		return selfctx->sendbuf;
	} else {
		return NULL;
	}
}

int self_send_data_by_handle(void *stream_hdl, u8_t *buf, u16_t len)
{
	if (stream_hdl == NULL || buf == NULL || len == 0) {
		return -1;
	}

	if (len > SELF_SENDBUF_SIZE) {
		selfapp_log_err("[BUG] sendbuf exceed %d_%d\n", len, SELF_SENDBUF_SIZE);
		len = SELF_SENDBUF_SIZE;
	}

	stream_write(stream_hdl, buf, len);

#ifdef SELFAPP_DEBUG
	if (buf[1] != 0x45) {
		printk("[Self] TX: 0x%x\n", (u32_t)stream_hdl);
		selfapp_log_dump(buf, len > 0x20 ? 0x20 : len);
	}
#endif
	return 0;
}

int self_send_data_all_handle(u8_t *buf, u16_t len)
{
	selfapp_context_t *selfctx = self_get_context();
	int  i, ret = 0;
	if (selfctx == NULL) {
		return -1;
	}
	selfapp_log_inf();

	// handle all connected stream
	for (i = 0; i < SELFSTREAM_MAX; i++) {
		if (selfctx->sfstream[i].handle && selfctx->sfstream[i].opened >= SFS_CONCED) {
			ret |= self_send_data_by_handle(selfctx->sfstream[i].handle, buf, len);
		}
	}
	return ret;
}

int self_send_data_major_handle(void *except_hdl, u8_t *buf, u16_t len)
{
	selfapp_context_t *selfctx = self_get_context();
	int  i, ret = 0;
	if (selfctx == NULL) {
		return -1;
	}
	selfapp_log_inf();

	// handle all connected stream
	for (i = 0; i < SELFSTREAM_MAX; i++) {
		if (selfctx->sfstream[i].handle && (selfctx->sfstream[i].opened >= SFS_CONCED) && selfctx->sfstream[i].major) {
			if (except_hdl && except_hdl == selfctx->sfstream[i].handle) {
				continue;
			}
			ret |= self_send_data_by_handle(selfctx->sfstream[i].handle, buf, len);
		}
	}
	return ret;
}

int self_send_data(u8_t *buf, u16_t len)
{
	selfapp_context_t *selfctx = self_get_context();
	void *stream_hdl = NULL;
	int   ret = -1;

	if (selfctx) {
		stream_hdl = selfctx->current_hdl;
	}
	ret = self_send_data_by_handle(stream_hdl, buf, len);

	if (selfapp_check_status_response(buf[1])) {
		self_send_data_major_handle(stream_hdl, buf, len);
	}
	return ret;
}

int selfapp_command_process_by_stream(void *handle)
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

	stream_handle = handle ? handle : selfctx->current_hdl;
	if (stream_handle == NULL) {
		selfapp_log_inf("no stream\n");
		return -1;
	}
	if (stream_tell(stream_handle) <= 0) {
		return -16;
	}

	// specified handle, all followed opertion is this handle
	if (selfctx->current_hdl != stream_handle) {
		selfapp_log_inf("switchhdl 0x%x -> 0x%x", (u32_t)(selfctx->current_hdl), (u32_t)stream_handle);
		selfctx->current_hdl =  stream_handle;
	}

	selfctx->sendbuf_tid = (uint32_t)k_current_get();
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
                selfapp_logsrv_init(stream_handle);
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
			recv_len += stream_read(stream_handle, payload_buf, PayloadLen);
		} else {
			stream_read(stream_handle, NULL, PayloadLen);	//indicate and drop it
		}
	}

	//+3(+4): the length of package header
	if (recv_len != PayloadLen + 3 && recv_len != PayloadLen + 4) {
		selfapp_log_inf("stream fail 0x%x:%d,%d\n", CmdID, recv_len, PayloadLen);
		return -1;
	}

	return selfapp_cmd_handler(buf, recv_len);
}

void selfapp_command_process(void)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t  i;
	if (selfctx == NULL) {
		return ;
	}

	// handle all connected stream
	for (i = 0; i < SELFSTREAM_MAX; i++) {
		if (selfctx->sfstream[i].handle && selfctx->sfstream[i].opened >= SFS_CONCED) {
			selfapp_command_process_by_stream(selfctx->sfstream[i].handle);
		}
	}
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
	if (selfctx->stream_handle_suspend) {
		return ;
	}

	selfapp_command_process();
}

static int selfapp_connect_init(selfstream_t *sfstream)
{
	selfapp_context_t *selfctx = self_get_context();
	if (selfctx == NULL || sfstream == NULL || sfstream->handle == NULL) {
		return -1;
	}

	sfstream->major = 0;
#if SELFSTREAM_PREOPEN
	if (sfstream->opened != SFS_OPENED) {
		selfapp_log_wrn("notmatch %d", sfstream->opened);
	}
	sfstream->opened = SFS_CONCED;
#else
	os_sched_lock();

	if (sfstream->opened == SFS_NONE) {
		if (stream_open(sfstream->handle, MODE_IN_OUT) != 0) {
			goto Label_conc_fail;
		}
		sfstream->opened = SFS_CONCED;
	}

	if (selfctx->sendbuf == NULL) {
		selfctx->sendbuf = mem_malloc(SELF_SENDBUF_SIZE + SELF_RECVBUF_SIZE);
		if (selfctx->sendbuf == NULL) {
			goto Label_conc_fail;
		}

		selfctx->sendbuf_tid = (uint32_t)k_current_get();
		selfctx->recvbuf = &(selfctx->sendbuf[SELF_SENDBUF_SIZE]);
	}

	os_sched_unlock();
#endif
	selfctx->connect_num += 1;
	if (selfctx->current_hdl == NULL) {
		selfctx->current_hdl  = sfstream->handle;
	}

	// things need to do only when the 1st connection
	if (selfctx->connect_num == 1) {
#ifdef SPEC_LED_PATTERN_CONTROL
		ledpkg_init();
#endif
	}

	selfapp_log_inf("idx %d/%d, handle 0x%x_%d", sfstream->index, selfctx->connect_num, (u32_t)(sfstream->handle), sfstream->connect_type);
	self_show_sfstream_status("connected");
	return 0;

#if (SELFSTREAM_PREOPEN == 0)
Label_conc_fail:
	sfstream->major = 0;
	selfapp_log_err("fail, idx %d/%d, %d_%p", sfstream->index, selfctx->connect_num, sfstream->opened, selfctx->sendbuf);
	if (selfctx->sendbuf && selfctx->connect_num == 0) {  // free sendbuf only when no connected
		mem_free(selfctx->sendbuf);
		selfctx->sendbuf = NULL;
		selfctx->recvbuf = NULL;
	}

	if (sfstream->opened >= SFS_CONCED) {
		stream_close(sfstream->handle);
		sfstream->opened = SFS_NONE;
	}
	os_sched_unlock();
	return -1;
#endif
}

static int selfapp_connect_deinit(selfstream_t *sfstream)
{
	selfapp_context_t *selfctx = self_get_context();
	if (selfctx == NULL || sfstream == NULL || sfstream->handle == NULL) {
		return -1;
	}

#ifdef CONFIG_LOGSRV_SELF_APP
	if (selfctx->logsrv && sfstream->handle == selfapp_logsrv_get_datastream()) {
		selfapp_logsrv_exit();
		selfctx->logsrv = NULL;
	}
#endif

	sfstream->major = 0;
#if SELFSTREAM_PREOPEN
	if (sfstream->opened != SFS_CONCED) {
		selfapp_log_wrn("notmatch %d", sfstream->opened);
	}
	sfstream->opened = SFS_OPENED;
#else
	if (sfstream->opened >= SFS_CONCED) {
		stream_close(sfstream->handle);
		sfstream->opened = SFS_NONE;
	}
#endif
	if (selfctx->connect_num > 0) {
		selfctx->connect_num -= 1;
	}
	if (selfctx->current_hdl == sfstream->handle) {
		selfctx->current_hdl =  NULL;
	}

	selfapp_log_inf("%d_0x%x, rest=%d", sfstream->index, (u32_t)(sfstream->handle), selfctx->connect_num);
	self_show_sfstream_status("disconnected");

	// other App connecting, don't free resource
	if (selfctx->connect_num > 0) {
		return 0;
	}

	if (selfctx->otadfu) {
		otadfu_NotifyDfuCancel(0);
	}
#ifdef SPEC_LED_PATTERN_CONTROL
	if (selfctx->ledpkg) {
		ledpkg_deinit();
	}
#endif
#if (SELFSTREAM_PREOPEN == 0)
	if (selfctx->sendbuf) {
		mem_free(selfctx->sendbuf);
		selfctx->sendbuf = NULL;
		selfctx->recvbuf = NULL;
	}
#endif
	if(selfctx->pause_player){
		selfctx->pause_player = 0;
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
			thread_timer_start(&selfctx->timer, 0, ROUTINE_INTERVAL);
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

static void selfapp_connect_callback(int connected, uint8_t connect_type, void *stream_hdl)
{
	selfapp_context_t *selfctx = self_get_context();

	selfapp_log_inf("%d(%d) 0x%x", connected, connect_type, (u32_t)stream_hdl);
	if (selfctx == NULL) {
		selfapp_log_wrn("Not inited.");
		return;
	}

	// pre-open stream, to avoid data lost
	if (connected) {
		selfstream_t *sfstream = self_find_sfstream_by_handle(stream_hdl);
		if (sfstream == NULL || sfstream->connect_type != NONE_CONNECT_TYPE) {
			selfapp_log_err("%d connected", sfstream ? sfstream->connect_type : 0xFF);
			return ;
		}
#if (SELFSTREAM_PREOPEN == 0)
		if (sfstream->opened == SFS_NONE) {
			if (stream_open(sfstream->handle, MODE_IN_OUT) != 0) {
				return ;
			}
			sfstream->opened = SFS_CONCED;
		}
#endif
	}

	// release to application-thread, avoid bluetooth-thread block too long
	selfapp_send_msg(MSG_SELFAPP_APP_EVENT, SELFAPP_CMD_CALLBACK, (connected<<4)|(connect_type&0xF), (int)stream_hdl);
}

static bool selfapp_connect_filter_cb(void *conn, uint8_t connect_type)
{
#ifdef CONFIG_OTA_SELF_APP
	int err = -1;

	if (selfapp_otadfu_is_working()) {
		err = hostif_bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		if (err) {
			SYS_LOG_INF("conn_disconnect failed (err %d)", err);
		}
		return true;
	}else {
		if (connect_type == GATT_OVER_BR_CONNECT_TYPE) {
			SYS_LOG_INF("0x%x, %x", (u32_t)conn, connect_type);
			if (self_find_sfstream_by_conctype(GATT_OVER_BR_CONNECT_TYPE)) {
				err = hostif_bt_gatt_over_br_disconnect(conn);
				if (err) {
					SYS_LOG_INF("BR_disconnect failed (err %d)", err);
				}
				return true;
			}
		}
	}
#endif
	return false;
}

int selfapp_init(p_logsrv_callback_t cb)
{
	selfapp_context_t *selfctx = self_get_context();
	struct sppble_stream_init_param stream_param;
	u8_t  i, has_stream = 0, color_id = 1;
	if (selfctx) {
		selfapp_log_inf("already");
		return 0;
	}

	selfctx = malloc(sizeof(selfapp_context_t));
	if (selfctx == NULL) {
		selfapp_log_inf("nomem");
		goto Label_init_fail;
	}
	memset(selfctx, 0, sizeof(selfapp_context_t));

	memset(&stream_param, 0, sizeof(struct sppble_stream_init_param));
	stream_param.spp_uuid     = (u8_t *)app_spp_uuid;
	stream_param.gatt_attr    = &app_gatt_attr[0];
	stream_param.attr_size    = sizeof(app_gatt_attr) / sizeof(app_gatt_attr[0]);
	stream_param.tx_chrc_attr = &app_gatt_attr[3];
	stream_param.tx_attr      = &app_gatt_attr[4];
	stream_param.tx_ccc_attr  = &app_gatt_attr[5];
	stream_param.rx_attr      = &app_gatt_attr[2];
	stream_param.connect_cb   = selfapp_connect_callback;
	stream_param.connect_filter_cb = selfapp_connect_filter_cb;
	stream_param.read_timeout  = OS_NO_WAIT;
	stream_param.write_timeout = OS_NO_WAIT;
	stream_param.read_buf_size = SELF_STREAM_SIZE;
	stream_param.write_attr_enable_ccc = 1;

	for (i = 0; i < SELFSTREAM_MAX; i++) {
		selfctx->sfstream[i].handle = sppble_stream_create(&stream_param);
		selfctx->sfstream[i].index  = i + 1;
		selfctx->sfstream[i].opened = SFS_NONE;

		if (selfctx->sfstream[i].handle) {
#if SELFSTREAM_PREOPEN
			if (stream_open(selfctx->sfstream[i].handle, MODE_IN_OUT) != 0) {
				stream_close(selfctx->sfstream[i].handle);
				selfctx->sfstream[i].handle = NULL;
				continue;
			}
			selfctx->sfstream[i].opened = SFS_OPENED;
#endif
			if (has_stream == 0) {
				has_stream = 1;
			}
		}
	}
	if (has_stream == 0) {
		selfapp_log_err("nostream");
		goto Label_init_fail;
	}

#if SELFSTREAM_PREOPEN
	if (selfctx->sendbuf == NULL) {
		selfctx->sendbuf = mem_malloc(SELF_SENDBUF_SIZE + SELF_RECVBUF_SIZE);
		if (selfctx->sendbuf == NULL) {
			goto Label_init_fail;
		}

		selfctx->sendbuf_tid = (uint32_t)k_current_get();
		selfctx->recvbuf = &(selfctx->sendbuf[SELF_SENDBUF_SIZE]);
	}
#endif

	thread_timer_init(&(selfctx->timer), selfapp_timer_handler, NULL);

#ifdef CONFIG_LOGSRV_SELF_APP
	selfctx->log_cb = cb;
#endif
	extern int get_color_id_from_ats(u8_t *color_id);
	get_color_id_from_ats(&color_id);
	selfapp_set_model_id(color_id);
	self_set_context(selfctx);
	selfapp_log_inf("stream handle:");
	self_show_sfstream_status("init");

	return 0;

 Label_init_fail:
	selfapp_deinit();
	return -1;
}

int selfapp_deinit(void)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t  i;
	if (selfctx == NULL) {
		return -1;
	}

	selfapp_routine_start_stop(0);
	if (thread_timer_is_running(&selfctx->creat_group_timer)){
		thread_timer_stop(&selfctx->creat_group_timer);
	}
	if (thread_timer_is_running(&selfctx->indication_timer)){
		thread_timer_stop(&selfctx->indication_timer);
	}

	for (i = 0; i < SELFSTREAM_MAX; i++) {
		if (selfctx->sfstream[i].handle) {
			selfapp_connect_deinit(&(selfctx->sfstream[i]));
#if SELFSTREAM_PREOPEN
			selfctx->sfstream[i].opened = SFS_NONE;
			stream_close(selfctx->sfstream[i].handle);
#endif
			stream_destroy(selfctx->sfstream[i].handle);
		}
	}

#if SELFSTREAM_PREOPEN
	if (selfctx->sendbuf) {
		mem_free(selfctx->sendbuf);
		selfctx->sendbuf = NULL;
		selfctx->recvbuf = NULL;
	}
#endif

	self_set_context(NULL);
	memset(selfctx, 0, sizeof(selfapp_context_t));
	mem_free(selfctx);
	selfapp_log_inf("finished");
	return 0;
}

void selfapp_on_connect_event(u8_t connect, u8_t connect_type, int stream_hdl)
{
	selfapp_context_t *selfctx = self_get_context();
	selfstream_t *sfstream = NULL;
	if (selfctx == NULL || stream_hdl == 0) {
		selfapp_log_err("conn 0x%d", stream_hdl);
		return;
	}
	selfapp_log_inf("%d(%d), 0x%x", connect, connect_type, (u32_t)stream_hdl);

	sfstream = self_find_sfstream_by_handle((void *)stream_hdl);  // find the specified sfstream
	if (sfstream == NULL) {
		selfapp_log_err("not found");
		return ;
	}

	if (connect) {
		sfstream->connect_type = connect_type;  // 0 NONE, 1 SPP, 2 BLE, 3 GATT_OVER_BR
		selfapp_connect_init(sfstream);
		selfapp_routine_start_stop(1);  // command-handler timer start
	}
	else {
		sfstream->connect_type = NONE_CONNECT_TYPE;
		selfapp_connect_deinit(sfstream);
		if (selfctx->connect_num == 0) {
			selfapp_routine_start_stop(0);  // command-handler timer stop
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

int selfapp_enter_standby(void)
{
	selfapp_log_inf();
	selfapp_send_msg(MSG_SELFAPP_APP_EVENT, SELFAPP_CMD_DISCONNECT, 0, 0);
	selfapp_log_inf("done");
	return 0;
}

int selfapp_disconnect_all_App(void)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t i;
	if (selfctx == NULL) {
		return 0;
	}
	selfapp_log_inf();

	for (i = 0; i < SELFSTREAM_MAX; i++) {
		sppble_stream_disconnect_conn(selfctx->sfstream[i].handle);
	}

	return 0;
}

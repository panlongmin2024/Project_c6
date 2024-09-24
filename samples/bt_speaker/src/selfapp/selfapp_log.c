
#include "selfapp_internal.h"
#include "selfapp_cmd_def.h"
#include "../bt_manager/bt_manager_inner.h"
#include "crc16.h"
#include <misc/byteorder.h>
#include <mem_manager.h>
#include <ringbuff_stream.h>

#define LOG_SERVICE_NAME	"logsrv"

#define LOG_HEAD_SIZE	(SVC_HEAD_SIZE + TLV_HEAD_SIZE)
#define LOG_BUFFER_SIZE	(256 + LOG_HEAD_SIZE)
#define LOG_BUFFER_NUM	(8u) /* must be power of 2 */

#define LOG_TX_PERIOD	(10)
#define LOG_RX_PERIOD	(50)

#define LOG_TYPE_DEFAULT	(0x00)
#define LOG_TYPE_SYSLOG	(0x01)
#define LOG_TYPE_COREDUMP	(0x02)
#define LOG_TYPE_RUN_LOG	(0x03)
#define LOG_TYPE_RAMDUMP    (0x04)
#define LOG_TYPE_EVENTDUMP  (0x05)


/* SVC service ID */
#define SVC_SRV_ID		(0x1D)
/* SVC command ID */
#define SVC_CMD_ID_RX	(0x10)
#define SVC_CMD_ID_ACK	(0x10)
#define SVC_CMD_ID_TX	(0xF0)
#define SVC_CMD_ID_TX_END	(0xF1)
/* SVC param type */
#define SVC_PARAM_TYPE_FIXED	(0x81)

/* TLV console input type */
#define TLV_TYPE_CONSOLE_CODE	(0x00) //控制台输入控制码
#define TLV_TYPE_CONSOLE_TEXT	(0x01) //控制台输入字符串

#define TLV_CODE_PREPARE			(0x01)
#define TLV_CODE_LOG_SYSLOG_START	(0x10)
#define TLV_CODE_LOG_COREDUMP_START	(0x11)
#define TLV_CODE_LOG_RUNTIME_START	(0x12)
#define TLV_CODE_LOG_DEFAULT_START	(0x20)
#define TLV_CODE_LOG_STOP			(0x21)
#define TLV_CDOE_LOG_RAMDUMP_START  (0x13)
#define TLV_CODE_LOG_EVENTDUMP_START (0x14)

/* TLV ack  */
#define TLV_TYPE_ACK	(0x7F)
#define TLV_ACK_OK		(100000)
#define TLV_ACK_INV		(1)

/* TLV log upload type */
#define TLV_TYPE_LOG_NONE		(0xFF)
#define TLV_TYPE_LOG_DEFAULT	(0x00)
#define TLV_TYPE_LOG_SYSLOG		(0x01)
#define TLV_TYPE_LOG_COREDUMP	(0x02)
#define TLV_TYPE_LOG_RUNTIME	(0x03)
#define TLV_TYPE_LOG_RAMDUMP    (0x04)
#define TLV_TYPE_LOG_EVENTDUMP  (0x05)

typedef struct svc_prot_head {
	uint8_t svc_id;
	uint8_t cmd;
	uint8_t param_type;
	uint8_t param_len[2];
} __attribute__((packed)) svc_prot_head_t;

#define SVC_HEAD_SIZE (sizeof(svc_prot_head_t))

typedef struct tlv_head {
	uint8_t type;
	uint8_t len[2]; /* litte-endian [0] lower 1 byte, [1] higher 1 type */
} __attribute__((packed)) tlv_head_t;

typedef struct tlv_dsc {
	uint8_t type;
	uint8_t __pad;
	uint16_t len;
	uint16_t max_len;
	uint8_t *buf;
} tlv_dsc_t;

#define TLV_HEAD_SIZE (sizeof(tlv_head_t))
#define TLV_SIZE(tlv) (TLV_HEAD_SIZE + (tlv)->len)

typedef struct logsrv_ctx {
	io_stream_t sppble_stream;
	uint8_t stream_opened : 1;
	uint8_t prepared : 1;
	uint8_t connect_type;
    uint8_t *log_buf;
	uint32_t log_len;

	/* log type */
	uint8_t log_type;
	uint8_t new_log_type;

	atomic_t drop_cnt;

	struct thread_timer ttimer;
	os_mutex mutex;
#ifdef CONFIG_LOGSRV_SELF_APP
	p_logsrv_callback_t log_cb;
#endif
} logsrv_ctx_t;

extern int print_hex_comm(const char *prefix, const void *data, unsigned int size);

logsrv_ctx_t *logsrv_get_handle(void)
{
	selfapp_context_t *selfctx = self_get_context();
	if (selfctx) {
		return selfctx->logsrv;
	}
	return NULL;
}

static int _sppble_get_rx_data(io_stream_t stream, void *buf, int size)
{
	uint8_t *buf8 = buf;
	int read_size;

	while (size > 0) {
		read_size = stream_read(stream, buf, size);
		if (read_size <= 0) {
			SYS_LOG_ERR("logsrv: rx failed (%d)", size);
			return -EIO;
		}

		size -= read_size;
		buf8 += read_size;
	}

	return 0;
}

static int _sppble_put_tx_data(io_stream_t stream, void *buf, int size)
{
	int len = stream_write(stream, buf, size);

	if (len != size) {
		SYS_LOG_ERR("logsrv: tx failed (%d/%d)", len, size);
		return -EIO;
	}

	return 0;
}

static int _sppble_drop_rx_data(io_stream_t stream, int size, int timeout)
{
	int wait_ms;
	uint8_t c;

	if (size == 0)
		return 0;

	if (timeout < 0 && size < 0)
		return -EINVAL;

	if (timeout < 0) /* wait forever */
		timeout = INT_MAX;

	if (size < 0) /* drop all data */
		size = INT_MAX;

	wait_ms = timeout;

	while (size > 0) {
		int data_len = stream_tell(stream);

		if (data_len > 0) {
			do {
				int res = _sppble_get_rx_data(stream, &c, sizeof(uint8_t));
				if (res)
					return res;
			} while (--data_len > 0 && --size > 0);

			wait_ms = timeout;
			continue;
		}

		if (wait_ms <= 0)
			break;

		os_sleep(10);
		wait_ms -= 10;
	}

	return (size > 0) ? -ETIME : 0;
}

static void * _tlv_pack_head(void *buf, uint8_t type, uint16_t total_len)
{
	tlv_head_t *head = (tlv_head_t *)buf;

	head->type = type;
    sys_put_le16(total_len,head->len);


	return (uint8_t *)buf + sizeof(*head);
}

static void * _tlv_pack_data(void *buf, uint16_t len, const void *data)
{
	uint8_t *buf8 = buf;

	if (len > 0) {
		memcpy(buf8, data, len);
		buf8 += len;
	}

	return buf8;
}

static void * _tlv_pack(void *buf, uint8_t type, uint16_t len,  const void *data)
{
	buf = _tlv_pack_head(buf, type, len);
	buf = _tlv_pack_data(buf, len, data);

	return buf;
}


__unused
static void * _tlv_pack_u8(void *buf, uint8_t type, uint8_t value)
{
	return _tlv_pack(buf, type, sizeof(value), &value);
}

__unused
static void * _tlv_pack_u16(void *buf, uint8_t type, uint16_t value)
{
	return _tlv_pack(buf, type, sizeof(value), &value);
}

__unused
static void * _tlv_pack_u32(void *buf, uint8_t type, uint32_t value)
{
	return _tlv_pack(buf, type, sizeof(value), &value);
}

static int _tlv_unpack_head(io_stream_t stream, tlv_dsc_t *tlv)
{
	tlv_head_t head;
	int res;

	/* read type */
	res = _sppble_get_rx_data(stream, &head, sizeof(head));
	if (res) {
		SYS_LOG_ERR("logsrv: read tlv head failed");
		return res;
	}

	tlv->type = head.type;
	tlv->len = sys_get_le16(head.len);
	return 0;
}

static int _tlv_unpack_data(io_stream_t stream, tlv_dsc_t *tlv)
{
	uint16_t data_len;

	data_len = (tlv->len <= tlv->max_len) ? tlv->len : tlv->max_len;
	if (data_len > 0) {
		int res = _sppble_get_rx_data(stream, tlv->buf, data_len);
		if (res) {
			SYS_LOG_ERR("logsrv: read tlv data failed");
			return res;
		}
	}

	if (tlv->len > tlv->max_len) {
		SYS_LOG_WRN("logsrv: tlv %d: len %u exceed max %u", tlv->type, tlv->len, tlv->max_len);
		_sppble_drop_rx_data(stream, tlv->len - tlv->max_len, -1);
		tlv->len = tlv->max_len;
	}

	return 0;
}

static int _tlv_unpack(io_stream_t stream, tlv_dsc_t *tlv)
{
	int res;

	res = _tlv_unpack_head(stream, tlv);
	if (res == 0 && tlv->len > 0)
		res = _tlv_unpack_data(stream, tlv);

	return res;
}

static void * _svc_prot_pack_head(void *buf, uint8_t cmd, uint16_t param_len)
{
	svc_prot_head_t *head = buf;

	head->svc_id = SVC_SRV_ID;
	head->cmd = cmd;
	head->param_type = SVC_PARAM_TYPE_FIXED;
	sys_put_le16(param_len, head->param_len);

	return (uint8_t *)buf + sizeof(*head);
}

static int _svc_prot_send_ack(io_stream_t stream)
{
	uint8_t buf[sizeof(svc_prot_head_t) + sizeof(tlv_head_t) + 4];
	uint8_t *tlv_buf = _svc_prot_pack_head(buf, SVC_CMD_ID_ACK, sizeof(tlv_head_t) + 4);

    SYS_LOG_INF();
	_tlv_pack_u32(tlv_buf, TLV_TYPE_ACK, TLV_ACK_OK);

	return _sppble_put_tx_data(stream, buf, sizeof(buf));
}

static int _svc_prot_send_end(io_stream_t stream)
{
	uint8_t buf[sizeof(svc_prot_head_t)];
	_svc_prot_pack_head(buf, SVC_CMD_ID_TX_END, 0);

	return _sppble_put_tx_data(stream, buf, sizeof(buf));
}

static int _logsrv_send_log_end(logsrv_ctx_t *ctx)
{
	return _svc_prot_send_end(ctx->sppble_stream);
}

static void _logsrv_clear_log(logsrv_ctx_t *ctx)
{
	os_mutex_lock(&ctx->mutex, OS_FOREVER);

	ctx->log_len = 0;

	os_mutex_unlock(&ctx->mutex);

}


/* Return number of data written */
static uint32_t _logsrv_save_log(logsrv_ctx_t *ctx, const uint8_t *data, uint32_t len, uint8_t type)
{
	uint32_t org_len = len;

    if(!ctx)
        return -EIO;

	os_mutex_lock(&ctx->mutex, OS_FOREVER);

	uint8_t *buf = ctx->log_buf;

    if(len > (LOG_BUFFER_SIZE - LOG_HEAD_SIZE)){
        org_len = LOG_BUFFER_SIZE - LOG_HEAD_SIZE;
    }
    else{
        org_len = len;
    }
	memcpy(buf + LOG_HEAD_SIZE, data, org_len);

	ctx->log_len = org_len;

	os_mutex_unlock(&ctx->mutex);

	return org_len;
}

static int _logsrv_send_log(logsrv_ctx_t *ctx)
{
    int drop_cnt;
    uint8_t *buf8 = ctx->log_buf;
    int ret;

    SYS_LOG_INF("%d",ctx->log_len);

    drop_cnt = atomic_set(&ctx->drop_cnt, 0);
    if (drop_cnt > 0) {
        SYS_LOG_WRN("logsrv: drop %d\n", drop_cnt);
    }

    buf8 = _svc_prot_pack_head(buf8, SVC_CMD_ID_TX, TLV_HEAD_SIZE + ctx->log_len);
    buf8 = _tlv_pack_head(buf8, ctx->log_type, ctx->log_len);

//    print_hex_comm("AAA", ctx->log_buf, ctx->log_len + LOG_HEAD_SIZE);
    ret = _sppble_put_tx_data(ctx->sppble_stream, ctx->log_buf,
            ctx->log_len + LOG_HEAD_SIZE);
    if (ret) {
        return ret;
    }

    ctx->log_len = 0;

    return 0;

}

static int _logsrv_actlog_traverse_callback(uint8_t *data, uint32_t len)
{
	logsrv_ctx_t *ctx = logsrv_get_handle();
	uint32_t xfer_len;
	if (ctx == NULL) {
		return -1;
	}

	while (len > 0) {
		xfer_len = _logsrv_save_log(ctx, data, len, ctx->log_type);
		if (xfer_len <= len) {
			/* make sure the drop cnt correct */
			atomic_dec(&ctx->drop_cnt);

			if (_logsrv_send_log(ctx)) {
				atomic_inc(&ctx->drop_cnt);
				break;
			}
            os_sleep(100);  // for flow control
		}

        len -= xfer_len;
        data += xfer_len;
	}

	return 0;
}

static void _logsrv_actlog_send_syslog(logsrv_ctx_t *ctx, uint8_t syslog_type)
{
	int drop_cnt;

	if(ctx->log_cb){
		ctx->log_cb(syslog_type, _logsrv_actlog_traverse_callback);
	}

	/* send the last logs */
//	if (_logsrv_send_log(ctx)) {
//		atomic_inc(&ctx->drop_cnt);
//	}

	_logsrv_send_log_end(ctx);

	drop_cnt = atomic_get(&ctx->drop_cnt);
	if (drop_cnt > 0) {
		SYS_LOG_WRN("logsrv: drop %d (type %u)\n", drop_cnt, ctx->log_type);
	}
}

static void _logsrv_update_log_type(logsrv_ctx_t *ctx)
{
	if (ctx->new_log_type == ctx->log_type) {
		return;
	}

	if (ctx->log_type == TLV_TYPE_LOG_DEFAULT) {
		_logsrv_send_log_end(ctx);
	}

	_logsrv_clear_log(ctx);
	atomic_set(&ctx->drop_cnt, 0);
	ctx->log_type = ctx->new_log_type;

	switch (ctx->log_type) {
	case TLV_TYPE_LOG_DEFAULT:
//		thread_timer_start(&ctx->ttimer, LOG_TX_PERIOD, LOG_TX_PERIOD);
		break;
	case TLV_TYPE_LOG_SYSLOG:
		_logsrv_actlog_send_syslog(ctx, LOG_TYPE_SYSLOG);
		break;
	case TLV_TYPE_LOG_COREDUMP:
		_logsrv_actlog_send_syslog(ctx, LOG_TYPE_COREDUMP);
		break;
	case TLV_TYPE_LOG_RAMDUMP:
		_logsrv_actlog_send_syslog(ctx, LOG_TYPE_RAMDUMP);
		break;
	case TLV_TYPE_LOG_EVENTDUMP:
		_logsrv_actlog_send_syslog(ctx, LOG_TYPE_EVENTDUMP);
		break;
	case TLV_TYPE_LOG_RUNTIME:
//		_logsrv_actlog_send_syslog(ctx, LOG_TYPE_RUN_LOG);
		break;
	default:
		break;
	}

//	if (ctx->log_type != TLV_TYPE_LOG_DEFAULT) {
//		thread_timer_start(&ctx->ttimer, LOG_RX_PERIOD, LOG_RX_PERIOD);
//	}
}

static int _logsrv_proc_console_codes(logsrv_ctx_t *ctx, tlv_dsc_t *tlv)
{
	uint16_t code;

	if (tlv->len != 2)
		return -EINVAL;

	code = sys_get_le16(tlv->buf);
	SYS_LOG_INF("logsrv: code 0x%x\n", code);

	switch (code) {
	case TLV_CODE_LOG_DEFAULT_START:
		ctx->new_log_type = TLV_TYPE_LOG_DEFAULT;
		break;
	case TLV_CODE_LOG_SYSLOG_START:
		ctx->new_log_type = TLV_TYPE_LOG_SYSLOG;
		break;
	case TLV_CODE_LOG_COREDUMP_START:
		ctx->new_log_type = TLV_TYPE_LOG_COREDUMP;
		break;
	case TLV_CODE_LOG_RUNTIME_START:
		ctx->new_log_type = TLV_TYPE_LOG_RUNTIME;
		break;
	case TLV_CDOE_LOG_RAMDUMP_START:
		ctx->new_log_type = TLV_TYPE_LOG_RAMDUMP;
		break;
	case TLV_CODE_LOG_EVENTDUMP_START:
		ctx->new_log_type = TLV_TYPE_LOG_EVENTDUMP;
		break;
	case TLV_CODE_LOG_STOP:
		ctx->new_log_type = TLV_TYPE_LOG_NONE;
		break;
	case TLV_CODE_PREPARE:
		SYS_LOG_INF("logsrv: prepared\n");
		ctx->prepared = 1;
		break;
	default:
		break;
	}

	return 0;
}

static int _logsrv_proc_console_text(logsrv_ctx_t *ctx, tlv_dsc_t *tlv)
{
	int ret = 0;

	if (tlv->len <= 0)
		return -ENODATA;

	tlv->buf[MIN(tlv->len, tlv->max_len - 1)] = 0;
	SYS_LOG_INF("logsrv: cmd(%d) %s", tlv->len, (char *)tlv->buf);

	/* send real-time logs */
	_logsrv_send_log(ctx);

	return ret;
}

static int _logsrv_proc_tlv_cmd(logsrv_ctx_t *ctx, uint16_t param_len)
{
	static uint8_t s_tlv_buf[64];
	tlv_dsc_t tlv = { .max_len = sizeof(s_tlv_buf), .buf = s_tlv_buf, };


	while (param_len > 0) {
		if (_tlv_unpack(ctx->sppble_stream, &tlv))
			return -ENODATA;

		SYS_LOG_INF("logsrv: tlv type 0x%x, len %u, param_len %d\n",
				tlv.type, tlv.len, param_len);

		switch (tlv.type) {
		case TLV_TYPE_CONSOLE_CODE:
			_logsrv_proc_console_codes(ctx, &tlv);
			break;
		case TLV_TYPE_CONSOLE_TEXT:
			_logsrv_proc_console_text(ctx, &tlv);
			break;
		default:
			SYS_LOG_ERR("logsrv: invalid tlv type %u\n", tlv.type);
			break;
		}

		param_len -= TLV_SIZE(&tlv);
	}

	return 0;
}

static void _logsrv_proc_svc_cmd(logsrv_ctx_t *ctx)
{
	svc_prot_head_t head;
	uint16_t param_len;
	int res = -EINVAL;

	if (!ctx->stream_opened ||
		stream_tell(ctx->sppble_stream) < sizeof(head)) {
		return;
	}

	if (_sppble_get_rx_data(ctx->sppble_stream, &head, sizeof(head))) {
		SYS_LOG_ERR("read svc_prot_head failed");
		return;
	}

	param_len = sys_get_le16(head.param_len);

	SYS_LOG_DBG("logsrv: svc_id 0x%x, cmd_id 0x%x, param type 0x%x, param len 0x%x\n",
		head.svc_id, head.cmd, head.param_type, param_len);

	if (head.svc_id != SVC_SRV_ID) {
		SYS_LOG_ERR("logsrv: invalid svc_id 0x%x\n", head.svc_id);
		goto out_exit;
	}

	if (head.param_type != SVC_PARAM_TYPE_FIXED) {
		SYS_LOG_ERR("logsrv: invalid param type 0x%x\n", head.param_type);
		goto out_exit;
	}

	switch (head.cmd) {
	case SVC_CMD_ID_RX:
		res = _logsrv_proc_tlv_cmd(ctx, param_len);
		break;
	default:
		SYS_LOG_ERR("logsrv: invalid svc cmd %d", head.cmd);
		break;
	}

out_exit:
	if (res) {
		SYS_LOG_ERR("logsrv: invalid svc, drop all\n");
		_sppble_drop_rx_data(ctx->sppble_stream, -1, 500);
	} else {
		_svc_prot_send_ack(ctx->sppble_stream);
	}
}

int selfapp_logsrv_check_id(u8_t *buf,u16_t len)
{
	svc_prot_head_t *head = (svc_prot_head_t *)buf;
	uint16_t param_len;
	tlv_head_t *tlv_head;
	uint16_t code;

    if(len < LOG_HEAD_SIZE + 2){
        return -EINVAL;
    }

	param_len = sys_get_le16(head->param_len);

	SYS_LOG_INF("svc 0x%x, cmd 0x%x,type 0x%x,len:0x%x\n",
		head->svc_id, head->cmd, head->param_type, param_len);

	if (head->svc_id != SVC_SRV_ID) {
        return -EINVAL;
	}

	if (head->param_type != SVC_PARAM_TYPE_FIXED) {
        return -EINVAL;
	}

	if(head->cmd != SVC_CMD_ID_RX) {
        return -EINVAL;
	}

    tlv_head = (tlv_head_t *)(buf + SVC_HEAD_SIZE);

    code = sys_get_le16(buf + LOG_HEAD_SIZE);

	if(code == TLV_CODE_PREPARE){
	    SYS_LOG_INF("logsrv: prepared\n");
	    return 0;
	}

    return -1;
}

void selfapp_logsrv_timer_routine(void)
{
    logsrv_ctx_t *ctx = logsrv_get_handle();
    if(!ctx){
        return;
    }
	_logsrv_proc_svc_cmd(ctx);
	_logsrv_update_log_type(ctx);

#if 0
	if (ctx->log_type == TLV_TYPE_LOG_DEFAULT) {
		/* send real-time logs */
		_logsrv_send_log(ctx);
	}
#endif
}

void *selfapp_logsrv_get_datastream(void)
{
	selfapp_context_t *selfctx = self_get_context();
	logsrv_ctx_t *ctx = NULL;
	if (selfctx && selfctx->logsrv) {
		ctx = (logsrv_ctx_t *)selfctx->logsrv;
		return ctx->sppble_stream;
	}
	return NULL;
}

int selfapp_logsrv_init(void *data_stream)
{
    logsrv_ctx_t *ctx;
	selfapp_context_t *selfctx = self_get_context();

    if (selfctx->logsrv) {
        selfapp_log_inf("logsrv: already\n");
        return 0;
    }

	ctx = app_mem_malloc(sizeof(logsrv_ctx_t));
	if (!ctx) {
		SYS_LOG_ERR("logsrv: alloc ctx failed\n");
		return -ENOMEM;
	}

	memset(ctx, 0, sizeof(*ctx));
	ctx->log_type = TLV_TYPE_LOG_NONE;
	ctx->new_log_type = TLV_TYPE_LOG_NONE;
    ctx->stream_opened = 1;
    ctx->log_buf = app_mem_malloc(LOG_BUFFER_SIZE);
	/* Just call stream_create once, for register spp/ble service
	 * not need call stream_destroy
	 */
	ctx->sppble_stream = (io_stream_t)data_stream;
	if (!ctx->sppble_stream) {
		SYS_LOG_ERR("logsrv: stream_create failed");
		app_mem_free(ctx);
		return -EIO;
	}

	os_mutex_init(&ctx->mutex);

    sppble_stream_modify_write_timeout(ctx->sppble_stream,OS_FOREVER);

    _svc_prot_send_ack(ctx->sppble_stream);
    ctx->prepared = 1;
    //ctx->connect_type = selfctx->connect_type;
    selfctx->logsrv = ctx;

	selfapp_log_inf("logsrv inited:%d\n",(uint32_t)selfctx->logsrv);
	return 0;
}

void selfapp_logsrv_exit(void)
{
	logsrv_ctx_t *logsrv_ctx = logsrv_get_handle();
	if (logsrv_ctx == NULL) {
		return ;
	}

	logsrv_ctx->new_log_type = TLV_TYPE_LOG_NONE;

//	thread_timer_stop(&logsrv_ctx->ttimer);

    if(logsrv_ctx->log_buf){
        app_mem_free(logsrv_ctx->log_buf);
    }
	app_mem_free(logsrv_ctx);

	logsrv_ctx = NULL;

	selfapp_log_inf("logsrv: exit\n");
}

int selfapp_logsrv_callback_register(p_logsrv_callback_t cb)
{
	logsrv_ctx_t *logsrv_ctx = logsrv_get_handle();
	if (logsrv_ctx) {
		logsrv_ctx->log_cb = cb;
	}

	return 0;
}


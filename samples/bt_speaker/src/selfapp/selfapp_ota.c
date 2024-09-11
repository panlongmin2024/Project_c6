#include "selfapp_internal.h"
#include "selfapp_cmd_def.h"
#include "../bt_manager/bt_manager_inner.h"
#include "crc16.h"

#include <mem_manager.h>
#include <ringbuff_stream.h>

#include <ota_backend_selfapp.h>
#include <ota_upgrade.h>
#include <fw_version.h>
#include <sys_event.h>


#define OTADFU_BY_DATA_STREAM   (1)  //use stream to manager dfu data

#define OTADFU_SLEEP_MS      (2)
#define OTADFU_MUTEX_TIMEOUT (OS_NO_WAIT)

#define OTADFU_FRAME_LEN     (34)
#define OTADFU_FRAME_DATA_LEN (OTADFU_FRAME_LEN - 2)
#define OTADFU_DATABUF_SIZE  (74 * 1024)
#define OTADFU_REMAIN_DATABUF_SIZE (1024)
#define OTADFU_READ_BLOCK    (512)

#define OTADFU_READ_TIMEOUT (5*1000)
#define OTADFU_CMD_TIMEOUT  (1 * 1000)

#define OTADFU_HEADER_DATA_SIZE (4 * 1024)
#define OTADFU_THREAD_DATA_SIZE (2 * 1024)

/* 2023/06/09, by liushuihua, App version: 5.7.8 (IOS DebugVer)
 * Actions's ota.bin is composed of index table and files,
 * which means the Header info(including index table) must be obtained whenever OTA starts.
 * OTA starts from 0x0 offset, and re-starts from breakpoint after all Header info received and verified successfully.
 * However, it seems that there is a BUG of JBL Portable App: The 2nd package after re-started is bad data.
 * Usually, the Header info is less than 0x600, so it takes 3 packages and 504 Bytes per package.
 * And then we send a Ready status with breakpoint (DFUCMD_NotifyDfuStatusChange) to re-start, discarding all received data.
 * The 1st package data comes and it does be from the breakpoint. But the 2nd package is from 4 packages after the breakpoint.
 * BP_SUGGESTION: move forward the breakpoint and takes data from the 2nd package (makes the 2nd package to be the breakpoint)
 */
#define OTADFU_BP_SUGGESTION     (1)
#define OTADFU_BP_RESUMELOG      (2)	// the 2nd package might error

enum dfu_engine_status_e {
	DFUSTA_ERROR = 0x00,

	DFUSTA_READY = 0x01,	// send to App so that it would start to send data from breakpoint offset
	DFUSTA_DOWNLOADING,	// when the 1st Package received
	DFUSTA_UPLOADING,	// all data received, send to App so that it would wait for reboot connection
	DFUSTA_CANCEL,

	DFUSTA_INITED = 0xEE,
	DFUSTA_UNKNOWN = 0xFF,
};

enum dfu_BPtype_e {
	BPTYPE_NormalCase = 0x01,
	BPTYPE_ResetBreakpoint = 0x02,
	BPTYPE_Silent_NormalCase = 0x11,
	BPTYPE_Silent_ResetBreakpoint = 0x12,
};

enum dfu_Image_type_e {
	DFU_IMAGE_READY = 0x01,
	DFU_IMAGE_CRC_ERROR = 0x02,
	DFU_IMAGE_UNKOWN_ERROR = 0x03,
};


typedef struct {
	u32_t dfu_crc;
	u32_t dfu_secidx:8;
	u32_t dfu_size:24;	// big endian
	u32_t dfu_version:24;
	u8_t dfu_bptype:8;
} dfu_start_t;

typedef struct __attribute__((packed)) {
	u8_t state;
	u8_t offset[4];
	u8_t offset_status;
} dfuack_start_t;

typedef struct {
	u8_t state;
	u8_t seqidx;
} dfuack_data_t;

typedef struct {
	dfu_start_t info;

	u8_t *data_buf;
#ifdef OTADFU_BY_DATA_STREAM
	struct acts_ringbuf ring_buf;
	io_stream_t data_stream;
	os_mutex stream_mutex;	// couldn't write and read at the same time

	u8_t stream_opened;
#else
	u32_t outer_len;	// the length of data in data_buf
	u32_t outer_temp_len;
#endif
	u8_t buf_flushing;	// data_buf is being flushing

	u8_t data_sequence;
	u8_t state;
	u8_t drop_count;

	u32_t file_offs;	// the offset of ota_bin_file that App has sent, for dropping useless data
	u32_t write_offs;	// the offset of ota_bin_file that App next write, for debug
	u32_t bp_offs;		// breakpoint of ota_bin_file, report to App so that it would resume

	u8_t flag_abort;	// error occured
	u8_t flag_cancel;	// cancel by App
	u8_t flag_reading;

	u8_t flag_delay_report;
	u8_t delayed_sequence;

	u8_t flag_bp_resumed;
	u8_t flag_bp_waiting;
#ifdef OTADFU_BP_SUGGESTION
	u8_t flag_bp_droppkg;
#endif
	u8_t flag_send_complete;
	u16_t head_data_len;
	u8_t *head_data_buf;
	u16_t last_index;
	u32_t last_frame_checksum;
	u32_t recv_size;
	u32_t recv_frame_size;

	uint8_t *remaining_data;
	uint8_t *check_data;
	uint16_t remaining_len;
	uint16_t check_size;

	os_sem read_sem;
	char *app_cmd_thread_stack;
	os_tid_t app_cmd_thread;
	uint8_t thread_terminated;
	uint8_t thread_need_terminated;
	uint8_t flag_no_send_ack;

} otadfu_handle_t;

static int dfu_data_buf_flush(otadfu_handle_t * otadfu);
static u8_t force_exit_ota;
static __ota_bss uint8_t ota_thread_data_buffer[OTADFU_THREAD_DATA_SIZE];
static __ota_bss uint8_t ota_data_buffer[OTADFU_DATABUF_SIZE] ;
static __ota_bss uint8_t ota_header_data_buffer[OTADFU_HEADER_DATA_SIZE];
static __ota_bss uint8_t ota_remain_data_buffer[OTADFU_REMAIN_DATABUF_SIZE];

K_MUTEX_DEFINE(otadfu_mutex);

otadfu_handle_t *otadfu_get_handle(void)
{
	selfapp_context_t *selfctx = self_get_context();
	if (selfctx && selfctx->otadfu) {
		return selfctx->otadfu;
	}
	return NULL;
}

int otadfu_set_handle(otadfu_handle_t * handle)
{
	selfapp_context_t *selfctx = self_get_context();
	if (selfctx) {
		selfctx->otadfu = (void *)handle;
		return 0;
	}
	return -1;
}

static int otadfu_set_state(u8_t new_state)
{
	otadfu_handle_t *otadfu = otadfu_get_handle();
	u8_t old_state;

	if (otadfu == NULL) {
		return -1;
	}

	old_state = otadfu->state;
	otadfu->state = new_state;

	selfapp_log_inf("%d->%d", old_state, new_state);

	return 0;
}

bool selfapp_otadfu_is_working(void)
{
	otadfu_handle_t *otadfu = otadfu_get_handle();
	if (otadfu) {
		if((otadfu->state == DFUSTA_DOWNLOADING) || (otadfu->state == DFUSTA_UPLOADING)){
		    return true;
		}
		else{
		    return false;
		}
	}
	return false;
}

u8_t otadfu_get_state(void)
{
	otadfu_handle_t *otadfu = otadfu_get_handle();
	if (otadfu) {
		return otadfu->state;
	}
	return DFUSTA_UNKNOWN;
}

#ifdef CONFIG_OTA_SELF_APP
static int dfureport_state(u8_t state)
{
	u8_t buf[8];
	u16_t sendlen = 0;

	sendlen +=
	    selfapp_pack_cmd_with_int(buf, DFUCMD_NotifyDfuStatusChange,
				 (u32_t) state, 1);
	if (state == DFUSTA_ERROR) {
		selfapp_log_wrn("dfureport ERROR: %x%x%02x%02x\n", buf[0], buf[1],
		       buf[2], buf[3]);
	}
	return self_send_data(buf, sendlen);
}

static int dfureport_image_state(u8_t state)
{
	u8_t buf[8];
	u16_t sendlen = 0;

	sendlen +=
	    selfapp_pack_cmd_with_int(buf, DFUCMD_RetImageCRCChecked,
				 (u32_t) state, 1);

	return self_send_data(buf, sendlen);
}


static void dfureport_state_error()
{
	otadfu_set_state(DFUSTA_ERROR);
	dfureport_state(DFUSTA_ERROR);
}

static int dfureport_state_sequence(u8_t state, u8_t sequence)
{
	u8_t buf[8];
	u16_t sendlen = 0;
	dfuack_data_t dataack;

	dataack.state = state;
	dataack.seqidx = sequence;
	sendlen +=
	    selfapp_pack_cmd_with_bytes(buf, DFUCMD_NotifyDfuStatusChange,
			       (u8_t *) & dataack, sizeof(dfuack_data_t));
	return self_send_data(buf, sendlen);
}

static int dfu_release(otadfu_handle_t * otadfu)
{
	if (otadfu == NULL) {
		return 0;
	}

	dfu_data_buf_flush(otadfu);

#ifdef OTADFU_BY_DATA_STREAM
	if (otadfu->data_stream) {
		stream_close(otadfu->data_stream);
		stream_destroy(otadfu->data_stream);
	}
#endif

	//if (otadfu->data_buf) {
	//	mem_free(otadfu->data_buf);
	//}

	mem_free(otadfu);
	otadfu_set_handle(NULL);

    bt_manager_set_user_visual(0,0,0,0);

	selfapp_log_inf("dfu released\n");
	return 0;
}

static int dfu_prepare(dfu_start_t * param)
{
	otadfu_handle_t *handle = otadfu_get_handle();
	int ret = 0;

	selfapp_log_inf("dfupre_param: 0x%x_%x_0x%x_0x%x_%x\n", param->dfu_crc,
	       param->dfu_secidx, param->dfu_size, param->dfu_version,
	       param->dfu_bptype);

	// App didn't find the ota.bin
	if (param->dfu_size <= 0) {
		selfapp_log_inf("dfupre Appnofile\n");
		return -1;
	}

	if (handle) {
		selfapp_log_inf("dfupre: already\n");
		handle->flag_abort = 0;
		handle->flag_cancel = 0;

		dfu_data_buf_flush(handle);
 		return 0;
	}

	handle = (otadfu_handle_t *) mem_malloc(sizeof(otadfu_handle_t));
	if (handle == NULL) {
		selfapp_log_wrn("dfupre nomem\n");
		return -1;
	}
	if (otadfu_set_handle(handle) != 0) {
		mem_free(handle);
		return -1;
	}

	memset(handle, 0, sizeof(otadfu_handle_t));
	memcpy(&(handle->info), param, sizeof(dfu_start_t));

	handle->data_buf = ota_data_buffer;
	if (handle->data_buf == NULL) {
		ret = -2;
		goto label_fail;
	}

#ifdef OTADFU_BY_DATA_STREAM
	acts_ringbuf_init(&(handle->ring_buf), handle->data_buf,
			  OTADFU_DATABUF_SIZE);
	handle->data_stream = ringbuff_stream_create(&(handle->ring_buf));
	if (handle->data_stream == NULL) {
		ret = -3;
		goto label_fail;
	}
	if (stream_open(handle->data_stream, MODE_IN_OUT) != 0) {
		ret = -4;
		goto label_fail;
	}
	handle->stream_opened = 1;
#endif

	handle->state = DFUSTA_UNKNOWN;

 label_fail:
	selfapp_log_inf("dfu prepare %d\n", ret);
	if (ret) {
		dfu_release(handle);
	}
	return ret;
}

static int dfureport_start(otadfu_handle_t * otadfu, u32_t offset)
{
	u8_t buf[16];
	u16_t sendlen = 0;
	dfuack_start_t startack;
	if (buf == NULL) {
		return -1;
	}

	startack.state = otadfu->state;
	startack.offset_status = 0;	//0x1 empty, 0x2 refresh, 0x3 fresh resume
	startack.offset[0] = (offset>>24) & 0xFF;
	startack.offset[1] = (offset>>16) & 0xFF;
	startack.offset[2] = (offset>>8) & 0xFF;
	startack.offset[3] = (offset) & 0xFF;

	sendlen +=
	    selfapp_pack_cmd_with_bytes(buf, DFUCMD_NotifyDfuStatusChange,
			       (u8_t *) & startack, sizeof(dfuack_start_t));
	selfapp_log_inf("dfustart: %d_0x%x_%d\n", startack.state, offset,
	       startack.offset_status);
	return self_send_data(buf, sendlen);
}

static int dfu_breakpoint_resume(otadfu_handle_t * otadfu,
				 u32_t suggestion_offset)
{
	u32_t bp_offset;

	// return 0 means OTA without breakpoint
	if (otadfu == NULL) {
		return 0;
	}

	bp_offset = otadfu->bp_offs;
#ifdef OTADFU_BP_SUGGESTION
	if (otadfu->bp_offs <= suggestion_offset) {
		selfapp_log_inf("dfubp: invalid 0x%x_0x%x\n", otadfu->bp_offs,
		       suggestion_offset);
		return 0;
	}
	otadfu->flag_bp_droppkg = 1;
	bp_offset = otadfu->bp_offs - suggestion_offset;

	bp_offset = (otadfu->bp_offs / OTADFU_FRAME_DATA_LEN) * OTADFU_FRAME_LEN - suggestion_offset;
#endif

	otadfu->drop_count = 0;
	otadfu->data_sequence = 0;
	otadfu->file_offs = otadfu->bp_offs;
	otadfu->write_offs = otadfu->file_offs;

	otadfu->bp_offs = 0;
	otadfu->flag_bp_resumed = OTADFU_BP_RESUMELOG;	// log the next packages
	selfapp_log_inf("dfubp: resume 0x%x, sug=0x%x_0x%x\n", otadfu->file_offs,
	       bp_offset, suggestion_offset);

	// dicard all received data (2/2)
	dfu_data_buf_flush(otadfu);

	otadfu_set_state(DFUSTA_READY);
	dfureport_start(otadfu, bp_offset);	// Ask App to send from bp_offs (need be Ready state)
	return 1;
}

static otadfu_handle_t *dfu_read_wait_sync(void)
{
	otadfu_handle_t *otadfu;

	os_mutex_unlock(&otadfu_mutex);
	os_sleep(OTADFU_SLEEP_MS);
	os_mutex_lock(&otadfu_mutex, OS_FOREVER);

	otadfu = otadfu_get_handle();
	return otadfu;
}

//int test_change_data = 0;

static int dfu_data_check_recv_seq(otadfu_handle_t * otadfu, uint8_t sequence, uint32_t len, uint32_t fram_checksum)
{
	otadfu->data_sequence = sequence;

	if(otadfu->last_index == 0xffff){
		if(sequence == 0){
			otadfu->last_index = sequence;
			otadfu->last_frame_checksum = fram_checksum;
		}else{
			//illegal seq
			otadfu->data_sequence = 0;
			dfureport_state_error();
			return -EIO;
		}
	}else{
		if(((otadfu->last_index + 1) & 0xff) == sequence){
			//printk("recv seq %x real seq %x len %u total len %u\n", sequence, (otadfu->last_index + 1), len, otadfu->recv_size);
			otadfu->last_index++;
			otadfu->last_frame_checksum = fram_checksum;
		}else{
			otadfu->last_index++;
			otadfu->last_index = (otadfu->last_index & 0xff00) | sequence;
			if (otadfu->flag_bp_droppkg == 0){
				printk("recv seq %x expected %x len %u total len %u\n", sequence, (otadfu->last_index + 1), len, otadfu->recv_size);
				//return -EIO;

				if(otadfu->last_frame_checksum == fram_checksum){
					printk("recv same frame drop!!!\n");
					return -EIO;
				}

				otadfu->last_frame_checksum = fram_checksum;
			}
		}
	}

	return 0;
}


uint32_t dfu_save_data_buffer(otadfu_handle_t * otadfu, u8_t *data, size_t len)
{
#ifndef OTADFU_BY_DATA_STREAM
	uint8_t *tmp_buf = &(otadfu->data_buf[otadfu->outer_temp_len]);

	if (otadfu->outer_temp_len + len > OTADFU_DATABUF_SIZE) {
		selfapp_log_inf("dfuwrite: data_buf exceed! %d+%d>%d\n",
			   otadfu->outer_temp_len, len,
			   OTADFU_DATABUF_SIZE);
		return 0;
	}

	memcpy(tmp_buf, data, len);

	otadfu->outer_temp_len += len;
#else
	int stream_space = 0;

	if (otadfu->data_stream && otadfu->stream_opened) {
		// must succ, otherwise data shall be dropped, +1: ensure mutex could be locked after reading
		os_mutex_lock(&(otadfu->stream_mutex),
			      OTADFU_MUTEX_TIMEOUT + 1);

		stream_space = stream_get_space(otadfu->data_stream);
		if (stream_space < len) {
			stream_read(otadfu->data_stream, NULL, len);
			otadfu->file_offs += len;
			otadfu->drop_count += 1;
			selfapp_log_inf("dfuwrite: %d drop %d, count %d\n", otadfu->data_sequence,
			       len, otadfu->drop_count);

			if (otadfu->drop_count >= 3) {
				os_mutex_unlock(&(otadfu->stream_mutex));
				dfureport_state_error();
				return -1;
			}
		}

		len = stream_write(otadfu->data_stream, data, len);
		os_mutex_unlock(&(otadfu->stream_mutex));
	}
#endif
	return len;
}

uint32_t dfu_get_data_buffer_size(otadfu_handle_t * otadfu)
{
#ifndef OTADFU_BY_DATA_STREAM
	return otadfu->outer_len;
#else
	return stream_get_length(otadfu->data_stream);
#endif
}

int dfu_get_data_buffer_data(otadfu_handle_t * otadfu, uint8_t *buf, uint32_t size)
{
	int read_len = 0;
#ifndef OTADFU_BY_DATA_STREAM
	if (otadfu->outer_len > size) {
		if (buf) {
			memcpy(buf, otadfu->data_buf, size);
		}
		read_len += size;
		otadfu->outer_len -= size;
		memcpy(otadfu->data_buf, &(otadfu->data_buf[size]),
			   otadfu->outer_len);
	} else {
		if (buf) {
			memcpy(buf, otadfu->data_buf,
				   otadfu->outer_len);
		}
		read_len += otadfu->outer_len;
		otadfu->outer_len = 0;
	}
#else
	if (stream_get_length(otadfu->data_stream) > size) {
		size = stream_read(otadfu->data_stream, buf, size);
		read_len += size;
	} else {
		size = stream_get_length(otadfu->data_stream);
		size = stream_read(otadfu->data_stream, buf, size);
		read_len += size;
	}

#endif
	return read_len;
}

static uint16_t calculate_checksum(u8_t *data, size_t len)
{
	uint32_t seed = 0xA5A5;
    uint32_t checksum = seed;
    uint32_t crc = 0;
	uint32_t i;

    for(i = 0; i < len; i++){
        checksum += data[i];
        crc += checksum;
	}

    return ((checksum | crc) & 0xFFFF);
}

static uint32_t calculate_checksum32(u8_t *data, size_t len)
{
	uint32_t seed = 0xA5A5;
    uint32_t checksum = seed;
    uint32_t crc = 0;
	uint32_t i;

    for(i = 0; i < len; i++){
        checksum += data[i];
        crc += checksum;
	}

    return (checksum | crc) ;
}


static int dfu_data_buf_read(otadfu_handle_t * otadfu, u8_t * buf, u32_t size,
			     u8_t update);

int dfu_process_received_data(otadfu_handle_t * otadfu, u8_t *data, size_t len) {
    uint16_t crc, calculated_crc;
	int write_len = 0;
	size_t to_copy;
	uint16_t check_cnt = 0;
	uint32_t remain_len_bak;

	if (otadfu->state == DFUSTA_READY) {
		otadfu_set_state(DFUSTA_DOWNLOADING);
	}

#ifdef OTADFU_BP_SUGGESTION
	// useless data, not part of ota_bin_file (BUG of App: 2023/06/09 by lsh)
	if (otadfu->flag_bp_droppkg > 0) {
		selfapp_log_inf("dfuwrite: drop package %d_%d_%d\n", otadfu->data_sequence, len, dfu_get_data_buffer_size(otadfu));
		otadfu->flag_bp_droppkg -= 1;
 		dfu_data_buf_read(otadfu, NULL, len, 0);
 		otadfu->remaining_len = 0;
		return -EAGAIN;
	}
#endif

	otadfu->recv_size += len;

	remain_len_bak = otadfu->remaining_len;

#ifndef OTADFU_BY_DATA_STREAM
	otadfu->outer_temp_len = otadfu->outer_len;
#endif

    if (otadfu->remaining_len > 0) {
		if(len > (OTADFU_FRAME_LEN - otadfu->remaining_len)){
			to_copy = OTADFU_FRAME_LEN - otadfu->remaining_len;
		}else{
			to_copy = len;
		}
        memcpy(otadfu->remaining_data + otadfu->remaining_len, data, to_copy);
        otadfu->remaining_len += to_copy;
        len -= to_copy;
        data += to_copy;
		check_cnt++;

        if (otadfu->remaining_len == OTADFU_FRAME_LEN) {
			memcpy(&crc, otadfu->remaining_data + (OTADFU_FRAME_DATA_LEN), sizeof(uint16_t));
            calculated_crc = calculate_checksum(otadfu->remaining_data, OTADFU_FRAME_DATA_LEN);
            if (crc != calculated_crc) {
                printk("CRC error in previous remaining data %x %x\n", crc, calculated_crc);
            }

            write_len += dfu_save_data_buffer(otadfu, otadfu->remaining_data, OTADFU_FRAME_DATA_LEN);

            otadfu->remaining_len = 0;
        }
    }


    while (len >= OTADFU_FRAME_LEN) {
        memcpy(&crc, data + OTADFU_FRAME_DATA_LEN, sizeof(uint16_t));
        calculated_crc = calculate_checksum(data, OTADFU_FRAME_DATA_LEN);
		check_cnt++;
        if (crc != calculated_crc){
            printk("CRC error in data frame %x %x\n", crc, calculated_crc);
			if(otadfu->data_sequence > 0){
				otadfu->data_sequence--;
			}else{
				otadfu->data_sequence = 0xff;
			}

			otadfu->last_index--;
			otadfu->remaining_len = remain_len_bak;
			return -EIO;
        }

        write_len += dfu_save_data_buffer(otadfu, data, OTADFU_FRAME_DATA_LEN);

        len -= OTADFU_FRAME_LEN;
        data += OTADFU_FRAME_LEN;
    }

    if (len > 0) {
        memcpy(otadfu->remaining_data, data, len);
        otadfu->remaining_len = len;
		//printk("remain size %d:%d check cnt %d\n", otadfu->recv_size, otadfu->remaining_len, check_cnt);
    }

	//update write ptr
	otadfu->recv_frame_size += write_len;

#ifndef OTADFU_BY_DATA_STREAM
	otadfu->outer_len = otadfu->outer_temp_len;
#endif
	//printk("recv size %d:%d:%d\n", otadfu->recv_size, otadfu->recv_frame_size, otadfu->outer_len);

	return write_len;
}

static int dfu_data_buf_write(otadfu_handle_t * otadfu, u8_t * seq, u8_t ** ptr,
			      int len)
{
	void *outer_stream = self_get_streamhdl();
	int write_len = 0;
#ifndef OTADFU_BY_DATA_STREAM
	u8_t *tmp_buf = NULL;
#endif
	int frame_checksum;

	if (outer_stream == NULL) {
		selfapp_log_inf("dfuwrite: nostream\n");
		return 0;
	}

	if (stream_read(outer_stream, seq, 1) == 1) {
		if (otadfu && otadfu->data_buf) {

			if (len > otadfu->check_size) {
				selfapp_log_inf("dfuwrite: check data_buf exceed! %d %d\n",
					   otadfu->check_size, len);
				stream_read(outer_stream, NULL, len);
				return 0;
			}

			stream_read(outer_stream, otadfu->check_data, len);

			frame_checksum = calculate_checksum32(otadfu->check_data, len);

			write_len = dfu_data_check_recv_seq(otadfu, *seq, len, frame_checksum);

			if(write_len != 0){
				return write_len;
			}

			//if(otadfu->last_index == 100 && test_change_data == 0){
			//	otadfu->check_data[40] = 0x12;
			//	otadfu->check_data[41] = 0x34;
			//	test_change_data = 1;
			//}

			write_len = dfu_process_received_data(otadfu, otadfu->check_data, len);

#ifndef OTADFU_BY_DATA_STREAM
			tmp_buf = &(otadfu->data_buf[otadfu->outer_len]);
			if (ptr) {
				*ptr = tmp_buf;
			}
#endif
		} else {
			write_len = stream_read(outer_stream, NULL, len);	// drop it
		}
	}

	return write_len;
}


static int dfu_data_buf_read(otadfu_handle_t * otadfu, u8_t * buf, u32_t size,
			     u8_t update)
{
	u32_t read_len = 0;

	if (dfu_get_data_buffer_size(otadfu) > 0) {
		// selfapp_log_inf("dfu readbuf: 0x%x_%d, %s_0x%x\n", otadfu->file_offs, otadfu->outer_len, buf?"read":"drop", size);

		read_len = dfu_get_data_buffer_data(otadfu, buf, size);

		if (update) {
			if((otadfu->file_offs < OTADFU_HEADER_DATA_SIZE) && (otadfu->head_data_len < OTADFU_HEADER_DATA_SIZE)){
				if ((otadfu->head_data_len + read_len) <= OTADFU_HEADER_DATA_SIZE){
					memcpy(otadfu->head_data_buf + otadfu->head_data_len, buf, read_len);
					otadfu->head_data_len += read_len;
				}else{
					memcpy(otadfu->head_data_buf + otadfu->head_data_len, buf, OTADFU_HEADER_DATA_SIZE - otadfu->head_data_len);
					otadfu->head_data_len = OTADFU_HEADER_DATA_SIZE;
				}
			}

			otadfu->file_offs += read_len;

		}
		// selfapp_log_inf("dfu readbuf: 0x%x_%d, %d\n\n", otadfu->file_offs, otadfu->outer_len, read_len);
	}

	return read_len;
}

static int dfu_data_buf_flush(otadfu_handle_t * otadfu)
{
	otadfu->buf_flushing = 1;

#ifdef OTADFU_BY_DATA_STREAM
 	if (otadfu->data_stream && otadfu->stream_opened) {
		stream_flush(otadfu->data_stream);
	}
#else
	if (otadfu->outer_len > 0) {
		memset(otadfu->data_buf, 0, otadfu->outer_len);
		otadfu->outer_len = 0;
	}
#endif

	selfapp_log_inf("dfu bufflush\n");

#ifndef OTADFU_BY_DATA_STREAM
	otadfu->outer_len = 0;
#endif

	otadfu->buf_flushing = 0;
 	return 0;
}

static void dfu_api_exit(struct ota_backend *backend)
{
	// selfapp_log_inf("dfuexit\n");
}

static void _selfapp_rx_data_callback(void)
{
	otadfu_handle_t *otadfu = otadfu_get_handle();

	os_sem_give(&otadfu->read_sem);
}

static void _selfapp_ota_app_cmd_thread(void *p1, void *p2, void *p3)
{
	int ret;
	otadfu_handle_t *otadfu = (otadfu_handle_t *)p1;

	SYS_LOG_INF("ota_app cmd thread init");

	while (!otadfu->thread_need_terminated) {

		ret = os_sem_take(&otadfu->read_sem, OTADFU_CMD_TIMEOUT);

		if(ret == 0){
			/* ota_app thread handle all App command
			 * otadata: BT -> sppble_stream -> otadfu_SetDfuData() -> dfu_api_read() -> ota app buffer
			 */
			selfapp_handler_by_stream(NULL);
		}else{
			//ios cannot send this command in downloading state, app may be abnormal
#if 0
			if(otadfu->state == DFUSTA_DOWNLOADING){
				SYS_LOG_ERR("wait cmd timeout!");
				//dfureport_state(otadfu->state);
			}
#endif
			printk("wait cmd timeout %d\n", otadfu->thread_need_terminated);
		}
	}

	SYS_LOG_INF("ota_app cmd thread exited");
	otadfu->thread_terminated = true;
}

static void ota_app_cmd_thread_start(otadfu_handle_t *otadfu)
{
	void *streamhdl = NULL;
	otadfu->thread_terminated = false;
	otadfu->thread_need_terminated = false;

	os_sem_init(&otadfu->read_sem, 0, 1);

	selfapp_stream_handle_suspend(true);

	streamhdl = self_get_streamhdl();
	if (streamhdl) {
		sppble_stream_set_rxdata_callback(streamhdl, _selfapp_rx_data_callback);
	}

	otadfu->app_cmd_thread_stack = ota_thread_data_buffer;

	otadfu->app_cmd_thread =  (os_tid_t)os_thread_create(otadfu->app_cmd_thread_stack, OTADFU_THREAD_DATA_SIZE, _selfapp_ota_app_cmd_thread,
								otadfu, NULL, NULL, 2, 0, OS_NO_WAIT);

}

static void ota_app_cmd_thread_stop(otadfu_handle_t *otadfu)
{
	uint32_t cur_time;
	void *streamhdl = self_get_streamhdl();
	if (streamhdl) {
		sppble_stream_set_rxdata_callback(streamhdl, NULL);
	}

	otadfu->thread_need_terminated = true;

	os_mutex_unlock(&otadfu_mutex);

	cur_time = k_uptime_get_32();
	while(otadfu->app_cmd_thread && !otadfu->thread_terminated){
		os_sem_give(&otadfu->read_sem);
		os_sleep(10);

		if((k_uptime_get_32() - cur_time) > 1000){
			cur_time = k_uptime_get_32();
			printk("wait ota app cmd thread exit\n");
		}
	}

	os_mutex_lock(&otadfu_mutex, OS_FOREVER);

	selfapp_stream_handle_suspend(false);

}



static int dfu_api_open(struct ota_backend *backend)
{
	otadfu_handle_t *otadfu = otadfu_get_handle();
	int ret = 0;

	if (otadfu == NULL) {
		ret = -16;
		goto label_fail;
	}
#ifdef OTADFU_BY_DATA_STREAM
	if (otadfu->data_stream && otadfu->stream_opened == 0) {
		ret = stream_open(otadfu->data_stream, MODE_IN_OUT);
	}

	if (ret != 0 || otadfu->data_stream == NULL) {
		goto label_fail;
	}
	otadfu->stream_opened = 1;
#endif

	os_mutex_lock(&otadfu_mutex, OS_FOREVER);

	otadfu = otadfu_get_handle();
	if (otadfu == NULL) {
		ret = -17;
		goto label_fail;
	}

	if (ret == 0) {
#ifdef OTADFU_BY_DATA_STREAM
		os_mutex_init(&(otadfu->stream_mutex));
#endif
		otadfu->data_sequence = 0;
		otadfu->drop_count = 0;

		otadfu->file_offs = 0;
		otadfu->write_offs = otadfu->file_offs;
		otadfu->head_data_buf = ota_header_data_buffer;
		otadfu->head_data_len = 0;
		otadfu->last_index = 0xffff;
		otadfu->remaining_data = ota_remain_data_buffer;
		otadfu->remaining_len = 0;
		otadfu->check_data = ota_remain_data_buffer + OTADFU_FRAME_LEN;
		otadfu->check_size = OTADFU_REMAIN_DATABUF_SIZE - OTADFU_FRAME_LEN;

		ota_app_cmd_thread_start(otadfu);

		//dfureport_start(otadfu, 0);	// report Ready

		os_mutex_unlock(&otadfu_mutex);

		return 0;
	}

	os_mutex_unlock(&otadfu_mutex);

 label_fail:
	selfapp_log_wrn("dfu erropen %d\n", ret);
	dfu_release(otadfu);
	return ret;
}

static int dfu_api_close(struct ota_backend *backend)
{

	selfapp_log_inf("dfuclose\n");

	os_mutex_lock(&otadfu_mutex, OS_FOREVER);

#ifdef CONFIG_OTA_SELF_APP
	selfapp_backend_otadfu_stop();
#endif
	otadfu_set_state(DFUSTA_UNKNOWN);
	//selfapp_routine_start_stop(0);

	os_mutex_unlock(&otadfu_mutex);

	selfapp_log_inf("dfuclosed\n");

	return 0;
}

static int dfu_api_read(struct ota_backend *backend, int offset, void *buf,
			int size)
{
	otadfu_handle_t *otadfu;
	u32_t need_drop = 0, read_len = 0, timeout = 0;
	int ret = -1;		// 0 succ, others fail
	u8_t *wbuf = (u8_t *) buf;

	//selfapp_log_inf("read offset %x size %x\n", offset, size);

	os_mutex_lock(&otadfu_mutex, OS_FOREVER);

	force_exit_ota = 0;
	
	otadfu = otadfu_get_handle();

	/* App send the data in order, which means device require data in order too.
	 *  But device might decide where to send from. */

	if (otadfu == NULL) {
		goto label_fail;
	}

#ifdef OTADFU_BY_DATA_STREAM
	if (otadfu->data_stream == NULL || otadfu->stream_opened == 0) {
		goto label_fail;
	}
#endif
	otadfu->flag_reading = 1;

	// get breakpoint offset
	if (otadfu->flag_bp_waiting && offset > 0) {
		otadfu->flag_bp_waiting = 0;
		otadfu->flag_bp_resumed = 0;
		otadfu->bp_offs = offset;
		timeout = os_uptime_get_32();
		selfapp_log_inf("dfuread: bp_offs=0x%x\n", otadfu->bp_offs);
	}

	// report error if an old offset required
	if (offset < otadfu->file_offs) {
		selfapp_log_inf("dfuread0: 0x%x dropped, cur=0x%x cache=0x%x size 0x%x\n", offset,
		       otadfu->file_offs, otadfu->head_data_len, size);
		if(otadfu->head_data_len >= otadfu->file_offs){
			if((otadfu->head_data_len - offset) <= size){
				memcpy(buf, otadfu->head_data_buf + offset, otadfu->head_data_len - offset);
				read_len += otadfu->head_data_len - offset;
				offset += otadfu->head_data_len - offset;
			}else{
				memcpy(buf, otadfu->head_data_buf + offset, size - offset);
				read_len += size - offset;
				offset += size - offset;
				size = 0;
			}
		}else{
			selfapp_log_inf("dfuread: 0x%x dropped, cur=0x%x cache=0x%x size 0x%x\n", offset,
			       otadfu->file_offs, otadfu->head_data_len, size);
			goto label_fail;
		}
	}

	ret = 0;

	// need bp resume, discard all received data (1/2)
	if (otadfu->bp_offs && dfu_get_data_buffer_size(otadfu) > 0) {
		dfu_data_buf_read(otadfu, NULL, dfu_get_data_buffer_size(otadfu), 0);
	}

	timeout = os_uptime_get_32();
	while (read_len < size) {
		 if(!otadfu || otadfu->flag_abort || otadfu->flag_cancel || otadfu->state == DFUSTA_CANCEL) {
			ret = -2;
			break;
		}

		// there might be old data in otadfu->data_buf
		if (otadfu->bp_offs == 0 && otadfu->flag_bp_droppkg == 0) {
			need_drop =
			    (offset >
			     otadfu->file_offs) ? (offset -
						   otadfu->file_offs) : 0;
			if (need_drop > 0) {
				need_drop -=
				    dfu_data_buf_read(otadfu, NULL, need_drop,
						      1);
			}
			read_len +=
			    dfu_data_buf_read(otadfu, &(wbuf[read_len]),
					      size - read_len, 1);
		}

		/* ota_app thread handle all App command
		 * otadata: BT -> sppble_stream -> otadfu_SetDfuData() -> dfu_api_read() -> ota app buffer
		 */
		//selfapp_handler_by_stream(NULL);

		// DFUCMD_SetDfuData handled and there are new data
		if (dfu_get_data_buffer_size(otadfu) > 0) {
			timeout = os_uptime_get_32();
			if (need_drop > 0) {
				need_drop -=
				    dfu_data_buf_read(otadfu, NULL, need_drop,
						      1);
			}
			read_len +=
			    dfu_data_buf_read(otadfu, &(wbuf[read_len]),
					      size - read_len, 1);
		} else {
			if (timeout + OTADFU_READ_TIMEOUT <= os_uptime_get_32()) {
				selfapp_log_inf("dfuread: timeout\n");
				ret = -3;
				break;
			}
			if(force_exit_ota){
				selfapp_log_inf("dfuread: force timeout\n");
				ret = -3;
				break;
			}
			otadfu = dfu_read_wait_sync();
		}
	}

 label_fail:
 	if(otadfu){
		otadfu->flag_reading = 0;
		if (ret || otadfu->flag_abort || otadfu->flag_cancel || otadfu->state == DFUSTA_CANCEL) {
			selfapp_log_inf("dfuread: ret=%d, offs=0x%x_%d, flag=%d_%d state=%d\n", ret,
			       offset, size, otadfu->flag_abort, otadfu->flag_cancel, otadfu->state);
		}
	}

	os_mutex_unlock(&otadfu_mutex);

	return ret;
}

static int dfu_api_ioctl(struct ota_backend *backend, int cmd,
			 unsigned int param)
{
	// selfapp_log_inf("dfuioctl\n");
	return 0;
}

static struct ota_backend_api otadfu_api = {
	.exit = dfu_api_exit,	//not used
	.open = dfu_api_open,
	.close = dfu_api_close,
	.read = dfu_api_read,
	.ioctl = dfu_api_ioctl,
};

static void otadfu_callback(int event)
{
    int timeout = 0;
	otadfu_handle_t *otadfu = otadfu_get_handle();
	selfapp_log_inf("dfucb: %d\n", event);
	if (otadfu == NULL) {
		return;
	}

	if (event == OTA_DONE) {
		//otadfu_set_state(DFUSTA_UPLOADING);
		//dfureport_state(otadfu->state);
		otadfu_NotifyDfuCancel(0xDD);	// release otadfu handle
	} else if (event == OTA_FAIL || event == OTA_CANCEL) {
		if (otadfu->flag_cancel == 0 && otadfu->flag_abort == 0) {
			otadfu->flag_abort = 1;
		}
		if (otadfu->state != DFUSTA_ERROR) {
			dfureport_state_error();
		}
		otadfu_NotifyDfuCancel(0xFA);	// release otadfu handle
		if (event == OTA_CANCEL) {	// fix OTA_CANCEL won't run ota_image_close()
			dfu_api_close(NULL);
		}
	} else if (event == OTA_FILE_WRITE) {
		if (otadfu->flag_delay_report >= 2) {
			selfapp_log_inf("do delayed report %d\n",
			       otadfu->delayed_sequence);
			dfureport_state_sequence(otadfu->state,
						 otadfu->delayed_sequence);
		}
		otadfu->flag_delay_report = 0;
		otadfu->delayed_sequence = 0;	// 0 is also a sequence
	} else if (event == OTA_FILE_VERIFY) {
		if (otadfu->flag_delay_report == 0) {
			otadfu->flag_delay_report = 1;	// ota thread will not read data for a long time
		}
	} else if (event == OTA_BREAKPOINT_REPORT) {
		otadfu->flag_bp_waiting = 1;
    } else if(event == OTA_UPLOADING) {
		dfureport_image_state(0x01);
        otadfu_set_state(DFUSTA_UPLOADING);
        dfureport_state(otadfu->state);
        while(!otadfu->flag_send_complete) {
            os_sleep(10);
            timeout++;
            if(timeout == 500) {
                SYS_LOG_WRN("wait send complete timeout\n");
                break;
            }
        }

		//when flag send complete has set, we must delay 700ms for app
		os_sleep(700);

		dfureport_image_state(0x0);
    } else if(event == OTA_INIT_FINISHED) {
		otadfu_set_state(DFUSTA_READY);
		dfureport_state(otadfu->state);
    }
}
#endif				//CONFIG_OTA_SELF_APP

u8_t otadfu_running(void)
{
	otadfu_handle_t *otadfu = otadfu_get_handle();
	if (otadfu && otadfu->state != DFUSTA_UNKNOWN) {
		return 1;
	}
	return 0;
}

int otadfu_SetDfuData(u8_t sequence, u8_t * data, int len)
{
	int ret = 0;		// >0 written length, other fail

#ifdef  CONFIG_OTA_SELF_APP
    otadfu_handle_t *otadfu = otadfu_get_handle();
	if (otadfu == NULL) {
		return 0;
	}

	// param data is illegal, renew it
	ret = dfu_data_buf_write(otadfu, &sequence, &data, len);

	if(ret < 0){
		dfureport_state_sequence(otadfu->state, otadfu->data_sequence);
		return ret;
	}


 	if (otadfu->buf_flushing) {
		return 3;
	}

	// resume breakpoint
	if (otadfu->bp_offs && otadfu->flag_bp_resumed == 0) {
		// selfapp_log_inf("bpresume: %d+1 * %d = %d\n", sequence, len, (sequence + 1) * len);
		if (dfu_breakpoint_resume(otadfu, (sequence + 1) * len) != 0) {
			return 1;
		}
	}

	if (otadfu->state == DFUSTA_READY || otadfu->flag_bp_resumed > 0) {
		selfapp_log_inf("dfuwrite: seq=%d_%d\n", sequence, len);
	}
	if (otadfu->flag_bp_resumed > 0) {
		otadfu->flag_bp_resumed -= 1;
		selfapp_log_dump(data, 0x10);
		selfapp_log_dump(data + len - 0x10, 0x10);
	}

	// if (otadfu->data_sequence != 0 && sequence != 0 && sequence != otadfu->data_sequence + 1) {
	//     selfapp_log_inf("dfuwrite: notcontinue %d_%d\n", otadfu->data_sequence, sequence);
	// }
	dfureport_state_sequence(otadfu->state, otadfu->data_sequence);

	otadfu->write_offs += ret;
#endif				//CONFIG_OTA_SELF_APP
	return ret;
}

int otadfu_NotifyDfuCancel(u8_t cancel_reason)
{
	otadfu_handle_t *otadfu;
	static uint8_t reentrancy_flag = 0;

	selfapp_log_inf("dfu cancel: 0x%x\n", cancel_reason);

	os_mutex_lock(&otadfu_mutex, OS_FOREVER);

	otadfu = otadfu_get_handle();
	if (otadfu == NULL) {
		os_mutex_unlock(&otadfu_mutex);
		return 0;
	}

	otadfu->flag_cancel = 1;	//let ota_app release reading

	selfapp_log_inf("dfu cancel: 0x%x_%d_%d\n", cancel_reason, otadfu->flag_reading, otadfu->flag_cancel);

#if 1
	// OTAing: don't dfu_release() until ota_app callback OTA_FAIL
	if (otadfu->flag_reading) {
		os_mutex_unlock(&otadfu_mutex);
		return 0;
	}
#else
	while (otadfu->flag_reading) {
		os_sleep(OTADFU_SLEEP_MS);	// let ota_app exit
	}
#endif

	if(k_current_get() == otadfu->app_cmd_thread){
		otadfu->thread_need_terminated = true;
		os_mutex_unlock(&otadfu_mutex);
		return 0;
	}

	if (reentrancy_flag) {
		os_mutex_unlock(&otadfu_mutex);
		return 0;
	}

	reentrancy_flag = 1;

	ota_app_cmd_thread_stop(otadfu);

	otadfu_set_state(DFUSTA_CANCEL);
#ifdef CONFIG_OTA_SELF_APP
	dfu_release(otadfu);
#endif

	reentrancy_flag = 0;

	os_mutex_unlock(&otadfu_mutex);

	return 0;
}

int otadfu_ReqDfuStart(dfu_start_t * param)
{
	int ret = -1;
#ifdef CONFIG_OTA_SELF_APP
	//int i;
	otadfu_handle_t *otadfu = NULL;

	os_mutex_lock(&otadfu_mutex, OS_FOREVER);

	if (dfu_prepare(param) == 0) {
		otadfu = otadfu_get_handle();
		ret =
		    selfapp_backend_otadfu_start(&otadfu_api, otadfu_callback);
		if (ret != 0) {
			dfu_release(otadfu);
			selfapp_backend_otadfu_stop();
		} else {
			//otadfu_set_state(DFUSTA_INITED);
		}
	}

	os_mutex_unlock(&otadfu_mutex);

    if(ret != 0) {
        dfureport_state_error();
    }else{
#if 0
        // wait otadfu->state to be set DFUSTA_READY
        for(i = 0; i < 3*1000 && otadfu->state != DFUSTA_READY; i++) {
#ifdef CONFIG_TASK_WDT
			task_wdt_feed(TASK_WDT_CHANNEL_SYSTEM_APP);
#endif
            os_sleep(1);
			if((i % 1000) == 0){
				printk("wait main thread switch to ota scene %d\n", otadfu->state);
			}

			ret = -EIO;
        }
        if(otadfu->state == DFUSTA_READY) {
            dfureport_state(otadfu->state);
        }
        else {
            dfureport_state_error();
        }
#endif
	}

	selfapp_log_inf("dfu reqDfuStart %s\n", ret == 0 ? "succ" : "fail");
#else
	selfapp_log_wrn("otadfu unsupported\n");
#endif
	return ret;
}


int otadfu_AppTriggerUpgrade(void)
{
#ifdef CONFIG_OTA_SELF_APP
	os_mutex_lock(&otadfu_mutex, OS_FOREVER);

    otadfu_handle_t* otadfu = otadfu_get_handle();

    if(otadfu){
        otadfu->flag_send_complete = true;
    }

	os_mutex_unlock(&otadfu_mutex);

	return 0;
#else
	return -1;
#endif
}
extern u32_t fw_version_get_sw_code(void);
extern u8_t fw_version_get_hw_code(void);
//#define HM_OTA_AUTO_TEST	1
int cmdgroup_otadfu(u8_t CmdID, u8_t * Payload, u16_t PayloadLen)
{
	u8_t *buf = self_get_sendbuf();
	u16_t sendlen = 0;
	int ret = -1;
	if (buf == NULL) {
		return ret;
	}

	switch (CmdID) {
	case DFUCMD_ReqVer:{
			u8_t vercode[4];	// 3Bytes is sw version, big endian, 1Byte is hw version
			u32_t hwver = fw_version_get_hw_code();
			u32_t swver = fw_version_get_sw_code();
			vercode[0] = hex2dec_digitpos((swver >> 16) & 0xFF);
			vercode[1] = hex2dec_digitpos((swver >>  8) & 0xFF);
			vercode[2] = hex2dec_digitpos( swver & 0xFF);
			vercode[3] = (u8_t) hwver;

			#ifdef HM_OTA_AUTO_TEST
			u32_t ota_swver = fw_version_get_code();
			u32_t swver_index;
			swver_index = (u8_t) swver + ((u8_t) (swver >> 8))*16 + ((u8_t) (swver >> 16))*256;
			printk("ota get version %x %x\n",swver_index,ota_swver);
			if((ota_swver - swver_index) % 2)
				vercode[3] = 8;
			#endif

			selfapp_log_inf("dfu reqVer=0x%x_%x\n", hwver, swver);
			sendlen +=
			    selfapp_pack_cmd_with_bytes(buf, DFUCMD_RetVer,
					       (u8_t *) vercode, 4);
			ret = self_send_data(buf, sendlen);
			break;
		}

	case DFUCMD_ReqDfuVer:{
			u8_t sw_vercode[3];
			u32_t swver = fw_version_get_sw_code();
			sw_vercode[0] = hex2dec_digitpos((swver >> 16) & 0xFF);
			sw_vercode[1] = hex2dec_digitpos((swver >>  8) & 0xFF);
			sw_vercode[2] = hex2dec_digitpos( swver & 0xFF);
			selfapp_log_inf("dfu reqDfuVer=0x%06x\n", swver);
			sendlen +=
			    selfapp_pack_cmd_with_bytes(buf, DFUCMD_RetDfuVer,
					       (u8_t *) sw_vercode, 3);
			ret = self_send_data(buf, sendlen);
			break;
		}

	case DFUCMD_ReqDfuStart:{
			dfu_start_t dfu_start;
			int result = 1;

			if (PayloadLen != sizeof(dfu_start_t)) {
				selfapp_log_inf("dfu reqDfuStart errlen %d\n",
				       PayloadLen);
				break;
			}
            bt_manager_set_user_visual(1,0,0,0);

			memcpy(&dfu_start, Payload, sizeof(dfu_start_t));
			dfu_start.dfu_size = (Payload[5] << 16) | (Payload[6] << 8) | Payload[7];	//big endian
			result = otadfu_ReqDfuStart(&dfu_start);
			result = result == 0 ? 0 : 1;

			sendlen +=
			    selfapp_pack_devack(buf, DFUCMD_ReqDfuStart,
					    (u8_t) result);
			ret = self_send_data(buf, sendlen);
			break;
		}

	case DFUCMD_NotifyDfuCancel:{
			u8_t cancel_reason = Payload[0];	//0 unknown, 1 user cancelled
			ret = otadfu_NotifyDfuCancel(cancel_reason);

			sendlen +=
			    selfapp_pack_devack(buf, DFUCMD_NotifyDfuCancel,
					    (u8_t) (ret != 0));
			ret = self_send_data(buf, sendlen);
			break;
		}

	case DFUCMD_SetDfuData:{
			os_mutex_lock(&otadfu_mutex, OS_FOREVER);

			ret =
			    otadfu_SetDfuData(Payload[0], &Payload[1],
					      PayloadLen - 1);

			os_mutex_unlock(&otadfu_mutex);
			ret = ret > 0 ? 0 : 1;
			break;
		}

	case DFUCMD_AppTriggerUpgrade:{
			ret = otadfu_AppTriggerUpgrade();
			break;
		}

	default:{
			selfapp_log_wrn("NoCmd OTADFU 0x%x\n", CmdID);
			break;
		}
	}			//switch
	return ret;
}

void otadfu_SetForce_exit_ota(void)
{
	force_exit_ota = 1;
}
/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <kernel.h>
#include <string.h>
#include <toolchain.h>
#include <partition.h>
#include <debug/stream_flash.h>
#include <misc/util.h>

#include <debug/coredump.h>
#include "coredump_internal.h"

#include <logging/sys_log.h>
#include <init.h>


/**
 * @file
 * @brief Simple coredump backend to store data in coredump region.
 *
 * This provides a simple backend to store coredump data in a flash
 * partition, labeled "coredump-partition" in devicetree.
 *
 * On the partition, a header is stored at the beginning with padding
 * at the end to align with flash write size. Then the actual
 * coredump data follows. The padding is to simplify the data read
 * function so that the first read of a data stream is always
 * aligned to flash write size.
 */

#define FLASH_WRITE_SIZE	(0x1000)
#define FLASH_BUF_SIZE		FLASH_WRITE_SIZE

#define HDR_VER			1

#define CONFIG_SPI_FLASH_NAME   CONFIG_XSPI_NOR_ACTS_DEV_NAME

typedef int (*data_read_cb_t)(void *arg, uint8_t *buf, size_t len);

static struct {
	/* For use with flash read/write */
	struct device		*storage_dev;

	/* For use with streaming flash */
	struct stream_flash_ctx		stream_ctx;

	/* Checksum of data so far */
	uint16_t			checksum;

	/* Error encountered */
	int				error;

	uint32_t        base_addr;
	uint32_t        total_size;

	uint8_t         crash_dump_flag;
} backend_ctx;

/* Buffer used in stream flash context */
static uint8_t stream_flash_buf[FLASH_BUF_SIZE];

/* Buffer used in data_read() */
static uint8_t data_read_buf[FLASH_BUF_SIZE];

/* Semaphore for exclusive flash access */
//K_SEM_DEFINE(flash_sem, 1, 1);


struct flash_hdr_t {
	/* 'C', 'D' */
	char		id[2];

	/* Header version */
	uint16_t	hdr_version;

	/* Coredump size, excluding this header */
	size_t		size;

	/* Flags */
	uint16_t	flags;

	/* Checksum */
	uint16_t	checksum;

	/* Error */
	int		error;
} __packed;

void coredump_set_save_to_region_flag(uint8_t enable)
{
	backend_ctx.crash_dump_flag = enable;
}

uint8_t coredump_get_save_to_region_flag(void)
{
	return backend_ctx.crash_dump_flag;
}

 uint32_t coredump_get_region_address(void)
{
	return backend_ctx.base_addr;
}

uint32_t coredump_get_region_size(void)
{
	return backend_ctx.total_size;
}
/**
 * @brief Open the coredump storage.
 *
 * @return device: storage device
 */
static struct device *coredump_storage_init(void)
{
	struct device *dev;

	printk("init coredump storage\n");

	dev = (struct device *)device_get_binding(CONFIG_SPI_FLASH_NAME);
	if (!dev) {
		SYS_LOG_ERR("cannot found device \'%s\'\n",
			    CONFIG_SPI_FLASH_NAME);
		return NULL;
	}
	return dev;
}


/**
 * @brief coredump init
 *
 * @return device: storage device
 */
int coredump_region_init(struct device *dev)
{
	struct device *storage_dev;
	const struct partition_entry *coredump_part;

	SYS_LOG_INF("init coredump config");

	coredump_part = partition_get_part(PARTITION_FILE_ID_COREDUMP);
	if (coredump_part) {
		backend_ctx.base_addr = coredump_part->offset;
		backend_ctx.total_size = coredump_part->size;
		SYS_LOG_INF("coredump:0x%x,0x%x\n", backend_ctx.base_addr, backend_ctx.total_size);
	} else {
		SYS_LOG_ERR("coredump partition NOT find");
	}

	storage_dev = coredump_storage_init();
	if (!storage_dev) {
		return -ENODEV;
	}

	backend_ctx.storage_dev = storage_dev;

	coredump_set_save_to_region_flag(false);

	return 0;
}

void coredump_flash_init(void)
{
	/* clear write regtion to avoid erasing in system */
	flash_erase(backend_ctx.storage_dev, backend_ctx.base_addr, backend_ctx.total_size);
}

/**
 * @brief Read data from flash partition.
 *
 * This reads @p len bytes in the flash partition starting
 * from @p off and put into buffer pointed by @p dst if
 * @p dst is not NULL.
 *
 * If @p cb is not NULL, data being read are passed to
 * the callback for processing. Note that the data being
 * passed to callback may only be part of the data requested.
 * The end of read is signaled to the callback with NULL
 * buffer pointer and zero length as arguments.
 *
 * @param off offset of partition to begin reading
 * @param dst buffer to read data into (can be NULL)
 * @param len number of bytes to read
 * @param cb callback to process read data (can be NULL)
 * @param cb_arg argument passed to callback
 * @return 0 if successful, error otherwise.
 */
static int data_read(off_t off, uint8_t *dst, size_t len,
		     data_read_cb_t cb, void *cb_arg)
{
	int ret = 0;
	off_t offset = off;
	size_t remaining = len;
	size_t copy_sz;
	uint8_t *ptr = dst;

	if (backend_ctx.storage_dev == NULL) {
		return -ENODEV;
	}

	copy_sz = FLASH_BUF_SIZE;
	offset += backend_ctx.base_addr;
	while (remaining > 0) {
		if (remaining < FLASH_BUF_SIZE) {
			copy_sz = remaining;
		}

		ret = flash_read(backend_ctx.storage_dev, offset,
				      data_read_buf, FLASH_BUF_SIZE);
		if (ret != 0) {
			break;
		}

		if (dst != NULL) {
			(void)memcpy(ptr, data_read_buf, copy_sz);
		}

		if (cb != NULL) {
			ret = (*cb)(cb_arg, data_read_buf, copy_sz);
			if (ret != 0) {
				break;
			}
		}

		ptr += copy_sz;
		offset += copy_sz;
		remaining -= copy_sz;
	}

	if (cb != NULL) {
		ret = (*cb)(cb_arg, NULL, 0);
	}

	return ret;
}

/**
 * @brief Callback to calculate checksum.
 *
 * @param arg callback argument (not being used)
 * @param buf data buffer
 * @param len number of bytes in buffer to process
 * @return 0
 */
static int cb_calc_buf_checksum(void *arg, uint8_t *buf, size_t len)
{
	int i;

	ARG_UNUSED(arg);

	for (i = 0; i < len; i++) {
		backend_ctx.checksum += buf[i];
	}

	return 0;
}

/**
 * @brief Process the stored coredump in flash partition.
 *
 * This reads the stored coredump data and processes it via
 * the callback function.
 *
 * @param cb callback to process the stored coredump data
 * @param cb_arg argument passed to callback
 * @return 1 if successful; 0 if stored coredump is not found
 *         or is not valid; error otherwise
 */
static int process_stored_dump(data_read_cb_t cb, void *cb_arg)
{
	int ret;
	struct flash_hdr_t hdr;
	off_t offset;

	/* Read header */
	ret = data_read(0, (uint8_t *)&hdr, sizeof(hdr), NULL, NULL);

	/* Verify header signature */
	if ((hdr.id[0] != 'C') && (hdr.id[1] != 'D')) {
		ret = 0;
		goto out;
	}

	/* Error encountered while dumping, so non-existent */
	if (hdr.error != 0) {
		ret = 0;
		goto out;
	}

	backend_ctx.checksum = 0;

	offset = ROUND_UP(sizeof(struct flash_hdr_t), FLASH_WRITE_SIZE);
	ret = data_read(offset, NULL, hdr.size, cb, cb_arg);

	if (ret == 0) {
		ret = (backend_ctx.checksum == hdr.checksum) ? 1 : 0;
	}

out:

	return ret;
}

/**
 * @brief Process the stored coredump in flash partition.
 *
 * This reads the stored coredump data and processes it via
 * the callback function.
 *
 * @return 0 if successful; error otherwise
 */
static int erase_flash_partition(void)
{
	int ret;

	/* Erase whole flash partition */
	ret = flash_erase(backend_ctx.storage_dev, backend_ctx.base_addr,
				backend_ctx.total_size);

	return ret;
}

/**
 * @brief Start of coredump session.
 *
 * This opens the flash partition for processing.
 */
static void coredump_flash_backend_start(void)
{
	struct device *flash_dev;
	size_t offset;
	int ret;

	SYS_LOG_ERR(COREDUMP_PREFIX_STR COREDUMP_BEGIN_STR);

	if(coredump_get_save_to_region_flag()){
		SYS_LOG_ERR("coredump already saved!");
		return;
	}

	/* Erase whole flash partition */
	ret = flash_erase(backend_ctx.storage_dev, backend_ctx.base_addr,
				    backend_ctx.total_size);

	if (ret == 0) {
		backend_ctx.checksum = 0;

		flash_dev = backend_ctx.storage_dev;

		/*
		 * Reserve space for header from beginning of flash device.
		 * The header size is rounded up so the beginning of coredump
		 * is aligned to write size (for easier read and seek).
		 */
		offset = backend_ctx.base_addr;
		offset += ROUND_UP(sizeof(struct flash_hdr_t),
				   FLASH_WRITE_SIZE);

		ret = stream_flash_init(&backend_ctx.stream_ctx, flash_dev,
					stream_flash_buf,
					sizeof(stream_flash_buf),
					offset,
					backend_ctx.total_size - ROUND_UP(sizeof(struct flash_hdr_t),
				   FLASH_WRITE_SIZE),
					NULL);
	}

	if (ret != 0) {
		SYS_LOG_ERR("Cannot start coredump!");
		backend_ctx.error = ret;
	}
}

/**
 * @brief End of coredump session.
 *
 * This ends the coredump session by flushing coredump data
 * flash, and writes the header in the beginning of flash
 * related to the stored coredump data.
 */
static void coredump_flash_backend_end(void)
{
	int ret;
	struct device *flash_dev;

	struct flash_hdr_t hdr = {
		.id = {'C', 'D'},
		.hdr_version = HDR_VER,
	};

	if (backend_ctx.storage_dev == NULL) {
		return;
	}

	if(coredump_get_save_to_region_flag()){
		SYS_LOG_ERR("coredump already saved!");
		return;
	}

	/* Flush buffer */
	backend_ctx.error = stream_flash_buffered_write(
				&backend_ctx.stream_ctx,
				stream_flash_buf, 0, true);

	/* Write header */
	hdr.size = stream_flash_bytes_written(&backend_ctx.stream_ctx);
	hdr.checksum = backend_ctx.checksum;
	hdr.error = backend_ctx.error;
	hdr.flags = 0;

	flash_dev = backend_ctx.storage_dev;

	/* Need to re-init context to write at beginning of flash */
	ret = stream_flash_init(&backend_ctx.stream_ctx, flash_dev,
				stream_flash_buf,
				sizeof(stream_flash_buf),
				backend_ctx.base_addr,
				backend_ctx.total_size, NULL);
	if (ret == 0) {
		ret = stream_flash_buffered_write(&backend_ctx.stream_ctx,
						  (void *)&hdr, sizeof(hdr),
						  true);
		if (ret != 0) {
			SYS_LOG_ERR("Cannot write coredump header!");
			backend_ctx.error = ret;
		}
	}

	if (backend_ctx.error != 0) {
		SYS_LOG_ERR("Error in coredump backend (%d)!",
			backend_ctx.error);
	}else{
		coredump_set_save_to_region_flag(true);
	}

	SYS_LOG_ERR(COREDUMP_PREFIX_STR COREDUMP_END_STR);
}

/**
 * @brief Write a buffer to flash partition.
 *
 * This writes @p buf into the flash partition. Note that this is
 * using the stream flash interface, so there is no need to keep
 * track of where on flash to write next.
 *
 * @param buf buffer of data to write to flash
 * @param buflen number of bytes to write
 */

#define LOG_BUF_SZ		64
#define LOG_BUF_SZ_RAW		(LOG_BUF_SZ + 1)

static int hex2char(uint8_t x, char *c)
{
	if (x <= 9) {
		*c = x + '0';
	} else  if (x <= 15) {
		*c = x - 10 + 'a';
	} else {
		return -EINVAL;
	}

	return 0;
}

static void coredump_logging_backend_buffer_output(uint8_t *buf, size_t buflen)
{
	uint8_t log_ptr = 0;
	size_t remaining = buflen;
	size_t i = 0;
    int error;
    char log_buf[LOG_BUF_SZ_RAW];


	if ((buf == NULL) || (buflen == 0)) {
		error = -EINVAL;
		remaining = 0;
	}

	while (remaining > 0) {
		if (hex2char(buf[i] >> 4, &log_buf[log_ptr]) < 0) {
			error = -EINVAL;
			break;
		}
		log_ptr++;

		if (hex2char(buf[i] & 0xf, &log_buf[log_ptr]) < 0) {
			error = -EINVAL;
			break;
		}
		log_ptr++;

		i++;
		remaining--;

		if ((log_ptr >= LOG_BUF_SZ) || (remaining == 0)) {
			log_buf[log_ptr] = '\0';
			SYS_LOG_ERR(COREDUMP_PREFIX_STR "%s", (char *)(log_buf));
			log_ptr = 0;
		}
	}
}


static void coredump_flash_backend_buffer_output(uint8_t *buf, size_t buflen)
{
	int i;
	size_t remaining = buflen;
	size_t copy_sz;
	uint8_t *ptr = buf;
	uint8_t tmp_buf[FLASH_BUF_SIZE];

	//add for logging dump
	coredump_logging_backend_buffer_output(buf, buflen);

	if ((backend_ctx.error != 0) || (backend_ctx.storage_dev == NULL)) {
		return;
	}


	if(coredump_get_save_to_region_flag()){
		return;
	}
	/*
	 * Since the system is still running, memory content is constantly
	 * changing (e.g. stack of this thread). We need to make a copy of
	 * part of the buffer, so that the checksum corresponds to what is
	 * being written.
	 */
	copy_sz = FLASH_BUF_SIZE;
	while (remaining > 0) {
		if (remaining < FLASH_BUF_SIZE) {
			copy_sz = remaining;
		}

		(void)memcpy(tmp_buf, ptr, copy_sz);

		for (i = 0; i < copy_sz; i++) {
			backend_ctx.checksum += tmp_buf[i];
		}

		backend_ctx.error = stream_flash_buffered_write(
					&backend_ctx.stream_ctx,
					tmp_buf, copy_sz, false);
		if (backend_ctx.error != 0) {
			break;
		}

		ptr += copy_sz;
		remaining -= copy_sz;
	}
}


/*
 * @brief Perform query on this backend.
 *
 * @param query_id ID of query
 * @param arg argument of query
 * @return depends on query
 */
static int coredump_flash_backend_query(enum coredump_query_id query_id,
					void *arg)
{
	int ret;

	switch (query_id) {
	case COREDUMP_QUERY_GET_ERROR:
		ret = backend_ctx.error;
		break;
	case COREDUMP_QUERY_HAS_STORED_DUMP:
		ret = process_stored_dump(cb_calc_buf_checksum, NULL);
		break;
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

/**
 * @brief Perform command on this backend.
 *
 * @param cmd_id command ID
 * @param arg argument of command
 * @return depends on query
 */
static int coredump_flash_backend_cmd(enum coredump_cmd_id cmd_id,
				      void *arg)
{
	int ret;

	switch (cmd_id) {
	case COREDUMP_CMD_CLEAR_ERROR:
		ret = 0;
		backend_ctx.error = 0;
		break;
	case COREDUMP_CMD_VERIFY_STORED_DUMP:
		ret = process_stored_dump(cb_calc_buf_checksum, NULL);
		break;
	case COREDUMP_CMD_ERASE_STORED_DUMP:
		ret = erase_flash_partition();
		break;
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}


struct z_coredump_backend_api z_coredump_backend_flash_dev = {
	.start = coredump_flash_backend_start,
	.end = coredump_flash_backend_end,
	.buffer_output = coredump_flash_backend_buffer_output,
	.query = coredump_flash_backend_query,
	.cmd = coredump_flash_backend_cmd,
};

SYS_INIT(coredump_region_init, POST_KERNEL, 10);


#ifdef CONFIG_DEBUG_COREDUMP_SHELL
#include <shell/shell.h>

/* Length of buffer of printable size */
#define PRINT_BUF_SZ		64

/* Length of buffer of printable size plus null character +#CD: +\r+\n*/
#define PRINT_BUF_SZ_RAW	(PRINT_BUF_SZ + 7)

/* Print buffer */
static char print_buf[PRINT_BUF_SZ_RAW];
static off_t print_buf_ptr;

/**
 * @brief Shell command to get backend error.
 *
 * @param shell shell instance
 * @param argc (not used)
 * @param argv (not used)
 * @return 0
 */
static int cmd_coredump_error_get(const struct shell *shell,
				  size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (backend_ctx.error == 0) {
		shell_print(shell, "No error.");
	} else {
		shell_print(shell, "Error: %d", backend_ctx.error);
	}

	return 0;
}

/**
 * @brief Shell command to clear backend error.
 *
 * @param shell shell instance
 * @param argc (not used)
 * @param argv (not used)
 * @return 0
 */
static int cmd_coredump_error_clear(const struct shell *shell,
				    size_t argc, char **argv)
{
	backend_ctx.error = 0;

	shell_print(shell, "Error cleared.");

	return 0;
}

/**
 * @brief Shell command to see if there is a stored coredump in flash.
 *
 * @param shell shell instance
 * @param argc (not used)
 * @param argv (not used)
 * @return 0
 */
static int cmd_coredump_has_stored_dump(const struct shell *shell,
					size_t argc, char **argv)
{
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	ret = coredump_flash_backend_query(COREDUMP_QUERY_HAS_STORED_DUMP,
					   NULL);

	if (ret == 1) {
		shell_print(shell, "Stored coredump found.");
	} else if (ret == 0) {
		shell_print(shell, "Stored coredump NOT found.");
	} else {
		shell_print(shell, "Failed to perform query: %d", ret);
	}

	return 0;
}

/**
 * @brief Shell command to verify if the stored coredump is valid.
 *
 * @param shell shell instance
 * @param argc (not used)
 * @param argv (not used)
 * @return 0
 */
static int cmd_coredump_verify_stored_dump(const struct shell *shell,
					   size_t argc, char **argv)
{
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	ret = coredump_flash_backend_cmd(COREDUMP_CMD_VERIFY_STORED_DUMP,
					 NULL);

	if (ret == 1) {
		shell_print(shell, "Stored coredump verified.");
	} else if (ret == 0) {
		shell_print(shell, "Stored coredump verification failed "
				   "or there is no stored coredump.");
	} else {
		shell_print(shell, "Failed to perform verify command: %d",
			    ret);
	}

	return 0;
}


/***

***/
#include <stdio.h>
typedef int (*bt_transfer_cb)(uint8_t *data, uint32_t max_len);

#ifndef CONFIG_ARM_UNWIND
int  bk_th_coredump_out(int (*out_cb)(uint8_t *data, uint32_t len));
int bk_th_coredump_set(uint32_t start, uint8_t *buf, uint32_t offset, int len);
static bool b_find_head, b_find_err, b_bk_ok;
static struct coredump_mem_hdr_t g_mem_hdr;
static uint32_t g_mem_data_size, g_mem_head_size;
static int backtrace_check_to_out(void *arg, uint8_t *buf, size_t len)
{
	struct coredump_arch_hdr_t *arch_head;
	size_t head_size,  size, mem_size;
	size_t index = 0;
	uint8_t *ph;
	//printk("len=%d\n", len);
	if(b_find_err)
		return len;

	if(!b_find_head){
		if(buf[0] != 'Z'){
			b_find_err = true;
			//printk("err hdr=%c\n", buf[0]);
			return len;
		}
		//printk("head hdr\n");
		index = sizeof(struct coredump_hdr_t);
		if(buf[index] != COREDUMP_ARCH_HDR_ID){
			b_find_err = true;
			//printk("err arch hdr=%c\n", buf[index]);
			return len;
		}
		head_size = sizeof(struct coredump_arch_hdr_t);
		arch_head = (struct coredump_arch_hdr_t *)&buf[index];
		//printk("arch hdr =%d, ds=0x%d\n",head_size, arch_head->num_bytes);
		index += head_size + arch_head->num_bytes;
		b_find_head = true;
		g_mem_head_size = 0;
		g_mem_data_size = 0;
	}

	ph =(uint8_t *)(&g_mem_hdr);
	while(index < len){
		 if(g_mem_head_size == 0){
			if(buf[index] != COREDUMP_MEM_HDR_ID){
				b_find_err = true;
				//printk("err mem hdr=%c\n", buf[index]);
				break;
			}
			ph[g_mem_head_size++] = buf[index++];
			g_mem_data_size = 0;
			//printk("mem cp start\n");
		 }else if(g_mem_head_size != sizeof(g_mem_hdr)){
			ph[g_mem_head_size++] = buf[index++];
		 }else{
			size = len - index;
			mem_size = g_mem_hdr.end - g_mem_hdr.start;
			//printk("mem:0x%x--0x%x, msize=0x%x, size=0x%x,cpy=0x%x\n",(uint32_t) g_mem_hdr.start,  (uint32_t)g_mem_hdr.end,  mem_size, size, g_mem_data_size);

			if(size + g_mem_data_size >= mem_size){
				size = mem_size-g_mem_data_size;
				if(bk_th_coredump_set((uint32_t)g_mem_hdr.start, buf+index, g_mem_data_size, size))
					b_bk_ok = true;
				index += size;
				g_mem_head_size = 0;
				//printk("mem cp end\n");
			}else{
				bk_th_coredump_set((uint32_t) g_mem_hdr.start, buf+index, g_mem_data_size, size);
				g_mem_data_size += size;
				index = len;
			}
		 }

	}
	return len;
}
#endif

static int cb_stored_dump(void *arg, uint8_t *buf, size_t len)
{
	int ret = 0;
	size_t i = 0;
	size_t remaining = len;
	bt_transfer_cb cb = (bt_transfer_cb)arg;
	if(len == 0)
		return 0;
	#ifndef CONFIG_ARM_UNWIND
	backtrace_check_to_out(arg, buf, len);
	#endif
	print_buf_ptr = 4;
	strcpy(print_buf, COREDUMP_PREFIX_STR);
	/* Do checksum for process_stored_dump() */
	cb_calc_buf_checksum(arg, buf, len);

	while (remaining > 0) {
		if (hex2char(buf[i] >> 4, &print_buf[print_buf_ptr++]) < 0) {
			ret = -EINVAL;
			break;
		}
		if (hex2char(buf[i] & 0xf, &print_buf[print_buf_ptr++]) < 0) {
			ret = -EINVAL;
			break;
		}
		remaining--;
		i++;
		if (print_buf_ptr == (PRINT_BUF_SZ_RAW-3)){
			print_buf[print_buf_ptr++] = '\r';
			print_buf[print_buf_ptr++] = '\n';
			print_buf[print_buf_ptr] = 0;
			cb(print_buf, print_buf_ptr);
			print_buf_ptr = 4;
		}
	}
	if(print_buf_ptr != 4){
		print_buf[print_buf_ptr++] = '\r';
		print_buf[print_buf_ptr++] = '\n';
		print_buf[print_buf_ptr] = 0;
		cb(print_buf, print_buf_ptr);
	}

   return ret;
}

int coredump_transfer(int (*traverse_cb)(uint8_t *data, uint32_t max_len))
{
   int ret;
   int traverse_len, len;
   if(traverse_cb == NULL)
	   return 0;
   #ifndef CONFIG_ARM_UNWIND
   b_find_head = false;
   b_find_err = false;
   b_bk_ok = false;
   #endif
   /* Verify first to see if stored dump is valid */
   ret = coredump_flash_backend_cmd(COREDUMP_CMD_VERIFY_STORED_DUMP,
					NULL);
   traverse_len = 0;
   if (ret == 0) {
	   traverse_len = sprintf(print_buf, "verification failed or no stored\r\n");
	   traverse_cb(print_buf, traverse_len);
	   goto out;
   } else if (ret != 1) {
	   traverse_len = sprintf(print_buf, "Failed to perform verify command: %d\r\n",
			   ret);
	   traverse_cb(print_buf, traverse_len);
	   goto out;
   }

   len = sprintf(print_buf, "%s%s\r\n", COREDUMP_PREFIX_STR, COREDUMP_BEGIN_STR);
   traverse_cb(print_buf, len);
   traverse_len += len;

   ret = process_stored_dump(cb_stored_dump, (void *)traverse_cb);

   len = sprintf(print_buf, "%s%s\r\n", COREDUMP_PREFIX_STR, COREDUMP_END_STR);
   traverse_cb(print_buf, len);
   traverse_len += len;
   #ifndef CONFIG_ARM_UNWIND
   if(b_bk_ok)
   		traverse_len += bk_th_coredump_out(traverse_cb);
   #endif

   if (ret == 1) {
	   len = sprintf(print_buf, "Stored coredump printed.\r\n");
   } else if (ret == 0) {
	   len = sprintf(print_buf,"verification failed\r\n");
   } else {
	   len = sprintf(print_buf, "Failed to print: %d\r\n", ret);
   }
   traverse_cb(print_buf, len);
   traverse_len += len;
out:
   return traverse_len;
}

static int print_str(uint8_t *data, uint32_t max_len)
{
	printk("%s", (char*)data);
	k_busy_wait(500);
	return 0;
}
static int cmd_coredump_print_stored_dump(const struct shell *shell,
					  size_t argc, char **argv)
{

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	coredump_transfer((print_str));
	return 0;
}

#if 0
/**
 * @brief Flush the print buffer to shell.
 *
 * This prints what is in the print buffer to the shell.
 *
 * @param shell shell instance.
 */
static void flush_print_buf(const struct shell *shell)
{
	shell_print(shell, "%s%s", COREDUMP_PREFIX_STR, print_buf);
	print_buf_ptr = 0;
	(void)memset(print_buf, 0, sizeof(print_buf));
}

/**
 * @brief Callback to print stored coredump to shell
 *
 * This converts the binary data in @p buf to hexadecimal digits
 * which can be printed to the shell.
 *
 * @param arg shell instance
 * @param buf binary data buffer
 * @param len number of bytes in buffer to be printed
 * @return 0 if no issues; -EINVAL if error converting data
 */
static int cb_print_stored_dump(void *arg, uint8_t *buf, size_t len)
{
	int ret = 0;
	size_t i = 0;
	size_t remaining = len;
	const struct shell *shell = (const struct shell *)arg;

	if (len == 0) {
		/* Flush print buffer */
		flush_print_buf(shell);

		goto out;
	}

	/* Do checksum for process_stored_dump() */
	cb_calc_buf_checksum(arg, buf, len);

	while (remaining > 0) {
		if (hex2char(buf[i] >> 4, &print_buf[print_buf_ptr]) < 0) {
			ret = -EINVAL;
			break;
		}
		print_buf_ptr++;

		if (hex2char(buf[i] & 0xf, &print_buf[print_buf_ptr]) < 0) {
			ret = -EINVAL;
			break;
		}
		print_buf_ptr++;

		remaining--;
		i++;

		if (print_buf_ptr == PRINT_BUF_SZ) {
			flush_print_buf(shell);
			k_busy_wait(500);
		}
	}

out:
	return ret;
}

/**
 * @brief Shell command to print stored coredump data to shell
 *
 * @param shell shell instance
 * @param argc (not used)
 * @param argv (not used)
 * @return 0
 */
static int cmd_coredump_print_stored_dump(const struct shell *shell,
					  size_t argc, char **argv)
{
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	/* Verify first to see if stored dump is valid */
	ret = coredump_flash_backend_cmd(COREDUMP_CMD_VERIFY_STORED_DUMP,
					 NULL);

	if (ret == 0) {
		shell_print(shell, "Stored coredump verification failed "
				   "or there is no stored coredump.");
		goto out;
	} else if (ret != 1) {
		shell_print(shell, "Failed to perform verify command: %d",
			    ret);
		goto out;
	}

	/* If valid, start printing to shell */
	print_buf_ptr = 0;
	(void)memset(print_buf, 0, sizeof(print_buf));

	shell_print(shell, "%s%s", COREDUMP_PREFIX_STR, COREDUMP_BEGIN_STR);

	ret = process_stored_dump(cb_print_stored_dump, (void *)shell);
	if (print_buf_ptr != 0) {
		shell_print(shell, "%s%s", COREDUMP_PREFIX_STR, print_buf);
	}

	if (backend_ctx.error != 0) {
		shell_print(shell, "%s%s", COREDUMP_PREFIX_STR,
			    COREDUMP_ERROR_STR);
	}

	shell_print(shell, "%s%s", COREDUMP_PREFIX_STR, COREDUMP_END_STR);

	if (ret == 1) {
		shell_print(shell, "Stored coredump printed.");
	} else if (ret == 0) {
		shell_print(shell, "Stored coredump verification failed "
				   "or there is no stored coredump.");
	} else {
		shell_print(shell, "Failed to print: %d", ret);
	}

	coredump_print();
out:
	return 0;
}
#endif

/**
 * @brief Shell command to erase stored coredump.
 *
 * @param shell shell instance
 * @param argc (not used)
 * @param argv (not used)
 * @return 0
 */
static int cmd_coredump_erase_stored_dump(const struct shell *shell,
					  size_t argc, char **argv)
{
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	ret = coredump_flash_backend_cmd(COREDUMP_CMD_ERASE_STORED_DUMP,
					 NULL);

	if (ret == 0) {
		shell_print(shell, "Stored coredump erased.");
	} else {
		shell_print(shell, "Failed to perform erase command: %d", ret);
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_coredump_error,
	SHELL_CMD(clear, NULL, "Clear Coredump error",
		  cmd_coredump_error_clear),
	SHELL_CMD(get, NULL, "Get Coredump error", cmd_coredump_error_get),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_coredump,
	SHELL_CMD(error, &sub_coredump_error,
		  "Get/clear backend error.", NULL),
	SHELL_CMD(erase, NULL,
		  "Erase stored coredump",
		  cmd_coredump_erase_stored_dump),
	SHELL_CMD(find, NULL,
		  "Query if there is a stored coredump",
		  cmd_coredump_has_stored_dump),
	SHELL_CMD(print, NULL,
		  "Print stored coredump to shell",
		  cmd_coredump_print_stored_dump),
	SHELL_CMD(verify, NULL,
		  "Verify stored coredump",
		  cmd_coredump_verify_stored_dump),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(coredump, &sub_coredump,
		   "Coredump commands (flash partition backend)", NULL);

#endif /* CONFIG_DEBUG_COREDUMP_SHELL */



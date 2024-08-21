/*******************************************************************************
 *                                      US283C
 *                            Module: usp Driver
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>    <time>           <version >             <desc>
 *       wuyufan    2023-4-27  11:39     1.0             build this file
*******************************************************************************/
/*!
 * \file     at_parser.c
 * \brief    at parser
 * \author   wuyufan
 * \par      GENERAL DESCRIPTION:
 *               function related to at parser
 * \par      EXTERNALIZED FUNCTIONS:
 *
 * \version 1.0
 * \date  2023-4-27
*******************************************************************************/
#include <device.h>
#include <stream.h>
#include <logging/sys_log.h>
#include <uart.h>
#include <acts_ringbuf.h>
//#include <malloc.h>
#include <soc.h>
#include <string.h>
#include <ringbuff_stream.h>
#include "letws_ota_stream.h"
#include <misc/byteorder.h>
#include <acts_ringbuf.h>

#define RAW_BUFFER_SIZE          (8 * 1024)


struct letws_ota_stream_ctx{
    u8_t raw_buf[RAW_BUFFER_SIZE];
    u8_t opened;
	u8_t init;
	struct acts_ringbuf cbuf;
	uint32_t drop_cnt;
	os_mutex letws_ota_stream_mutex;
	void (*send_cb)(uint8_t *data, uint32_t len);
};


static __ota_bss struct letws_ota_stream_ctx  g_letws_ota_stream_ctx;

struct letws_ota_stream_ctx *letws_ota_stream_get_ctx(void)
{
	return &g_letws_ota_stream_ctx;
}

int letws_ota_stream_init(io_stream_t handle, void *param)
{
	struct letws_ota_stream_param *uparam = (struct letws_ota_stream_param *)param;
	struct letws_ota_stream_ctx *ctx = letws_ota_stream_get_ctx();

	acts_ringbuf_init(&ctx->cbuf, ctx->raw_buf, RAW_BUFFER_SIZE);
	ctx->send_cb = uparam->send_cb;
	os_mutex_init(&ctx->letws_ota_stream_mutex);

	uparam->cbuf = (void *)(&ctx->cbuf);

    ctx->init = true;

    handle->data = ctx;

	return 0;
}

int letws_ota_stream_open(io_stream_t handle, stream_mode mode)
{
	struct letws_ota_stream_ctx *ctx = (struct letws_ota_stream_ctx *)handle->data;

	os_mutex_lock(&ctx->letws_ota_stream_mutex, OS_FOREVER);

	if(ctx->opened == 0){
		ctx->opened = true;
	}

	os_mutex_unlock(&ctx->letws_ota_stream_mutex);

    return 0;
}

int letws_ota_stream_write(io_stream_t handle, unsigned char *buf, int num)
{
	struct letws_ota_stream_ctx *ctx = (struct letws_ota_stream_ctx *)handle->data;

    if (!ctx)
        return -EACCES;

    if (!ctx->opened) {
        return 0;
    }

	os_mutex_lock(&ctx->letws_ota_stream_mutex, OS_FOREVER);

	if(ctx->send_cb){
		ctx->send_cb(buf, num);
	}

	os_mutex_unlock(&ctx->letws_ota_stream_mutex);

	return 0;
}

int letws_ota_stream_read(io_stream_t handle, unsigned char *buf, int num)
{
	int ret;

    struct letws_ota_stream_ctx *ctx = (struct letws_ota_stream_ctx *)handle->data;

    if (!ctx)
        return -EACCES;

    if (!ctx->opened) {
        return 0;
    }

	os_mutex_lock(&ctx->letws_ota_stream_mutex, OS_FOREVER);

	ret = acts_ringbuf_get(&ctx->cbuf, buf, num);

	os_mutex_unlock(&ctx->letws_ota_stream_mutex);

	return ret;
}

int letws_ota_stream_get_length(io_stream_t handle)
{
    struct letws_ota_stream_ctx *ctx = (struct letws_ota_stream_ctx *)handle->data;

    if (!ctx)
        return -EACCES;

    if (!ctx->opened) {
        return 0;
    }

	return acts_ringbuf_length(&ctx->cbuf);
}

int letws_ota_stream_tell(io_stream_t handle)
{
    struct letws_ota_stream_ctx *ctx = (struct letws_ota_stream_ctx *)handle->data;

    if (!ctx)
        return -EACCES;

    if (!ctx->opened) {
        return 0;
    }

    return acts_ringbuf_length(&ctx->cbuf);
}


int letws_ota_stream_get_space(io_stream_t handle)
{
    struct letws_ota_stream_ctx *ctx = (struct letws_ota_stream_ctx *)handle->data;

    if (!ctx)
        return -EACCES;

    if (!ctx->opened) {
        return 0;
    }

    return acts_ringbuf_space(&ctx->cbuf);
}

int letws_ota_stream_flush(io_stream_t handle)
{
    struct letws_ota_stream_ctx *ctx = (struct letws_ota_stream_ctx *)handle->data;

    if (!ctx)
        return -EACCES;

    if (!ctx->opened) {
        return 0;
    }

	return acts_ringbuf_drop_all(&ctx->cbuf);
}

int letws_ota_stream_close(io_stream_t handle)
{
    struct letws_ota_stream_ctx *ctx = (struct letws_ota_stream_ctx *)handle->data;

	os_mutex_lock(&ctx->letws_ota_stream_mutex, OS_FOREVER);

	ctx->opened = false;

	os_mutex_unlock(&ctx->letws_ota_stream_mutex);

    return 0;

}

int letws_ota_stream_destory(io_stream_t handle)
{
    handle->data = NULL;

	return 0;
}


const stream_ops_t letws_ota_stream_ops = {
    .init = letws_ota_stream_init,
    .open = letws_ota_stream_open,
    .read = letws_ota_stream_read,
    .tell = letws_ota_stream_tell,
    .get_length = letws_ota_stream_get_length,
    .get_space = letws_ota_stream_get_space,
    .write = letws_ota_stream_write,
    .flush = letws_ota_stream_flush,
    .close = letws_ota_stream_close,
    .destroy = letws_ota_stream_destory,
};

io_stream_t letws_ota_stream_create(void *param)
{
	return stream_create(&letws_ota_stream_ops, param);
}



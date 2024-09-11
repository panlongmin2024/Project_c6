/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bt service interface
 */

#ifndef _BTSERVICE_GFP_API_H_
#define _BTSERVICE_GFP_API_H_
#include <stream.h>
#include <btservice_base.h>
#include <gfp/sys_comm.h>
#include <thread_timer.h>
#include <btservice_api.h>

struct btsrv_gfp_context_info {
    btsrv_gfp_callback gfp_ev_callback;
    loop_buffer_t  tx_loop_buf;
    struct thread_timer auth_timer;
    struct thread_timer running_timer;
    os_delayed_work auth_work;
    os_delayed_work spp_initial_provider_work;
    io_stream_t gfp_ble_stream;
    void *gfp_handle;
    uint8_t gfp_gatt_state;
    uint8_t gfp_spp_chl;
    uint8_t ble_stream_opened :1;
    uint8_t gfp_spp_connected :1;
    uint8_t gfp_running_timer_init :1;
};

typedef int (*btsrv_gfp_pairing_request_callback)(uint8_t* , uint32_t);
typedef void (*btsrv_gfp_auth_timeout_start)(uint32_t);
typedef void (*btsrv_gfp_auth_timeout_stop)(void);

void gfp_running_timer_handler(struct thread_timer *timer, void* pdata);
void gfp_ble_stream_init(void);
void gfp_spp_stream_init(void);

void btsrv_gfp_pairing_request_reg(btsrv_gfp_pairing_request_callback callback);
void btsrv_gfp_cap_io_set(bool enable);
void btsrv_gfp_confirm_pairing_reply(bool success);
void btsrv_gfp_timeout_start_cb(btsrv_gfp_auth_timeout_start cb);
void btsrv_gfp_timeout_stop_cb(btsrv_gfp_auth_timeout_stop cb);

extern struct btsrv_gfp_context_info *btsrv_gfp;

#endif

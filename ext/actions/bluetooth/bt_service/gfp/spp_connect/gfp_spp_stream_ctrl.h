/*
 * Copyright (c) 2019 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file 
 * @brief 
 *
 * Change log:
 *
 */

#ifndef __GFP_SPP_STREAM_CTRL_
#define __GFP_SPP_STREAM_CTRL_

#include <thread_timer.h>
#include <sys_comm.h>


int gfp_spp_send_data(u8_t	*data_ptr, u16_t length);
int rfcomm_message_stream_deal(uint8_t* data, uint16_t size);
void fastpair_initial_provider_info(void);


#endif /*__GFP_SPP_STREAM_CTRL_*/







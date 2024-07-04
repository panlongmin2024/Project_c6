/*
 * Copyright (c) 2022 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _APP_HOT_H_
#define _APP_HOT_H_

/* Add app to active list,  and launch app if possible*/
bool system_app_launch_add(uint8_t plugin_id, uint8_t switch_plugin);
/* Remove app from active list*/
void system_app_launch_del(uint8_t plugin_id);
bool system_app_launch_switch(uint8_t old_plugin_id, uint8_t new_plugin_id);

int system_app_launch_init(void);

void system_app_switch(void);
uint32_t system_app_get_fault_app(void);

#ifdef CONFIG_AURACAST
uint8_t system_app_get_auracast_mode(void);
void system_app_set_auracast_mode(int mode);
void system_app_switch_auracast(bool auracast);
#endif

#endif

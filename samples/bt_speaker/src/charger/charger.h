/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief charger header file.
*/
#ifndef _CHARGER_APP_H
#define _CHARGER_APP_H

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "charger"

#include <logging/sys_log.h>
#include <mem_manager.h>
#include <app_manager.h>
#include <msg_manager.h>
#include <sys_manager.h>
#include <thread_timer.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <global_mem.h>
#include <soc_dvfs.h>
#include "app_defines.h"
#include "app_ui.h"


#ifdef CONFIG_FILE_SYSTEM
#include <fs.h>
#include <fs_manager.h>
#endif

enum
{
	MSG_BOOT_TO_SYSTEM = MSG_APP_INPUT_MESSAGE_CMD_START,
};

void charger_view_init(void);
void charger_view_deinit(void);
int charger_mode_check(void);

#endif


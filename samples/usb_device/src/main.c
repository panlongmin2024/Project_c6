/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <drivers/usb/usb_phy.h>
#include <usb/usb_device.h>
#include <shell/shell.h>

#include <misc/printk.h>
#include "./usb_cdc_acm/usb_cdc_acm.h"

void main(void)
{
	printk("hello world\n");
	k_sleep(K_MSEC(1000));
	usb_phy_init();
	usb_phy_enter_b_idle();

	usb_cdc_acm_start();

	while(1){
		k_sleep(K_MSEC(3000));
	}
}


/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SYS_LOG_LEVEL 4
#define SYS_LOG_DOMAIN "main"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_MAIN

#include <zephyr.h>
#include <init.h>

#include <usb/usb_device.h>
#include <usb/class/usb_hid.h>
#include <greatest_api.h>

#define REPORT_ID_1	0x01
#define REPORT_ID_2	0x02

#if 0
static const u8_t hid_report_desc[] =
{
    0x05, 0x0c, //USAGE_PAGE (Consumer)
    0x09, 0x01, //USAGE (Consumer Control)
    0xa1, 0x01, //COLLECTION (Application)
    0x15, 0x00, //Logical Minimum (0x00)
    0x25, 0x01, //Logical Maximum (0x01)
    0x09, 0xe9, //USAGE (Volume Up)
    0x09, 0xea, //USAGE (Volume Down)
    0x09, 0xe2, //USAGE (Mute)
    0x09, 0xcd, //USAGE (Play/Pause)
    0x09, 0xb5, //USAGE (Scan Next Track)
    0x09, 0xb6, //USAGE (Scan Previous Track)
    0x09, 0xb3, //USAGE (Fast Forward)
    0x09, 0xb7, //USAGE (Stop)
    0x75, 0x01, //Report Size (0x01)
    0x95, 0x08, //Report Count (0x08),报告的个数为8，即总共有8个bits
    0x81, 0x42, //Input (Data,Variable,Absolute,No Wrap,Linear,Preferred State,Null state)
    0xc0, //END_COLLECTION           //END_COLLECTION
    0xa1, 0x01, //COLLECTION (Application)
    0x15, 0x00, //Logical Minimum (0x00)
    0x25, 0x01, //Logical Maximum (0x01)
    0x09, 0x08,
    0x09, 0x0f,
    0x09, 0x18,
    0x09, 0x1f,
    0xc0, //END_COLLECTION           //END_COLLECTION
};
#endif
#if 1
/* Some HID sample Report Descriptor */
static const u8_t hid_report_desc[] = {
	/* 0x05, 0x01,		USAGE_PAGE (Generic Desktop)		*/
	HID_GI_USAGE_PAGE, USAGE_GEN_DESKTOP,
	/* 0x09, 0x00,		USAGE (Undefined)			*/
	HID_LI_USAGE, USAGE_GEN_DESKTOP_UNDEFINED,
	/* 0xa1, 0x01,		COLLECTION (Application)		*/
	HID_MI_COLLECTION, COLLECTION_APPLICATION,
	/* 0x15, 0x00,			LOGICAL_MINIMUM one-byte (0)	*/
	HID_GI_LOGICAL_MIN(1), 0x00,
	/* 0x26, 0xff, 0x00,		LOGICAL_MAXIMUM two-bytes (255)	*/
	HID_GI_LOGICAL_MAX(2), 0xFF, 0x00,
	/* 0x85, 0x01,			REPORT_ID (1)			*/
	HID_GI_REPORT_ID, REPORT_ID_1,
	/* 0x75, 0x08,			REPORT_SIZE (8) in bits		*/
	HID_GI_REPORT_SIZE, 0x08,
	/* 0x95, 0x01,			REPORT_COUNT (1)		*/
	HID_GI_REPORT_COUNT, 0x01,
	/* 0x09, 0x00,			USAGE (Undefined)		*/
	HID_LI_USAGE, USAGE_GEN_DESKTOP_UNDEFINED,
	/* v0x81, 0x82,			INPUT (Data,Var,Abs,Vol)	*/
	HID_MI_INPUT, 0x82,
	/* 0x85, 0x02,			REPORT_ID (2)			*/
	HID_GI_REPORT_ID, REPORT_ID_2,
	/* 0x75, 0x08,			REPORT_SIZE (8) in bits		*/
	HID_GI_REPORT_SIZE, 0x08,
	/* 0x95, 0x01,			REPORT_COUNT (1)		*/
	HID_GI_REPORT_COUNT, 0x01,
	/* 0x09, 0x00,			USAGE (Undefined)		*/
	HID_LI_USAGE, USAGE_GEN_DESKTOP_UNDEFINED,
	/* 0x91, 0x82,			OUTPUT (Data,Var,Abs,Vol)	*/
	HID_MI_OUTPUT, 0x82,
	/* 0xc0			END_COLLECTION			*/
	HID_MI_COLLECTION_END,
};
#endif

int debug_cb(struct usb_setup_packet *setup, s32_t *len,
	     u8_t **data)
{
	ACT_LOG_ID_DBG(ALF_STR_debug_cb__DEBUG_CALLBACK, 0);

	return -ENOTSUP;
}

int set_idle_cb(struct usb_setup_packet *setup, s32_t *len,
		u8_t **data)
{
	ACT_LOG_ID_DBG(ALF_STR_set_idle_cb__SET_IDLE_CALLBACK, 0);

	/* TODO: Do something */

	return 0;
}

int get_report_cb(struct usb_setup_packet *setup, s32_t *len,
		  u8_t **data)
{
	ACT_LOG_ID_DBG(ALF_STR_get_report_cb__GET_REPORT_CALLBACK, 0);

	/* TODO: Do something */

	return 0;
}

static const struct hid_ops ops = {
	.get_report = get_report_cb,
	.get_idle = debug_cb,
	.get_protocol = debug_cb,
	.set_report = debug_cb,
	.set_idle = set_idle_cb,
	.set_protocol = debug_cb,
};

void hid_main(void)
{
    
    uint8_t key_value = 0x20;
	u8_t report_1[2] = { REPORT_ID_1, 0x00 };

	ACT_LOG_ID_INF(ALF_STR_hid_main__STARTING__HID_APPLIC, 0);

#ifndef CONFIG_USB_COMPOSITE_DEVICE
333
	usb_hid_register_device(hid_report_desc, sizeof(hid_report_desc), &ops);
	usb_hid_init();
#endif

	while (true) {
		int ret, wrote;

		k_sleep(K_SECONDS(2));

		report_1[1]++;

        
		//ret = hid_int_ep_write(&key_value, 1, &wrote);

		ret = hid_int_ep_write(report_1, sizeof(report_1), &wrote);
		ACT_LOG_ID_INF(ALF_STR_hid_main__WROTE_D_BYTES_WITH_R, 2, wrote, ret);
	}
}

#ifdef CONFIG_USB_COMPOSITE_DEVICE
static int composite_pre_init(struct device *dev)
{
	usb_hid_register_device(hid_report_desc, sizeof(hid_report_desc), &ops);

	return usb_hid_init();
}

//SYS_INIT(composite_pre_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
#endif


TEST tts_usb_hid(void)
{
    hid_main();
}


ADD_TEST_CASE("hid")
{
    RUN_TEST(tts_usb_hid);
}

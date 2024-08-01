/*
 * Copyright (c) 2018 Actions Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SYS_LOG_LEVEL SYS_LOG_LEVEL_WARNING
#define SYS_LOG_DOMAIN "usb-audio-dongle"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_USB_HANDLER

#include <zephyr.h>
#include <init.h>
#include <string.h>

#include <usb/usb_device.h>
#include <usb/class/usb_hid.h>
#include <usb/class/usb_audio.h>

#include "usb_handler.h"
#include "hid_desc.h"

#define UAC_TX_UNIT_SIZE	32
#define UAC_TX_UNIT_NUM	(160 / UAC_TX_UNIT_SIZE)
#define UAC_TX_UNIT_COUNT	40
#define UAC_TX_COUNT	(UAC_TX_UNIT_NUM * UAC_TX_UNIT_COUNT)
#define UAC_TX_BUF_SIZE	(UAC_TX_UNIT_SIZE * UAC_TX_COUNT)

/*
 * Start transfer until tx_buf has at least UAC_TX_SIZE_OPT bytes.
 */
#ifdef UAC_TX_OPT
#define UAC_TX_COUNT_OPT	20
#define UAC_TX_SIZE_OPT	(UAC_TX_UNIT_SIZE * UAC_TX_COUNT_OPT)
#else
#define UAC_TX_SIZE_OPT	0
#endif




#if 0
static int debug_cb(struct usb_setup_packet *setup, s32_t *len,
	     u8_t **data)
{
	ACT_LOG_ID_DBG(ALF_STR_XXX__DEBUG_CALLBACK, 0);

	return -ENOTSUP;
}

static const struct hid_ops ops = {
	.get_report = debug_cb,
	.get_idle = debug_cb,
	.get_protocol = debug_cb,
	.set_report = debug_cb,
	.set_idle = debug_cb,
	.set_protocol = debug_cb,
	.int_in_ready = hid_int_in_ready,
};
#endif






#if 0
static int usb_audio_tx_unit(const u8_t *buf)
{
	u32_t wrote;

	return usb_write(CONFIG_USB_AUDIO_SOURCE_IN_EP_ADDR, buf,
					UAC_TX_UNIT_SIZE, &wrote);
}

#define UAC_TX_DUMMY_SIZE	96 //32

static int usb_audio_tx_dummy(void)
{
	/* NOTE: Could use stack to save memory */
	//static const u8_t dummy_buf[UAC_TX_DUMMY_SIZE];
	
	u8_t dummy_buf[UAC_TX_DUMMY_SIZE];
	u32_t wrote;
    u8_t i = 0;
    //ACT_LOG_ID_INF(ALF_STR_XXX__INIT_BUFN, 0);
    for (i = 0; i < UAC_TX_DUMMY_SIZE; i++)
    {
        dummy_buf[i] = i;
    }

	return usb_write(CONFIG_USB_AUDIO_SOURCE_IN_EP_ADDR, dummy_buf,
					sizeof(dummy_buf), &wrote);
}

static int usb_audio_ep_start(void)
{
	const u8_t *buf;

	//buf = uac_dequeue();
	buf = NULL;
	if (!buf) {
        
        ACT_LOG_ID_INF(ALF_STR_XXX__BUF__NULLN, 0);
        //usb_usound_ep_write_dma();
        //return 0;
		return usb_audio_tx_dummy();
	} else {
		return usb_audio_tx_unit(buf);
	}
}

/*
 * Interrupt Context
 */
static void usb_audio_ep_complete(u8_t ep,
		enum usb_dc_ep_cb_status_code cb_status)
{
	usb_audio_ep_start();
}

static void usb_audio_start_stop(bool start)
{
	if (!start) {
		usb_audio_source_ep_flush();
		//uac_cleanup();
	} else {
		//usb_audio_tx_dummy();
        //transer_iso_out_data_start();
	}
}
#endif

void usb_audio_tx_flush(void)
{
	usb_audio_source_ep_flush();
}

bool usb_audio_tx_enabled(void)
{
	return usb_audio_source_enabled();
}

int usb_hid_tx(const u8_t *buf, u16_t len)
{
	u32_t wrote;
	int ret;

	do {
		ret = hid_int_ep_write(buf, len, &wrote);
	} while (ret == -EAGAIN);

	if (ret) {
		ACT_LOG_ID_ERR(ALF_STR_usb_hid_tx__RET_D, 1, ret);
	} else if (!ret && wrote != len) {
		ACT_LOG_ID_ERR(ALF_STR_usb_hid_tx__WROTE_D_LEN_D, 2, wrote, len);
	}

	return ret;
}

#if 0
static int composite_pre_init(struct device *dev)
//static int composite_pre_init(void)
{
	/* Register to HID core */
	usb_hid_register_device(hid_report_desc, sizeof(hid_report_desc), &ops);

	/* USB HID initialize */
	usb_hid_init();

	/* Register callbacks */
	usb_audio_source_register_start_cb(usb_audio_start_stop);
	usb_audio_source_register_ep_cb(usb_audio_ep_complete);

	/* USB Audio Source initialize */
	usb_audio_source_init();

	return 0;
}
#endif

//SYS_INIT(composite_pre_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

/*int usb_usound_init(void)
{
	return composite_pre_init();
}*/

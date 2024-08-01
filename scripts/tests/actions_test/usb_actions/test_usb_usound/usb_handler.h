/*
 * Copyright (c) 2018 Actions Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __USB_HANDLER_H_
#define __USB_HANDLER_H_

void usb_audio_tx_flush(void);

int usb_audio_tx(const u8_t *buf, u16_t len);

bool usb_audio_tx_enabled(void);

void usb_audio_set_tx_start(bool start);

int usb_hid_tx(const u8_t *buf, u16_t len);

//int usb_usound_init(void);
int usb_audio_source_exit(void);


#endif /* __USB_HANDLER_H_ */


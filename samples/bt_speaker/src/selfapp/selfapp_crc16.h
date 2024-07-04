/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *	crc16.h - CRC-16 routine
 *
 * Implements the standard CRC-16:
 *   Width 16
 *   Poly  0x8005 (x^16 + x^15 + x^2 + 1)
 *   Init  0
 *
 * Copyright (c) 2005 Ben Gardner <bgardner@wabtec.com>
 */

#ifndef __CRC16_H
#define __CRC16_H

#include <zephyr/types.h>

extern u16_t const crc16_table[256];

extern u16_t self_crc16(u16_t crc, const u8_t * buffer, unsigned int len);

static inline u16_t crc16_byte(u16_t crc, const u8_t data)
{
	return (crc >> 8) ^ crc16_table[(crc ^ data) & 0xff];
}

#endif				/* __CRC16_H */

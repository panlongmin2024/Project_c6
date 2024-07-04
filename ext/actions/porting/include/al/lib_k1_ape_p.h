/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __K1_APE_P_H__
#define __K1_APE_P_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	int start_frame;
	int skip_frame;
} cb_info_t;

typedef struct
{
	/* Derived fields */
	uint32_t      junklength;
	uint32_t      firstframe;
	uint32_t      totalsamples;

	/* Info from Descriptor Block */
	char          magic[4];
	int16_t       fileversion;
	int16_t       padding1;
	uint32_t      descriptorlength;
	uint32_t      headerlength;
	uint32_t      seektablelength;
	uint32_t      wavheaderlength;
	uint32_t      audiodatalength;
	uint32_t      audiodatalength_high;
	uint32_t      wavtaillength;
	uint8_t       md5[16];

	/* Info from Header Block */
	uint16_t      compressiontype;
	uint16_t      formatflags;
	uint32_t      blocksperframe;
	uint32_t      finalframeblocks;
	uint32_t      totalframes;
	uint16_t      bps;
	uint16_t      channels;
	uint32_t      samplerate;

	uint32_t      startframeoffset;
	uint32_t      ape_seek_ms_to;

	void*         cbbuf;//ringbuf
} parser_ape_t;

#ifdef __cplusplus
}
#endif

#endif /* __K1_APE_P_H__ */

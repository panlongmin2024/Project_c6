/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __K1_WAV_P_H__
#define __K1_WAV_P_H__

#ifdef __cplusplus
extern "C" {
#endif

/* wav pcm format */
#define WAV_LPCM        0x1
#define WAV_MS_ADPCM    0x2
#define WAV_DBLE        0x3
#define WAV_ALAW        0x6
#define WAV_ULAW        0x7
#define WAV_IMA_ADPCM   0x11
#define WAV_BPCM        0x21  /* */

#define WAV_ADPCM_SWF   0xd  /* */

/* wav parser pcm info */
typedef struct
{
	int32_t format;             /* pcm的类型 */
	int32_t channels;           /* 声道数 */
	int32_t samples_per_frame;  /* 每帧包含的样本数 */
	int32_t bits_per_sample;    /* 每个样本解码出来所应包含的bit数 */
	int32_t bytes_per_frame;    /* 每帧包含的bytes数 */
} parser_wav_t;

#ifdef __cplusplus
}
#endif

#endif /* __K1_WAV_P_H__ */

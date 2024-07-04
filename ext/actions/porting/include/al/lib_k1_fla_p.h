/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __K1_FLA_P_H__
#define __K1_FLA_P_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	uint32_t min_blocksize;
	uint32_t max_blocksize;
	uint32_t min_framesize;
	uint32_t max_framesize;
	uint32_t sample_rate;
	uint32_t channels;
	uint32_t bits_per_sample;
	uint32_t total_time;
	int64_t total_samples;
} metadata_streaminfo_t;

typedef struct
{
	metadata_streaminfo_t streaminfo;
	void *inbuf;
} parser_flac_t;

#ifdef __cplusplus
}
#endif

#endif /* __K1_FLA_P_H__ */

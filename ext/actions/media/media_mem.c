/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file media mem interface
 */
#define SYS_LOG_DOMAIN "media"
#include <linker/section_tags.h>
#include <os_common_api.h>
#include <mem_manager.h>
#include <media_service.h>
#include <audio_system.h>

#include "media_mem.h"

struct media_memory_cell {
	uint8_t mem_type;
	uint32_t mem_base;
	uint32_t mem_size;
};

struct media_memory_block {
	uint8_t stream_type;
	struct media_memory_cell mem_cell[24];
};

#ifdef CONFIG_MEDIA
/* 0x8000 32k media share media ram */
extern char __share_media_ram_start[];
//__in_section_unique(DSP_SHARE2_MEDIA_RAM) static char share2_media_ram_start[0x4000];
extern char __share2_media_ram_start[];
extern char __share3_media_ram_start[];
extern char __share4_media_ram_start[];

__in_section_unique(DSP_SHARE_MEDIA_RAM) static char playback_input_buffer[0x4000];
__in_section_unique(DSP_SHARE_MEDIA_RAM) static char playback_output_decoder_buffer[0x1000];
#ifdef CONFIG_DSP_OUTPUT_1_CH_IN_BMS
__in_section_unique(DSP_SHARE_MEDIA_RAM) char output_pcm2[0x400];
__in_section_unique(DSP_SHARE4_MEDIA_RAM) char output_pcm[0x2200];
#else
#ifdef CONFIG_MUSIC_EXTERNAL_EFFECT
__in_section_unique(DSP_SHARE_MEDIA_RAM) char output_pcm[0x4600];
#else
__in_section_unique(DSP_SHARE_MEDIA_RAM) char output_pcm[0x1e00];
#endif
#endif
// __in_section_unique(DSP_SHARE_MEDIA_RAM) static char parser_chuck_buffer[0x1000];
__in_section_unique(DSP_SHARE3_MEDIA_RAM) char parser_output_buffer[0x1000];

// static char output_pk_hdr[0x100]  __aligned(16);
// static char sbc_dec_global_bss[0x0d20]  __aligned(16);
// static char sbc_enc_global_bss[0x0bec]  __aligned(16);

#ifdef CONFIG_TOOL_ASQT
/* asqt */
__in_section_unique(ASQT_TOOL_STUB_BUF) static char asqt_tool_stub_buf[1548 * 2] ;
#endif

#ifdef CONFIG_TOOL_ECTT
static char ectt_tool_buf[3072] ;
#endif

#ifdef CONFIG_ACTIONS_DECODER
__in_section_unique(MEDIA_CODEC_STACK_RAM) static char codec_stack[1536] __aligned(STACK_ALIGN);
// static char parser_stack[2048] __aligned(STACK_ALIGN);
#endif

static const struct media_memory_block media_memory_config[] = {
	{
		.stream_type = AUDIO_STREAM_MUSIC,
		.mem_cell = {
			{.mem_type = INPUT_PLAYBACK,  .mem_base = (u32_t)__share_media_ram_start + 0x0, .mem_size = 0x4000,},
			{.mem_type = OUTPUT_DECODER,  .mem_base = (u32_t)__share2_media_ram_start, .mem_size = 0x1000,},
			{.mem_type = OUTPUT_RESAMPLE, .mem_base = (u32_t)__share2_media_ram_start + 0x1000, .mem_size = 0x1000,},
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (u32_t)__share2_media_ram_start + 0x2000, .mem_size = 0x1000,},
			{.mem_type = OUTPUT_PKG_HDR,  .mem_base = (u32_t)__share3_media_ram_start, .mem_size = 0x100,},
		    {.mem_type = ENERGY_BUFFER,   .mem_base = (u32_t)__share3_media_ram_start + 0x100, .mem_size = 320,},
			//{.mem_type = OUTPUT_PCM,      .mem_base = (uint32_t)&output_pcm[0], .mem_size = sizeof(output_pcm),},
			// {.mem_type = SBC_ENCODER_GLOBAL_DATA, .mem_base = (uint32_t)&sbc_enc_global_bss[0], .mem_size = sizeof(sbc_enc_global_bss),},
			// {.mem_type = SBC_DECODER_SHARE_DATA,      .mem_base = (uint32_t)&sbc_dec_global_bss[0], .mem_size = sizeof(sbc_dec_global_bss),},
		},
	},

	{
		.stream_type = AUDIO_STREAM_LOCAL_MUSIC,
		.mem_cell = {
			{.mem_type = INPUT_PLAYBACK,  .mem_base = (uint32_t)&playback_input_buffer[0], .mem_size = 0x1400,},
			{.mem_type = OUTPUT_DECODER,  .mem_base = (uint32_t)&playback_output_decoder_buffer[0], .mem_size = 0x800,},
			{.mem_type = OUTPUT_RESAMPLE,  .mem_base = (uint32_t)&playback_input_buffer[0x1400], .mem_size = 0x800,},
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (uint32_t)&playback_output_decoder_buffer[0x800], .mem_size = 0x800,},
			{.mem_type = OUTPUT_PCM,      .mem_base = (uint32_t)__share2_media_ram_start + 0x2800, .mem_size = 0x1000,},
			{.mem_type = PARSER_CHUCK,    .mem_base = (uint32_t)__share2_media_ram_start + 0x1800, .mem_size = 0x1000,},
			{.mem_type = OUTPUT_PARSER,   .mem_base = (uint32_t)__share3_media_ram_start, .mem_size = 0x1000,},
		#ifdef CONFIG_ACTIONS_DECODER
			{.mem_type = PARSER_STACK, 	  .mem_base = (u32_t)__share2_media_ram_start + 0x3800, .mem_size = 0x800,},
		#endif

		#ifdef CONFIG_ACTIONS_DECODER
			{.mem_type = CODEC_STACK, .mem_base = (uint32_t)&codec_stack[0], .mem_size = sizeof(codec_stack),},
		#endif

		#ifdef CONFIG_TOOL_ECTT
			{.mem_type = TOOL_ECTT_BUF, .mem_base = (uint32_t)&ectt_tool_buf[0], .mem_size = sizeof(ectt_tool_buf),},
		#endif

		},
	},
	{
		.stream_type = AUDIO_STREAM_LINEIN,
		.mem_cell = {
#ifdef CONFIG_MUSIC_EXTERNAL_EFFECT
			{.mem_type = INPUT_CAPTURE,   .mem_base = (u32_t)__share_media_ram_start + 0x7600, .mem_size = 0x600,},
#else
			{.mem_type = INPUT_CAPTURE,   .mem_base = (u32_t)__share4_media_ram_start, .mem_size = 0x1000,},
#endif
			{.mem_type = INPUT_CAPTURE2,   .mem_base = (u32_t)__share_media_ram_start, .mem_size = 0x1200,},
			{.mem_type = OUTPUT_CAPTURE,  .mem_base = (u32_t)__share2_media_ram_start, .mem_size = 0x800,},
			{.mem_type = OUTPUT_CAPTURE2,  .mem_base = (u32_t)__share2_media_ram_start + 0x400, .mem_size = 0x400,}, //part of OUTPUT_CAPTURE

			{.mem_type = INPUT_PLAYBACK,  .mem_base = (u32_t)__share2_media_ram_start + 0x1000, .mem_size = 0x3000,},
			{.mem_type = OUTPUT_DECODER,  .mem_base = (u32_t)__share2_media_ram_start + 0x800, .mem_size = 0x800,},
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (u32_t)__share_media_ram_start + 0x1200, .mem_size = 0x5400,},
			{.mem_type = OUTPUT_RESAMPLE, .mem_base = (u32_t)__share3_media_ram_start, .mem_size = 0x1000,},

#ifdef CONFIG_ACTIONS_DECODER
			{.mem_type = CODEC_STACK, .mem_base = (u32_t)&codec_stack[0], .mem_size = sizeof(codec_stack),},
#endif

		},
	},
	{
		.stream_type = AUDIO_STREAM_VOICE,
		.mem_cell = {
			{.mem_type = INPUT_PLAYBACK,  .mem_base = (u32_t)__share_media_ram_start, .mem_size = 0x2A8,},
			{.mem_type = OUTPUT_DECODER,  .mem_base = (u32_t)__share_media_ram_start + 0x2A8, .mem_size = 0x400,},
			{.mem_type = OUTPUT_RESAMPLE,  .mem_base = (u32_t)__share_media_ram_start + 0x6A8, .mem_size = 0x400,}, //16 * 4 * 16ms
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (u32_t)__share_media_ram_start + 0xAA8, .mem_size = 0x2000,}, //96 * 4 * 16ms + 2048, align to 0x400

			//{.mem_type = INPUT_PCM,      .mem_base = (u32_t)&output_pcm[0x000], .mem_size = 0x200,},
			{.mem_type = INPUT_CAPTURE,  .mem_base = (u32_t)__share_media_ram_start + 0x2AA8, .mem_size = 0x500,},
			{.mem_type = INPUT_ENCBUF,   .mem_base = (u32_t)__share_media_ram_start + 0x2FA8, .mem_size = 0x400,},
			{.mem_type = INPUT_RESAMPLE,  .mem_base = (u32_t)__share_media_ram_start + 0x33A8, .mem_size = 0x200,},
			{.mem_type = OUTPUT_CAPTURE, .mem_base = (u32_t)__share_media_ram_start + 0x35A8, .mem_size = 0x15C,},
			{.mem_type = AEC_REFBUF0,    .mem_base = (u32_t)__share_media_ram_start + 0x3704, .mem_size = 0x400,},

			{.mem_type = OUTPUT_SCO, .mem_base = (u32_t)__share2_media_ram_start, .mem_size = 0xE8,},
			{.mem_type = TX_SCO,     .mem_base = (u32_t)__share2_media_ram_start + 0xE8, .mem_size = 0x7C,},
			{.mem_type = RX_SCO,     .mem_base = (u32_t)__share2_media_ram_start + 0xE8 + 0x7C, .mem_size = 0xF0,},
			//{.mem_type = OUTPUT_PCM,      .mem_base = (u32_t)&output_pcm[0x200], .mem_size = 0xA00,},

			{.mem_type = INPUT_CAPTURE2,  .mem_base = (u32_t)__share2_media_ram_start + 0x1000, .mem_size = 0x800,},
			{.mem_type = INPUT_RESAMPLE2,  .mem_base = (u32_t)__share2_media_ram_start + 0x1800, .mem_size = 0x200,},
		#ifdef CONFIG_TOOL_ASQT
			{.mem_type = TOOL_ASQT_STUB_BUF, .mem_base = (uint32_t)&asqt_tool_stub_buf[0], .mem_size = sizeof(asqt_tool_stub_buf),},
			//{.mem_type = TOOL_ASQT_DUMP_BUF, .mem_base = (uint32_t)&parser_chuck_buffer[0], .mem_size = 3072 * 2,},
		#endif

		#ifdef CONFIG_ACTIONS_DECODER
			{.mem_type = CODEC_STACK, .mem_base = (uint32_t)&codec_stack[0], .mem_size = sizeof(codec_stack),},
		#endif
		},
	},

	{
		.stream_type = AUDIO_STREAM_USOUND,
		.mem_cell = {
			{.mem_type = INPUT_CAPTURE,   .mem_base = (u32_t)__share_media_ram_start, .mem_size = 0x1200,}, //96 * 4 * 2 * 11 = 8448
			{.mem_type = OUTPUT_CAPTURE,  .mem_base = (u32_t)__share2_media_ram_start + 0x800, .mem_size = 0x800,}, //104 * 10
			{.mem_type = OUTPUT_CAPTURE2,  .mem_base = (u32_t)__share2_media_ram_start + 0x800 + 0x400, .mem_size = 0x400,}, //104 * 10

			{.mem_type = INPUT_PLAYBACK,  .mem_base = (u32_t)__share2_media_ram_start + 0x1000, .mem_size = 0x3000,},
			{.mem_type = OUTPUT_DECODER,  .mem_base = (u32_t)__share2_media_ram_start, .mem_size = 0x800,},
#ifdef CONFIG_DSP_OUTPUT_1_CH_IN_BMS
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (u32_t)__share_media_ram_start + 0x1200, .mem_size = 0x4000,},
#else
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (u32_t)__share_media_ram_start + 0x1200, .mem_size = 0x5400,}, //96 * 4 * 2 * 20
#endif
			{.mem_type = OUTPUT_RESAMPLE, .mem_base = (u32_t)__share3_media_ram_start, .mem_size = 0x1000,},

#ifdef CONFIG_ACTIONS_DECODER
			{.mem_type = CODEC_STACK, .mem_base = (u32_t)&codec_stack[0], .mem_size = sizeof(codec_stack),},
#endif
		},
	},

	{
		.stream_type = AUDIO_STREAM_SOUNDBAR,
		.mem_cell = {
			{.mem_type = OUTPUT_CAPTURE,  .mem_base = (u32_t)__share2_media_ram_start, .mem_size = 0x800,},
			{.mem_type = OUTPUT_CAPTURE2,  .mem_base = (u32_t)__share2_media_ram_start + 0x400, .mem_size = 0x400,},

			{.mem_type = INPUT_PLAYBACK,  .mem_base = (u32_t)__share2_media_ram_start + 0x800, .mem_size = 0x4800,},
#ifdef CONFIG_DSP_OUTPUT_1_CH_IN_BMS
			{.mem_type = INPUT_CAPTURE,   .mem_base = (u32_t)__share4_media_ram_start, .mem_size = 0x1200,},
			{.mem_type = OUTPUT_DECODER,  .mem_base = (u32_t)__share_media_ram_start, .mem_size = 0x800,},
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (u32_t)__share_media_ram_start + 0x800, .mem_size = 0x4c00,},
			{.mem_type = OUTPUT_RESAMPLE, .mem_base = (u32_t)__share4_media_ram_start + 0x1200, .mem_size = 0x1000,},
			//{.mem_type = INPUT_RESAMPLE, .mem_base = (u32_t) __share4_media_ram_start + 0x2200, .mem_size = 0x800,},
#else
			{.mem_type = INPUT_CAPTURE,   .mem_base = (u32_t)__share_media_ram_start, .mem_size = 0x1200,},
			{.mem_type = OUTPUT_DECODER,  .mem_base = (u32_t)__share_media_ram_start + 0x1200, .mem_size = 0x800,},
#ifdef CONFIG_MUSIC_EXTERNAL_EFFECT
			{.mem_type = OUTPUT_RESAMPLE, .mem_base = (u32_t)__share_media_ram_start + 0x7e00, .mem_size = 0x1000,},
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (u32_t)__share_media_ram_start + 0x1a00, .mem_size = 0x6400,},
			{.mem_type = INPUT_RESAMPLE, .mem_base = (u32_t) __share_media_ram_start + 0x8e00, .mem_size = 0x800,},
#else
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (u32_t)__share_media_ram_start + 0x1a00, .mem_size = 0x5400,},
			{.mem_type = OUTPUT_RESAMPLE, .mem_base = (u32_t)__share4_media_ram_start, .mem_size = 0x1000,},
#endif
#endif

#ifdef CONFIG_ACTIONS_DECODER
			{.mem_type = CODEC_STACK, .mem_base = (u32_t)&codec_stack[0], .mem_size = sizeof(codec_stack),},
#endif
		}
	},

	{
		.stream_type = AUDIO_STREAM_TTS,
		.mem_cell = {
			{.mem_type = TWS_LOCAL_INPUT, .mem_base = (uint32_t)&playback_input_buffer[0], .mem_size = 0x1400,},
			{.mem_type = OUTPUT_PCM,      .mem_base = (uint32_t)&output_pcm[0], .mem_size = 4096,},
			{.mem_type = OUTPUT_DECODER,  .mem_base = (uint32_t)&playback_input_buffer[0x1400], .mem_size = 0x0800,},
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (uint32_t)&playback_input_buffer[0x2e00], .mem_size = 0x1000,},
			{.mem_type = OUTPUT_RESAMPLE, .mem_base = (uint32_t)&playback_output_decoder_buffer[0], .mem_size = sizeof(playback_output_decoder_buffer),},

		#ifdef CONFIG_ACTIONS_DECODER
			{.mem_type = CODEC_STACK, .mem_base = (uint32_t)&codec_stack[0], .mem_size = sizeof(codec_stack),},
		#endif

		#ifdef CONFIG_TOOL_ECTT
			{.mem_type = TOOL_ECTT_BUF, .mem_base = (uint32_t)&ectt_tool_buf[0], .mem_size = sizeof(ectt_tool_buf),},
		#endif

			{.mem_type = OTA_THREAD_STACK,  .mem_base = (u32_t)__share2_media_ram_start, .mem_size = 0x800,},
			{.mem_type = OTA_UPGRADE_BUF,	.mem_base = (u32_t)__share2_media_ram_start + 0x800, .mem_size = 0x1000,},
		},
	},

	{
		.stream_type = AUDIO_STREAM_LE_AUDIO,
		.mem_cell = {
			{.mem_type = INPUT_CAPTURE,   .mem_base = (u32_t)__share2_media_ram_start, .mem_size = 0x2100,},
			{.mem_type = OUTPUT_CAPTURE,  .mem_base = (u32_t)__share2_media_ram_start + 0x2100, .mem_size = 0x800,},
			{.mem_type = OUTPUT_CAPTURE2, .mem_base = (u32_t)__share2_media_ram_start + 0x2100 + 0x400, .mem_size = 0x400,},

			{.mem_type = INPUT_PLAYBACK,  .mem_base = (u32_t)__share2_media_ram_start + 0x2100 + 0x800, .mem_size = 0x1000,},
			{.mem_type = OUTPUT_DECODER,  .mem_base = (u32_t)__share2_media_ram_start + 0x2100 + 0x800 + 0x1000, .mem_size = 0xf00,},

#ifdef CONFIG_DSP_OUTPUT_1_CH_IN_BMS
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (u32_t)__share_media_ram_start, .mem_size = 0x4c00,},
			{.mem_type = OUTPUT_RESAMPLE, .mem_base = (u32_t)__share4_media_ram_start, .mem_size = 0x1000,},
#else
			{.mem_type = OUTPUT_PLAYBACK, .mem_base = (u32_t)__share_media_ram_start, .mem_size = 0x6400,},
			{.mem_type = OUTPUT_RESAMPLE, .mem_base = (u32_t)__share_media_ram_start + 0x6400, .mem_size = 0x1000,},
#endif

#ifdef CONFIG_ACTIONS_DECODER
			{.mem_type = CODEC_STACK, .mem_base = (u32_t)&codec_stack[0], .mem_size = sizeof(codec_stack),},
#endif
		}
	},
};

static const struct media_memory_block *_memdia_mem_find_memory_block(int stream_type)
{
	const struct media_memory_block *mem_block = NULL;
	if (stream_type == AUDIO_STREAM_FM
		|| stream_type == AUDIO_STREAM_I2SRX_IN
		|| stream_type == AUDIO_STREAM_SPDIF_IN
		|| stream_type == AUDIO_STREAM_MIC_IN) {
		stream_type = AUDIO_STREAM_LINEIN;
	}

	for (int i = 0; i < ARRAY_SIZE(media_memory_config) ; i++) {
		mem_block = &media_memory_config[i];
		if (mem_block->stream_type == stream_type) {
			return mem_block;
		}
	}

	return NULL;
}

static const struct media_memory_cell *_memdia_mem_find_memory_cell(const struct media_memory_block *mem_block, int mem_type)
{
	const struct media_memory_cell *mem_cell = NULL;

	for (int i = 0; i < ARRAY_SIZE(mem_block->mem_cell) ; i++) {
		mem_cell = &mem_block->mem_cell[i];
		if (mem_cell->mem_type == mem_type) {
			return mem_cell;
		}
	}

	return NULL;
}

void *media_mem_get_cache_pool(int mem_type, int stream_type)
{
	const struct media_memory_block *mem_block = NULL;
	const struct media_memory_cell *mem_cell = NULL;
	void *addr = NULL;

	mem_block = _memdia_mem_find_memory_block(stream_type);

	if (!mem_block) {
		goto exit;
	}

	mem_cell = _memdia_mem_find_memory_cell(mem_block, mem_type);

	if (!mem_cell) {
		goto exit;
	}

	return (void *)mem_cell->mem_base;

exit:
	return addr;
}

int media_mem_get_cache_pool_size(int mem_type, int stream_type)
{
	const struct media_memory_block *mem_block = NULL;
	const struct media_memory_cell *mem_cell = NULL;
	int mem_size = 0;

	mem_block = _memdia_mem_find_memory_block(stream_type);

	if (!mem_block) {
		goto exit;
	}

	mem_cell = _memdia_mem_find_memory_cell(mem_block, mem_type);

	if (!mem_cell) {
		goto exit;
	}

	return mem_cell->mem_size;

exit:
	return mem_size;
}

int media_mem_get_codec_frame_size(int format, int channel, int sample_rate_khz, int bit_rate_kbps,
	int interval_us, int sample_bits)
{
	int samples = sample_rate_khz * interval_us / 1000;
	int a = (bit_rate_kbps * samples) / (8 * sample_rate_khz);
	int l = a/2 + a%2;
	int r = a/2;

	if (format != NAV_TYPE) {
		printk("not support format %d\n", format);
		return 0;
	}

	if (sample_rate_khz != 16 || sample_rate_khz != 48){
		printk("not support sample_rate %d\n", sample_rate_khz);
		return 0;
	}

	if (sample_bits != 16) {
		printk("not support sample_bits %d\n", sample_bits);
		return 0;
	}

	if (interval_us != 5000 || interval_us != 10000){
		printk("not support interval %d\n", interval_us);
		return 0;
	}

	if(channel == 1)
		return a + a%2;
	else
		return l + r + a%2;
}

int media_mem_get_cache_pool_size_ext(int mem_type, int stream_type, int format, int block_size, int block_num)
{
	int max_size = media_mem_get_cache_pool_size(mem_type, stream_type);
	int need_size = max_size;

	if (format == NAV_TYPE) {
		need_size = (block_size + 4) * block_num;
		if (need_size > max_size) {
			printk("need_size:%d > max_size:%d\n", need_size, max_size);
			return 0;
		}
	}

	return need_size;
}

#else
void *media_mem_get_cache_pool(int mem_type, int stream_type)
{
	return NULL;
}
int media_mem_get_cache_pool_size(int mem_type, int stream_type)
{
	return 0;
}

int media_mem_get_cache_pool_size_ext(int mem_type, int stream_type, int format, int block_size, int block_num)
{
	return 0;
}
#endif





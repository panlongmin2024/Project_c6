/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio shell
*/
#if defined(CONFIG_SYS_LOG)
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "audio"
#endif

#include <shell/shell.h>
#include <init.h>
#include <stdlib.h>
#include <string.h>

#include <audio_system.h>

#define AUDIO_MODEL_VERSION "1.0.0"

static int shell_cmd_version(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	printk("Audio module version: %s\n", AUDIO_MODEL_VERSION);
	return 0;
}

static int shell_cmd_dump_records(int argc, char *argv[])
{
	printk("Not implemented!\n");

	return 0;
}

static int shell_cmd_dump_tracks(int argc, char *argv[])
{
	struct audio_track_t *track;
	track = audio_system_get_track();
	if (NULL == track) {
		printk("No track!\n");
	} else {

		printk("track %p dump: begin ...\n", track);

		printk("\tstream_type: %d\n", track->stream_type);
		printk("\taudio_format: %d\n", track->audio_format);
		printk("\taudio_mode: %d\n", track->audio_mode);
		printk("\tframe_size: %d\n", track->frame_size);
		printk("\tsample_rate: %d\n", track->sample_rate);
		printk("\toutput_sample_rate: %d\n", track->output_sample_rate);

		printk("\tmuted: %d\n", track->muted);
		printk("\tstarted: %d\n", track->started);
		printk("\twaitto_start: %d\n", track->waitto_start);
		printk("\tfill_cnt: %d\n", track->fill_cnt);
		printk("\toutput_samples: %d\n", track->output_samples);
		printk("\tlast_samples_cnt: %d\n", track->last_samples_cnt);
		printk("\tsamples_overflow_cnt: %d\n",
		       (int)track->samples_overflow_cnt);
		printk("\tphy_dma: 0x%x\n", track->phy_dma);

		printk("\ntrack dump: completed ...\n");
	}
	return 0;
}

const struct shell_cmd audio_shell_commands[] = {
	{"version", shell_cmd_version, "show version of audio module"},
	{"dumprecord", shell_cmd_dump_records, "dump audio records"},
	{"dumptrack", shell_cmd_dump_tracks, "dump audio tracks"},
	{NULL, NULL, NULL}
};

SHELL_REGISTER("audio", audio_shell_commands);

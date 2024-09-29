/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt music function.
 */

#include "btmusic.h"

#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif
#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif

void btmusic_delay_resume(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	bt_manager_media_play();
}

void btmusic_media_active_check(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	uint32_t media_handle = bt_manager_media_get_active_br_handle();

	SYS_LOG_INF("%x", media_handle);
	if (((btmusic) && (!btmusic->playback_player)) || (!media_handle)) {
		bt_manager_media_set_active_br_handle(0);
	}
}

static void btmusic_user_pause_media_active_check(struct thread_timer *ttimer,
				   void *expiry_fn_arg){
	uint16_t media_handle = *((uint16_t *)expiry_fn_arg);

	SYS_LOG_INF("%x, %x, %x",bt_manager_media_get_active_br_handle(), media_handle,
				bt_manager_media_get_local_passthrough_status(media_handle));

	if (bt_manager_media_get_active_br_handle() != media_handle) {
		return;
	}
	if (bt_manager_media_get_local_passthrough_status(media_handle) == BT_STATUS_PAUSED) {
		bt_manager_media_set_user_pause_pending(media_handle);
		bt_manager_media_set_active_br_handle(0);
	}
}

void btmusic_user_pause_media_active_timer_stop(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic) {
		return;
	}

	if (thread_timer_is_running(&btmusic->user_pause_media_active_timer)) {
		SYS_LOG_INF("");
		thread_timer_stop(&btmusic->user_pause_media_active_timer);
	}
}

void btmusic_user_pause_media_active_timer_start(uint32_t handle)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	uint32_t media_handle = bt_manager_media_get_active_br_handle();

	if (!btmusic) {
		return;
	}

	SYS_LOG_INF("%x, %x", handle, media_handle);
	if(handle != media_handle){
		return;
	}

	if(thread_timer_is_running(&btmusic->user_pause_media_active_timer)){
		thread_timer_stop(&btmusic->user_pause_media_active_timer);
	}
	btmusic->user_pause_media_active = media_handle;
	thread_timer_init(&btmusic->user_pause_media_active_timer, btmusic_user_pause_media_active_check, &btmusic->user_pause_media_active);
	thread_timer_start(&btmusic->user_pause_media_active_timer, 2000, 0);
}

#ifdef CONFIG_ESD_MANAGER
void btmusic_esd_restore(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (esd_manager_check_esd()) {
		u8_t play_state = 0;

		esd_manager_restore_scene(TAG_PLAY_STATE, &play_state, 1);

		if (play_state) {
			if (thread_timer_is_running(&btmusic->resume_timer)) {
				thread_timer_stop(&btmusic->resume_timer);
			}
			thread_timer_start(&btmusic->resume_timer, 500, 0);
		}
#ifdef CONFIG_PLAYTTS
		tts_manager_unlock();
#endif
		esd_manager_reset_finished();
		return;
	}
}
#endif

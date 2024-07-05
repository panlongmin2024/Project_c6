#include "self_media_effect/self_media_effect.h"
#include <os_common_api.h>
#include "media_player.h"
#include "sys_event.h"

#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "self_media_effect"
#include <logging/sys_log.h>
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#include <thread_timer.h>
#include <bt_manager.h>
#include "wltmcu_manager_supply.h"
static struct thread_timer bypass_led_timer;
#endif
static bool self_music_effect_ctrl_bypass = true; 
static os_mutex self_music_effect_ctrl_mutex;

bool self_music_effect_ctrl_get_enable(void)
{
    bool res;

    os_mutex_lock(&self_music_effect_ctrl_mutex, OS_FOREVER);
    res = !self_music_effect_ctrl_bypass;
    os_mutex_unlock(&self_music_effect_ctrl_mutex);

    SYS_LOG_INF("en:%d\n", res);

    return res;
}

void self_music_effect_ctrl_set_enable(bool en)
{
    int change_flag = 0;
    media_player_t *player;
    int stream_type;

    os_mutex_lock(&self_music_effect_ctrl_mutex, OS_FOREVER);
    
    if (en == !self_music_effect_ctrl_bypass)
    {
        SYS_LOG_INF("already en:%d\n", en);
    }
    else 
    {
        SYS_LOG_INF("en:%d\n", en);

        self_music_effect_ctrl_bypass = !en;
        change_flag = 1;
    }

    os_mutex_unlock(&self_music_effect_ctrl_mutex);

    if (change_flag == 1)
    {
        player = media_player_get_current_main_player();
        stream_type = media_player_get_current_main_player_stream_type();
        
        media_player_check_audio_effect(player, stream_type);
    }
}

void self_music_effect_ctrl_init(void)
{
    SYS_LOG_INF("music effect enable:%d\n", self_music_effect_ctrl_get_enable());

    os_mutex_init(&self_music_effect_ctrl_mutex);
}
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
static void bypass_led_timer_fn(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	thread_timer_stop(&bypass_led_timer);
    pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(0)|AC_LED_STATE(0)|BAT_LED_STATE(0));
    pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,0);
    bt_manager_update_led_display();
    pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_RESET);
}
#endif
void self_music_effect_ctrl_info_dump_by_tts(void)
{
    if (self_music_effect_ctrl_get_enable() == true)
    {
        sys_event_notify(SYS_EVENT_PLAY_MUSIC_EFFECT_ON_TTS);
    }
    else
    {
        sys_event_notify(SYS_EVENT_PLAY_MUSIC_EFFECT_OFF_TTS);
    }
    #ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
    pd_srv_event_notify(PD_EVENT_BT_LED_DISPLAY,SYS_EVENT_BT_CONNECTED);
    pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,1);
    pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_NORMAL_ON); // all led turn on
    pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(1)|AC_LED_STATE(1)|BAT_LED_STATE(1));
    thread_timer_init(&bypass_led_timer, bypass_led_timer_fn, NULL);
    thread_timer_start(&bypass_led_timer,2000,2000);
    #endif
}

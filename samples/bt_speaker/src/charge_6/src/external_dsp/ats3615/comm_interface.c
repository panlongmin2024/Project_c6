#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gpio.h>
#include <tts_manager.h>
#include "include/comm_interface.h"

#ifdef CONFIG_SYS_LOG
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "ats3615_driver"
#include <logging/sys_log.h>
#endif

#define NUM_EQ_PRESETS 6
#define NUM_EQ_BANDS 5

static dolphin_host2dsp_t ats3615_com_host_data_cache = { 0 };

int ats3615_comm_send_volume(float volume_dB)
{
    dolphin_com_t comm;
    memset(&comm, 0, sizeof(comm));
    comm.host.volume_db = volume_dB;
    comm.host.change_flags |= FLAG_CHANGE_VOLUME_DB;

    ats3615_com_host_data_cache.volume_db = volume_dB;
    ats3615_com_host_data_cache.change_flags |= FLAG_CHANGE_VOLUME_DB;

    int ret = dolphin_comm_send_ex(&comm);
    return ret;
}

int ats3615_comm_send_battery_volt(float battery_volt)
{
    dolphin_com_t comm;
    memset(&comm, 0, sizeof(comm));
    comm.host.battery_level_v = battery_volt;
    comm.host.change_flags |= FLAG_REPORT_BATTERY_LEVEL_V;
    
    ats3615_com_host_data_cache.battery_level_v = battery_volt;
    ats3615_com_host_data_cache.change_flags |= FLAG_REPORT_BATTERY_LEVEL_V;

    int ret = dolphin_comm_send_ex(&comm);
    return ret;
}

int ats3615_comm_send_user_eq(dolphin_eq_band_t * eq_bands)
{
    if (!eq_bands)
        return -1;

    dolphin_com_t comm;
    memset(&comm, 0, sizeof(comm));

    // copy preset to dolphin_com structure
    memcpy(comm.host.usereq, eq_bands, sizeof(dolphin_eq_band_t) * NUM_EQ_BANDS);

    // update bit mask for bands to be updated (all of them !)
    comm.host.change_bits_usereq_bands = (1 << NUM_EQ_BANDS) - 1;

    // update change flags
    comm.host.change_flags |= FLAG_CHANGE_USER_EQ;

    memcpy(ats3615_com_host_data_cache.usereq, eq_bands, sizeof(dolphin_eq_band_t) * NUM_EQ_BANDS);
    ats3615_com_host_data_cache.change_bits_usereq_bands = (1 << NUM_EQ_BANDS) - 1;
    ats3615_com_host_data_cache.change_flags |= FLAG_CHANGE_USER_EQ;

    int ret = dolphin_comm_send_ex(&comm);
    return ret;
}

int ats3615_comm_set_audio_path(int audio_path)
{
    dolphin_com_t comm;
    memset(&comm, 0, sizeof(comm));
    comm.host.audio_path = audio_path;
    comm.host.change_flags |= FLAG_CHANGE_AUDIO_PATH;

    ats3615_com_host_data_cache.audio_path = audio_path;
    ats3615_com_host_data_cache.change_flags |= FLAG_CHANGE_AUDIO_PATH;

    int ret = dolphin_comm_send_ex(&comm);
    return ret;
}

int ats3615_comm_set_dsp_run(void)
{
    dolphin_com_t comm;
    memset(&comm, 0, sizeof(comm));
    comm.host.change_flags |= FLAG_RUN_DSP_CODE;

    // ats3615_com_host_data_cache.change_flags |= FLAG_RUN_DSP_CODE;

    int ret = dolphin_comm_send_ex(&comm);
    return ret;
}



int ats3615_re_comm_after_reset(void)
{
    dolphin_com_t comm;
    memset(&comm, 0, sizeof(comm));
    memcpy(&comm.host, &ats3615_com_host_data_cache, sizeof(dolphin_host2dsp_t));

    int ret = dolphin_comm_send_ex(&comm);
    return ret;
}



#ifdef CONFIG_USE_EXT_DSP_VOLUME_GAIN
extern int audio_system_get_ext_dsp_volume_gain(int volume);
extern int ats3615_comm_send_volume(float volume_dB);

static int ext_dsp_volume_index = 0;
static int ext_dsp_volume_delay_set = 0;

int ext_dsp_get_volume_index(void)
{
    return ext_dsp_volume_index;
}

void ext_dsp_volume_control(int volume, int wait_tts_finish)
{
    // static int last_volume = -1;
    // if (last_volume == volume)
    //     return;
    // last_volume = volume;

    ext_dsp_volume_index = volume;

    if (wait_tts_finish /*&& tts_manager_is_playing_without_lock()*/)
    {
        ext_dsp_volume_delay_set = 1;
        SYS_LOG_INF("tts_is_playing, delay to change volume");
        return;
    }
    ext_dsp_volume_delay_set = 0;

    int vol_gain = audio_system_get_ext_dsp_volume_gain(volume);
    float vol_gain_f = ((float)vol_gain) / 10;
    printk("ext_dsp_volume_control , vol %d , gain %d.%d dB \n", volume, vol_gain/10, (-vol_gain) % 10);
    ats3615_comm_send_volume(vol_gain_f);
    return;
}

int ext_dsp_volume_control_delay_proc(void)
{
    if (!ext_dsp_volume_delay_set)
        return 0;

/*     if (tts_manager_is_playing_without_lock())
    {
        printk("tts_is_playing, delay to change volume\n");
        return -1;
    } */

    ext_dsp_volume_delay_set = 0;
    ext_dsp_volume_control(ext_dsp_volume_index, 1);
    return 0;
}


#endif




static dolphin_eq_band_t eq_presets[NUM_EQ_PRESETS][NUM_EQ_BANDS] =  /* xtreme 4 given by David */
{ 
    /* User EQ (band gains can be modified by user) */
    {
        { 120,   0, 0.707,  DOLPHIN_EQ_TYPE_LS2 },
        { 400,   0, 1.5,    DOLPHIN_EQ_TYPE_EQ2 },
        { 1250,  0, 1.5,    DOLPHIN_EQ_TYPE_EQ2 },
        { 4000,  0, 1.5,    DOLPHIN_EQ_TYPE_EQ2 },
        { 8000,  0, 0.707,  DOLPHIN_EQ_TYPE_HS2 },
    },
    /* Signature */
    {
        { 100,    0, 1, DOLPHIN_EQ_TYPE_EQ2 },
        { 1000,   0, 1, DOLPHIN_EQ_TYPE_EQ2 },
        { 2000,   0, 1, DOLPHIN_EQ_TYPE_EQ2 },
        { 7000,   0, 1, DOLPHIN_EQ_TYPE_EQ2 },
        { 10000,  0, 1, DOLPHIN_EQ_TYPE_EQ2 },
    },
    /* Relaxing	*/
    {
        { 200, -4, 0.5   	, DOLPHIN_EQ_TYPE_LS2 },
        { 1000, 0, 1	    , DOLPHIN_EQ_TYPE_EQ2 },
        { 1500, -3, 1       , DOLPHIN_EQ_TYPE_EQ2 },
        { 6000, -3, 0.5     , DOLPHIN_EQ_TYPE_HS2 },
        { 10000, 0, 0.7     , DOLPHIN_EQ_TYPE_LP2 },
    },
    /* Vocal */
    {
        { 80, -5, 0.5	    , DOLPHIN_EQ_TYPE_LS2 },
        { 1000, 4, 1	    , DOLPHIN_EQ_TYPE_EQ2 },
        { 4000, 4, 1	    , DOLPHIN_EQ_TYPE_EQ2 },
        { 9000, -2, 0.7	    , DOLPHIN_EQ_TYPE_EQ2 },
        { 9000, 0, 0.7      , DOLPHIN_EQ_TYPE_LP2 },
    },
    /* Energetic */
    {
        { 60, -2, 0.7	, DOLPHIN_EQ_TYPE_LS2 },
        { 200, 7, 1.5	, DOLPHIN_EQ_TYPE_EQ2 },
        { 1500, 3, 2	, DOLPHIN_EQ_TYPE_EQ2 },
        { 7000, 2, 2	, DOLPHIN_EQ_TYPE_EQ2 },
        { 10000, 0, 1   , DOLPHIN_EQ_TYPE_EQ2 },
    },
    /* Party */
    {
        { 100, 0, 1	    , DOLPHIN_EQ_TYPE_EQ2 },
        { 250, 5, 3	    , DOLPHIN_EQ_TYPE_EQ2 },
        { 3000, 3, 1	, DOLPHIN_EQ_TYPE_EQ2 },
        { 7000, 5, 0.7  , DOLPHIN_EQ_TYPE_HS2 },
        { 10000, 0, 1   , DOLPHIN_EQ_TYPE_EQ2 },
    }
};


void ext_dsp_switch_eq_presets(void)
{
    static u8_t eq_index = -1;
    eq_index = (eq_index + 1) % NUM_EQ_PRESETS;

    printk("ext_dsp_switch_eq_presets  %d \n", eq_index);
    // ats3615_comm_send_user_eq(&(eq_presets[eq_index][0]));
    ext_dsp_set_eq_param(&(eq_presets[eq_index][0]), NUM_EQ_BANDS);
}


int ext_dsp_set_bypass(int bypass)
{
    printk("ext_dsp_set_bypass  %d \n", bypass);

    int audio_path = bypass ? AUDIO_PATH_DSP_BYPASS : AUDIO_PATH_DSP_NORMAL;
    return ats3615_comm_set_audio_path(audio_path);
}

int ext_dsp_send_battery_volt(float battery_volt)
{
    printk("ext_dsp_send_battery_volt , voltage %d mV \n", (int)(battery_volt*1000));

    int ret = ats3615_comm_send_battery_volt(battery_volt);

    return ret;
}

int ext_dsp_set_eq_param(dolphin_eq_band_t * eq_bands, int bands_count)
{
    int i;
    SYS_LOG_INF("\n");
    for (i=0; i<bands_count; i++) {
        printk("bands[%d].frequency = %d \n", i, (int)eq_bands[i].freq);
        printk("bands[%d].gain      = %d.%01d \n", i, (int)eq_bands[i].gain, ((int)(eq_bands[i].gain * 10)) % 10);
        printk("bands[%d].q_value   = %d.%01d \n", i, (int)eq_bands[i].q, ((int)(eq_bands[i].q * 10)) % 10);
        printk("bands[%d].type      = %d \n", i, eq_bands[i].type);
    }

    int ret = ats3615_comm_send_user_eq(eq_bands);

    return ret;
}



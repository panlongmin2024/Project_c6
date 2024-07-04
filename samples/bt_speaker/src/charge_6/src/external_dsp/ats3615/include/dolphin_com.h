/*******************************************************************************
 * Project        Dolphin DSP ATS3615
 * Copyright      2023
 * Company        Harman
 *                All rights reserved
 ******************************************************************************/
#ifndef DOLPHIN_COM__H
#define DOLPHIN_COM__H

#include <stdint.h>

enum
{
    /* Address at which the address of the dolphin_com_t struct can be read */
    DOLPHIN_COM_STRUCT_ADDRESS  = 0x40000170,

    /* Magic number to check that the struct is valid */
    DOLPHIN_COM_STRUCT_MAGIC    = 0xC0DEBEEF,

    /* struct version = this file version, to make sure DSP & HOST agree on the same struct definition */
    DOLPHIN_COM_STRUCT_VERSION  = 0x00000003,
};

enum
{
    FLAG_CHANGE_VOLUME_DB = 1 << 0,
    FLAG_REPORT_BATTERY_LEVEL_V = 1 << 1,
    FLAG_CHANGE_USER_EQ = 1 << 2,
    FLAG_CHANGE_PARAMESTIM = 1 << 3,
    FLAG_CHANGE_AUDIO_PATH = 1 << 4,
    FLAG_RUN_DSP_CODE = 1 << 5,
};

enum
{
	AUDIO_PATH_DSP_NORMAL = 0,
	AUDIO_PATH_DSP_BYPASS = 1,
};

#define USER_EQ_MAX_NUM_BANDS 8

typedef enum
{
    DOLPHIN_EQ_TYPE_BYP   = 0,  // bypass
    DOLPHIN_EQ_TYPE_HP1   = 1,  // 1st order highpass
    DOLPHIN_EQ_TYPE_HP2   = 2,  // 2nd order highpass
    DOLPHIN_EQ_TYPE_LP1   = 5,  // 1st order lowpass
    DOLPHIN_EQ_TYPE_LP2   = 6,  // 2nd order lowpass
    DOLPHIN_EQ_TYPE_EQ2   = 11, // peaking filter (equalizer)
    DOLPHIN_EQ_TYPE_BP2   = 14, // 2nd order bandpass
    DOLPHIN_EQ_TYPE_AMP   = 15, // amplifier
    DOLPHIN_EQ_TYPE_HS2   = 16, // 2nd order highshelf
    DOLPHIN_EQ_TYPE_LS2   = 17, // 2nd order lowshelf
} dolphin_eq_type_t;

typedef struct
{
    float freq;
    float gain;
    float q;
    dolphin_eq_type_t type;
} dolphin_eq_band_t;

typedef struct
{
    uint32_t    magic;
    uint32_t    struct_version;
    uint32_t    dsp_version;
} dolphin_struct_header_t;

typedef struct
{
    /* add more fields here... */
    uint32_t    framecount;
} dolphin_dsp2host_t;

typedef struct
{
    /* add more change_flags & fields here... */
    uint32_t    change_flags;
    float       volume_db;
    float       battery_level_v;
    /* bit 0 = band #1, etc... */
    int         change_bits_usereq_bands;
    dolphin_eq_band_t usereq[USER_EQ_MAX_NUM_BANDS];
    int         param_estim;
    int         audio_path;
} dolphin_host2dsp_t;

/* this structure will be located at the adddress found at DOLPHIN_COM_STRUCT_ADDRESS in the DSP memory */
typedef struct
{
    dolphin_struct_header_t hdr;
    dolphin_dsp2host_t      dsp;
    dolphin_host2dsp_t      host;
} dolphin_com_t;

#endif

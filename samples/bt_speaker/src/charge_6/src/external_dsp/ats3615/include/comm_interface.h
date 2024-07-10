#ifndef _ATS3615_COMM_INTERFACE_H_
#define _ATS3615_COMM_INTERFACE_H_

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gpio.h>

#include "include/ats3615_inner.h"


#include "include/dolphin_firmware.h"


#include "include/dolphin_rw.h"
#include "include/dolphin_com.h"




extern int dolphin_comm_send_wait_finish(dolphin_com_t * p_comm);
extern int dolphin_comm_send_ex(dolphin_com_t * p_comm);

int ats3615_comm_send_volume(float volume_dB);

int ats3615_comm_send_battery_volt(float battery_volt);
int ext_dsp_send_battery_volt(float battery_volt);
int ext_dsp_set_eq_param(dolphin_eq_band_t * eq_bands, int bands_count);

int ats3615_comm_send_user_eq(dolphin_eq_band_t * eq_bands);

extern int ext_dsp_set_bypass(int bypass);



#endif

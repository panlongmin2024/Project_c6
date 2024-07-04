#ifndef __ATS3615_HEAD_H__
#define __ATS3615_HEAD_H__

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern int external_dsp_ats3615_load(int effect);
extern void external_dsp_ats3615_deinit(void);
extern void external_dsp_ats3615_timer_start(void);
#endif 

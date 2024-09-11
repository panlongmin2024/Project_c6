
#ifndef _SELFAPP_CONFIG_H_
#define _SELFAPP_CONFIG_H_

#include "selfapp_internal.h"

#define SELFAPP_CONFIG_VERSION 0xAC000001
#define DEDAULT_STEREO_GROUP_NAME    "My Stereo Group"

int selfapp_config_set_ac_group(const struct AURACAST_GROUP* group);
int selfapp_config_get_ac_group(struct AURACAST_GROUP* group);
u8_t selfapp_get_feedback_tone(void);
void selfapp_set_feedback_tone(u8_t enable);
#ifdef SPEC_LED_PATTERN_CONTROL
void selfapp_config_set_ledmovespeed(u8_t speed);
u8_t selfapp_config_get_ledmovespeed(void);
void selfapp_config_set_ledbrightness(u8_t value, u8_t status, u8_t project);
void selfapp_config_get_ledbrightness(u8_t *value, u8_t *status, u8_t *project);
#endif
#ifdef SPEC_REMOTE_CONTROL
void selfapp_config_set_mfbstatus(u8_t status);
u8_t selfapp_config_get_mfbstatus(void);
#endif
#ifdef SPEC_ONE_TOUCH_MUSIC_BUTTON
void selfapp_config_set_one_touch_music_button(u16_t button);
u16_t selfapp_config_get_one_touch_music_button(void);
#endif
void selfapp_config_set_eq(u8_t eqid, const u8_t* data, u16_t len);
u8_t selfapp_config_get_eq_id(void);
int selfapp_config_get_customer_eq_c1(u8_t *scope, u8_t *bands);
int selfapp_config_get_customer_eq_c2(u8_t *buf, u16_t len);
u8_t* selfapp_config_get_eq_data(void);
void selfapp_config_set_customer_eq_c1(u8_t eqid, u8_t count, u8_t scope, u8_t* bands);
void selfapp_config_set_customer_eq_c2(u8_t eqid, u8_t* data, uint16_t len);
void selfapp_config_init(void);
void self_stamem_save(u8_t confirmed);

#endif

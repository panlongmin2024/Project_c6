
#ifndef SELFAPP_ADAPTOR_H_
#define SELFAPP_ADAPTOR_H_

u8_t selfapp_get_bat_power(void);
u8_t selfapp_get_bat_level(void);
u8_t selfapp_get_role(void);
void selfapp_get_mac(u8_t* mac);

void selfapp_set_link_mode(u8_t mode);
int selfapp_set_indication(u8_t dev_idx);
u8_t selfapp_get_audio_source(void);
void selfapp_set_model_id(u8_t mid);
u8_t selfapp_get_model_id(void);
void selfapp_switch_lasting_stereo_mode(int enable);

#endif

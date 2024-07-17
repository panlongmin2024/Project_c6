
#ifndef SELFAPP_ADAPTOR_H_
#define SELFAPP_ADAPTOR_H_

u8_t selfapp_get_bat_level(void);
u8_t selfapp_get_role(void);
u8_t selfapp_get_lasting_stereo_mode(void);
u8_t selfapp_lasting_stereo_is_primary_role(void);
void selfapp_get_mac(u8_t* mac);

void selfapp_set_link_mode(u8_t mode);
int selfapp_set_indication(u8_t dev_idx);
u8_t selfapp_get_audio_source(void);
void selfapp_set_model_id(u8_t mid);
u8_t selfapp_get_model_id(void);
void selfapp_switch_lasting_stereo_mode(int enable);
void selfapp_set_lasting_stereo_group_info_to_slave(struct AURACAST_GROUP *group);
u8_t selfapp_get_lasting_stereo_role(void);
void selfapp_mute_player(u8_t mute);
void selfapp_reset_player(void);

#endif

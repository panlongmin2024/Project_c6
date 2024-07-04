#ifndef __SELF_MEDIA_EFFECT_H__
#define __SELF_MEDIA_EFFECT_H__

#include <stdbool.h>

bool self_music_effect_ctrl_get_enable(void);
void self_music_effect_ctrl_set_enable(bool en);
void self_music_effect_ctrl_init(void);
void self_music_effect_ctrl_info_dump_by_tts(void);

#endif

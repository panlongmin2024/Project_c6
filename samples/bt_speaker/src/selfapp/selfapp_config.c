#include "selfapp_internal.h"
#include "selfapp_config.h"
#include <string.h>

#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif
#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif

#include <media_effect_param.h>
#include <audio_system.h>

typedef struct {
	u32_t version;
	struct AURACAST_GROUP group;
	u8_t feedback_tone:1;	// 1 enable, 0 disable
	u8_t reserved_bits:7;	// 1 enable, 0 disable
	u8_t eq_id;
	u8_t eq_scope;
	u8_t eq_bands[2*BAND_COUNT_MAX];
	u8_t eq_data[EQ_DATA_SIZE];

#ifdef SPEC_REMOTE_CONTROL
	u8_t mfb_status;	// MFB_Status_e, 0x00 play/pause, 0x01 voice trigger
#endif
#ifdef SPEC_LED_PATTERN_CONTROL
	u8_t led_brightness;	// 0x00 to 0xFF

	u8_t bright_status:2;	// Status_e, 1 on, 0 off
	u8_t project_status:2;	// Status_e, 1 on, 0 off
	u8_t led_move_speed:4;	// LED_Movement_Speed_e
#endif

#ifdef SPEC_ONE_TOUCH_MUSIC_BUTTON
	u16_t one_touch; //one touch music button
#endif
} self_stamem_t;

static self_stamem_t selfapp_config; // nvram saved


K_MUTEX_DEFINE(self_stamem_mutex);

self_stamem_t *self_get_stamem(void)
{
	return &selfapp_config;
}

void self_stamem_save(u8_t confirmed)
{
	self_stamem_t *selfsta = self_get_stamem();
	if (selfsta) {
#ifdef CONFIG_PROPERTY
		selfapp_log_inf("set %d", confirmed);
		property_set(SELF_NVRAM_STA, (char *)selfsta,
			     sizeof(self_stamem_t));
		if (confirmed) {
			property_flush(SELF_NVRAM_STA);
		}
#endif
	}
}

int selfapp_config_set_ac_group(const struct AURACAST_GROUP* group)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (selfsta) {
		os_mutex_lock(&self_stamem_mutex, OS_FOREVER);
		memcpy(&selfsta->group,group,sizeof(struct AURACAST_GROUP));
		os_mutex_unlock(&self_stamem_mutex);
		self_stamem_save(0);
	}

	return 0;
}

int selfapp_config_get_ac_group(struct AURACAST_GROUP* group)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (selfsta) {
		os_mutex_lock(&self_stamem_mutex, OS_FOREVER);
		memcpy(group,&selfsta->group,sizeof(struct AURACAST_GROUP));
		os_mutex_unlock(&self_stamem_mutex);
	} else {
		return -EINVAL;
	}

	return 0;
}

void selfapp_set_feedback_tone(u8_t enable)
{
	self_stamem_t *selfsta = self_get_stamem();

#ifdef CONFIG_PLAYTTS
	if (enable) {
		while (tts_manager_is_locked()) {
			tts_manager_unlock();
		}
	} else {
		tts_manager_lock();
	}
#endif
	if (selfsta) {
		selfsta->feedback_tone = enable;
		self_stamem_save(0);
	}
}

u8_t selfapp_get_feedback_tone(void)
{
#ifdef CONFIG_PLAYTTS
	if (tts_manager_is_locked()) {
		return 0;
	} else {
		return 1;
	}
#else
	return 0;
#endif
}

#ifdef SPEC_LED_PATTERN_CONTROL
void selfapp_config_set_ledmovespeed(u8_t speed)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return;
	}

	selfsta->led_move_speed = speed;
	self_stamem_save(0);
}

u8_t selfapp_config_get_ledmovespeed(void)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return 1;
	}

	return selfsta->led_move_speed;
}

void selfapp_config_set_ledbrightness(u8_t value, u8_t status, u8_t project)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return;
	}

	selfsta->led_brightness = value;
	selfsta->bright_status = status;
	selfsta->project_status = project;

	self_stamem_save(0);
}

void selfapp_config_get_ledbrightness(u8_t *value, u8_t *status, u8_t *project)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return;
	}

	*value = selfsta->led_brightness; //0x00-0xFF
	*status = selfsta->bright_status; //0-off, 1-on
	*project = selfsta->project_status; //0-off, 1-on

}
#endif

#ifdef SPEC_REMOTE_CONTROL
void selfapp_config_set_mfbstatus(u8_t status)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return;
	}

	selfsta->mfb_status = status;
	self_stamem_save(0);
}

u8_t selfapp_config_get_mfbstatus(void)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return 0;
	}

	return selfsta->mfb_status;
}
#endif

#ifdef SPEC_ONE_TOUCH_MUSIC_BUTTON
void selfapp_config_set_one_touch_music_button(u16_t button)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return;
	}

	selfsta->one_touch = button;
}

u16_t selfapp_config_get_one_touch_music_button(void)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return 0;
	}

	return selfsta->one_touch;
}
#endif

void selfapp_config_set_eq(u8_t eqid, const u8_t* data, u16_t len)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return;
	}

	selfsta->eq_id = eqid;
	if(len <= EQ_DATA_SIZE) {
		memcpy(selfsta->eq_data, data, len);
	} else {
		selfapp_log_wrn("wrong len %d", len);
	}
	self_stamem_save(0);

	selfapp_eq_cmd_update(eqid, data, len);
}

u8_t selfapp_config_get_eq_id(void)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return 0;
	}

	return selfsta->eq_id;
}

u8_t* selfapp_config_get_eq_data(void)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return NULL;
	}

	return selfsta->eq_data;
}

void selfapp_config_set_customer_eq(u8_t eqid, u8_t count, u8_t scope, u8_t* bands)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return;
	}

	if (count != BAND_COUNT_MAX) {
		selfapp_log_wrn("Wrong band count.");
		return;
	}

	selfsta->eq_scope = scope;
	selfsta->eq_id = eqid;
	memcpy(selfsta->eq_bands, bands, BAND_COUNT_MAX*2);
	self_stamem_save(0);
}

int selfapp_config_get_customer_eq(u8_t *scope, u8_t *bands)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return -1;
	}

	*scope = selfsta->eq_scope;
	memcpy(bands, selfsta->eq_bands, BAND_COUNT_MAX * 2);

	return 0;
}

void selfapp_config_reset(void)
{
	self_stamem_t *selfsta = self_get_stamem();

	if (NULL == selfsta) {
		return;
	}

	selfapp_log_inf("to reset");
	memset(selfsta, 0, sizeof(self_stamem_t));

	selfsta->version = SELFAPP_CONFIG_VERSION;

#ifdef CONFIG_PLAYTTS
	selfsta->feedback_tone = 1;
#else
	selfsta->feedback_tone = 0;	//disable
#endif

	selfsta->eq_id = EQCATEGORY_DEFAULT;
	selfsta->eq_scope = DEFAULT_SCOPE;
	memcpy(selfsta->eq_data, selfapp_eq_get_default_data(), EQ_DATA_SIZE);

	for (int idx = 0; idx < BAND_COUNT_MAX; idx++) {
		selfsta->eq_bands[idx*2] = (Custom_Band_1 + idx);
		selfsta->eq_bands[idx*2 + 1] = 0;
	}

#ifdef SPEC_REMOTE_CONTROL
	selfsta->mfb_status = MFBSTA_VOICE_TRIGER;
#endif
#ifdef SPEC_LED_PATTERN_CONTROL
	selfsta->led_brightness = 0x50;
	selfsta->bright_status = STA_ON;
	selfsta->project_status = STA_ON;
	selfsta->led_move_speed = 0x2;
#endif
#ifdef SPEC_ONE_TOUCH_MUSIC_BUTTON
	selfsta->one_touch = (0x01 << 8) | 0x01; //long press & light-show
#endif
	self_stamem_save(1);
}

void selfapp_config_init(void)
{
	self_stamem_t *selfsta = self_get_stamem();
	u8_t* addr;
	u16_t size;
	int ret = 0;

	if (NULL == selfsta) {
		return;
	}

	os_mutex_init(&self_stamem_mutex);

#ifdef CONFIG_PROPERTY
	ret = property_get(SELF_NVRAM_STA, (char *)selfsta, sizeof(self_stamem_t));
#endif

	if(ret < 0 || selfsta->version != SELFAPP_CONFIG_VERSION) {
		selfapp_config_reset();
	}

	selfapp_log_dump(selfsta, sizeof(self_stamem_t));

#ifdef CONFIG_PLAYTTS
	if (selfsta->feedback_tone != !tts_manager_is_locked()) {
		if (selfsta->feedback_tone == 0) {
			selfapp_log_inf("feedbacktone off\n");
			tts_manager_lock();
		}
	}
#endif

	selfapp_eq_cmd_update(selfsta->eq_id, selfsta->eq_data, EQ_DATA_SIZE);
	addr = selfapp_eq_cmd_get(&size);
	if(NULL != addr) {
		SYS_LOG_INF("set effect %p %d", addr, size);
		media_effect_set_user_param(AUDIO_STREAM_MUSIC, 0, addr, size);
	}
}

int selfapp_get_feedback_tone_ext(void)
{
	self_stamem_t *selfsta = self_get_stamem();
	if (selfsta) {
		return selfsta->feedback_tone;
	}
	return -1;
}
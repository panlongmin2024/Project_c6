#include "selfapp_internal.h"
#include "selfapp_cmd_def.h"
#include "selfapp_config.h"

#include <mem_manager.h>
#include <property_manager.h>
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif
#include <media_effect_param.h>
#include <media_player.h>

#if 1 //def CONFIG_C_EXTERNAL_DSP_ATS3615
	#include "../charge_6/src/external_dsp/ats3615/include/dolphin_com.h"
	extern int ext_dsp_set_eq_param(dolphin_eq_band_t * eq_bands, int bands_count);

	int ext_dsp_set_eq_by_app(u8_t id, u8_t pre_id, const u8_t *data, u8_t len);
#endif

typedef struct __attribute__((packed)) {
	u8_t id;		// PresetEQ_Band_Type_e
	u32_t gain;		// Band Gain value
	u32_t frequency;
	u32_t q_value;
} preseteq_band_t;

typedef struct __attribute__((packed)) {
	u8_t category;
	u8_t band_count;
	u32_t sample_rate;
	preseteq_band_t bands[0];
} preseteq_param_nti_t;

typedef struct {
	u8_t type;		// Band type
	s8_t level;		// Band scope level
} customeq_band_t;

typedef struct __attribute__((packed)) {
	u8_t category;
	u8_t level_scope;	// CustomEQ_Level_Scope_e
	u8_t band_count;
	customeq_band_t bands[0];
} customeq_param_nti_t;

static const u8_t eq_signature_data[EQ_DATA_SIZE] = 
{
0x00, 0x00, 0x00, 0x20, 0x62, 0xed, 0x6b, 0xc0, 0x9f, 0x77, 0x95, 0x1f, 0x9f, 0x12, 0x94, 0x3f,
0x62, 0x88, 0x6a, 0xe0, 0x00, 0x00, 0x00, 0x20, 0x92, 0x57, 0x6f, 0xc4, 0xef, 0x3c, 0x14, 0x1c,
0x6f, 0xa8, 0x90, 0x3b, 0x12, 0xc3, 0xeb, 0xe3, 0x00, 0x00, 0x00, 0x20, 0xb8, 0x9b, 0x43, 0xc9,
0x07, 0xb2, 0xaa, 0x18, 0x49, 0x64, 0xbc, 0x36, 0xfa, 0x4d, 0x55, 0xe7, 0x00, 0x00, 0x00, 0x20,
0xe9, 0xcc, 0x1a, 0xe4, 0x19, 0xb4, 0xd2, 0x0d, 0x18, 0x33, 0xe5, 0x1b, 0xe8, 0x4b, 0x2d, 0xf2,
0x00, 0x00, 0x00, 0x20, 0x38, 0x87, 0xd4, 0xf4, 0xf6, 0x26, 0x28, 0x0b, 0xc9, 0x78, 0x2b, 0x0b,
0x0b, 0xd9, 0xd7, 0xf4
};

int spkeq_RetNTIEQ(u8_t * buf)
{
	u8_t active_id, size;
	u16_t hdrlen = 0, eqlen = 0;
	customeq_param_nti_t *ntieq_ctm = NULL;

	size = sizeof(customeq_param_nti_t) +
	    BAND_COUNT_MAX * sizeof(customeq_band_t);
	ntieq_ctm = mem_malloc(size);
	if (ntieq_ctm == NULL) {
		return 0;
	}

	ntieq_ctm->category = EQCATEGORY_CUSTOM_1;	// fixedly need Custom category, not the actived
	selfapp_config_get_customer_eq(&ntieq_ctm->level_scope, (u8_t*)ntieq_ctm->bands);
	ntieq_ctm->band_count = BAND_COUNT_MAX;

	hdrlen = selfapp_get_header_len(CMD_NTI_EQ_Ret);

	active_id = selfapp_config_get_eq_id();
	eqlen += selfapp_pack_int(buf + hdrlen, active_id, 1);	// Firstly  pack actived category
	eqlen += selfapp_pack_bytes(buf + hdrlen + eqlen, (u8_t *) ntieq_ctm, size);	// Secondly pack Custom category
	eqlen += selfapp_pack_header(buf, CMD_NTI_EQ_Ret, eqlen);

	mem_free(ntieq_ctm);
	return eqlen;
}

int spkeq_SetNTIEQ(u8_t * param, u16_t plen)
{
	u8_t active_id = param[0];
	u8_t pre_id;

	selfapp_log_inf("id: 0x%x, len=%d\n", active_id, plen);

	pre_id = selfapp_config_get_eq_id();
	selfapp_config_set_eq(active_id, param+1, plen-1);

	// save CustomEQ parameters
	if (active_id == EQCATEGORY_CUSTOM_1) {
		customeq_param_nti_t *ntieq_ctm = (customeq_param_nti_t *)&(param[1]);
		selfapp_log_inf("custom 0x%x_%d_%d\n", ntieq_ctm->category,
				ntieq_ctm->level_scope, ntieq_ctm->band_count);
		selfapp_config_set_customer_eq(active_id, ntieq_ctm->band_count, ntieq_ctm->level_scope, (u8_t *)ntieq_ctm->bands);
	}

#if 1 //def CONFIG_C_EXTERNAL_DSP_ATS3615
	ext_dsp_set_eq_by_app(active_id, pre_id, param+1, plen-1);
#else
	selfapp_eq_cmd_switch_eq(active_id, pre_id, param+1, plen-1);
#endif

	return 0;
}

bool selfapp_eq_is_playtimeboost(void) 
{
	if (selfapp_config_get_eq_id() == 0x21) {
		return true;
	} else {
		return false;
	}
}

bool selfapp_eq_is_default(void) 
{
	if (selfapp_config_get_eq_id() == EQCATEGORY_DEFAULT) {
		return true;
	} else {
		return false;
	}
}

//EQ ID according to JBL app implenmentation.
static u8_t eq_id_table[6] = {
0x06, //JBL SIGNATURE
0x22, //CHILL
0x08, //ENERGETIC
0x03, //VOCAL
0xC1, //Customer
0x21, //PlaytimeBoost
};

u8_t selfapp_eq_get_index(void)
{
	u8_t i;
	u8_t id;

	id = selfapp_config_get_eq_id();

	for(i = 0; i < sizeof(eq_id_table); i++) {
		if(id == eq_id_table[i]) {
			break;
		}
	}

	if(i >= sizeof(eq_id_table)) {
		selfapp_log_wrn("Unexpected eq id 0x%x", id);
	}

	return i;
}

u8_t selfapp_eq_get_id_by_index(u8_t index)
{
	u8_t id;

	if(index >= sizeof(eq_id_table)) {
		selfapp_log_wrn("Unexpected eq index 0x%x", index);
		id = eq_id_table[0];
	} else {
		id = eq_id_table[index];
	}

	return id;
}

const u8_t* selfapp_eq_get_default_data(void)
{
	return eq_signature_data;
}


#if 1//def CONFIG_C_EXTERNAL_DSP_ATS3615

static void bytes_reverse(u8_t * dst, u8_t * src, int size)
{
	dst += size;
	for (int i=0; i<size; i++)
		*--dst = *src++;
}

#define MAX_APP_EQ_BANDS		5
int ext_dsp_set_eq_by_app(u8_t id, u8_t pre_id, const u8_t *data, u8_t len)
{
	int i;
	int bands_count;
	dolphin_eq_band_t bands[MAX_APP_EQ_BANDS] = {
		{64.0,    0.0, 1.0, DOLPHIN_EQ_TYPE_LS2},
		{250.0,   0.0, 1.0, DOLPHIN_EQ_TYPE_EQ2},
		{1000.0,  0.0, 1.0, DOLPHIN_EQ_TYPE_EQ2},
		{4000.0,  0.0, 1.0, DOLPHIN_EQ_TYPE_EQ2},
		{16000.0, 0.0, 1.0, DOLPHIN_EQ_TYPE_EQ2},
	};

	u8_t EQ_Filter_Table[] = {
		//   PresetEQ_Band_Type_e		dolphin_eq_type_t
		/* IIRFilter_LowShelf  = 0 */	DOLPHIN_EQ_TYPE_LS2,
		/* IIRFilter_Peaking   = 1 */	DOLPHIN_EQ_TYPE_EQ2,
		/* IIRFilter_HighShelf = 2 */	DOLPHIN_EQ_TYPE_HS2,
		/* IIRFilter_LowPass   = 3 */	DOLPHIN_EQ_TYPE_LP2,
		/* IIRFilter_HighPass  = 4 */	DOLPHIN_EQ_TYPE_HP2,
	};

	if (id == EQCATEGORY_CUSTOM_1) {
		customeq_param_nti_t * customeq = (customeq_param_nti_t *)data;
		bands_count = customeq->band_count;
		if (bands_count > MAX_APP_EQ_BANDS) {
			SYS_LOG_ERR(" bands_count = %d > %d ", bands_count, MAX_APP_EQ_BANDS);
			return -1;
		}

		// SYS_LOG_INF("customeq->category       = %d", customeq->category);
		// SYS_LOG_INF("customeq->level_scope    = %d", customeq->level_scope);
		// SYS_LOG_INF("customeq->band_count     = %d", customeq->band_count);
		// SYS_LOG_INF("customeq->bands[0].type    = %d", customeq->bands[0].type);
		// SYS_LOG_INF("customeq->bands[0].level   = %d", customeq->bands[0].level);

		for (i=0; i<bands_count; i++) {
			bands[i].gain = (float)customeq->bands[i].level;
		}
		ext_dsp_set_eq_param(&bands[0], bands_count);
	} else {
		preseteq_param_nti_t * preseteq = (preseteq_param_nti_t * )(data);
		bands_count = preseteq->band_count;
		if (bands_count > MAX_APP_EQ_BANDS) {
			SYS_LOG_ERR(" bands_count = %d > %d ", bands_count, MAX_APP_EQ_BANDS);
			return -1;
		}

		// SYS_LOG_INF("preseteq->category    = %d", preseteq->category);
		// SYS_LOG_INF("preseteq->band_count  = %d", preseteq->band_count);
		// SYS_LOG_INF("preseteq->sample_rate = %d", preseteq->sample_rate);
		// SYS_LOG_INF("preseteq->bands[0].id        = %d", preseteq->bands[0].id);
		// SYS_LOG_INF("preseteq->bands[0].gain      = %08x", preseteq->bands[0].gain);
		// SYS_LOG_INF("preseteq->bands[0].frequency = %08x", preseteq->bands[0].frequency);
		// SYS_LOG_INF("preseteq->bands[0].q_value   = %08x", preseteq->bands[0].q_value);

		for (i=0; i<bands_count; i++) {
			// bands[i].freq = bytes_reverse_32bit(preseteq->bands[i].frequency);
			// bands[i].gain = bytes_reverse_32bit(preseteq->bands[i].gain);
			// bands[i].q    = bytes_reverse_32bit(preseteq->bands[i].q_value);

			bytes_reverse((u8_t *)&(bands[i].freq), (u8_t *)&(preseteq->bands[i].frequency), 4);
			bytes_reverse((u8_t *)&(bands[i].gain), (u8_t *)&(preseteq->bands[i].gain), 4);
			bytes_reverse((u8_t *)&(bands[i].q),    (u8_t *)&(preseteq->bands[i].q_value), 4);

			u8_t band_type = preseteq->bands[i].id;
			if (band_type <= IIRFilter_HighPass)
				bands[i].type = EQ_Filter_Table[band_type];
			else
				bands[i].type = DOLPHIN_EQ_TYPE_BYP;

			// SYS_LOG_INF("bands[%d].frequency = %08x", i, (u32_t)bands[i].freq);
			// SYS_LOG_INF("bands[%d].gain      = %08x", i, (u32_t)bands[i].gain);
			// SYS_LOG_INF("bands[%d].q_value   = %08x", i, (u32_t)bands[i].q);
			// SYS_LOG_INF("bands[%d].type      = %d",   i, (u32_t)bands[i].type);
		}
		ext_dsp_set_eq_param(&bands[0], bands_count);
	}
	return 0;
}
#endif

int spkeq_SetCus(void)
{
	u8_t active_id = selfapp_config_get_eq_id();
	u8_t* param = selfapp_config_get_eq_data();
	return ext_dsp_set_eq_by_app(active_id, EQCATEGORY_DEFAULT, param, EQ_DATA_SIZE);
}


#include "selfapp_internal.h"
#include "selfapp_cmd_def.h"
#include "selfapp_config.h"

#ifdef SPEC_LED_PATTERN_CONTROL
#include <mem_manager.h>
#include <string.h>

enum LED_Movement_Speed_e {
	LEDMOVE_SPEED_LOW = 1,
	LEDMOVE_SPEED_MID,
	LEDMOVE_SPEED_HIGH,
};

enum LED_Package_ID_e {
	LEDP_Nature = 0x01,
	LEDP_Party,
	LEDP_Spiritual,
	LEDP_Weather,		// IOS App: Weather would be after Cocktail
	LEDP_Cocktail,

	LEDP_CANVAS = 0xc1,
};

enum LED_Pattern_ID_e {
	LEDPN_Nature_Campfire = 0x01,
	LEDPN_Nature_NorthernLights,
	LEDPN_Nature_SeaWave,
	LEDPN_Nature_Universe,

	LEDPN_Party_Strobe,	//0x05
	LEDPN_Party_Equalizer,
	LEDPN_Party_Geometry,
	LEDPN_Party_Spin,
	LEDPN_Party_Rainbow,

	LEDPN_Spiritual_DynamicWave,	//0x0A
	LEDPN_Spiritual_Lava,
	LEDPN_Spiritual_Focus,

	LEDPN_Weather_SkySunny,	//0x0D
	LEDPN_Weather_Rain,
	LEDPN_Weather_Snow,
	LEDPN_Weather_Storm,
	LEDPN_Weather_Cloud,
	LEDPN_Weather_Thunder,

	LEDPN_Cocktail_FruitGin,	//0x13
	LEDPN_Cocktail_Mojito,
	LEDPN_Cocktail_TakilaSunrise,
	LEDPN_Cocktail_CherryMargarita,
};

#define LEDPACKAGE_COUNT  (5)

#define LEDPKG_CANVAS_PATTERN_MAX (5)

typedef struct {
	u8_t id;		//LED_Package_ID_e
	u8_t actived;		//active pattern count
	u8_t total;		//total pattern count
	u8_t patterns[9];	//total pattern ids, the front "actived" ones are actived
} led_pattern_t;

typedef struct {
	led_pattern_t pn;
	u8_t color_effect;	//0 static color, 1 colorful(loop color)
	u8_t static_color[3];	//static mode: 1st Red, 2nd Green, 3rd Blue; colorful mode: previous static color
} led_package_t;

typedef struct __attribute__((packed)) {
	u8_t pkg_id;		//LEDP_CANVAS
	u8_t enabled;		//0 Disable/Remove this Canvas pattern, 1 Enable/Add
	u16_t pattern_id;
	u8_t patterns[462];	//appointed in protocol, none if enabled=0
} led_canvas_pattern_t;

typedef struct {
	u8_t id;		//LEDP_CANVAS
	u8_t actived;		//active pattern count, max 5
	u16_t patterns[LEDPKG_CANVAS_PATTERN_MAX];	//id in 2bytes
} led_canvas_package_t;

typedef struct {
	u8_t enable_notify;
	u8_t active_pkgid;
	u8_t reserved[2];

	led_package_t pkgs[LEDPACKAGE_COUNT];
	led_canvas_package_t canvas_pkg;
	led_canvas_pattern_t *canvas_pattern;	//need malloc
} ledpkg_handle_t;

static ledpkg_handle_t *ledpkg_get_handle(void)
{
	selfapp_context_t *selfctx = self_get_context();
	if (selfctx && selfctx->ledpkg) {
		return selfctx->ledpkg;
	}
	return NULL;
}

static int ledpkg_set_handle(ledpkg_handle_t * handle)
{
	selfapp_context_t *selfctx = self_get_context();
	if (selfctx) {
		selfctx->ledpkg = (void *)handle;
		return 0;
	}
	return -1;
}

static inline u8_t is_canvas(u8_t id)
{
	ledpkg_handle_t *ledpkg = ledpkg_get_handle();
	return ledpkg ? (id == ledpkg->canvas_pkg.id) : 0;
}

static led_package_t *find_package(u8_t pkg_id)
{
	u8_t i;
	ledpkg_handle_t *ledpkg = ledpkg_get_handle();
	if (ledpkg == NULL || is_canvas(pkg_id)) {
		return NULL;
	}

	for (i = 0; i < LEDPACKAGE_COUNT; i++) {
		if (ledpkg->pkgs[i].pn.id == pkg_id) {
			return &(ledpkg->pkgs[i]);
		}
	}

	return NULL;
}

static int pack_package(u8_t * buf, u8_t id)
{
	u16_t len, packlen = 0;
	led_package_t *pkg = NULL;

	if (is_canvas(id)) {
		ledpkg_handle_t *ledpkg = ledpkg_get_handle();
		if (ledpkg) {
			len =
			    sizeof(led_canvas_package_t) -
			    sizeof(ledpkg->canvas_pkg.patterns);
			len += ledpkg->canvas_pkg.actived * sizeof(u16_t);
			packlen +=
			    selfapp_pack_bytes(buf, (u8_t *) & (ledpkg->canvas_pkg), len);
		}
		return packlen;
	}

	pkg = find_package(id);
	if (pkg == NULL) {
		return 0;
	}

	len =
	    sizeof(led_pattern_t) - sizeof(pkg->pn.patterns) +
	    pkg->pn.total * sizeof(u8_t);
	packlen += selfapp_pack_bytes(buf, (u8_t *) & (pkg->pn), len);

	len = sizeof(led_package_t) - sizeof(led_pattern_t);
	packlen += selfapp_pack_bytes(buf + packlen, (u8_t *) & (pkg->color_effect), len);

	return packlen;
}

int ledpkg_EnableNotifyLedPatternInfo(u8_t enable)
{
	ledpkg_handle_t *ledpkg = ledpkg_get_handle();
	if (ledpkg) {
		selfapp_log_inf("EnableNotifyLedPatternInfo: %d->%d\n",
				ledpkg->enable_notify, enable);
		ledpkg->enable_notify = enable;
	}
	return 0;
}

int ledpkg_RetLedPackage(u8_t * buf, u8_t ids[], u8_t count)
{
	u8_t i, canvas_id = 0, pkg_count = 0;
	u8_t *count_ptr = NULL;
	u16_t hdrlen = 0, pkglen = 0;
	ledpkg_handle_t *ledpkg = ledpkg_get_handle();
	if (ledpkg == NULL) {
		selfapp_log_inf("pack ledpkg fail\n");
		return 0;
	}

	hdrlen = selfapp_get_header_len(LEDPCMD_RetLedPackageInfo);

	pkglen += selfapp_pack_int(buf + hdrlen, count, 1);
	pkglen += selfapp_pack_int(buf + hdrlen + pkglen, ledpkg->active_pkgid, 1);
	count_ptr = buf + hdrlen;

	for (i = 0; i < count; i++) {
		// pack canvas_package later
		if (ids[i] == LEDP_CANVAS) {
			canvas_id = LEDP_CANVAS;
			// selfapp_log_inf("canvas id 0x%x found\n", canvas_id);
			continue;
		}

		pkglen += pack_package(buf + hdrlen + pkglen, ids[i]);
		pkg_count++;
	}

	if (canvas_id) {
		// selfapp_log_inf("canvas packing\n");
		pkglen += pack_package(buf + hdrlen + pkglen, canvas_id);
		pkg_count++;
	}

	if (count_ptr) {
		*count_ptr = pkg_count;
	}

	pkglen += selfapp_pack_header(buf, LEDPCMD_RetLedPackageInfo, pkglen);
	return pkglen;
}

int ledpkg_SetLedPackage(u8_t * param, u16_t plen)
{
	u16_t len;
	led_package_t *oldpkg, *newpkg = (led_package_t *) param;
	if (plen !=
	    sizeof(led_package_t) - sizeof(newpkg->pn.patterns) +
	    newpkg->pn.total) {
		selfapp_log_inf("SetLedPackage: len not match %d,%d\n", plen,
				newpkg->pn.total);
	}

	selfapp_log_inf("ledpkg SetLedPackage 0x%x\n", newpkg->pn.id);
	oldpkg = find_package(newpkg->pn.id);
	if (oldpkg) {
		len =
		    sizeof(led_pattern_t) - sizeof(newpkg->pn.patterns) +
		    newpkg->pn.total;
		memcpy(oldpkg, newpkg, len);

		//pointer is mistaken because sizeof(pn.patterns) fixed
		oldpkg->color_effect = *(param + len);
		memcpy(oldpkg->static_color, param + len + 1,
		       sizeof(oldpkg->static_color));
	}

	return (int)(oldpkg->pn.id);
}

int ledpkg_NotifyLedPackageInfo(u8_t * buf, u8_t pkg_id)
{
	u16_t hdrlen = 0, packlen = 0;
	hdrlen = selfapp_get_header_len(LEDPCMD_NotifyLedPackageInfo);
	packlen += pack_package(buf + hdrlen, pkg_id);
	packlen += selfapp_pack_header(buf, LEDPCMD_NotifyLedPackageInfo, packlen);
	// selfapp_log_inf("NotifyLedPackageInfo:\n");
	// selfapp_log_dump(buf, packlen);
	return packlen;
}

int ledpkg_init(void)
{
	ledpkg_handle_t *ledpkg;
	u8_t idx;
	u32_t *color;

	if (ledpkg_get_handle() != NULL) {
		selfapp_log_inf("ledpkg inited\n");
		return -1;
	}

	ledpkg = mem_malloc(sizeof(ledpkg_handle_t));
	if (ledpkg == NULL) {
		selfapp_log_inf("ledpkg init nomem\n");
		return -1;
	}
	if (ledpkg_set_handle(ledpkg) != 0) {
		mem_free(ledpkg);
		return -1;
	}

	memset(ledpkg, 0, sizeof(ledpkg_handle_t));
	ledpkg->enable_notify = 0;
	ledpkg->active_pkgid = LEDP_Nature;

	ledpkg->pkgs[0].pn.id = LEDP_Nature;
	ledpkg->pkgs[0].pn.actived = 4;
	ledpkg->pkgs[0].pn.total = 4;
	color = (u32_t *) & (ledpkg->pkgs[0].color_effect);
	*color = 0x00000001;
	for (idx = 0; idx < ledpkg->pkgs[0].pn.total; idx++) {
		ledpkg->pkgs[0].pn.patterns[idx] = LEDPN_Nature_Campfire + idx;
	}

	ledpkg->pkgs[1].pn.id = LEDP_Party;
	ledpkg->pkgs[1].pn.actived = 5;
	ledpkg->pkgs[1].pn.total = 5;
	color = (u32_t *) & (ledpkg->pkgs[1].color_effect);
	*color = 0x0000FF00;
	for (idx = 0; idx < ledpkg->pkgs[1].pn.total; idx++) {
		ledpkg->pkgs[1].pn.patterns[idx] = LEDPN_Party_Strobe + idx;
	}

	ledpkg->pkgs[2].pn.id = LEDP_Spiritual;
	ledpkg->pkgs[2].pn.actived = 3;
	ledpkg->pkgs[2].pn.total = 3;
	color = (u32_t *) & (ledpkg->pkgs[2].color_effect);
	*color = 0x00FF0000;
	for (idx = 0; idx < ledpkg->pkgs[2].pn.total; idx++) {
		ledpkg->pkgs[2].pn.patterns[idx] =
		    LEDPN_Spiritual_DynamicWave + idx;
	}

	ledpkg->pkgs[3].pn.id = LEDP_Cocktail;
	ledpkg->pkgs[3].pn.actived = 4;
	ledpkg->pkgs[3].pn.total = 4;
	color = (u32_t *) & (ledpkg->pkgs[3].color_effect);
	*color = 0xFF000000;
	for (idx = 0; idx < ledpkg->pkgs[3].pn.total; idx++) {
		ledpkg->pkgs[3].pn.patterns[idx] =
		    LEDPN_Cocktail_FruitGin + idx;
	}

	ledpkg->pkgs[4].pn.id = LEDP_Weather;
	ledpkg->pkgs[4].pn.actived = 5;	//App could multi-select max 5
	ledpkg->pkgs[4].pn.total = 6;
	color = (u32_t *) & (ledpkg->pkgs[4].color_effect);
	*color = 0x00FFFF00;
	for (idx = 0; idx < ledpkg->pkgs[4].pn.total; idx++) {
		ledpkg->pkgs[4].pn.patterns[idx] = LEDPN_Weather_SkySunny + idx;
	}

	ledpkg->canvas_pattern = NULL;
	ledpkg->canvas_pkg.id = LEDP_CANVAS;
	ledpkg->canvas_pkg.actived = 0;
	for (idx = 0; idx < ledpkg->canvas_pkg.actived; idx++) {
		ledpkg->canvas_pkg.patterns[idx] = 0xcc;
	}

	selfapp_log_inf("ledpkg init\n");
	return 0;
}

int ledpkg_deinit(void)
{
	ledpkg_handle_t *ledpkg = ledpkg_get_handle();
	if (ledpkg) {
		mem_free(ledpkg);
		ledpkg_set_handle(NULL);
		selfapp_log_inf("ledpkg deinit\n");
	}

	return 0;
}

int cmdgroup_led_pattern_control(u8_t CmdID, u8_t * Payload, u16_t PayloadLen)
{
	int ret = -1;
	switch (CmdID) {
	case 0:{
			break;
		}
	default:{
			selfapp_log_wrn("NoCmd LedCtrl 0x%x\n", CmdID);
			break;
		}
	}			//switch
	return ret;
}

int cmdgroup_led_package(u8_t CmdID, u8_t * Payload, u16_t PayloadLen)
{
	u8_t *buf = self_get_sendbuf();
	u16_t sendlen = 0;
	int ret = -1;
	if (buf == NULL) {
		return ret;
	}

	switch (CmdID) {
	case LEDPCMD_ReqLedPackageInfo:{
			//0x83
			u8_t ids[] = { LEDP_Nature, LEDP_Party, LEDP_Spiritual,
				LEDP_Weather, LEDP_Cocktail
			};
			sendlen +=
			    ledpkg_RetLedPackage(buf, ids,
						 sizeof(ids) / sizeof(ids[0]));
			ret = self_send_data(buf, sendlen);
			break;
		}

	case LEDPCMD_SetLedPackage:{
			//0x85
			u8_t pkgid =
			    (u8_t) ledpkg_SetLedPackage(Payload, PayloadLen);
			sendlen += ledpkg_NotifyLedPackageInfo(buf, pkgid);
			ret = self_send_data(buf, sendlen);
			break;
		}

	case LEDPCMD_SetLedCanvasPackage:{
			//0x86
			break;
		}

	case LEDPCMD_EnableNotifyLedPatternInfo:{
			//0x89
			if (Payload) {
				ret =
				    ledpkg_EnableNotifyLedPatternInfo(Payload
								      [0]);
			}
			break;
		}

	case LEDPCMD_SetLedBrightness:
	case LEDPCMD_ReqLedBrightness:
	{
		u8_t brightness[3] = { 0x50, STA_ON, STA_ON };
		if (CmdID == LEDPCMD_SetLedBrightness && Payload) {
			selfapp_config_set_ledbrightness(Payload[0], Payload[1], Payload[2]);
		}
		selfapp_config_get_ledbrightness(&brightness[0], &brightness[1], &brightness[2]);
		selfapp_log_inf("LedBrightness: 0x%x_%d_%d\n",
				brightness[0], brightness[1], brightness[2]);
		sendlen +=
			selfapp_pack_cmd_with_bytes(buf, LEDPCMD_RetLedBrightness,
					brightness, sizeof(brightness));
		ret = self_send_data(buf, sendlen);
		break;
	}

	case LEDPCMD_SetLedMovementSpeed:
	case LEDPCMD_ReqLedMovementSpeed:{
			u8_t led_speed = LEDMOVE_SPEED_MID;
			if (CmdID == LEDPCMD_SetLedMovementSpeed && Payload) {
				selfapp_config_set_ledmovespeed(Payload[0]);
				led_speed = Payload[0];
			} else {
				led_speed = selfapp_config_get_ledmovespeed();
			}
			selfapp_log_inf("LedSpeed: %d\n", led_speed);
			sendlen +=
			    selfapp_pack_cmd_with_int(buf,
						 LEDPCMD_RetLedMovementSpeed,
						 led_speed, 1);
			ret = self_send_data(buf, sendlen);
			break;
		}

	case LEDPCMD_SwitchLedPackage:{
			//0x90
			if (Payload) {
				ledpkg_handle_t *ledpkg = ledpkg_get_handle();
				if (ledpkg) {
					selfapp_log_inf
					    ("SwitchLedPackage: %d->%d\n",
					     ledpkg->active_pkgid, Payload[0]);
					ledpkg->active_pkgid = Payload[0];
					sendlen +=
					    ledpkg_NotifyLedPackageInfo(buf,
									ledpkg->
									active_pkgid);
					ret = self_send_data(buf, sendlen);
				}
			}
			break;
		}

	case LEDPCMD_PreviewPattern:{
			//0x91
			if (Payload && PayloadLen == 2) {
				selfapp_log_inf("PreviePattern: 0x%x_%d\n",
						Payload[0], Payload[1]);
				// TODO: show light-show of the pattern
				sendlen +=
				    ledpkg_NotifyLedPackageInfo(buf,
								Payload[0]);
				ret = self_send_data(buf, sendlen);
			}
			break;
		}

	case LEDPCMD_PreviewCanvasPattern:{
			//0x92
			break;
		}

	default:
		sendlen = 0;
		break;
	}			//switch
	return ret;
}

#endif

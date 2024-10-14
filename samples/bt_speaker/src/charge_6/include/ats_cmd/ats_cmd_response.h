#ifndef __ATS_CMD_RESPONSE_H__
#define __ATS_CMD_RESPONSE_H__

#define ATS_CMD_RESP_OK    "SUCCESS"
#define ATS_CMD_RESP_FAIL  "FAIL"

#define ATS_CMD_RESP_COLOR_READ                         "TL_GET_COLOR="
#define ATS_CMD_RESP_GFPS_MODEL_ID_READ                 "TL_GET_MODEID="
#define ATS_CMD_RESP_READ_BT_DEV_NAME                   "TL_GET_BTNAME="
#define ATS_CMD_RESP_READ_BT_DEV_MAC_ADDR               "TL_GET_BTMAC="
#define ATS_CMD_RESP_VERSION_INFO_DUMP                  "TL_DUT_SW_VER=BT:"
#define ATS_CMD_RESP_BATTERY_LEVEL                      "TL_GET_BAT_LEVEL=BAT1:"
#define ATS_CMD_RESP_BATTERY_AD_VALUE                   "TL_GET_BAT_VOLTAGE=BAT1:"
#define ATS_CMD_RESP_BATTERY_CUR_VALUE                  "TL_GET_BAT_CHARGING_CV="
#define ATS_CMD_RESP_DISABLE_CHARGER                    "TL_SET_CHARGING_OFF="
#define ATS_CMD_RESP_READ_RSSI                          "TL_BT_RSSI="

#define ATS_CMD_RESP_PD_VERSION_INFO_DUMP               "TL_GET_CHG_PDVER="
#define ATS_CMD_RESP_ENTER_KEY_CHECK                    "AT+HMDevKEY="
#define ATS_CMD_RESP_NTC_TEMPERATURE_DUMP               "TL_GET_NTC=BAT1:"
#define ATS_CMD_RESP_ODM_DUMP                           "AT+HMDevODM="
#define ATS_CMD_RESP_MUSIC_BYPASS_ALLOW_FLAG_READ      "AT+HMDevbp="
#define ATS_CMD_RESP_MUSIC_BYPASS_STATUS_READ           "AT+HMDevbpmR="
#define ATS_CMD_RESP_SERIAL_NUMBER_READ                 "AT+HMDevSNRE="
#define ATS_CMD_RESP_MUSIC_PLAY_TIME_READ               "TL_READ_PLAYBACK_TIME="
#define ATS_CMD_RESP_POWER_ON_RUN_TIME_DUMP             "TL_READ_POWERON_TIM="

#define ATS_CMD_RESP_DSN_SET_ID                         "TL_SET_DSN="
#define ATS_CMD_RESP_PSN_SET_ID                         "TL_SET_PSN="
#define ATS_CMD_RESP_HW_VERSION_INFO_DUMP               "TL_DUT_HW_VER=BT:"
#define ATS_CMD_RESP_DSN_GET_ID                         "TL_GET_DSN="
#define ATS_CMD_RESP_PSN_GET_ID                         "TL_GET_PSN="
#define ATS_CMD_RESP_BT_STATUS_READ                     "TL_BT_ST="

#define ATS_CMD_RESP_SW_VERSION_INFO_DUMP               "TL_DUT_SW_VER=BT:"
#define ATS_CMD_RESP_HW_VERSION_INFO_DUMP               "TL_DUT_HW_VER=BT:"

#define ATS_CMD_RESP_DUT_SET                            "TL_ATS_OUT="
#define ATS_CMD_RESP_CLEAR_TIME                         "TL_TIM_RST="
#define ATS_CMD_RESP_DUT_REBOOT                         "TL_DUT_REBOOT="
#define ATS_CMD_RESP_RET_ANALYTICS                      "TL_RET_ANALYTICS="
#define ATS_CMD_RESP_SET_COLOR                          "TL_SET_COLOR="
#define ATS_CMD_RESP_SET_MODEID                         "TL_SET_MODEID="

#define ATS_CMD_RESP_LED_ALL_ON                         "TL_LED_ALL_ON="
#define ATS_CMD_RESP_LED_ALL_OFF                         "TL_LED_ALL_OFF="
#define ATS_CMD_RESP_LED_ON                             "TL_LED_ON_xx="
#define ATS_CMD_RESP_SET_CHARGE_LEVEL                   "TL_SET_CHG_LEVEL=XX"
#define ATS_CMD_RESP_SET_VOL                            "TL_SET_VOL=XX"
#define ATS_CMD_RESP_ENTER_PAIR                         "TL_ENTER_PAIR=XX"
#define ATS_CMD_RESP_APK_ALL_ON                         "TL_APK_ALL_ON="
#define ATS_CMD_RESP_APK_ALL_OFF                        "TL_APK_ALL_OFF="

#define ATS_CMD_RESP_KEY_TEST_IN                         "TL_KEY_IN="
#define ATS_CMD_RESP_KEY_READ                           "TL_KEY="
#define ATS_CMD_RESP_KEY_TEST_OUT                       "TL_KEY_OUT="

#define ATS_CMD_RESP_ENTER_AURACAST                      "TL_ENTER_AUROCAST="
#define ATS_CMD_RESP_EXIT_AURACAST                       "TL_EXIT_AUROCAST="

#define ATS_CMD_RESP_DUT_MODE_IN                         "TL_DUT_MODE_IN="
#define ATS_CMD_RESP_NOSIGTESTMODE_IN                    "TL_NOSIGTESTMODE_IN="

#define ATS_CMD_RESP_USER_DATA_READ                     "TL_CusRead="
#define ATS_CMD_RESP_USER_DATA_WRITE                    "TL_CUS_WRITE="

#define ATS_CMD_RESP_WATER_STATUS_READ                   "TL_GET_USB_WATER="

#define ATS_CMD_RESP_ENTER_ATS_MODULE         			"TL_ENTER_ATS_MODULE="
#define ATS_CMD_RESP_EXIT_ATS_MODULE          			"TL_EXIT_ATS_MODULE="

#define ATS_CMD_RESP_ENTER_SPK_BYPASS         			"TL_SPK_BYPASS="
#define ATS_CMD_RESP_EXIT_SPK_BYPASS          			"TL_SPK_BYPASS_OFF="

#define ATS_CMD_RESP_GET_USB_NTC_STATUS       			"TL_GET_USB_NTC="
#define ATS_CMD_RESP_TURN_ON_ONE_SPEAKER       			"TL_SPK_ON_XX="

#define ATS_CMD_RESP_MAC_SET                         	"TL_SET_MAC="

#define ATS_CMD_RESP_ENTER_STANDBY           			"TL_ENTER_STANDBY="
#define ATS_CMD_RESP_EXIT_STANDBY           			"TL_EXIT_STANDBY="

#define ATS_CMD_RESP_GET_CHIP_ID           				"TL_GET_CHIP_ID="
#define ATS_CMD_RESP_GET_LOGIC_VER           			"TL_GET_LOGIC_VER="
#define ATS_CMD_RESP_GET_ALL_VER           				"TL_DUT_ALL_VER="
#define ATS_CMD_RESP_RST_REBOOT	           				"TL_RST_REBOOT="
#define ATS_CMD_RESP_POWER_OFF                			"TL_POWREOFF="

#define ATS_CMD_RESP_SWITCH_DEMO	           			"TL_SWITCH_DEMO="
#define ATS_CMD_RESP_ENTER_DEMO	           				"TL_ENTER_DEMO="
#define ATS_CMD_RESP_EXIT_DEMO	           				"TL_EXIT_DEMO="
#define ATS_CMD_RESP_BAT_VOLTAGE_CURRENT     			"TL_GET_BAT_VOLTAGE_CURRENT="

#define ATS_CMD_RESP_VERIFY_UUID	           			"TL_VERIFY_UUID="
#define ATS_CMD_RESP_KEY_ALL_TEST_IN         			"TL_KEY_ALL_IN="
#define ATS_CMD_RESP_KEY_ALL_TEST_OUT         			"TL_KEY_ALL_OUT="
#define ATS_CMD_RESP_GET_KEY_ALL	        			"TL_KEY_ALL_GET="

#define ATS_CMD_RESP_GET_BAT_LEV_VOL        			"TL_GET_BAT_LEV_VOL="
#define ATS_CMD_RESP_GET_AURACAST_STATE	    			"TL_GET_AURACAST_ST="

#define ATS_CMD_RESP_ENTER_DEBUG_MODE        			"TL_ENTER_DEBUG_MODE="
#define ATS_CMD_RESP_EXIT_DEBUG_MODE	        		"TL_EXIT_DEBUG_MODE="


#endif

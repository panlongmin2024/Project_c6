#include <zephyr.h>
#include <device.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common/sys_reboot.h"
#include <mem_manager.h>
#include <property_manager.h>
#include <bt_manager.h>
#include "soft_version/soft_version.h"
#include <power_manager.h>
#include <board.h>
#include "audio_policy.h"
#include <hex_str.h>
#include <data_analy.h>
#include <fw_version.h>
#include <nvram_config.h>
#include <uart_stream.h>
#include <logging/sys_log.h>
#include <trace.h>
#include <gpio.h>
#include "board.h"
#include "ats_cmd/ats.h"
#include "hex_str.h"
#include <soc.h>


#ifdef CONFIG_WLT_ATS_ENABLE
#define CFG_WLT_INFO_0   			"WLT_INFO_0"
#define CFG_WLT_INFO_1   			"WLT_INFO_1"

#define ATS_WLT_THREAD_STACK_SZ		2048
#define ATS_WLT_THREAD_PRIO			5
#define	ATS_WLT_UART_RX_LEN_MAX		60
#define	ATS_WLT_UART_TX_LEN_MAX		60

/* wlt factory test command start */
#define ATS_AT_CMD_WLT_TAIL                    	"<CR><LF>"
#define ATS_AT_CMD_RESP_OK    					"SUCCESS"
#define ATS_AT_CMD_RESP_FAIL  					"FAILED"

/* cmd */
#define ATS_CMD_SET_BTEDR_MAC				"WLT_SET_BTEDR_MAC"
#define ATS_CMD_SET_BTBLE_MAC				"WLT_SET_BTBLE_MAC"
#define ATS_CMD_GET_BTEDR_MAC				"WLT_GET_BTEDR_MAC"
#define ATS_CMD_GET_BTBLE_MAC				"WLT_GET_BTBLE_MAC"
#define ATS_CMD_SET_BTEDR_NAME				"WLT_SET_BTEDR_NAME"
#define ATS_CMD_SET_BTBLE_NAME				"WLT_SET_BTBLE_NAME"
#define ATS_CMD_GET_BTEDR_NAME				"WLT_GET_BTEDR_NAME"
#define ATS_CMD_GET_BTBLE_NAME				"WLT_GET_BTBLE_NAME"
#define ATS_CMD_GET_FIRMWARE_VERSION        "WLT_GET_FIRMWARE_VER"
#define ATS_CMD_GPIO                		"WLT_TEST_GPIO"
#define ATS_CMD_ENTER_SIGNAL		        "WLT_ENTER_BT_SIGNAL"
#define ATS_CMD_ENTER_NON_SIGNAL	        "WLT_ENTER_BT_NON_SIGNAL"
#define ATS_CMD_ENTER_ADFU		            "WLT_ENTER_ADFU"
#define ATS_CMD_DEVICE_RESET		        "WLT_DEVICE_RESET"
#define ATS_CMD_ENTER_WLT_ATS	            "TL_ENTER_FAC_MODE_OK"

#define ATS_CMD_SET_HARMAN_KEY		        "WLT_HASH_UUID="
#define ATS_CMD_GET_IC_UUID			        "WLT_READ_UUID"

/* resp */
#define ATS_RESP_SET_BTEDR_MAC				"WLT_SET_BTEDR_MAC="
#define ATS_RESP_SET_BTBLE_MAC				"WLT_SET_BTBLE_MAC="
#define ATS_RESP_GET_BTEDR_MAC				"WLT_GET_BTEDR_MAC="
#define ATS_RESP_GET_BTBLE_MAC				"WLT_GET_BTBLE_MAC="
#define ATS_RESP_SET_BTEDR_NAME				"WLT_SET_BTEDR_NAME="
#define ATS_RESP_SET_BTBLE_NAME				"WLT_SET_BTBLE_NAME="
#define ATS_RESP_GET_BTEDR_NAME				"WLT_GET_BTEDR_NAME="
#define ATS_RESP_GET_BTBLE_NAME				"WLT_GET_BTBLE_NAME="
#define ATS_RESP_GET_FIRMWARE_VERSION       "WLT_GET_FIRMWARE_VER="
#define ATS_RESP_GPIO                		"WLT_TEST_GPIO="
#define ATS_RESP_ENTER_SIGNAL		        "WLT_ENTER_BT_SIGNAL="
#define ATS_RESP_ENTER_NON_SIGNAL	        "WLT_ENTER_BT_NON_SIGNAL="
#define ATS_RESP_ENTER_ADFU		            "WLT_ENTER_ADFU="
#define ATS_RESP_DEVICE_RESET		        "WLT_DEVICE_RESET="

#define ATS_RESP_SET_HARMAN_KEY		        "WLT_HASH_UUID="
#define ATS_RESP_CMD_GET_IC_UUID			"WLT_READ_UUID"

/* enter comm */
#define ATS_RESP_ENTER_WLT_ATS	            "TL_ENTER_FAC_MODE_OK"
#define ATS_SEND_ENTER_WLT_ATS	            "TL_ENTER_FAC_MODE<CR><LF>"
#define ATS_SEND_ENTER_WLT_ATS_ACK          "TL_ENTER_FAC_MODE_OKOK<CR><LF>"

/* wlt factory test command end */

struct _ats_wlt_var {
    os_mutex ats_mutex;
    uint8_t *ats_cmd_resp_buf;
    int ats_cmd_resp_buf_size;
    uint8_t ats_enable:1;
};

struct _wlt_driver_ctx_t {
	struct device *ats_uart_dev;

 	struct k_thread thread_data;
	u8_t *thread_stack;
	bool enabled;	
	u8_t thread_running;
	struct k_msgq msgq;
	char *msg_buf;
	struct thread_timer rx_timer;

	char data_buf[ATS_WLT_UART_RX_LEN_MAX];
};

typedef struct ats_wlt_uart {
    io_stream_t uio;
    u8_t uio_opened;
    char rx_buffer[ATS_WLT_UART_RX_LEN_MAX];
} ats_wlt_uart;

struct _ats_wlt_thread_msg_t {
    u8_t type;
    u8_t cmd;
    u32_t value;
};

typedef enum 
{
	WLT_ATS_ENTER_OK,
	WLT_ATS_EXIT,
}ats_wlt_type_e;

enum 
{
	ATS_WLT_RET_NG,
	ATS_WLT_RET_OK,
};



static inline void wlt_hex_to_string_4(u32_t num, u8_t *buf)
{
	buf[0] = '0' + num%10000/1000;
	buf[1] = '0' + num%1000/100;
	buf[2] = '0' + num%100/10;
	buf[3] = '0' + num%10;	
}
static inline void wlt_hex_to_string_2(u32_t num, u8_t *buf) {
	buf[0] = '0' + num%100/10;
	buf[1] = '0' + num%10;
}


#endif




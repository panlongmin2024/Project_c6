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
#define ATS_AT_CMD_SET_BTEDR_MAC				"WLT_SET_BTEDR_MAC"
#define ATS_AT_CMD_SET_BTBLE_MAC				"WLT_SET_BTBLE_MAC"
#define ATS_AT_CMD_GET_BTEDR_MAC				"WLT_GET_BTEDR_MAC"
#define ATS_AT_CMD_GET_BTBLE_MAC				"WLT_GET_BTBLE_MAC"
#define ATS_AT_CMD_SET_BTEDR_NAME				"WLT_SET_BTEDR_NAME"
#define ATS_AT_CMD_SET_BTBLE_NAME				"WLT_SET_BTBLE_NAME"
#define ATS_AT_CMD_GET_BTEDR_NAME				"WLT_GET_BTEDR_NAME"
#define ATS_AT_CMD_GET_BTBLE_NAME				"WLT_GET_BTBLE_NAME"
#define ATS_AT_CMD_GET_FIRMWARE_VERSION         "WLT_GET_FIRMWARE_VER"
#define ATS_AT_CMD_GPIO                			"WLT_TEST_GPIO"
#define ATS_AT_CMD_SET_HARMAN_KEY		        "WLT_SET_HARMAN_KEY"
#define ATS_AT_CMD_ENTER_SIGNAL		            "WLT_ENTER_BT_SIGNAL"
#define ATS_AT_CMD_ENTER_NON_SIGNAL	            "WLT_ENTER_BT_NON_SIGNAL"
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




#endif




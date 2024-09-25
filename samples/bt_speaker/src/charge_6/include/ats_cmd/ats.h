#ifndef __ATS_H__
#define __ATS_H__

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ats_at_cmd.h"
#include "ats_cmd_response.h"
#include "ats_complete_cmd.h"
#include "ats_complete_cmd_response.h"
#include <device.h>

#define ATS_SYS_REBOOT_DELAY_TIME_MS    (100)

int ats_cmd_response(uint8_t *buf, int size);
int ats_usb_cdc_acm_cmd_response(struct device *dev, uint8_t *buf, int size);
int ats_usb_cdc_acm_cmd_response_at_data(struct device *dev, u8_t *cmd, int cmd_len, u8_t *ext_data, int ext_data_len);

void ats_usb_cdc_acm_write_data(unsigned char *buf, int len);

void enter_ats(void);

int ats_init(void);
bool ats_is_enable(void);
void ats_set_enable(bool en);
int ats_sys_reboot(void);
int ats_sys_power_off(void);
int ats_color_write(uint8_t *buf, int size);
int ats_color_read(uint8_t *buf, int size);
int get_color_id_from_ats(u8_t *color_id);
int ats_gfps_model_id_write(uint8_t *buf, int size);
int ats_gfps_model_id_read(uint8_t *buf, int size);
int ats_dsn_write(uint8_t *buf, int size);
int ats_dsn_read(uint8_t *buf, int size);
int ats_psn_write(uint8_t *buf, int size);
int ats_psn_read(uint8_t *buf, int size);
int get_gfp_model_id_from_ats(u8_t *model_id);
int ats_bt_dev_name_read(uint8_t *buf, int size);
int ats_bt_dev_mac_addr_read(uint8_t *buf, int size);
int ats_enter_key_check(bool enable);
bool ats_get_enter_key_check_record(void);
int ats_usb_cdc_acm_key_check_report(struct device *dev, uint32_t key_value);
int ats_music_bypass_allow_flag_write(uint8_t *buf, int size);
int ats_music_bypass_allow_flag_read(uint8_t *buf, int size);
int ats_music_bypass_allow_flag_update(void);
bool ats_get_music_bypass_allow_flag(void);
int ats_serial_number_write(uint8_t *buf, int size);
int ats_serial_number_read(uint8_t *buf, int size);

int ats_usb_cdc_acm_init(void);
int ats_usb_cdc_acm_deinit(void);

struct _ats_usb_cdc_acm_thread_msg_t {
    u8_t type;
    u8_t cmd;
    u32_t value;
};

struct k_msgq *get_ats_usb_cdc_acm_thread_msgq(void);

int ats_module_test_mode_write(uint8_t *buf, int size);
int ats_module_test_mode_read(uint8_t *buf, int size);

int ats_usb_cdc_acm_shell_command_handler(struct device *dev, u8_t *buf, int size);
int ats_mac_write(uint8_t *buf, int size);

bool ats_get_pwr_key_pressed(void);
void ats_set_pwr_key_pressed(bool pressed);

#endif

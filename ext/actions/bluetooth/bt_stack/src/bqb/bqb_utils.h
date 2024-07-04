/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bqb utils functions
 */

#include <os_common_api.h>
#include <acts_bluetooth/host_interface.h>
#include "common/log.h"


int bqb_utils_command_handler(int argc, char *argv[]);

int bqb_utils_strlen(const char *s);
int bqb_utils_atoi(const char *s);
int bqb_utils_strtoul(const char *nptr, char **endptr, int base);
void bqb_utils_char2hex(char c, uint8_t *x);
void bqb_utils_str2hex(char *pbDest, char *pszSrc, int nLen);
int bqb_utils_hex2str(char *pszDest, char *pbSrc, int nLen);
void bqb_utils_bytes_reverse(uint8_t *dst, uint8_t *src, uint8_t len);

void bqb_utils_print_addr(uint8_t val[6], bool revert);

uint8_t bqb_utils_read_public_addr(bt_addr_le_t *addr);
uint8_t bqb_utils_get_local_public_addr(bt_addr_le_t *addr);

int bqb_utils_write_pts_addr(char* pts_addr);

int bqb_utils_read_pts_addr(bt_addr_t* addr);


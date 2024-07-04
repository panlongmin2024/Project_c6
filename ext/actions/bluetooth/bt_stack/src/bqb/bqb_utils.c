/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief utils for bqb test.
 */

#include <os_common_api.h>
#include <acts_bluetooth/host_interface.h>
#include <acts_bluetooth/addr.h>

#include <soc_dvfs.h>
#include <hex_str.h>
#include <stdlib.h>

#include "bqb_utils.h"
#include "bqb_gap.h"

#include "property_manager.h"

#ifdef CONFIG_PROPERTY

//#define BQB_UTILS_PROPERTY_PTS_ADDR
#ifdef BQB_UTILS_PROPERTY_PTS_ADDR
#define CFG_BT_PTS_ADDR     "BQB_TEST_PTS_ADDR"
#endif
#endif

static bt_addr_t  s_pts_addr = {0};

static bt_addr_le_t  bqb_utils_local_le_addr = {0};

extern uint8_t bt_id_read_public_addr(bt_addr_le_t *addr);
extern void bqb_core_init(void);

uint8_t bqb_utils_get_local_public_addr(bt_addr_le_t *addr)
{
    int ret = -1;

    if (addr) {
        memcpy((void *)addr, (const void *)(&bqb_utils_local_le_addr), sizeof(bt_addr_le_t));
        ret = 0;
    }
    return ret;
}

uint8_t bqb_utils_read_public_addr(bt_addr_le_t *addr)
{
    bt_id_read_public_addr(&bqb_utils_local_le_addr);
    bqb_utils_print_addr(bqb_utils_local_le_addr.a.val, false);

    if (addr) {
        memcpy((void *)addr, (const void *)(&bqb_utils_local_le_addr), sizeof(bt_addr_le_t));
    }

    return 0;
}

void bqb_utils_print_addr(u8_t val[6], bool revert)
{
    if (revert) {
        printk("BDA:0x%x:%x:%x:%x:%x:%x\n", val[5],val[4],val[3],val[2],val[1],val[0]);
    } else {
        printk("BDA:0x%x:%x:%x:%x:%x:%x\n", val[0],val[1],val[2],val[3],val[4],val[5]);
    }
}

int bqb_utils_strlen(const char *s)
{
    return (int)strlen(s);
}

void bqb_utils_char2hex(char c, uint8_t *x)
{
    char2hex(c, x);
}

void bqb_utils_str2hex(char *pbDest, char *pszSrc, int nLen)
{
    str_to_hex(pbDest, pszSrc, nLen);
}

int bqb_utils_hex2str(char *pszDest, char *pbSrc, int nLen)
{
    return (int)hex_to_str(pszDest, pbSrc, nLen);
}

int bqb_utils_atoi(const char *s)
{
    return atoi(s);
}

int bqb_utils_strtoul(const char *nptr, char **endptr, int base)
{
    return strtoul(nptr, endptr, base);
}

void bqb_utils_bytes_reverse(uint8_t *dst, uint8_t *src, uint8_t len)
{
    uint8_t i;
    if ( ((dst < src) && (dst + len <= src))
        || ((dst > src) && (dst - len >= src)) )
    {
        for (i = 0; i < len; i++)
        {
            dst[i] = src[len - 1 - i];
        }
    }
    else if (dst == src)
    {
        for (i = 0; i < len/2; i++)
        {
            dst[i] ^= src[len - 1 - i];
            src[len - 1 - i]  ^= dst[i];
            dst[i] ^= src[len - 1 - i];
        }
    }
    else
    {
        //QA
    }
}

static void bqb_utils_get_bd_addr(void)
{
    bt_addr_t bd_addr ={0};
    char add_str[13] = {0};
#ifdef CONFIG_PROPERTY
    property_get_byte_array(CFG_BT_MAC, bd_addr.val, sizeof(bt_addr_t), NULL);
#endif
    bqb_utils_hex2str(add_str, (char *)bd_addr.val, 6);
    printk("\n|---------------------------------------------------|");
    printk("\n| IUT BDA: %s", add_str);
    printk("\n| Pls input to the <TSPX_bd_addr_iut> of the PTS UI.|");
    printk("\n|---------------------------------------------------|");
    printk("\n");
}

void bqb_test_enable(bool enable)
{
    if (enable) {
     #ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
        soc_dvfs_set_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "bqb test");
#endif
        bqb_core_init();
    } else {
    #ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
        soc_dvfs_unset_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "bqb test");
    #endif
    }
}
int bqb_utils_write_pts_addr(char* pts_addr)
{
    int ret = 0;
    uint8_t temp_addr[6] = {0};
    bt_addr_t addr = {0};

    bqb_utils_str2hex(temp_addr, pts_addr, 6);
    bqb_utils_bytes_reverse(addr.val, temp_addr, 6);
    bqb_utils_print_addr(addr.val, true);

#ifdef BQB_UTILS_PROPERTY_PTS_ADDR
    ret = property_set(CFG_BT_PTS_ADDR, addr.val, sizeof(bt_addr_t));
    ret = property_flush(CFG_BT_PTS_ADDR);
#endif

    memcpy(&s_pts_addr, &addr, sizeof(bt_addr_t));
	return ret;
}

int bqb_utils_read_pts_addr(bt_addr_t* addr)
{
	int ret = 0;
	if (!addr) {
		BT_ERR("the read buff is null!");
		return -1;
	}
#ifdef BQB_UTILS_PROPERTY_PTS_ADDR
    ret = property_get(CFG_BT_PTS_ADDR, (char*)&s_pts_addr.val, sizeof(bt_addr_t));
#endif

    memcpy(addr, &s_pts_addr, sizeof(bt_addr_t));
	return ret;
}


int bqb_utils_command_handler(int argc, char *argv[])
{
    char *cmd = NULL;

    if (argc < 2) {
        return -EINVAL;
    }

    cmd = argv[1];

    if (!strcmp(cmd, "getbda")) {
        bqb_utils_get_bd_addr();
    } else if (!strcmp(cmd, "pts-on")) {
        bqb_test_enable(true);
    } else if (!strcmp(cmd, "pts-off")) {
        bqb_test_enable(false);
    } else if (!strcmp(cmd, "setptsbda")) {
        bqb_utils_write_pts_addr(argv[2]);
    } else if (!strcmp(cmd, "getptsbda")) {
        bt_addr_t addr = {0};
        bqb_utils_read_pts_addr(&addr);
        bqb_utils_print_addr(addr.val,false);
    } else {
        printk("invalid parameters\n");
    }

    return 0;
}


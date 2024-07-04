/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <zephyr.h>
#include <device.h>
#include <stdbool.h>
#include <errno.h>
#include <stddef.h>
#include <misc/util.h>
#include <net/buf.h>
#include <net/net_if.h>
#include <net/net_core.h>
#include <net/ethernet.h>
#include <net/net_mgmt.h>
#include "sdio_io.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SDIO_IO

#define SDIO_WIFI_BLK_SIZE	512
struct sdio_func *wifi_sdio_func = NULL;
static sdio_irq_handler_t *sdio_handle = NULL;

static int sdio_bus_probe(void)
{
	int ret = 0;

	ret = sdio_bus_scan(0);
	if (ret)
	{
		ACT_LOG_ID_INF(ALF_STR_sdio_bus_probe__CANNOT_FOUND_DEVICE_, 0);
		return ret;
	} else {
		ACT_LOG_ID_INF(ALF_STR_sdio_bus_probe__FOUND_WIFI_DEVICEN, 0);
	}

	wifi_sdio_func = sdio_get_func(0);
	if (wifi_sdio_func == NULL)
	{
		ACT_LOG_ID_INF(ALF_STR_sdio_bus_probe__SDIO_GET_FUNC_WIFI_S, 0);
		return -1;
	}

	sdio_claim_host(wifi_sdio_func);
	ret = sdio_set_block_size(wifi_sdio_func, SDIO_WIFI_BLK_SIZE);
	sdio_release_host(wifi_sdio_func);
	if (ret)
	{
		ACT_LOG_ID_INF(ALF_STR_sdio_bus_probe__SDIO_SET_BLOCK_SIZE_, 1, ret);
		return ret;
	}

	sdio_claim_host(wifi_sdio_func);
	ret = sdio_enable_func(wifi_sdio_func);
	sdio_release_host(wifi_sdio_func);
	if (ret)
	{
		ACT_LOG_ID_INF(ALF_STR_sdio_bus_probe__SDIO_ENABLE_FUNC_RET, 1, ret);
		return ret;
	}

	return 0;
}

static int sdio_bus_remove(void)
{
	printk("%s,%d, do nothing\n", __func__, __LINE__);
	return 0;
}

static uint16_t sdio_readw(struct sdio_func *func, uint32_t addr, int *err_ret)
{
	u8_t buf[2];
	uint16_t val = 0;

	memset(buf, 0, 2);
	sdio_memcpy_fromio(func, buf, addr, 2);
	val = buf[0] | (buf[1] << 8);

	return val;
}

static uint32_t sdio_readl(struct sdio_func *func, uint32_t addr, int *err_ret)
{
	u8_t buf[4];
	uint32_t val = 0;

	memset(buf, 0, 4);
	sdio_memcpy_fromio(func, buf, addr, 4);
	val = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
	return val;
}

static void sdio_writew(struct sdio_func *func, uint16_t val, uint32_t addr, int *err_ret)
{
	sdio_memcpy_toio(func, addr, &val, 2);
}

static void sdio_writel(struct sdio_func *func, uint32_t val, uint32_t addr, int *err_ret)
{
	sdio_memcpy_toio(func, addr, &val, 4);
}

static void sdio_hander_wrapper(struct sdio_func *func)
{
	if (sdio_handle) {
		sdio_claim_host(func);
		sdio_handle(func);
		sdio_release_host(func);
	}
}

static int sdio_claim_irq_wrapper(struct sdio_func *func, sdio_irq_handler_t *handler)
{
	sdio_handle = handler;
	return sdio_claim_irq(func, sdio_hander_wrapper);
}

static int sdio_release_irq_wrapper(struct sdio_func *func)
{
	sdio_release_irq(func);
	sdio_handle = NULL;
	return 0;
}

SDIO_BUS_OPS rtw_sdio_bus_ops = {
	sdio_bus_probe,		//int (*bus_probe)(void);
	sdio_bus_remove,	//int (*bus_remove)(void);
	sdio_enable_func,	//int (*enable)(struct sdio_func*func);
	sdio_disable_func,	//int (*disable)(struct sdio_func *func);
	NULL,	//int (*reg_driver)(struct sdio_driver*);		//  Not used
	NULL,	//void (*unreg_driver)(struct sdio_driver *);	//  Not used
	sdio_claim_irq_wrapper,	//int (*claim_irq)(struct sdio_func *func, void(*handler)(struct sdio_func *));
	sdio_release_irq_wrapper,	//int (*release_irq)(struct sdio_func*func);
	sdio_claim_host,	//void (*claim_host)(struct sdio_func*func);
	sdio_release_host,	//void (*release_host)(struct sdio_func *func);
	sdio_readb,	//unsigned char (*readb)(struct sdio_func *func, unsigned int addr, int *err_ret);
	sdio_readw,	//unsigned short (*readw)(struct sdio_func *func, unsigned int addr, int *err_ret);
	sdio_readl,	//unsigned int (*readl)(struct sdio_func *func, unsigned int addr, int *err_ret);
	sdio_writeb,	//void (*writeb)(struct sdio_func *func, unsigned char b,unsigned int addr, int *err_ret);
	sdio_writew,	//void (*writew)(struct sdio_func *func, unsigned short b,unsigned int addr, int *err_ret);
	sdio_writel,	//void (*writel)(struct sdio_func *func, unsigned int b,unsigned int addr, int *err_ret);
	sdio_memcpy_fromio,	//int (*memcpy_fromio)(struct sdio_func *func, void *dst,unsigned int addr, int count);
	sdio_memcpy_toio	//int (*memcpy_toio)(struct sdio_func *func, unsigned int addr,void *src, int count);
};


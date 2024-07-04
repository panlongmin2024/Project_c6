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
#include <kernel.h>
#include <device.h>
#include <gpio.h>
#include <board.h>
#include <stdbool.h>
#include <errno.h>
#include <stddef.h>
#include <misc/util.h>
#include <net/buf.h>
#include <net/net_if.h>
#include <net/net_core.h>
#include <net/ethernet.h>
#include <net/net_mgmt.h>
#include "../sdio/sdio_io.h"
#include "../osdep/net_intf.h"
#include "wifi_conf.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_RTL8189FTV


#define DEBUG_RTL8189FTV_API	1

#ifdef DEBUG_RTL8189FTV_API
#define RTL8189FTV_DEBUG(fmt, ...) printk("[%s,%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define RTL8189FTV_DEBUG(fmt, ...)
#endif

#define RTL8189FTV_ERR(fmt, ...) printk("[ERR:%s,%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)

extern void* os_frame_alloc(int32_t timeout);
extern void os_frame_free(void *frame);

struct rtl8189ftv_context {
	struct net_if *iface;
	uint8_t mac_addr[6];
	struct net_linkaddr ll_addr;
};

static struct rtl8189ftv_context rtl8189ftv_context_data;
static bool	wifi_net_ready = false;

static int rtl8189ftv_send(struct net_if *iface, struct net_pkt *pkt)
{
	uint32_t len;
	uint8_t *data;
	char *frame;

	if (!wifi_net_ready) {
		return -EIO;
	}

	len = net_pkt_ll_reserve(pkt) + net_pkt_get_len(pkt);
	if (!pkt->frags) {
		/* No data? */
		return -ENODATA;
	}

	data = net_pkt_ll(pkt);

	/* 分配buf在这里copy数据，预留头，wifi driver可以直接使用frame   */
	frame = (char *)os_frame_alloc(K_FOREVER);
	if (!frame) {
		/* Tx thread will unref net buf */
		return -ENOMEM;
	}

	memcpy(&frame[ETHERNET_WIFI_RESERVE], data, len);
	if (netif_tx(0, &frame[ETHERNET_WIFI_RESERVE], len)) {
		os_frame_free(frame);
		return -EIO;
	}
	net_pkt_unref(pkt);

	/* 如果wifi driver 零copy，wifi driver用完需要调用os_frame_free(frame) 释放，否则这里需要释放 */
#ifdef CONFIG_TX_ZERO_COPY
	/*rltk_wlan_sec函数用来获取加密方式 如果加密方式不是open和AES，就不进行零拷贝 */
	//if (rltk_wlan_sec(0) != 0x0 && rltk_wlan_sec(0) != 0x4){
	if (false) {
		os_frame_free(frame);
	}
#else
	os_frame_free(frame);
#endif
	return 0;
}

int rtl8189ftv_recv_cb(struct net_pkt *pkt)
{
	struct rtl8189ftv_context *rtl8189ftv = &rtl8189ftv_context_data;

	if (!wifi_net_ready) {
		RTL8189FTV_ERR("iface is down\n");
		net_pkt_unref(pkt);
		return 0;
	}

	if (rtl8189ftv->iface) {
		if (net_recv_data(rtl8189ftv->iface, pkt) != 0) {
			net_pkt_unref(pkt);
		}
	} else {
		RTL8189FTV_ERR("iface if NULL!\n");
		net_pkt_unref(pkt);
	}
	return 0;
}

void rtl8189ftv_set_netif_info(int idx_wlan, void *dev, unsigned char *dev_addr)
{
	struct rtl8189ftv_context *rtl8189ftv = &rtl8189ftv_context_data;
	struct net_linkaddr *ll_addr = &rtl8189ftv->ll_addr;

	memcpy(rtl8189ftv->mac_addr, dev_addr, 6);
	ll_addr->addr = rtl8189ftv->mac_addr;
	ll_addr->len = 6;

	if (rtl8189ftv->iface) {
		net_if_set_link_addr(rtl8189ftv->iface, ll_addr->addr, ll_addr->len,
					NET_LINK_ETHERNET);
	} else {
		RTL8189FTV_ERR("iface not initialize ERROR!\n");
	}
}

int rtl8189ftv_is_valid_IP(int idx, unsigned char *ip_dest)
{
	struct rtl8189ftv_context *rtl8189ftv = &rtl8189ftv_context_data;

	if (!rtl8189ftv->iface) {
		RTL8189FTV_ERR("iface is NULL!\n");
		return 0;
	}

	/* Just treat valid ip, tcp/ip stack will check ip address */
	return 1;
}

unsigned char *rtl8189ftv_get_ip(int idx)
{
	struct rtl8189ftv_context *rtl8189ftv = &rtl8189ftv_context_data;
	struct net_if *iface;

	if (!rtl8189ftv->iface) {
		RTL8189FTV_ERR("ERROR!\n");
		return NULL;
	}

	iface = rtl8189ftv->iface;
	return iface->ipv4.unicast[0].address.in_addr.s4_addr;
}

static void rtl8189ftv_iface_init(struct net_if *iface)
{
	struct rtl8189ftv_context *rtl8189ftv = net_if_get_device(iface)->driver_data;

	if (rtl8189ftv) {
		RTL8189FTV_DEBUG("\n");
		rtl8189ftv->iface = iface;
	} else {
		RTL8189FTV_ERR("error\n");
	}
}

static struct net_if_api rtl8189ftv_if_api = {
	.init = rtl8189ftv_iface_init,
	.send = rtl8189ftv_send,
};

void rtl8189ftv_dhcpc_start(void)
{
	struct rtl8189ftv_context *rtl8189ftv = &rtl8189ftv_context_data;

	wifi_net_ready = true;
	if (rtl8189ftv->iface) {
		RTL8189FTV_DEBUG("\n");
		net_if_up(rtl8189ftv->iface);
		net_dhcpv4_start(rtl8189ftv->iface);
	} else {
		RTL8189FTV_ERR("iface is NULL!\n");
	}
}

void rtl8189ftv_dhcpc_stop(void)
{
	struct rtl8189ftv_context *rtl8189ftv = &rtl8189ftv_context_data;

	wifi_net_ready = false;
	if (rtl8189ftv->iface) {
		RTL8189FTV_DEBUG("\n");
		net_if_down(rtl8189ftv->iface);
		net_dhcpv4_stop(rtl8189ftv->iface);
	} else {
		RTL8189FTV_ERR("iface is NULL!\n");
	}
}

static void rtl8189ftv_wifi_power_on(void)
{
	struct device *wifi_gpio_port;

	wifi_gpio_port = device_get_binding(BOARD_WIFI_POWER_EN_GPIO_NAME);
	if (!wifi_gpio_port) {
		ACT_LOG_ID_INF(ALF_STR_rtl8189ftv_wifi_power_on__CANNOT_FOUND_DEV_GPI, 0);
		return;
	}

	gpio_pin_configure(wifi_gpio_port, BOARD_WIFI_POWER_EN_GPIO, GPIO_DIR_OUT);

	gpio_pin_write(wifi_gpio_port, BOARD_WIFI_POWER_EN_GPIO, 0);
	k_sleep(10);
	gpio_pin_write(wifi_gpio_port, BOARD_WIFI_POWER_EN_GPIO, 1);
	k_sleep(50);
}

static int rtl8189ftv_init(struct device *dev)
{
	rtl8189ftv_wifi_power_on();

	if (0 == rtw_sdio_bus_ops.bus_probe()) {
		/* Call this in shell command for test */
		rtl8189ftv_context_data.iface = net_if_get_default();
		wifi_on(RTW_MODE_STA);
	} else {
		RTL8189FTV_ERR("Can't scan wifi device!\n");
	}
	return 0;
}

NET_DEVICE_INIT(rtl8189ftv, "rtl8189ftv", rtl8189ftv_init, &rtl8189ftv_context_data,
		NULL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &rtl8189ftv_if_api,
		ETHERNET_L2, NET_L2_GET_CTX_TYPE(ETHERNET_L2), 1500);


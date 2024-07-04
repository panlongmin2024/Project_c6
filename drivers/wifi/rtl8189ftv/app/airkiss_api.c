/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <kernel.h>
#include <shell/shell.h>
#include <stdlib.h>
#include <limits.h>
#include <misc/printk.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <mem_manager.h>
#include <net/buf.h>
#include <net/net_if.h>
#include <net/net_core.h>
#include <net/udp.h>
#include <net/ethernet.h>

#include "wifi_conf.h"
#include "wifi_api.h"
#include "../osdep/net_intf.h"
#include "airkiss.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_AIRKISS_API
#ifdef CONFIG_SWITCH_APPFAMILY
#include <soc_reboot.h>
#endif

#define AIRKISS_CHANGE_CHANNEL_INTERVAL		200		/* 200---100ms */
#define AIRKISS_COMPLETE_TIME_CNT			300		/* 300---30s */


/******** customer optional function **************/
// If not recv target beacon when CHANNEL_LOCKED informed: 1-switch channel until target channel; 0-stop switching channel.
#define AIRKISS_LOCK_EXACT_CHNL			1
// 1:pre-filter eligible 80211 packet before handling; 0: not pre-filter
#define AIRKISS_PKT_PRE_FILTER			1
// 1:pre-handle airkiss ap guide pkt to lock channel faster; 0: not pre-handle
#define AIRKISS_GUIDE_PRE_HANDLE		1

//macros
#define CHANNEL_SWITCH_TIME 100	//channel switch time,units:ms

//add for switch channel again
#define MAX_GUIDE_NUM 4
struct guide_ap_t
{
	struct guide_ap_t *next;
	unsigned char bssid[6];
	unsigned char buf[MAX_GUIDE_NUM][100];  //save guide packets
	unsigned char map[MAX_GUIDE_NUM][2];  //save packet index in buf[][] to map[][0] and packet len to map[][1], the array is ordered by packet len from small to large
	unsigned char cnt;  //num of validated guide packets
};

extern struct guide_ap_t *airkiss_guide_ap_list;
extern void guide_ap_packet_delete_all(struct guide_ap_t *ap);
extern void wifi_mem_free(void *ptr);
extern void *wifi_mem_malloc(unsigned int num_bytes);
extern int promisc_get_fixed_channel(void *fixed_bssid,u8 *ssid,int *ssid_length);

uint8_t airkiss_fix_flag;
#if AIRKISS_PKT_PRE_FILTER
uint8_t airkiss_fix_rx_cnt;
#endif
#if AIRKISS_PROMISC_DIG_ENABLE
uint32_t airkiss_rx_data_cnt;
#endif

u8 gbssid[ETH_ALEN];
uint32_t bcn_ssid_len;
static u8 security_type;


static u8_t airkiss_run = 0;
static u8_t airkiss_state;
static u8_t change_channel_work_runing = 0;
static u8_t promisc_cb_runing = 0;
static u16_t airkiss_random = 0xFFFF;
static u16_t airkiss_lock_channel_cnt;
static airkiss_context_t *akcontex;
static struct k_delayed_work change_channel_work;

extern void wifi_enter_promisc_mode();
extern void wifi_airkiss_event_cb(u8_t *ssid, u8_t ssid_len, u8_t *pwd);
extern u16_t net_calc_chksum_ipv4(struct net_pkt *buf);
extern u16_t net_calc_chksum(struct net_pkt *buf, u8_t proto);
///add by may

u8 lock_channel;
uint32_t channel_index;
#define MAX_CHANNEL_LIST_NUM  17
uint8_t airkiss_channel_list[MAX_CHANNEL_LIST_NUM]=
{ 1, 2, 3, 0, 4, 5, 6, 0, 7, 8, 9, 0, 10, 11, 12, 0, 13 };

static u8 relock_flag; //用来判断信道是否已经锁定   1：锁定 0：未锁定


#if AIRKISS_PKT_PRE_FILTER
extern int airkiss_pre_filter_pkt(u8 *, u32, ieee80211_frame_info_t *);
#endif

#if AIRKISS_GUIDE_PRE_HANDLE
extern void guide_ap_list_free();
extern int airkiss_guide_prehandle(u8 *, u32 , ieee80211_frame_info_t *);
#endif

#if AIRKISS_PROMISC_DIG_ENABLE
extern void promisc_update_candi_ap_rssi_avg(s8 , u8);
#endif

static struct net_pkt *prepare_udp(void* data, u32 len, u32 srcip, u16 srcport, u32 dstip, u16 dstport)
{
	struct net_if *iface;
	struct net_pkt *pkt;
	struct net_buf *frag;
	struct net_ipv4_hdr *ipv4;
	struct net_udp_hdr hdr, *udp;
	struct in_addr tmpaddr;
	uint16_t tmplen;

	iface = net_if_get_default();

	pkt = net_pkt_get_reserve_tx(0, K_FOREVER);
	if (!pkt) {
		return NULL;
	}

	frag = net_pkt_get_reserve_tx_data(net_if_get_ll_reserve(iface, NULL),
					 K_FOREVER);
	if (!frag) {
		net_pkt_unref(pkt);
		return NULL;
	}

	net_pkt_set_ll_reserve(pkt, net_buf_headroom(frag));
	net_pkt_set_iface(pkt, iface);
	net_pkt_set_family(pkt, AF_INET);
	net_pkt_set_ip_hdr_len(pkt, sizeof(struct net_ipv4_hdr));

	net_pkt_frag_add(pkt, frag);

	/* Leave room for IPv4 + UDP headers */
	net_buf_add(pkt->frags, NET_IPV4UDPH_LEN);

	memcpy((frag->data + NET_IPV4UDPH_LEN), data, len);
	net_buf_add(frag, len);

	ipv4 = NET_IPV4_HDR(pkt);
	udp = net_udp_get_hdr(pkt, &hdr);

	tmplen = net_buf_frags_len(pkt->frags);

	/* Setup IPv4 header */
	memset(ipv4, 0, sizeof(struct net_ipv4_hdr));

	ipv4->vhl = 0x45;
	ipv4->ttl = 0xFF;
	ipv4->proto = IPPROTO_UDP;
	ipv4->len[0] = tmplen >> 8;
	ipv4->len[1] = (uint8_t)tmplen;
	ipv4->chksum = ~net_calc_chksum_ipv4(pkt);

	tmpaddr.s_addr = srcip;
	net_ipaddr_copy(&ipv4->src, &tmpaddr);
	tmpaddr.s_addr = dstip;
	net_ipaddr_copy(&ipv4->dst, &tmpaddr);

	tmplen -= NET_IPV4H_LEN;
	/* Setup UDP header */
	udp->src_port = htons(srcport);
	udp->dst_port = htons(dstport);
	udp->len = htons(tmplen);
	udp->chksum = 0;
	udp->chksum = ~net_calc_chksum(pkt, IPPROTO_UDP);

	return pkt;
}

int airkiss_udp_send(void)
{
	struct net_pkt *pkt;
	int i;
	u8_t ramdom;

#ifdef CONFIG_SWITCH_APPFAMILY
	if(sys_get_reboot_cmd() != REBOOT_CMD_NONE) {
		airkiss_random = sys_get_reboot_paramater();
		sys_set_reboot_paramater(0);
	}
#endif

	if (airkiss_random == 0xFFFF) {
		return 0;
	}

	ramdom = (u8_t)airkiss_random;
	for (i=0; i <20; i++) {
		pkt = prepare_udp(&ramdom, 1, 0, 9999, 0xFFFFFFFF, 10000);
		if (!pkt) {
			continue;
		}

		if (net_send_data(pkt) < 0) {
			net_pkt_unref(pkt);
		}
	}

	airkiss_random = 0xFFFF;
	return 0;
}

const airkiss_config_t akconf =
{
	(airkiss_memset_fn)&memset,
	(airkiss_memcpy_fn)&memcpy,
	(airkiss_memcmp_fn)&memcmp,
	0
};

static void airkiss_finish(void)
{
	s8_t err;
	airkiss_result_t result;
	err = airkiss_get_result(akcontex, &result);
	if (err == 0)
	{
		ACT_LOG_ID_INF(ALF_STR_airkiss_finish__AIRKISS_GET_RESULT_O, 0);
		printk("ssid: %s, pwd: %s, ssid_length: %d, pwd_length: %d, random: 0x%02x\n",
				result.ssid, result.pwd, result.ssid_length, result.pwd_length, result.random);
		wifi_airkiss_event_cb(result.ssid, result.ssid_length, result.pwd);
		airkiss_random = result.random;
#ifdef CONFIG_SWITCH_APPFAMILY
		sys_set_reboot_paramater(airkiss_random);
#endif
	} else {
		ACT_LOG_ID_INF(ALF_STR_airkiss_finish__AIRKISS_GET_RESULT_F, 0);
	}
}


void rtl_frame_recv_handle(char *buf, int len, int to_fr_ds)
{
	int ret = 0;
	int fixed_channel;
	char bcn_ssid[33] = {0};
	char *current_bssid = NULL;

	ret = airkiss_recv(akcontex, buf, len);
	if(ret == AIRKISS_STATUS_CHANNEL_LOCKED) {
		// LOCKED status only once
		//k_delayed_work_cancel(&change_channel_work);
		lock_channel = airkiss_channel_list[channel_index];

		if(1 == to_fr_ds)
			current_bssid = buf + 4 + ETH_ALEN;
		else if(2 == to_fr_ds)
			current_bssid = buf + 4;
		memcpy(gbssid, current_bssid, ETH_ALEN);
		fixed_channel = promisc_get_fixed_channel(current_bssid, bcn_ssid, &bcn_ssid_len);
		if (fixed_channel != 0) {
				lock_channel = fixed_channel;

			ACT_LOG_ID_INF(ALF_STR_rtl_frame_recv_handle__RN_FIXED_CHANNEL__DR, 1, fixed_channel);
			if(fixed_channel != airkiss_channel_list[channel_index])
				wifi_set_channel(fixed_channel);

			relock_flag = 1;
		}
#if AIRKISS_LOCK_EXACT_CHNL
		else{
			k_delayed_work_submit(&change_channel_work, 0);
		}
#endif

		airkiss_fix_flag = 1;
#if AIRKISS_GUIDE_PRE_HANDLE
		guide_ap_list_free();
#endif
	}
	else if(ret == AIRKISS_STATUS_COMPLETE){
		airkiss_state = AIRKISS_STATUS_COMPLETE;
		airkiss_finish();
		printf("==> AIRKISS_STATUS_COMPLETE !");

	}
}


static void promisc_cb_all(unsigned char *buf, unsigned int len, void* userdata)
{
	int to_fr_ds;
	ieee80211_frame_info_t *promisc_info = (ieee80211_frame_info_t *)userdata;
	promisc_cb_runing = 1; ////代表还处于airkiss配网过程中
	if (airkiss_state == AIRKISS_STATUS_COMPLETE || (!airkiss_run)) {
		promisc_cb_runing = 0; ///表示airkiss配网已经结束
		return;
	}
	//enhance airkiss
	// 1 - fr DS, 2 - to DS
	to_fr_ds = ((promisc_info->i_fc & 0x0100) == 0x0100)? 2: 1;
///添加限定条件：虑掉非 data 包、非广播包、guide 包长范围外的包
#if AIRKISS_PKT_PRE_FILTER
	if( airkiss_pre_filter_pkt(buf, len, promisc_info) < 0 ){
			promisc_cb_runing = 0;
			return;
		}
#endif
//收集guide包，如果集齐了4个长度递增1的包就一起给airkiss lib
#if AIRKISS_GUIDE_PRE_HANDLE
	if( airkiss_guide_prehandle(buf, len, promisc_info) < 0){
			promisc_cb_runing = 0;
			return;
		}
#endif
	if(1 == airkiss_fix_flag)
	{
#if AIRKISS_PROMISC_DIG_ENABLE
		if(1 == to_fr_ds)
			promisc_update_candi_ap_rssi_avg(promisc_info->rssi, ((++airkiss_rx_data_cnt)%32));
#endif
		security_type = promisc_info->encrypt;
	}
	rtl_frame_recv_handle(buf, len, to_fr_ds);
	promisc_cb_runing = 0;
}

static void airkiss_delay_work(struct k_work *work)
{
	change_channel_work_runing = 1;

	if (!airkiss_run) {
		goto work_exit;
	}

#if AIRKISS_PKT_PRE_FILTER
	if(airkiss_fix_rx_cnt >= 2) {  ///如果在channel上收到guide包大于等于2笔，则在当前channel上延长AIRKISS_CHANGE_CHANNEL_INTERVAL(100ms)
		k_delayed_work_submit(&change_channel_work, AIRKISS_CHANGE_CHANNEL_INTERVAL);
		airkiss_fix_rx_cnt = 0;
	}
	else
#endif
	{
		if(0 == relock_flag) {//如果信道还没有锁定
			u8 fixed_channel;
#if AIRKISS_LOCK_EXACT_CHNL
			if(lock_channel != 0)
			{
				fixed_channel = promisc_get_fixed_channel(gbssid, NULL, NULL);
				if(fixed_channel){
					ACT_LOG_ID_INF(ALF_STR_airkiss_delay_work__RN_REFIXED_CHANNEL__, 1, fixed_channel);
					lock_channel = fixed_channel;
					if(fixed_channel != airkiss_channel_list[channel_index])
						wifi_set_channel(fixed_channel);
					relock_flag = 1;
					k_delayed_work_submit(&change_channel_work, AIRKISS_CHANGE_CHANNEL_INTERVAL);
					goto work_exit;
				}
			}
#endif
			channel_index = (channel_index + 1)%MAX_CHANNEL_LIST_NUM; ///切channel
			if (0 == airkiss_channel_list[channel_index]) {
				channel_index = (channel_index + 1)%MAX_CHANNEL_LIST_NUM;
			}

			//ACT_LOG_ID_INF(ALF_STR_airkiss_delay_work__RNDCHANNEL_DT, 2, xTaskGetTickCount(), airkiss_channel_list[channel_index]);
			wifi_set_channel(airkiss_channel_list[channel_index]);
#if AIRKISS_PKT_PRE_FILTER
			airkiss_fix_rx_cnt = 0;
#endif
		}
		else if((airkiss_state != AIRKISS_STATUS_COMPLETE) &&(1 == relock_flag))
		{
			if (++airkiss_lock_channel_cnt > AIRKISS_COMPLETE_TIME_CNT) {
				airkiss_lock_channel_cnt = 0;
				airkiss_init(akcontex, &akconf);
				airkiss_state = AIRKISS_STATUS_CONTINUE;
				relock_flag = 0;
				lock_channel = 0;
#if AIRKISS_GUIDE_PRE_HANDLE
				//guide_ap_list_free();//将之前存储的ap信息清除掉
#endif
			}
		}
		if (airkiss_state != AIRKISS_STATUS_COMPLETE) {
			k_delayed_work_submit(&change_channel_work, AIRKISS_CHANGE_CHANNEL_INTERVAL);
		}
	}

work_exit:
	change_channel_work_runing = 0;
}

int airkiss_api_start(void)
{
	s8_t ret;

	if (airkiss_run) {
		return 0;
	}

///初始化一些变量
	relock_flag		= 0;
	channel_index	= 0;
	bcn_ssid_len		= 0;
	airkiss_fix_flag		= 0;
	lock_channel 		= 0;
	security_type		= 0xFF;
#if AIRKISS_PKT_PRE_FILTER
	airkiss_fix_rx_cnt	= 0;
#endif
#if AIRKISS_PROMISC_DIG_ENABLE
	airkiss_rx_data_cnt	= 0;
#endif
	memset(gbssid, 0xFF, ETH_ALEN);

//airkiss context
	akcontex = (airkiss_context_t *)mem_malloc(sizeof(airkiss_context_t));
	if (akcontex == NULL) {
		ACT_LOG_ID_INF(ALF_STR_airkiss_api_start__AIRKISS_INIT_MEM_MAL, 0);
		return -ENOMEM;
	}

	ret = airkiss_init(akcontex, &akconf);
	if (ret < 0) {
		mem_free(akcontex);
		akcontex = NULL;
		ACT_LOG_ID_INF(ALF_STR_airkiss_api_start__AIRKISS_INIT_FAILEDR, 0);
		return -EAGAIN;
	}

	airkiss_random = 0xFFFF;
	airkiss_lock_channel_cnt = 0;
	airkiss_state = AIRKISS_STATUS_CONTINUE;

	k_sleep(100);
	wifi_enter_promisc_mode();
	wifi_set_promisc(RTW_PROMISC_ENABLE_2, promisc_cb_all, 1);
	wifi_set_channel(airkiss_channel_list[channel_index]);

	k_delayed_work_init(&change_channel_work, airkiss_delay_work);////切channel
	k_delayed_work_submit(&change_channel_work, AIRKISS_CHANGE_CHANNEL_INTERVAL);
	airkiss_run = 1;

	ACT_LOG_ID_INF(ALF_STR_airkiss_api_start__NAIRKISS_START_SUCCE, 0);
	return 0;
}


extern u8 *AP_INFO;
extern int used_index;
int airkiss_api_stop(void)
{
	if (!airkiss_run) {
		return 0;
	}
	airkiss_run = 0;
	airkiss_state = AIRKISS_STATUS_COMPLETE;
	k_delayed_work_cancel(&change_channel_work);
	while (change_channel_work_runing) {
		k_sleep(10);
	}
	wifi_set_promisc(RTW_PROMISC_DISABLE, NULL, 0);

	if(AP_INFO){
		wifi_mem_free((u8*)AP_INFO);
		AP_INFO = NULL;
	}
	used_index = 0;
#if AIRKISS_GUIDE_PRE_HANDLE
	guide_ap_list_free();
#endif
	while (promisc_cb_runing) {
		k_sleep(10);
	}

	if (akcontex) {
		mem_free(akcontex);
		akcontex = NULL;
	}

	k_sleep(50);
	ACT_LOG_ID_INF(ALF_STR_airkiss_api_stop__STOP_AIRKISS_FINISHR, 0);
	return 0;
}


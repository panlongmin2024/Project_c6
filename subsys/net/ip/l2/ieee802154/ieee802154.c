#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_IEEE802154
/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_NET_DEBUG_L2_IEEE802154)
#define SYS_LOG_DOMAIN "net/ieee802154"
#define NET_LOG_ENABLED 1
#endif

#include <net/net_core.h>
#include <net/net_l2.h>
#include <net/net_if.h>

#include "ipv6.h"

#include <errno.h>

#ifdef CONFIG_NET_6LO
#ifdef CONFIG_NET_L2_IEEE802154_FRAGMENT
#include "ieee802154_fragment.h"
#endif
#include <6lo.h>
#endif /* CONFIG_NET_6LO */

#include <net/ieee802154_radio.h>

#include "ieee802154_frame.h"
#include "ieee802154_mgmt.h"
#include "ieee802154_security.h"

#if 0

#include <misc/printk.h>

static inline void hexdump(u8_t *pkt, u16_t length, u8_t reserve)
{
	int i;

	for (i = 0; i < length;) {
		int j;

		ACT_LOG_ID_INF(ALF_STR_XXX__T, 0);

		for (j = 0; j < 10 && i < length; j++, i++) {
#if defined(CONFIG_SYS_LOG_SHOW_COLOR)
			if (i < reserve && reserve) {
				printk(SYS_LOG_COLOR_YELLOW);
			} else {
				printk(SYS_LOG_COLOR_OFF);
			}
#endif
			ACT_LOG_ID_INF(ALF_STR_XXX__02X_, 1, *pkt++);
		}

#if defined(CONFIG_SYS_LOG_SHOW_COLOR)
		if (i < reserve) {
			printk(SYS_LOG_COLOR_OFF);
		}
#endif
		ACT_LOG_ID_INF(ALF_STR_XXX__N, 0);
	}
}

static void pkt_hexdump(struct net_pkt *pkt, bool each_frag_reserve)
{
	u16_t reserve = each_frag_reserve ? net_pkt_ll_reserve(pkt) : 0;
	struct net_buf *frag;

	ACT_LOG_ID_INF(ALF_STR_XXX__IEEE_802154_PACKET_C, 0);

	frag = pkt->frags;
	while (frag) {
		hexdump(each_frag_reserve ?
			frag->data - reserve : frag->data,
			frag->len + reserve, reserve);

		frag = frag->frags;
	}
}
#else
#define pkt_hexdump(...)
#endif

#ifdef CONFIG_NET_L2_IEEE802154_ACK_REPLY
static inline void ieee802154_acknowledge(struct net_if *iface,
					  struct ieee802154_mpdu *mpdu)
{
	struct net_pkt *pkt;
	struct net_buf *frag;

	if (!mpdu->mhr.fs->fc.ar) {
		return;
	}

	pkt = net_pkt_get_reserve_tx(IEEE802154_ACK_PKT_LENGTH, K_FOREVER);
	if (!pkt) {
		return;
	}

	frag = net_pkt_get_frag(pkt, K_FOREVER);

	net_pkt_frag_insert(pkt, frag);

	if (ieee802154_create_ack_frame(iface, pkt, mpdu->mhr.fs->sequence)) {
		const struct ieee802154_radio_api *radio =
			iface->dev->driver_api;

		net_buf_add(frag, IEEE802154_ACK_PKT_LENGTH);

		radio->tx(iface->dev, pkt, frag);
	}

	net_pkt_unref(pkt);

	return;
}
#else
#define ieee802154_acknowledge(...)
#endif /* CONFIG_NET_L2_IEEE802154_ACK_REPLY */

static inline void set_pkt_ll_addr(struct net_linkaddr *addr, bool comp,
				   enum ieee802154_addressing_mode mode,
				   struct ieee802154_address_field *ll)
{
	if (mode == IEEE802154_ADDR_MODE_NONE) {
		return;
	}

	if (mode == IEEE802154_ADDR_MODE_EXTENDED) {
		addr->len = IEEE802154_EXT_ADDR_LENGTH;

		if (comp) {
			addr->addr = ll->comp.addr.ext_addr;
		} else {
			addr->addr = ll->plain.addr.ext_addr;
		}
	} else {
		/* ToDo: Handle short address (lookup known nbr, ...) */
		addr->len = 0;
		addr->addr = NULL;
	}

	addr->type = NET_LINK_IEEE802154;
}

#ifdef CONFIG_NET_6LO
static inline
enum net_verdict ieee802154_manage_recv_packet(struct net_if *iface,
					       struct net_pkt *pkt)
{
	enum net_verdict verdict = NET_CONTINUE;
	u32_t src;
	u32_t dst;

	/* Upper IP stack expects the link layer address to be in
	 * big endian format so we must swap it here.
	 */
	if (net_pkt_ll_src(pkt)->addr &&
	    net_pkt_ll_src(pkt)->len == IEEE802154_EXT_ADDR_LENGTH) {
		sys_mem_swap(net_pkt_ll_src(pkt)->addr,
			     net_pkt_ll_src(pkt)->len);
	}

	if (net_pkt_ll_dst(pkt)->addr &&
	    net_pkt_ll_dst(pkt)->len == IEEE802154_EXT_ADDR_LENGTH) {
		sys_mem_swap(net_pkt_ll_dst(pkt)->addr,
			     net_pkt_ll_dst(pkt)->len);
	}

	/** Uncompress will drop the current fragment. Pkt ll src/dst address
	 * will then be wrong and must be updated according to the new fragment.
	 */
	src = net_pkt_ll_src(pkt)->addr ?
		net_pkt_ll_src(pkt)->addr - net_pkt_ll(pkt) : 0;
	dst = net_pkt_ll_dst(pkt)->addr ?
		net_pkt_ll_dst(pkt)->addr - net_pkt_ll(pkt) : 0;

#ifdef CONFIG_NET_L2_IEEE802154_FRAGMENT
	verdict = ieee802154_reassemble(pkt);
	if (verdict == NET_DROP) {
		goto out;
	}
#else
	if (!net_6lo_uncompress(pkt)) {
		NET_DBG("Packet decompression failed");
		verdict = NET_DROP;
		goto out;
	}
#endif
	net_pkt_ll_src(pkt)->addr = src ? net_pkt_ll(pkt) + src : NULL;
	net_pkt_ll_dst(pkt)->addr = dst ? net_pkt_ll(pkt) + dst : NULL;

	pkt_hexdump(pkt, false);
out:
	return verdict;
}

static inline bool ieee802154_manage_send_packet(struct net_if *iface,
						 struct net_pkt *pkt)
{
	bool ret;

	pkt_hexdump(pkt, false);

#ifdef CONFIG_NET_L2_IEEE802154_FRAGMENT
	ret = net_6lo_compress(pkt, true, ieee802154_fragment);
#else
	ret = net_6lo_compress(pkt, true, NULL);
#endif

	pkt_hexdump(pkt, false);

	return ret;
}

#else /* CONFIG_NET_6LO */

#define ieee802154_manage_recv_packet(...) NET_CONTINUE
#define ieee802154_manage_send_packet(...) true

#endif /* CONFIG_NET_6LO */

static enum net_verdict ieee802154_recv(struct net_if *iface,
					struct net_pkt *pkt)
{
	struct ieee802154_mpdu mpdu;

	if (!ieee802154_validate_frame(net_pkt_ll(pkt),
				       net_pkt_get_len(pkt), &mpdu)) {
		return NET_DROP;
	}

	if (mpdu.mhr.fs->fc.frame_type == IEEE802154_FRAME_TYPE_BEACON) {
		return ieee802154_handle_beacon(iface, &mpdu);
	}

	if (ieee802154_is_scanning(iface)) {
		return NET_DROP;
	}

	if (mpdu.mhr.fs->fc.frame_type == IEEE802154_FRAME_TYPE_MAC_COMMAND) {
		return ieee802154_handle_mac_command(iface, &mpdu);
	}

	/* At this point the frame has to be a DATA one */

	ieee802154_acknowledge(iface, &mpdu);

	net_pkt_set_ll_reserve(pkt, mpdu.payload - (void *)net_pkt_ll(pkt));
	net_buf_pull(pkt->frags, net_pkt_ll_reserve(pkt));

	set_pkt_ll_addr(net_pkt_ll_src(pkt), mpdu.mhr.fs->fc.pan_id_comp,
			mpdu.mhr.fs->fc.src_addr_mode, mpdu.mhr.src_addr);

	set_pkt_ll_addr(net_pkt_ll_dst(pkt), false,
			mpdu.mhr.fs->fc.dst_addr_mode, mpdu.mhr.dst_addr);

	if (!ieee802154_decipher_data_frame(iface, pkt, &mpdu)) {
		return NET_DROP;
	}

	pkt_hexdump(pkt, true);

	return ieee802154_manage_recv_packet(iface, pkt);
}

static enum net_verdict ieee802154_send(struct net_if *iface,
					struct net_pkt *pkt)
{
	struct ieee802154_context *ctx = net_if_l2_data(iface);
	u8_t reserved_space = net_pkt_ll_reserve(pkt);
	struct net_buf *frag;

	if (net_pkt_family(pkt) != AF_INET6) {
		return NET_DROP;
	}

	if (!ieee802154_manage_send_packet(iface, pkt)) {
		return NET_DROP;
	}

	frag = pkt->frags;
	while (frag) {
		if (frag->len > IEEE802154_MTU) {
			NET_ERR("Frag %p as too big length %u",
				frag, frag->len);
			return NET_DROP;
		}

		if (!ieee802154_create_data_frame(ctx, net_pkt_ll_dst(pkt),
						  frag, reserved_space)) {
			return NET_DROP;
		}

		frag = frag->frags;
	}

	pkt_hexdump(pkt, true);

	net_if_queue_tx(iface, pkt);

	return NET_OK;
}

static u16_t ieee802154_reserve(struct net_if *iface, void *data)
{
	return ieee802154_compute_header_size(iface, (struct in6_addr *)data);
}

NET_L2_INIT(IEEE802154_L2,
	    ieee802154_recv, ieee802154_send, ieee802154_reserve, NULL);

void ieee802154_init(struct net_if *iface)
{
	struct ieee802154_context *ctx = net_if_l2_data(iface);
	const struct ieee802154_radio_api *radio =
		iface->dev->driver_api;
	const u8_t *mac = iface->link_addr.addr;
	u8_t long_addr[8];

	NET_DBG("Initializing IEEE 802.15.4 stack on iface %p", iface);

	ieee802154_mgmt_init(iface);

#ifdef CONFIG_NET_L2_IEEE802154_SECURITY
	if (ieee802154_security_init(&ctx->sec_ctx)) {
		NET_ERR("Initializing link-layer security failed");
	}
#endif

	sys_memcpy_swap(long_addr, mac, 8);

	radio->set_ieee_addr(iface->dev, long_addr);
	memcpy(ctx->ext_addr, long_addr, 8);

	if (!radio->set_txpower(iface->dev,
				CONFIG_NET_L2_IEEE802154_RADIO_DFLT_TX_POWER)) {
		ctx->tx_power = CONFIG_NET_L2_IEEE802154_RADIO_DFLT_TX_POWER;
	}

	radio->start(iface->dev);
}

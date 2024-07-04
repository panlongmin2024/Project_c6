/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt pawr manager.
 */
#define SYS_LOG_DOMAIN "btmgr_pawr"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <misc/byteorder.h>

#include <msg_manager.h>
#include <mem_manager.h>
#include <acts_bluetooth/host_interface.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <hex_str.h>
#include <acts_ringbuf.h>
#include <property_manager.h>


/******************************************************************/
//pawr  adv
#ifdef CONFIG_BT_PAWR

#define NUM_RSP_SLOTS	  2
#define NUM_SUBEVENTS	  2
//#define PACKET_SIZE	  5
#define SUBEVENT_INTERVAL 32//60//60/*80*///16

#define PAWR_SEND_BUFF_SIZE 512
#define PAWR_RSP_BUFF_SIZE 512
#define PAWR_RSP_DATA_COUNT 2

#define PAWR_MAX_SUBEVENT_DATA_LEN 59
#define PAWR_MAX_RSP_DATA_LEN      59

#define RSP_SLOT_SPACING       2 //((PAWR_MAX_RSP_DATA_LEN + 3)*4/125)

#define PAWR_NAME "pawr demo"
#define PAWR_NAME_LEN (sizeof(PAWR_NAME) - 1)
#define NAME_LEN 32

#define PER_SYNC_TIMEOUT  0xaa


uint8_t pawr_name[33];

struct pawr_mgr_adv_info {

    uint8_t advertising;
    uint8_t remote_dev_flag;
    uint8_t pending_release;
    bt_addr_le_t local_ble_addr;
	bt_addr_le_t remote_ble_addr;
    uint8_t pawr_subevent_data[PAWR_MAX_SUBEVENT_DATA_LEN];
    uint8_t pawr_send_buff[PAWR_SEND_BUFF_SIZE];

    struct acts_ringbuf *pawr_send_cache_buff;
	struct bt_le_ext_adv *pawr_adv;
	bt_pawr_vnd_rsp_cb rsp_cb;
	uint32_t last_time;
};

struct pawr_mgr_receive_info {
    bool per_adv_found;
	bt_addr_le_t local_ble_addr;
    uint8_t pending_release;
    uint8_t per_sid;
    uint8_t pa_synced;
    uint8_t pawr_rsp_data_len;
	uint8_t rsp_data_expect_count;
	uint8_t rsp_data_send_count;
    uint8_t pawr_rsp_data[PAWR_MAX_RSP_DATA_LEN];
    uint8_t pawr_rsp_buff[PAWR_RSP_BUFF_SIZE];
    struct acts_ringbuf *pawr_rsp_cache_buff;
	struct bt_le_per_adv_sync *pawr_sync;
	bt_pawr_vnd_rec_cb rec_data_cb;
};

struct pawr_mgr_adv_info *pawr_adv_info = NULL;
struct pawr_mgr_receive_info *pawr_receive_info = NULL;

static const struct bt_le_adv_param ext_adv_params = {
	/* BT_LE_EXT_ADV_NCONN */
	.id = BT_ID_DEFAULT,
	/* [150ms, 150ms] by default */
	.interval_min = BT_GAP_ADV_FAST_INT_MAX_2,
	.interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
    .options = BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_IDENTITY,
	.sid = BT_EXT_ADV_SID_PAWR,
};

// static const struct bt_le_per_adv_param per_adv_params = {
// 	.interval_min = 80,
// 	.interval_max = 80,
// 	.options = 0,
// 	.num_subevents = NUM_SUBEVENTS,
// 	.subevent_interval = SUBEVENT_INTERVAL,
// 	.response_slot_delay = 9/*0x2*/,
// 	.response_slot_spacing = 10/*20*/,
// 	.num_response_slots = NUM_RSP_SLOTS,
// };
#define RUNNING_CYCLES(end, start)	((uint32_t)((long)(end) - (long)(start)))

static void pawr_check_remote_dev_sync_lost(void)
{
    uint32_t cycles;
    uint32_t now_time;
	int err = 0;
	
	os_sched_lock();
    now_time = k_cycle_get_32();
	os_sched_unlock();
	
	if ((pawr_adv_info->last_time) && pawr_adv_info->remote_dev_flag) {
       cycles = RUNNING_CYCLES(now_time, pawr_adv_info->last_time);
       //SYS_LOG_INF("request_tine:%d \n",cycles);
	   if (cycles > ((PER_SYNC_TIMEOUT-15)*10*1000*24)) {

		  pawr_adv_info->remote_dev_flag = 0;
		  pawr_adv_info->last_time = 0;
	      bt_manager_event_notify(BT_PAWR_SYNC_LOST, NULL, 0);
		  if (pawr_adv_info->pawr_adv && (!pawr_adv_info->pending_release)) {
			err = hostif_bt_le_ext_adv_start(pawr_adv_info->pawr_adv, BT_LE_EXT_ADV_START_DEFAULT);
			SYS_LOG_INF("restart ext adv:%d \n",err);
		  }
		  SYS_LOG_INF("lost:\n");
	   }
    }
}

static void pawr_data_request_cb(struct bt_le_ext_adv *adv, const struct bt_le_per_adv_data_request *request)
{
	int err;
	uint8_t to_send;
	uint8_t p_buf[3];
	uint8_t p_size;
	struct net_buf_simple p_net_bufs;
	struct bt_le_per_adv_subevent_data_params subevent_data_params[NUM_SUBEVENTS];

    int ret;

	if (pawr_adv_info->pawr_adv != adv)
	{
		return;
	}
    
	pawr_check_remote_dev_sync_lost();
	
	os_sched_lock();    
	if (acts_ringbuf_length(pawr_adv_info->pawr_send_cache_buff) < 2){
		goto err_exit;
	}

    ret = acts_ringbuf_get(pawr_adv_info->pawr_send_cache_buff, p_buf, 1);
	if (ret != 1) {
		SYS_LOG_WRN("want read %d bytes\n", ret);
		goto err_exit;
	}
	p_size = p_buf[0];
    ret = acts_ringbuf_get(pawr_adv_info->pawr_send_cache_buff, pawr_adv_info->pawr_subevent_data, p_size);
	if (ret != p_size) {
		SYS_LOG_WRN("want read %d(%d) bytes\n", p_size,ret);
	}
	os_sched_unlock();
	net_buf_simple_init_with_data(&p_net_bufs, pawr_adv_info->pawr_subevent_data,
					     p_size);

	/* Continuously send the same dummy data and listen to all response slots */
	to_send = MIN(request->count, ARRAY_SIZE(subevent_data_params));
	for (size_t i = 0; i < to_send; i++) {
		subevent_data_params[i].subevent =
			(request->start + i) % NUM_SUBEVENTS;
		subevent_data_params[i].response_slot_start = 0;
		subevent_data_params[i].response_slot_count = NUM_RSP_SLOTS;
		subevent_data_params[i].data = &p_net_bufs;
	}

	err = hostif_bt_le_per_adv_set_subevent_data(adv, to_send, subevent_data_params);
	if (err) {
		printk("Failed to set subevent data (err %d)\n", err);
	}
    return;

err_exit:
    os_sched_unlock();

}

bool get_address(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = user_data;

	if (data->type == BT_DATA_LE_BT_DEVICE_ADDRESS) {
		memcpy(addr->a.val, data->data, sizeof(addr->a.val));
		addr->type = data->data[sizeof(addr->a)];

		return false;
	}

	return true;
}

bool pawr_get_vnd_data(struct bt_data *data, void *user_data)
{

    printk("%d\n",data->type);
	switch (data->type) {
	case BT_DATA_MANUFACTURER_DATA:
        printk("%d,%x,%x,%x,%x\n",data->data_len,data->data[0],data->data[1],data->data[2],data->data[3]);
		return false;
	default:
		return true;
	}   

}

void pawr_vnd_protocol_deal(const uint8_t *data,uint8_t data_len,uint8_t dir)
{
    if ((data[0] == 0xff) && (data[1] == 0xff) && (data[2] == 0xff) && (data[3] == 0xff)) {
	    //bt_manager_event_notify(BT_PAWR_SYNCED, NULL, 0);	   
	} else {
 		if (pawr_adv_info->rsp_cb) {
			pawr_adv_info->rsp_cb(data,data_len);
		}
	}
}

bool pawr_get_vnd_rsp_data(struct bt_data *data, void *user_data)
{
    //printk("vnd_type:%d\n",data->type);
	switch (data->type) {
	case BT_DATA_MANUFACTURER_DATA:

        //printk("%d,%x,%x,%x,%x\n",data->data_len,data->data[0],data->data[1],data->data[2],data->data[3]);
		pawr_vnd_protocol_deal(data->data,data->data_len,0);
		return false;
	default:
		return true;
	} 
}


static void pawr_response_cb(struct bt_le_ext_adv *adv, struct bt_le_per_adv_response_info *info,
			struct net_buf_simple *buf)
{
    //printk("pawr rsp cb:\n");
#if 1
    uint8_t p_rsp_data[6];
	struct net_buf_simple_state state;
	bt_addr_le_t p_peer;
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err = 0;
	
	if (pawr_adv_info->pawr_adv != adv)
	{
		return;
	}

	if (!buf) {
		return;
	}

	net_buf_simple_save(buf, &state);
	bt_data_parse(buf, get_address, &p_peer);		
	net_buf_simple_restore(buf, &state);

    if (!(pawr_adv_info->remote_dev_flag)) {
       memcpy(&(pawr_adv_info->remote_ble_addr),&p_peer,sizeof(p_peer));
	   bt_addr_le_to_str(&p_peer, addr_str, sizeof(addr_str));
	   printk("sync_dev:%s\n", addr_str);
	   pawr_adv_info->remote_dev_flag = 1;
	   bt_manager_event_notify(BT_PAWR_SYNCED, NULL, 0);

	   //todo 1v1 : close ext adv?
	   err = hostif_bt_le_ext_adv_stop(pawr_adv_info->pawr_adv);
	   SYS_LOG_INF("ext adv stop:%d \n",err);
	} else {		
	   if (memcmp(&p_peer, &(pawr_adv_info->remote_ble_addr),sizeof(p_peer)) != 0) {
		/* No address found */
		return;
	   }		
	}

	os_sched_lock();
    pawr_adv_info->last_time = k_cycle_get_32();
    os_sched_unlock();

	bt_data_parse(buf, pawr_get_vnd_rsp_data,p_rsp_data);
#endif 	
}

static const struct bt_le_ext_adv_cb pawr_adv_cb = {
	.pawr_data_request = pawr_data_request_cb,
	.pawr_response = pawr_response_cb,
};


static int pawr_adv_init_bufs(void)
{
    int res;

	pawr_adv_info = mem_malloc(sizeof(struct pawr_mgr_adv_info));
	if (!pawr_adv_info) {
		SYS_LOG_ERR("pawr adv info malloc failed\n");
		res = -ENOMEM;
		goto err_exit;		
	}
	memset(pawr_adv_info,0,sizeof(struct pawr_mgr_adv_info));
	SYS_LOG_INF("\n");

    pawr_adv_info->pawr_send_cache_buff = mem_malloc(sizeof(struct acts_ringbuf));

	if (!pawr_adv_info->pawr_send_cache_buff) {
		res = -ENOMEM;
		SYS_LOG_ERR("send cache malloc failed\n");
		goto err_exit;
	}
    memset(pawr_adv_info->pawr_send_cache_buff,0,sizeof(struct acts_ringbuf));
	acts_ringbuf_init(pawr_adv_info->pawr_send_cache_buff,
				pawr_adv_info->pawr_send_buff,
				PAWR_SEND_BUFF_SIZE);
    return 0;

err_exit:

	if (pawr_adv_info) {
		mem_free(pawr_adv_info);
		pawr_adv_info = NULL;
	}	
	return res;	
}

int pawr_adv_deinit_bufs(void)
{
    if (!pawr_adv_info){
 		return -ENOMEM;   
	}
    
	os_sched_lock();
	if (pawr_adv_info->pawr_send_cache_buff) {
		mem_free(pawr_adv_info->pawr_send_cache_buff);
		pawr_adv_info->pawr_send_cache_buff = NULL;
	}
	if (pawr_adv_info){
		mem_free(pawr_adv_info);
		pawr_adv_info = NULL;
	}
    os_sched_unlock();

    return 0;
}

#define PAWR_EXT_AD_ITEMS 2

static int pawr_set_ext_adv(struct bt_le_ext_adv *adv)
{
    struct bt_data ad_data[PAWR_EXT_AD_ITEMS];
	int items = 0;
	int err;

	ad_data[items].type = BT_DATA_NAME_COMPLETE;
	ad_data[items].data_len = strlen(pawr_name);
	ad_data[items].data = pawr_name;
	items++;

	err = hostif_bt_le_ext_adv_set_data(adv, ad_data,
				items, NULL, 0);
	if (err != 0) {
		printk("set data: %d", err);
		return err;
	}

	return 0;
}

#endif /* #ifdef CONFIG_BT_PAWR */

int bt_manager_pawr_vnd_per_send(uint8_t *buf,uint8_t len, uint8_t type)
{
#ifdef CONFIG_BT_PAWR

    uint8_t ad_data[3];
    int ret = 0;

    if (len > PAWR_MAX_SUBEVENT_DATA_LEN) {
       SYS_LOG_WRN("data too long \n");        
       return -ENOMEM;
	}   
	if (!pawr_adv_info) {
        return -ENOMEM;		
	}
	os_sched_lock();

    if (acts_ringbuf_space(pawr_adv_info->pawr_send_cache_buff) < (len + 3) ) {
       SYS_LOG_WRN("no spc");
	   goto err_exit;
	}

	ad_data[0] = len + 2; //total data len
	ad_data[1] = len + 1; //data len + type
	ad_data[2] = type;
	ret = acts_ringbuf_put(pawr_adv_info->pawr_send_cache_buff, ad_data, 3);
	if (ret != 3) {
		SYS_LOG_WRN("want write (%d) bytes\n",ret);
	}
	ret = acts_ringbuf_put(pawr_adv_info->pawr_send_cache_buff, buf, len);
    os_sched_unlock();
    if (ret != len) {
		SYS_LOG_WRN("want write %d(%d) bytes\n", len, ret);
	}
	return ret;

err_exit:
    os_sched_unlock();
    return -ENOMEM;
#else

   return 0;

#endif  /*#ifdef CONFIG_BT_PAWR*/  
}


int bt_manager_pawr_adv_start(bt_pawr_vnd_rsp_cb cb,struct bt_le_per_adv_param *per_adv_param)
{	
#ifdef CONFIG_BT_PAWR

	int err;
	struct bt_le_per_adv_param pawr_per_adv_params = { 0 };

    printk("pawr_adv_start:%p \n",pawr_adv_info);
    if ((pawr_adv_info != NULL) && (pawr_adv_info->advertising)) {
		SYS_LOG_INF("already");
		return -EALREADY;
	}

	err = pawr_adv_init_bufs();
	if (err) {
		printk("Failed to init bufs (err %d)\n", err);
	}
    pawr_adv_info->rsp_cb = cb;
    SYS_LOG_INF(":\n");

    err = property_get("PAWR_NAME", pawr_name,
			sizeof(pawr_name) - 1);
	if (err <= 0) {
		SYS_LOG_WRN("failed to get pawr name\n");
		memcpy(pawr_name, PAWR_NAME, PAWR_NAME_LEN);
	}
	/* Create a non-connectable non-scannable advertising set */
	err = hostif_bt_le_ext_adv_create(&ext_adv_params, &pawr_adv_cb, &(pawr_adv_info->pawr_adv));
	if (err) {
		printk("Failed to create advertising set (err %d)\n", err);
		return 0;
	}

	/* Set periodic advertising parameters */
	pawr_per_adv_params.interval_min = per_adv_param->interval_min;
	pawr_per_adv_params.interval_max = per_adv_param->interval_max;
	pawr_per_adv_params.options = 0;
	pawr_per_adv_params.num_subevents = NUM_SUBEVENTS;
	pawr_per_adv_params.subevent_interval = per_adv_param->subevent_interval;
	pawr_per_adv_params.response_slot_delay = per_adv_param->response_slot_delay;
	pawr_per_adv_params.response_slot_spacing = RSP_SLOT_SPACING;
	pawr_per_adv_params.num_response_slots = NUM_RSP_SLOTS;

	SYS_LOG_INF("per_param:%d,%d,%d,%d \n",pawr_per_adv_params.interval_min,pawr_per_adv_params.subevent_interval,
	                                    pawr_per_adv_params.response_slot_delay,pawr_per_adv_params.response_slot_spacing);	
	
	err = hostif_bt_le_per_adv_set_param(pawr_adv_info->pawr_adv, &pawr_per_adv_params);
	if (err) {
		printk("Failed to set periodic advertising parameters (err %d)\n", err);
		return 0;
	}
    pawr_set_ext_adv(pawr_adv_info->pawr_adv);
	/* Enable Periodic Advertising */
	err = hostif_bt_le_per_adv_start(pawr_adv_info->pawr_adv);
	if (err) {
		printk("Failed to enable periodic advertising (err %d)\n", err);
		return 0;
	}

	printk("Start Periodic Advertising\n");
	err = hostif_bt_le_ext_adv_start(pawr_adv_info->pawr_adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Failed to start extended advertising (err %d)\n", err);
		return 0;
	}
	pawr_adv_info->advertising = 1;

#if 1 //config req start slot & rsp start slot
    uint8_t test_data[6];
	test_data[0] = 0xe0;
	test_data[1] = 0x3;
	test_data[2] = 0x3;
	test_data[3] = 0x4; 
    bt_manager_pawr_vnd_per_send(test_data,4,BT_DATA_MANUFACTURER_DATA);
#endif

#endif /* #ifdef CONFIG_BT_PAWR */

	return 0;	
}

int bt_manager_pawr_adv_stop(void)
{
#ifdef CONFIG_BT_PAWR
	int err = 0;
  	
	if (!pawr_adv_info) {
        return -ENOMEM;		
	}
    pawr_adv_info->pending_release = 1;
	if (pawr_adv_info->pawr_adv) {

		/* Stop periodic advertising */
		err = hostif_bt_le_per_adv_stop(pawr_adv_info->pawr_adv);
		if (err) {
			SYS_LOG_ERR("per_adv: %d", err);
			return err;
		}

		/* Stop extended advertising */
		err = hostif_bt_le_ext_adv_stop(pawr_adv_info->pawr_adv);
		if (err) {
			SYS_LOG_ERR("ext_adv: %d", err);
//			return err;
		}
		hostif_bt_le_ext_adv_delete(pawr_adv_info->pawr_adv); 
	}
    pawr_adv_deinit_bufs();
	SYS_LOG_INF(":");
	return err;
#else
    return 0;
#endif /*#ifdef CONFIG_BT_PAWR*/
}


/////////////////////////////////////////////////////////////////////////////

//adv_sync

#ifdef CONFIG_BT_PAWR

static bt_addr_le_t per_addr;

static void pawr_sync_cb(struct bt_le_per_adv_sync *sync, struct bt_le_per_adv_sync_synced_info *info)
{
	struct bt_le_per_adv_sync_subevent_params params;
	//uint8_t subevents[2];
	uint8_t subevents[1];
	char le_addr[BT_ADDR_LE_STR_LEN];
	int err;
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

    if (!pawr_receive_info) {
		return;
	}
	if (pawr_receive_info->pawr_sync != sync) {
		return;
	}    
	pawr_receive_info->pa_synced = 1;
	printk("Synced to %s with %d subevents\n", le_addr, info->num_subevents);
	params.properties = 0;
	params.num_subevents = 1/*2*/;
	params.subevents = subevents;
	subevents[0] = 0;
	//subevents[1] = 1;
	err = hostif_bt_le_per_adv_sync_subevent(sync, &params);
	if (err) {
		printk("Failed to set subevents to sync to (err %d)\n", err);
	}
	bt_manager->per_synced_count++;
	if (bt_manager->per_synced_count > bt_manager->total_per_sync_count) {
		SYS_LOG_ERR(":%d,%d \n",bt_manager->per_synced_count,bt_manager->total_per_sync_count);
	}
    bt_manager_check_per_adv_synced();

	bt_manager_event_notify(BT_PAWR_SYNCED, NULL, 0);
}

static int pawr_scan_start(void)
{
 	struct bt_le_scan_param param;
	int ret;    
    
    param.type = BT_LE_SCAN_TYPE_PASSIVE;
	param.options = BT_LE_SCAN_OPT_NONE/*BT_LE_SCAN_OPT_FILTER_DUPLICATE*//*BT_LE_SCAN_OPT_NONE*/;
	/* [65ms, 110ms], almost 60% duty cycle by default */
	param.interval = 176;
	param.window = 104;
	param.timeout = 0;
	ret = hostif_bt_le_scan_start(&param, NULL);
    SYS_LOG_INF("%d",ret);
	return ret;
}

static void pawr_term_cb(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_term_info *info)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

	printk("Sync term %d(reason %d),%d\n", info->sid,info->reason,pawr_receive_info->pending_release);
 
    if (!pawr_receive_info) {
		return;
	}
	if (pawr_receive_info->pawr_sync != sync) {
		return;
	}
    if (pawr_receive_info->pawr_sync)
    {
	   hostif_bt_le_per_adv_sync_delete(pawr_receive_info->pawr_sync);
	   pawr_receive_info->pawr_sync = NULL;
    } 
	
	if (bt_manager->per_synced_count) {
        bt_manager->per_synced_count--;
	}
	pawr_receive_info->per_adv_found = 0;
	if ((info->reason != 0x16) && (!pawr_receive_info->pending_release)) {
	   /*restart scan*/
       pawr_scan_start();
	}
	bt_manager_event_notify(BT_PAWR_SYNC_LOST, NULL, 0);
	SYS_LOG_INF(":");
}


static void pawr_send_rsp_data(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_recv_info *info)
{
    struct bt_le_per_adv_response_params p_rsp_params;
    struct net_buf_simple p_rsp_bufs;
    uint8_t p_size;
    uint8_t p_buf[3];
    int ret;

    net_buf_simple_reset(&p_rsp_bufs);
	
	os_sched_lock();    
	if (acts_ringbuf_length(pawr_receive_info->pawr_rsp_cache_buff) < 2){
        if ((pawr_receive_info->pawr_rsp_data_len != 0) && 
		    ((pawr_receive_info->rsp_data_expect_count == 0) || 
			(pawr_receive_info->rsp_data_send_count < pawr_receive_info->rsp_data_expect_count))) {
          goto re_send_data;
		} else {
		  goto err_exit;
		}
	}

    ret = acts_ringbuf_get(pawr_receive_info->pawr_rsp_cache_buff, p_buf, 1);
	if (ret != 1) {
		SYS_LOG_WRN("want read %d bytes\n", ret);
		goto err_exit;
	}
	p_size = p_buf[0];
    ret = acts_ringbuf_get(pawr_receive_info->pawr_rsp_cache_buff, pawr_receive_info->pawr_rsp_data, p_size);
	if (ret != p_size) {
		SYS_LOG_WRN("want read %d(%d) bytes\n", p_size,ret);
	}
	pawr_receive_info->pawr_rsp_data_len = p_size;
    pawr_receive_info->rsp_data_send_count = 0;

re_send_data:
	os_sched_unlock();
	net_buf_simple_init_with_data(&p_rsp_bufs, pawr_receive_info->pawr_rsp_data,
					     pawr_receive_info->pawr_rsp_data_len);
    
	p_rsp_params.request_event = info->periodic_event_counter;
	p_rsp_params.request_subevent = info->subevent;
	p_rsp_params.response_subevent = info->subevent;
	p_rsp_params.response_slot = 1;

	ret = hostif_bt_le_per_adv_set_response_data(sync, &p_rsp_params, &p_rsp_bufs);
	if (ret) {
		printk("Failed to send response (err %d)\n", ret);
	}
	pawr_receive_info->rsp_data_send_count++;
	return;

err_exit:
   	os_sched_unlock();  

}

static void pawr_recv_cb(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_recv_info *info, struct net_buf_simple *buf)
{
    uint8_t p_rsp_data[6];
	char le_addr[BT_ADDR_LE_STR_LEN];

	if (pawr_receive_info->pawr_sync != sync) {
		return;
	}

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));
	//printk("recv %s with %d subevent\n", le_addr, info->subevent);
	
	if (buf && buf->len) {
		/* Respond with own address for the advertiser to connect to */	
        bt_data_parse(buf, pawr_get_vnd_data,p_rsp_data);			
	} else if (buf) {
//		printk("Rec empty ind: sub %d\n", info->subevent);
	} else {
		printk("Failed to rec ind: sub %d\n", info->subevent);
	}
	pawr_send_rsp_data(sync,info);
}

static struct bt_le_per_adv_sync_cb pawr_sync_callbacks = {
	.synced = pawr_sync_cb,
	.term = pawr_term_cb,
	.recv = pawr_recv_cb,
};


static bool data_cb(struct bt_data *data, void *user_data)
{
	char *name = user_data;
	uint8_t len;

	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		len = MIN(data->data_len, NAME_LEN - 1);
		memcpy(name, data->data, len);
		name[len] = '\0';
		return false;
	default:
		return true;
	}
}


static bool broadcast_audio_announcement(struct bt_data *data, void *user_data)
{
	uint32_t *broadcast_id = user_data;
	uint16_t uuid;

	if (data->type != BT_DATA_SVC_DATA16) {
		return true;
	}

    /*
	 BROADCAST_AUDIO_SIZE == 5 
	*/
	if (data->data_len != 5) {
		return true;
	}

	uuid = sys_get_le16(data->data);
	if (uuid != BT_UUID_BROADCAST_AUDIO_VAL) {
		return true;
	}

	*broadcast_id = sys_get_le24(data->data + BT_UUID_SIZE_16);

	/* Stop parsing */
	return false;
}

static void pawr_scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
	char name[NAME_LEN];
	struct bt_le_per_adv_sync_param sync_create_param;
	int err;

	struct net_buf_simple_state state;
	uint32_t broadcast_id;


	if (info->adv_type != BT_GAP_ADV_TYPE_EXT_ADV) {
		return;
	}

	net_buf_simple_save(buf, &state);
	broadcast_id = INVALID_BROADCAST_ID;
	bt_data_parse(buf, broadcast_audio_announcement, (void *)&broadcast_id);
	net_buf_simple_restore(buf, &state);

	/* found a broadcast audio */
	if (broadcast_id != INVALID_BROADCAST_ID) {
		return;
	}

	(void)memset(name, 0, sizeof(name));
	bt_data_parse(buf, data_cb, name);
    printk(":%s \n",name);
	if (strcmp(name, pawr_name)) {
		return;
	}

	if (!(pawr_receive_info->per_adv_found) && info->interval) {
		pawr_receive_info->per_adv_found = true;

		pawr_receive_info->per_sid = info->sid;
		bt_addr_le_copy(&per_addr, info->addr);

	    printk("Creating Periodic Advertising Sync \n");
	    bt_addr_le_copy(&sync_create_param.addr, &per_addr);
	    sync_create_param.options = 0;
	    sync_create_param.sid = pawr_receive_info->per_sid;
	    sync_create_param.skip = 0;
	    sync_create_param.timeout = PER_SYNC_TIMEOUT;
	    err = hostif_bt_le_per_adv_sync_create(&sync_create_param, &(pawr_receive_info->pawr_sync));
	    if (err) {
		    printk("Failed to create sync (err %d)\n", err);
		    pawr_receive_info->per_adv_found = 0;
	    }
	}
}

static struct bt_le_scan_cb pawr_scan_callbacks = {
	.recv = pawr_scan_recv,
};


static int pawr_receive_init_bufs(void)
{
	pawr_receive_info = mem_malloc(sizeof(struct pawr_mgr_receive_info));
	if (!pawr_receive_info) {
		SYS_LOG_ERR("pawr rsp info malloc failed\n");
		return -ENOMEM;
	}
	memset(pawr_receive_info,0,sizeof(struct pawr_mgr_receive_info));
	SYS_LOG_INF("\n");

    pawr_receive_info->pawr_rsp_cache_buff = mem_malloc(sizeof(struct acts_ringbuf));

	if (!pawr_receive_info->pawr_rsp_cache_buff) {
//		res = -ENOMEM;
		goto err_exit;
	}
    memset(pawr_receive_info->pawr_rsp_cache_buff,0,sizeof(struct acts_ringbuf));
	acts_ringbuf_init(pawr_receive_info->pawr_rsp_cache_buff,
				pawr_receive_info->pawr_rsp_buff,
				PAWR_RSP_BUFF_SIZE);
    return 0;

err_exit:

	if (pawr_receive_info) {
		mem_free(pawr_receive_info);
		pawr_receive_info = NULL;
	}	
	return -ENOMEM;
}

static int pawr_receive_deinit_bufs(void)
{
    if (!pawr_receive_info){
 		return -ENOMEM;   
	}
	os_sched_lock();
	if (pawr_receive_info->pawr_rsp_cache_buff) {
		mem_free(pawr_receive_info->pawr_rsp_cache_buff);
		pawr_receive_info->pawr_rsp_cache_buff = NULL;
	}
	if (pawr_receive_info){
		mem_free(pawr_receive_info);
		pawr_receive_info = NULL;
	}
	os_sched_unlock();
    return 0;
}
#endif /*#ifdef CONFIG_BT_PAWR*/

int bt_manager_pawr_vnd_rsp_send(uint8_t *buf,uint8_t len, uint8_t type)
{
#ifdef CONFIG_BT_PAWR

    uint8_t ad_data[12];
    int ret = 0;

    if (len > PAWR_MAX_RSP_DATA_LEN) {
       SYS_LOG_WRN("data too long \n");        
       return -ENOMEM;
	}
    if (!pawr_receive_info)
	{
		return -ENOMEM;
	}	

	os_sched_lock();
    if (acts_ringbuf_space(pawr_receive_info->pawr_rsp_cache_buff) < (len + 3) ) {
       SYS_LOG_WRN("no spc");
	   goto err_exit;
	}

    ad_data[0] = 9 + len + 2; //total len
	/*
	  le addr:
	*/
    ad_data[1] = sizeof(bt_addr_le_t) + 1;
    ad_data[2] = BT_DATA_LE_BT_DEVICE_ADDRESS; 
	memcpy(&(ad_data[3]),&(pawr_receive_info->local_ble_addr),sizeof(bt_addr_le_t));

	ad_data[10] = len + 1; // data len + type
	ad_data[11] = type;
	ret = acts_ringbuf_put(pawr_receive_info->pawr_rsp_cache_buff, ad_data, 12);
	if (ret != 12) {
		SYS_LOG_WRN("want write (%d) bytes\n",ret);
	}
	ret = acts_ringbuf_put(pawr_receive_info->pawr_rsp_cache_buff, buf, len);
    os_sched_unlock();
    if (ret != len) {
		SYS_LOG_WRN("want write %d(%d) bytes\n", len, ret);
	}
	return ret;

err_exit:
    os_sched_unlock(); 
    return -ENOMEM;
#else
    return 0;
#endif /*#ifdef CONFIG_BT_PAWR*/     
}

int bt_manager_pawr_receive_start(void)
{	
#ifdef CONFIG_BT_PAWR
	int err;

    struct bt_manager_context_t *bt_manager = bt_manager_get_context();

    err = pawr_receive_init_bufs();
	if (err) {
        SYS_LOG_WRN("failed to init rsp buf\n");
	}
    btif_ble_get_local_mac(&(pawr_receive_info->local_ble_addr));

    err = property_get("PAWR_NAME", pawr_name,
			sizeof(pawr_name) - 1);
	if (err <= 0) {
		SYS_LOG_WRN("failed to get pawr name\n");
		memcpy(pawr_name, PAWR_NAME, PAWR_NAME_LEN);
	}

	hostif_bt_le_scan_cb_register(&pawr_scan_callbacks);
	hostif_bt_le_per_adv_sync_cb_register(&pawr_sync_callbacks);

	err = pawr_scan_start();
	if (err) {
		printk("failed (err %d)\n", err);
        pawr_receive_deinit_bufs();
		return 0;
	}
#if 1
    /**/
	uint8_t test_data[6];
	test_data[0] = 0xff;
	test_data[1] = 0xff;
	test_data[2] = 0xff;
	test_data[3] = 0xff;
    bt_manager_pawr_vnd_rsp_send(test_data,4,BT_DATA_MANUFACTURER_DATA);
#endif

	pawr_receive_info->rsp_data_expect_count = 0/*PAWR_RSP_DATA_COUNT*/;
	bt_manager->total_per_sync_count++;
	SYS_LOG_INF(":%d \n",bt_manager->total_per_sync_count);
	return err;
#else
    return 0;
#endif  /*#ifdef CONFIG_BT_PAWR*/ 
	
}

int bt_manager_pawr_receive_stop(void)
{
#ifdef CONFIG_BT_PAWR
    int err;
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();
    if (!pawr_receive_info)
	{
        SYS_LOG_WRN("absent\n");
		return -ENOMEM;
	}

    if (pawr_receive_info) {
		pawr_receive_info->pending_release = 1;
	}
    err = hostif_bt_le_scan_stop();
    if (err) {
	   SYS_LOG_INF("err: %d", err);
    }

    if (pawr_receive_info->pawr_sync)
    {
	   hostif_bt_le_per_adv_sync_delete(pawr_receive_info->pawr_sync);
	   pawr_receive_info->pawr_sync = NULL;
    } 
    hostif_bt_le_scan_cb_unregister(&pawr_scan_callbacks);
    hostif_bt_le_per_adv_sync_cb_unregister(&pawr_sync_callbacks);
    pawr_receive_deinit_bufs();

	if (bt_manager->total_per_sync_count) {
		bt_manager->total_per_sync_count--;
	}
	if (bt_manager->per_synced_count) {
		bt_manager->per_synced_count--;
	}
    
	SYS_LOG_INF(":\n");
    return err;
#else
    return 0;
#endif /*#ifdef CONFIG_BT_PAWR*/    
}


/*

*/
#define SYS_LOG_DOMAIN "btmgr_ble_tws"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <msg_manager.h>
#include <mem_manager.h>
#include <acts_bluetooth/host_interface.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <hex_str.h>

#include <property_manager.h>	

#define CFG_BTMGR_LETWS_INFO	"BTMGR_LETWS_INFO"

static struct btmgr_letws_context_t letws_context;

struct btmgr_letws_context_t *btmgr_get_letws_context(void)
{
	return &letws_context;
}

int bt_manager_letws_get_dev_role(void)
{
	return letws_context.tws_role;
}

uint16_t bt_manager_letws_get_handle(void)
{
	return letws_context.tws_handle;
}

int bt_manager_letws_adv_start(uint8 dir_flag)
{	
	int err;
	struct bt_le_adv_param ext_adv_params = { 0 };
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	if (context->ext_adv) {
		SYS_LOG_ERR("tws adv is exist:");
		os_mutex_unlock(&context->letws_mutex);
		return 0;
	}

	if (dir_flag && (!context->le_remote_addr_valid)) {
		 SYS_LOG_ERR("dir ext adv err:");
	}	
	/* BT_LE_EXT_ADV_NCONN */
	ext_adv_params.id = BT_ID_DEFAULT;
	/* [150ms, 150ms] by default */
    ext_adv_params.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
    ext_adv_params.interval_max = BT_GAP_ADV_FAST_INT_MIN_2;
    ext_adv_params.options = (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_NO_2M);

    if (dir_flag == 1) {   
        ext_adv_params.options |= BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY;
#if 1
		ext_adv_params.options |= BT_LE_ADV_OPT_USE_IDENTITY;
#endif 

// #ifdef CONFIG_BT_WHITELIST
// 		ext_adv_params.options |= BT_LE_ADV_OPT_FILTER_CONN;
// #endif

// #ifdef CONFIG_BT_PRIVACY
// 	//	context->remote_ble_addr
// 		ext_adv_params.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;
// #endif
        ext_adv_params.peer = &context->remote_ble_addr;
    }
    ext_adv_params.sid = BT_EXT_ADV_SID_LETWS,

	err = hostif_bt_le_ext_adv_create(&ext_adv_params, NULL, &context->ext_adv);
    if (err) {
		SYS_LOG_INF("create:%d \n",err);
	}
//  pawr_set_ext_adv(ext_adv);
	err = hostif_bt_le_ext_adv_start(context->ext_adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		SYS_LOG_INF("sta err:%d\n",err);
	}
	os_mutex_unlock(&context->letws_mutex);

	print_buffer(&context->remote_ble_addr,1,7,16,0);
    SYS_LOG_INF(":%d \n",dir_flag);
	return 0;
	
}	

int bt_manager_letws_adv_stop(void) {
	
	int err;
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	if (context->ext_adv == NULL) {
		SYS_LOG_ERR("ext_adv is null:");
		os_mutex_unlock(&context->letws_mutex);
		return 0;
	}  
    /* Stop extended advertising */
	err = hostif_bt_le_ext_adv_stop(context->ext_adv);
	if (err) {
		SYS_LOG_ERR("ext_adv: %d", err);
		os_mutex_unlock(&context->letws_mutex);
		return err;
	}
	err = hostif_bt_le_ext_adv_delete(context->ext_adv);
    if (err) {
		SYS_LOG_ERR("adv del err: %d", err);
	}
	context->ext_adv = NULL;
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF(":");
	return err;
}

void bt_manager_letws_stop_pair_search(void)
{
	struct bt_manager_context_t* bt_manager = bt_manager_get_context();
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	if (bt_manager->letws_mode_state == BT_LETWS_MODE_SCAN) {
		bt_manager_audio_le_pause_scan();
		bt_manager->letws_mode_state = BT_LETWS_MODE_STATE_NONE;
	} else if (bt_manager->letws_mode_state == BT_LETWS_MODE_ADV) {
		bt_manager_letws_adv_stop();
		bt_manager->letws_mode_state = BT_LETWS_MODE_STATE_NONE;
	}
	os_delayed_work_cancel(&bt_manager->letws_pair_search_work);

	if(context->tws_role == BTSRV_TWS_PENDING){
		context->tws_role = 0;
		context->temp_tws_role = 0;
	}
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF(":");
}

void bt_manager_letws_start_pair_search(uint8_t role,int time_out_s)
{
	struct bt_manager_context_t* bt_manager = bt_manager_get_context();
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	bt_manager_letws_stop_pair_search();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	if(context->tws_role == BTSRV_TWS_NONE){
		context->temp_tws_role = role;
		context->tws_role = BTSRV_TWS_PENDING;
	}else if(role != context->tws_role || context->tws_handle){
		SYS_LOG_INF("pair failed %d 0x%x\n",context->tws_role,context->tws_handle);
		os_mutex_unlock(&context->letws_mutex);
		return;
	}
	/*
	 le tws pair search
	*/
	if (role == BTSRV_TWS_MASTER) {
		bt_manager_audio_le_resume_scan();
		bt_manager->letws_mode_state = BT_LETWS_MODE_SCAN;  
	} else if (role == BTSRV_TWS_SLAVE) {
		//bt_manager_audio_dir_adv_init();
		bt_manager_letws_adv_start(1);
		bt_manager->letws_mode_state = BT_LETWS_MODE_ADV;
	} else {
		;
	}
	if(time_out_s){
		os_delayed_work_submit(&bt_manager->letws_pair_search_work,time_out_s*1000);
	}else{
		//os_delayed_work_submit(&bt_manager->letws_pair_search_work,60*1000);
	}
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF("t:%d",time_out_s);
}


void bt_manager_init_letws_info(bt_letws_vnd_rx_cb cb)
{
    int ret;
    bt_mgr_saved_letws_info_t p;
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	context->rx_cb = cb;
	os_mutex_init(&context->letws_mutex);

    ret = property_get(CFG_BTMGR_LETWS_INFO, (char *)&p, sizeof(bt_mgr_saved_letws_info_t));
    if (ret <= 0) {
        SYS_LOG_ERR("letws info err:");
		return;
	}
	memcpy(&(context->info),&p,sizeof(bt_mgr_saved_letws_info_t));
	bt_mamager_letws_reconnect();
	SYS_LOG_INF(":");
}


void bt_manager_save_letws_info(uint8_t role,bt_addr_le_t *addr)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	context->info.dev_role = role;
	memcpy(&(context->info.addr),addr,sizeof(bt_addr_le_t));
	os_mutex_unlock(&context->letws_mutex);
	property_set(CFG_BTMGR_LETWS_INFO, (char *)&context->info, sizeof(bt_mgr_saved_letws_info_t));
	SYS_LOG_INF(":");
	print_buffer(addr,1,7,16,0);
}

void bt_manager_clear_letws_info(void)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	memset(&context->info,0,sizeof(bt_mgr_saved_letws_info_t));
	btif_audio_set_ble_tws_addr(NULL);
	os_mutex_unlock(&context->letws_mutex);

	property_set(CFG_BTMGR_LETWS_INFO, NULL, 0);
	SYS_LOG_INF(":");
}

int bt_mamager_set_remote_ble_addr(bt_addr_le_t *addr)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	if (!addr)
	{
		SYS_LOG_ERR("addr is null:");
		return 0;
	}
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	context->le_remote_addr_valid = 1;
	memcpy(&(context->remote_ble_addr),addr,sizeof(bt_addr_le_t));
	btif_audio_set_ble_tws_addr(&(context->remote_ble_addr));
	os_mutex_unlock(&context->letws_mutex);

	print_buffer(&context->remote_ble_addr,1,7,16,0);
	SYS_LOG_INF(":");	 
	return 0;
}

int bt_mamager_letws_disconnect(int reason)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	struct bt_manager_context_t* bt_manager = bt_manager_get_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	if(bt_manager->letws_mode_state == BT_LETWS_MODE_CONNECTED){
		//bt_manager_audio_conn_disconnect(context->tws_handle);
		btif_conn_disconnect_by_handle(context->tws_handle,reason);
		bt_manager->letws_mode_state = BT_LETWS_MODE_DISCONNECT;
	}
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF(":%d",reason);
	return 0;
}

int bt_mamager_letws_reconnect(void)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	struct bt_manager_context_t* bt_manager = bt_manager_get_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	if(bt_manager->letws_mode_state == BT_LETWS_MODE_STATE_NONE
		&& context->info.dev_role != BTSRV_TWS_NONE){
		memcpy(&(context->remote_ble_addr),&(context->info.addr),sizeof(bt_addr_le_t));
		context->le_remote_addr_valid = 1;
		btif_audio_set_ble_tws_addr(&(context->remote_ble_addr));

		bt_manager_letws_start_pair_search(context->info.dev_role,0);
	}
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF(":");
	return 0;
}

void bt_manager_letws_reset(void)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	bt_manager_letws_stop_pair_search();
	if (context->tws_role == BTSRV_TWS_MASTER) {
		bt_mamager_letws_disconnect(BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
	bt_manager_clear_letws_info();
	os_mutex_unlock(&context->letws_mutex);

	SYS_LOG_INF(":");
}

static int bt_mamager_letws_vnd_rx_cb(uint16_t handle, const uint8_t *buf, uint16_t len)
{
	SYS_LOG_INF("buf: %p, len: %d\n", buf, len);
    int i;

	for (i = 0; i < 4; i++) {
		printk("0x%x \n", buf[i]);
	}
	printk("\n");

	if (letws_context.rx_cb) {
		letws_context.rx_cb(handle,buf,len);
	}	
    return 0;
}

int bt_mamager_letws_connected(uint16_t handle)
{ 
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	bt_addr_le_t *le_addr =  btif_get_le_addr_by_handle(handle);
	int role = btif_get_conn_role(handle);;
	struct bt_manager_context_t* bt_manager = bt_manager_get_context();

	if (!le_addr) {
		SYS_LOG_ERR("invalid handle 0x%x\n",handle);
		return -EINVAL;
	}

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	if(context->tws_role == BTSRV_TWS_PENDING){
		context->tws_role = context->temp_tws_role;
		context->temp_tws_role = 0;
	}else if(context->tws_role == BTSRV_TWS_NONE){
		if(role == BT_ROLE_SLAVE)
			context->tws_role = BTSRV_TWS_SLAVE;
	}

	context->tws_handle = handle;
    bt_manager_save_letws_info(context->tws_role,le_addr);

    bt_manager_audio_le_vnd_register_rx_cb(handle, bt_mamager_letws_vnd_rx_cb);
    bt_manager->letws_mode_state = BT_LETWS_MODE_CONNECTED;

	if (context->tws_role == BTSRV_TWS_MASTER) {
		bt_manager_audio_le_pause_scan();
	} else if (context->tws_role == BTSRV_TWS_SLAVE) {
		bt_manager_letws_adv_stop();
	}
	os_delayed_work_cancel(&bt_manager->letws_pair_search_work);
	if(context->rx_cb){
		letws_context.rx_cb(handle,NULL,0);
	}
	os_mutex_unlock(&context->letws_mutex);

    SYS_LOG_INF("role:%d %d\n",context->tws_role,context->temp_tws_role);
//    print_buffer(le_addr,1,7,16,0);	
	return bt_manager_event_notify(BT_TWS_CONNECTION_EVENT, &handle, sizeof(handle));

}

void bt_mamager_letws_disconnected(uint16_t handle, uint8_t role, uint8_t reason)
{
	struct bt_manager_context_t* bt_manager = bt_manager_get_context();
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
    if (bt_manager_audio_get_type(handle) == BT_TYPE_BR) {
		return;
	}

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	bt_manager->letws_mode_state = BT_LETWS_MODE_STATE_NONE;
	SYS_LOG_INF(":0x%x \n",reason);
	context->tws_handle = 0;
	context->tws_role = 0;
	context->temp_tws_role = 0;
	if(context->rx_cb){
		letws_context.rx_cb(0,NULL,0);
	}
	os_mutex_unlock(&context->letws_mutex);
	if(reason == 0x8){
		bt_mamager_letws_reconnect();
	}
	bt_manager_event_notify(BT_TWS_DISCONNECTION_EVENT, &reason, sizeof(reason));
}


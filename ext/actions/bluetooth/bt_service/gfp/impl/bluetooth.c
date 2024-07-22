#include <stdio.h>
#include <string.h>
#include <fast_pair.h>
#include "bluetooth.h"
#include <helper.h>

#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_comm.h>
#include <gfp_ble_stream.h>
#include <gfp_ble_stream_ctrl.h>


#define BT_NAME_LEN		(32+1)	/* 32(len) + 1(NULL) */


//typedef u8_t (*gfp_command_callback)(uint8_t* packet, int packet_length);



/////////////////////////////////////////////////////////////////
// Bluetooth Provider.
//
// Provides stubs for Bluetooth functionality, this
// implementation will vary significatly based on chipset.
/////////////////////////////////////////////////////////////////

//static fp_is_fastpair_start is_fastpair_started;
static uint8_t io_display_yesno = 0;

void static bytes_reverse(uint8_t *dst, uint8_t *src, uint8_t len)       
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

void get_ble_address(uint8_t* address)
{

    //get_nvram_mac_info(address,CFG_BT_MAC,6);

    //ble_host_get_local_addr(address);
    //bd_address_t addr;
    //btif_br_get_local_mac(addr.val);
    bt_addr_le_t addr;
    btif_ble_get_local_mac(&addr);
    bytes_reverse(addr.a.val, addr.a.val, MAC_ADDRESS_LENGTH);
	memcpy(address,addr.a.val,MAC_ADDRESS_LENGTH);
    //addr.val[5] = addr.val[5]|0xc0;
}

void get_public_address(uint8_t* address) 
{
    //get_nvram_mac_info(address,CFG_BT_MAC,6);
    bd_address_t addr;
    btif_br_get_local_mac(&addr);
    bytes_reverse(addr.val, addr.val, MAC_ADDRESS_LENGTH);
	memcpy(address,addr.val,MAC_ADDRESS_LENGTH);
}

uint16 get_personalized_name(uint8_t * name)
{
    uint16 name_len = 0;
    int ret_val;

    ret_val = property_get(VM_FAST_PERSONALIZED_NAME, name, PERSONALIZED_NAME_SIZE);
    if ((ret_val < PERSONALIZED_NAME_SIZE) || (strlen(name) < 1)) {
//        ret_val = property_get(CFG_BT_NAME, name, (BT_NAME_LEN-1));

        SYS_LOG_WRN("no name!");
        return 0;
    }

    name_len = strlen(name);
    if (name_len > PERSONALIZED_NAME_SIZE)
        name_len = PERSONALIZED_NAME_SIZE;

    SYS_LOG_INF("name_len %d.", name_len);
    return name_len;
}

void update_personalized_name(uint8_t * name,uint8_t length, bool additional)
{
    int ret_val;

    if(name == NULL){
        SYS_LOG_INF("ERROR");
        return;
    }

#ifdef CONFIG_PROPERTY
    SYS_LOG_INF("name:%s",name);
    u8_t tmp_name[64];

    memset(tmp_name, 0, PERSONALIZED_NAME_SIZE);
    if (length > PERSONALIZED_NAME_SIZE)
        length = PERSONALIZED_NAME_SIZE;

    memcpy(tmp_name, name, length);
    ret_val = property_set(VM_FAST_PERSONALIZED_NAME,tmp_name,PERSONALIZED_NAME_SIZE);
    if (ret_val < 0) {
        SYS_LOG_ERR("failed to upgade gfp bt name,ret %d\n", ret_val);
    }
    else{
        if(additional){
            property_set(CFG_BT_NAME, tmp_name, BT_NAME_LEN);
            property_set(CFG_BLE_NAME, tmp_name, BT_NAME_LEN);
            property_set(CFG_BT_LOCAL_NAME, tmp_name, BT_NAME_LEN);
            btif_br_update_devie_name();
        }
    }
#endif
}

int is_paired_device(uint8_t* address)
{
    struct bt_mgr_dev_info *p_dev = NULL;
    bd_address_t addr;

    bytes_reverse(addr.val, address, MAC_ADDRESS_LENGTH);
    p_dev = bt_mgr_find_dev_info(&addr);

    if(p_dev != NULL){
        return 1;
    }
    else{
        return 0;
    }
}

int is_pairing(void)
{
    return bt_manager_is_pair_mode();
}

void begin_pairing_mode(void)
{
    //pairing = 1;
}

void stop_pairing_mode(void)
{
    //pairing = 0;
}

void set_capabilities_display_yes_no(void)
{
  //  TODO: Set device capabilities to Display/YesNo.
    SYS_LOG_INF("set_capabilities_display_yes_no %d.",io_display_yesno);
    if(io_display_yesno == 0){
        io_display_yesno = 1;
        //register_auth_cb(true);
	    btsrv_gfp_cap_io_set(true);
    }
}

/* 协议栈默认的是noinputnooutput，这里做恢复操作 */
void set_capabilities_noinputnooutput()
{
    SYS_LOG_INF("set_capabilities_noinputnooutput %d",io_display_yesno);
    if (io_display_yesno){ 
        //register_auth_cb(false);
        btsrv_gfp_cap_io_set(false);
        io_display_yesno = 0;
    }
}

void initiate_bonding(uint8_t* address)
{
    //TODO: Initiate bonding with the provided address.
    print_hex_comm("BONDING ADDR:",address,6);  
}
#if 0
void set_pairing_request_callback(fp_pairing_request_callback callback)
{
    created_pairing_request_callback = callback;
}
#endif
void confirm_pairing(uint8_t* address, bool success)
{

	btsrv_gfp_confirm_pairing_reply(success);
#if 0
	if (success){
        hostif_bt_conn_ssp_confirm_reply(gfp_conn);
    }
    else{
        hostif_bt_conn_ssp_confirm_neg_reply(gfp_conn);
    }
#endif
}

const BluetoothProvider bluetooth = {
  .get_ble_address                   = get_ble_address,
  .get_public_address                = get_public_address,
  .notify                            = gfp_send_pkg_to_stream,
  .is_pairing                        = is_pairing,
  .set_capabilities_display_yes_no   = set_capabilities_display_yes_no,
  .set_capabilities_noinputnooutput  = set_capabilities_noinputnooutput,
  .initiate_bonding                  = initiate_bonding,
  .set_pairing_request_callback      = btsrv_gfp_pairing_request_reg,
  //.set_get_fastpair_state_callback   = set_get_fastpair_state_callback,
  .confirm_pairing                   = confirm_pairing,
  .is_paired_device                  = is_paired_device,
  .get_personalized_name             = get_personalized_name,
  .update_personalized_name          = update_personalized_name,
};

void init_bluetooth(BluetoothProvider** bluetooth_p)
{
    *bluetooth_p = (BluetoothProvider*)&bluetooth;
}




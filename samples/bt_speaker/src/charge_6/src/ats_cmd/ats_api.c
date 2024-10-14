#include "ats_cmd/ats.h"
#include <os_common_api.h>
#include <msg_manager.h>
#include <sys_event.h>
#include <sys_manager.h>
#include <common/sys_reboot.h>
#include <property_manager.h>
#include <mem_manager.h>
#include <input_manager.h>
#include <sys_wakelock.h>
#include <device.h>
#include <hex_str.h>

#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "ats_api"
#include <logging/sys_log.h>
#include <nvram_config.h>

struct _ats_ctx {
    os_mutex ats_mutex;
    uint8_t *ats_cmd_resp_buf;
    int ats_cmd_resp_buf_size;
    uint8_t ats_enable:1;
    uint8_t ats_enable_enter_key_check:1;
    uint8_t ats_enter_key_check_record:1;
    uint8_t ats_music_bypass_allow_flag:1;
	uint8_t ats_enter_all_key_check_record:1;
	uint8_t ats_key_pwr_pressed:1;
	uint8_t ats_key_all_pressed:1;
};

static struct _ats_ctx *p_ats_ctx;

static inline os_mutex *ats_get_mutex(void)
{
    return &p_ats_ctx->ats_mutex;
}

static inline uint8_t *ats_get_cmd_resp_buf(void)
{
    return p_ats_ctx->ats_cmd_resp_buf;
}

static inline int ats_get_cmd_resp_buf_size(void)
{
    return p_ats_ctx->ats_cmd_resp_buf_size;
}

int ats_cmd_response(uint8_t *buf, int size)
{
    if (size > ats_get_cmd_resp_buf_size())
    {
        SYS_LOG_ERR("size > buf\n");
        return -1;
    }

    memset(ats_get_cmd_resp_buf(), 0, ats_get_cmd_resp_buf_size());
    memcpy(ats_get_cmd_resp_buf(), buf, size);
	
    SYS_LOG_INF("dump(num:%d):\n", size);
    print_buffer(ats_get_cmd_resp_buf(), 1, size, 16, 0);

	return 0;
}

int ats_usb_cdc_acm_cmd_response(struct device *dev, uint8_t *buf, int size)
{
    ats_usb_cdc_acm_write_data(buf, size);
	return 0;
}

void enter_ats(void)
{
    int ret;

    ret = ats_init();
    if (ret)
    {
        SYS_LOG_ERR("init err\n");
        return;
    }

    ats_usb_cdc_acm_init();
}

int ats_init(void)
{
    int ret = 0;


    if(p_ats_ctx){
        SYS_LOG_ERR("have init\n");
        ret = -1;
        goto err_exit;
    }

    p_ats_ctx = malloc(sizeof(struct _ats_ctx));
    if (p_ats_ctx == NULL)
    {
        SYS_LOG_ERR("ctx malloc fail\n");
        ret = -1;
        goto err_exit;
    }

    memset(p_ats_ctx, 0, sizeof(struct _ats_ctx));

    p_ats_ctx->ats_enable = property_get_int(CFG_AUTO_ENTER_ATS, 0);
    SYS_LOG_INF("ats_enable:%d\n", p_ats_ctx->ats_enable);
    
    if (p_ats_ctx->ats_enable)
    {
        SYS_LOG_INF("poweron force enter ats mode.\n");
    }

    p_ats_ctx->ats_cmd_resp_buf_size = 60;

    p_ats_ctx->ats_cmd_resp_buf = malloc(p_ats_ctx->ats_cmd_resp_buf_size);
    if (p_ats_ctx->ats_cmd_resp_buf == NULL)
    {
        SYS_LOG_ERR("buf malloc fail\n");
        ret = -1;
        goto err_exit;
    }

    memset(p_ats_ctx->ats_cmd_resp_buf, 0, p_ats_ctx->ats_cmd_resp_buf_size);

    ats_music_bypass_allow_flag_update();

    os_mutex_init(ats_get_mutex());

    return ret;

err_exit:
    if (p_ats_ctx)
    {
        if (p_ats_ctx->ats_cmd_resp_buf)
        {
            free(p_ats_ctx->ats_cmd_resp_buf);
            p_ats_ctx->ats_cmd_resp_buf = NULL;
        }

        free(p_ats_ctx);
        p_ats_ctx = NULL;
    }
    return ret;
}

/* in ats key test mode, verify power key pressed. */
bool ats_get_pwr_key_pressed(void)
{
    if(!p_ats_ctx){
        return false;
    }	
	else{
		return p_ats_ctx->ats_key_pwr_pressed;
	}
}
void ats_set_pwr_key_pressed(bool pressed)
{
    if(!p_ats_ctx){
        return;
    }	
	else{
		p_ats_ctx->ats_key_pwr_pressed = pressed;
	}
}

bool ats_is_enable(void)
{
    bool enable = false;
    if(!p_ats_ctx){
        return enable;
    }
    os_mutex_lock(ats_get_mutex(), OS_FOREVER);

    enable = p_ats_ctx->ats_enable;

    os_mutex_unlock(ats_get_mutex());

    return (enable);
}

void ats_set_enable(bool en)
{
    SYS_LOG_INF("en:%d\n", en);

    os_mutex_lock(ats_get_mutex(), OS_FOREVER);

    p_ats_ctx->ats_enable = en;

    if (en)
    {
        sys_wake_lock(6);
    }
    else 
    {
        sys_wake_unlock(6);
    }

    os_mutex_unlock(ats_get_mutex());
}
extern void sys_pm_reboot(int type);
int ats_sys_reboot(void)
{
  //  k_sleep(100);
	sys_pm_reboot(0);

   // sys_reboot_by_user(0);
    return 0;
}

int ats_sys_power_off(void)
{
    //k_sleep(100);

    sys_event_notify(SYS_EVENT_POWER_OFF);
    return 0;
}

int ats_color_write(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);
    print_buffer(buf, 1, size, 16, 0);

    ret = property_set_factory(CFG_ATS_COLOR, buf, size);
    if (ret != 0)
    {
        SYS_LOG_ERR("nvram set err\n");
    }
   	property_flush(CFG_ATS_COLOR);
    return ret;
}

int ats_dbg_open_flag_write(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);
    print_buffer(buf, 1, size, 16, 0);

    ret = property_set_factory(CFG_DUG_OPEN_OR_CLOSE, buf, size);
    if (ret != 0)
    {
        SYS_LOG_ERR("nvram set err\n");
    }
    return ret;
}
void write_dbg_flag(int setvalue)
{
	char temp_data[4] = {0};
	char buf_read[4] = {0};
	snprintf(temp_data, sizeof(temp_data), "%x", setvalue);
    ats_dbg_open_flag_write(temp_data,strlen(temp_data));


	property_get(CFG_DUG_OPEN_OR_CLOSE, buf_read, 4);
	for(int i = 0;i<4;i++){
		SYS_LOG_INF("------> buf[%d] = %d\n",i,buf_read[i]);
	}
}
int ats_dbg_open_flag_read(void)
{
    int ret;
	uint8_t buf[5] ={0};
	int result = 0;
    ret = property_get(CFG_DUG_OPEN_OR_CLOSE, buf, 4);
	print_buffer(buf, 1, 4, 16, 0);
	
    if (ret < 0)
    {
         SYS_LOG_ERR("err\n");
         result = 0;		
    }
    else 
    {
        if (!strcmp(buf, "0x01"))
        {
            result = 1;
        }
		else if(!strcmp(buf, "0x00"))
		{
             result = 0;
		}
			
    }	
    SYS_LOG_INF("result:%d\n", result);


    return result;
}

int ats_color_read(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);

    ret = property_get(CFG_ATS_COLOR, buf, size);
    if (ret < 0)
    {
        SYS_LOG_ERR("nvram get err:%d\n", ret);
        return ret;
    }

    SYS_LOG_INF("ret:%d\n", ret);
    print_buffer(buf, 1, size, 16, 0);

    return ret;
}

int get_color_id_from_ats(u8_t *color_id)
{
    int result;
    uint8_t buffer[2+1] = {0};
    char str[4+1] = "0x00";

    result = ats_color_read(buffer, sizeof(buffer)-1);
    if (result < 0)
    {
		SYS_LOG_ERR("err\n");
        return -1;
    }
    
    str[2] = buffer[0];
    str[3] = buffer[1];

    *color_id = atoi(str);
    
    SYS_LOG_INF("ok,color_id:0x%02X\n", *color_id);

    return 0;
}

int ats_gfps_model_id_write(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);
    print_buffer(buf, 1, size, 16, 0);

    ret = property_set_factory(CFG_ATS_GFPS_MODEL_ID, buf, size);
    if (ret != 0)
    {
        SYS_LOG_ERR("nvram set err\n");
    }

    return ret;
}

int ats_gfps_model_id_read(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);

    ret = property_get(CFG_ATS_GFPS_MODEL_ID, buf, size);
    if (ret < 0)
    {
        SYS_LOG_ERR("nvram get err:%d\n", ret);
        return ret;
    }

    SYS_LOG_INF("ret:%d\n", ret);

    print_buffer(buf, 1, size, 16, 0);

    return ret;
}

int ats_dsn_write(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);
    print_buffer(buf, 1, size, 16, 0);

    ret = property_set_factory(CFG_ATS_DSN_ID, buf, size);
    if (ret != 0)
    {
        SYS_LOG_ERR("nvram set err\n");
    }

    return ret;
}

int ats_dsn_read(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);

    ret = property_get(CFG_ATS_DSN_ID, buf, size);
    if (ret < 0)
    {
        SYS_LOG_ERR("nvram get err:%d\n", ret);
        return ret;
    }

    SYS_LOG_INF("ret:%d\n", ret);

    print_buffer(buf, 1, size, 16, 0);

    return ret;
}
int ats_psn_write(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);
    print_buffer(buf, 1, size, 16, 0);

    ret = property_set_factory(CFG_ATS_PSN_ID, buf, size);
    if (ret != 0)
    {
        SYS_LOG_ERR("nvram set err\n");
    }

    return ret;
}

int ats_psn_read(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);

    ret = property_get(CFG_ATS_PSN_ID, buf, size);
    if (ret < 0)
    {
        SYS_LOG_ERR("nvram get err:%d\n", ret);
        return ret;
    }

    SYS_LOG_INF("ret:%d\n", ret);

    print_buffer(buf, 1, size, 16, 0);

    return ret;
}

int get_gfp_model_id_from_ats(u8_t *model_id)
{
	int result;
    u8_t buffer[3*2+1] = {0};

	result = ats_gfps_model_id_read(buffer, sizeof(buffer)-1);
    if (result < 0)
    {
		SYS_LOG_ERR("err\n");
        return -1;
    }
    else 
    {
        str_to_hex(model_id, buffer, (sizeof(buffer)-1)/2);

		SYS_LOG_INF("ok,model_id:0x%02X_%02X_%02X\n", model_id[0], model_id[1], model_id[2]);

        return 0;
    }
}

int ats_bt_dev_name_read(uint8_t *buf, int size)
{
    int ret;

    ret = property_get(CFG_BT_NAME, buf, size);
    if (ret < 0) {
	 
        return ret;
	}

    return ret;
}

int ats_bt_dev_mac_addr_read(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);

    ret = property_get(CFG_BT_MAC, buf, size);
    if (ret < 0) {
	    SYS_LOG_ERR("nvram get err:%d\n", ret);
        return ret;
	}
    
    SYS_LOG_INF("ret:%d\n", ret);

    print_buffer(buf, 1, size, 16, 0);

    return ret;
}

int ats_enter_key_check(bool enable)
{
    if (p_ats_ctx->ats_enable_enter_key_check == enable)
    {
        SYS_LOG_ERR("already:%d\n", enable);
        return 0;
    }

    SYS_LOG_INF("%d\n", enable);

    if (enable == true)
    {
        p_ats_ctx->ats_enable_enter_key_check = 1;
        p_ats_ctx->ats_enter_key_check_record = 1;
    }
    else 
    {
        p_ats_ctx->ats_enable_enter_key_check = 0;
    }

    return 0;
}
bool ats_get_enter_key_check_record(void)
{
    if(p_ats_ctx){
        SYS_LOG_INF("---->:p_ats_ctx is inited!\n");
        return p_ats_ctx->ats_enable_enter_key_check;
    }
    else{
        SYS_LOG_INF("---->:p_ats_ctx not inited!\n");
        return false;
    }
}

void ats_set_all_key_check(bool enable)
{
    if (!p_ats_ctx->ats_enable_enter_key_check){
        return;
    }

	p_ats_ctx->ats_enter_all_key_check_record = enable;
}
bool ats_get_all_key_check(void)
{
	bool ret = false;
    if(p_ats_ctx){
		if(!p_ats_ctx->ats_enable_enter_key_check){
			ret = false;
		}
		else{
        	ret = p_ats_ctx->ats_enter_all_key_check_record;
		}
    }
    else{
        ret = false;
    }
	return ret;
}
void ats_set_all_key_pressed(bool pressed)
{
	if(!p_ats_ctx){
		return;
	}
	p_ats_ctx->ats_key_all_pressed = pressed;
}
bool ats_get_all_key_pressed(void)
{
	if(!p_ats_ctx){
		return false;
	}
	else{
		return p_ats_ctx->ats_key_all_pressed;
	}
}

int ats_usb_cdc_acm_key_check_report(struct device *dev, uint32_t key_value)
{
    uint8_t key_buf[2+1] = {0};
	
    if (p_ats_ctx->ats_enable_enter_key_check)
    {
        SYS_LOG_INF("origin_key_value:0x%X\n", key_value);
        
        key_value &= ~KEY_TYPE_ALL;
        
        SYS_LOG_INF("key_value:0x%X\n", key_value);

        if (key_value == KEY_VOLUMEUP)
        {
            memcpy(key_buf, "01", 2);
        }
        else if (key_value == KEY_PAUSE_AND_RESUME)
        {
            memcpy(key_buf, "02", 2);
        }
        else if (key_value == KEY_VOLUMEDOWN)
        {
            memcpy(key_buf, "00", 2);
        }
        else if (key_value == KEY_BT)
        {
            memcpy(key_buf, "03", 2);
        }
        else if (key_value == KEY_BROADCAST)
        {
            memcpy(key_buf, "04", 2);
        }
        else if (key_value == KEY_ATS_PWR_KEY)
        {
            memcpy(key_buf, "05", 2);
			if(ats_get_all_key_check()){
				ats_set_pwr_key_pressed(true);
			}
        }	
        else if (key_value == KEY_ATS_ALL_KEY)
        {
            memcpy(key_buf, "06", 2);
			if(ats_get_all_key_check()){
				if(ats_get_pwr_key_pressed()){
					/* powerkey first, then other key trigger */
			        ats_set_all_key_pressed(true);			
				}
			}
        }			
        else 
        {
            return -1;
        }
        
        SYS_LOG_INF("key_str:%s", key_buf);
		
		if(!ats_get_all_key_check()){
	        ats_usb_cdc_acm_cmd_response_at_data(
	            dev, ATS_CMD_RESP_KEY_READ, sizeof(ATS_CMD_RESP_KEY_READ)-1, 
	            key_buf, sizeof(key_buf)-1);
		}
		
        return 0;
    }
    else
    {
        SYS_LOG_INF("not enable ats key check\n");

        return -1;
    }
}

int ats_music_bypass_allow_flag_write(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);
    print_buffer(buf, 1, size, 16, 0);

    ret = property_set_factory(CFG_ATS_MUSIC_BYPASS_ALLOW_FLAG, buf, size);
    if (ret != 0)
    {
        SYS_LOG_ERR("nvram set err\n");
    }

    return ret;
}

int ats_music_bypass_allow_flag_read(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);

    ret = property_get(CFG_ATS_MUSIC_BYPASS_ALLOW_FLAG, buf, size);
    if (ret < 0) {
	    SYS_LOG_ERR("nvram get err:%d\n", ret);
        return ret;
	}
    
    SYS_LOG_INF("ret:%d\n", ret);

    print_buffer(buf, 1, size, 16, 0);

    return ret;
}

int ats_music_bypass_allow_flag_update(void)
{
    int ret = 0;
	int result;
    uint8_t buf[4+1] = {0};

    result = ats_music_bypass_allow_flag_read(buf, sizeof(buf)-1);
    if (result < 0)
    {
        SYS_LOG_ERR("err\n");
        ret = -1;
    }
    else 
    {
        if (!strcmp(buf, "0x01"))
        {
            p_ats_ctx->ats_music_bypass_allow_flag = 1;
        }
        else if (!strcmp(buf, "0x00"))
        {
            p_ats_ctx->ats_music_bypass_allow_flag = 0;
        }
        else 
        {
            SYS_LOG_ERR("err\n");
            ret = -1;
        }
    }

    if (ret == 0)
    {
        SYS_LOG_INF("%d\n", p_ats_ctx->ats_music_bypass_allow_flag);
    }

    return ret;
}

bool ats_get_music_bypass_allow_flag(void)
{
    return p_ats_ctx->ats_music_bypass_allow_flag;
}

int ats_serial_number_write(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);
    print_buffer(buf, 1, size, 16, 0);

    ret = property_set_factory(CFG_ATS_SN, buf, size);
    if (ret != 0)
    {
        SYS_LOG_ERR("nvram set err\n");
    }

    return ret;
}

int ats_serial_number_read(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("dump(num:%d)\n", size);

    ret = property_get(CFG_ATS_SN, buf, size);
    if (ret < 0) {
	    SYS_LOG_ERR("nvram get err:%d\n", ret);
        return ret;
	}
    
    SYS_LOG_INF("ret:%d\n", ret);

    print_buffer(buf, 1, size, 16, 0);

    return ret;
}

int ats_module_test_mode_write(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("ats_module_test_mode_write\n");

    ret = property_set(CFG_USER_IN_OUT_ATS_MODULE, buf, 1);
    if (ret != 0)
    {
        SYS_LOG_ERR("nvram set err\n");
    }
	ret = property_flush(CFG_USER_IN_OUT_ATS_MODULE);
    return ret;
}

int ats_module_test_mode_read(uint8_t *buf, int size)
{
    int ret;

    SYS_LOG_INF("ats_module_test_mode_read\n");

    ret = property_get(CFG_USER_IN_OUT_ATS_MODULE,buf, size);
    if (ret < 0)
    {
        SYS_LOG_ERR("auto_enter_ats_module get err:%d\n", ret);
        return ret;
    }

    SYS_LOG_INF("ret:%d\n", ret);

    print_buffer(buf, 1, size, 16, 0);

    return ret;
}

int ats_mac_write(uint8_t *buf, int size)
{
    int ret = 0;
	char read_mac_fac[12]={0};
	char read_mac_user[12]={0};

    SYS_LOG_INF("write mac to flash rw!\n");
	ret = property_set_factory(CFG_BT_MAC, buf, 12);
    if (ret != 0){
        return -1;
    }
	SYS_LOG_INF("write mac to flash user!\n");
	ret = property_set(CFG_BT_MAC, buf, 12);
    if (ret != 0){
        return -1;
    }
	ret = property_flush(CFG_BT_MAC);
    if (ret != 0){
        return -1;
    }	
    k_sleep(50);
	property_get(CFG_BT_MAC, read_mac_user, 12);
	nvram_config_get_factory(CFG_BT_MAC, read_mac_fac , 12);
	
	if(memcmp(read_mac_user, read_mac_fac, 12)){
		return -1;
	}
		
	if(memcmp(buf, read_mac_fac, 12)){
		return -1;
	}
	
    return ret;
}

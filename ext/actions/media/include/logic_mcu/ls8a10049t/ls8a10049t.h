#ifndef __LS8A10049T_H__
#define __LS8A10049T_H__

#include <device.h>
#include <mcu_manager_supply.h>

struct logic_mcu_ls8a10049t_device_data_t {
	//struct device *gpio_dev;
	struct device *i2c_dev;
    mcu_callback_t  mcu_notify;
    uint8_t mcu_update_fw;
    bool enable_bt_led;
    bool enable_pty_boost_led;   
    bool enable_bat_led; 
};

#if 1
struct logic_mcu_ls8a10049t_driver_api_t {
    int (*int_handle)(struct logic_mcu_ls8a10049t_device_data_t * dev);
};



struct logic_mcu_ls8a10049t_device_data_t * logic_mcu_ls8a10049t_get_device_data(void);

struct logic_mcu_ls8a10049t_driver_api_t * mcu_ls8a10049t_get_api(void);

static inline int logic_mcu_ls8a10049t_int_handle(void)
{
    struct logic_mcu_ls8a10049t_driver_api_t * api = mcu_ls8a10049t_get_api();
    struct logic_mcu_ls8a10049t_device_data_t * dev_data = logic_mcu_ls8a10049t_get_device_data();
	return api->int_handle(dev_data);
}
#endif

extern int logic_mcu_LS8A10049T_init(void);
extern void logic_mcu_ls8a10049t_int_clear(void);
extern void logic_mcu_ls8a10049t_power_off(void);


#endif

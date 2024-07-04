#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <init.h>
#include <kernel.h>

#include <thread_timer.h>
#include <i2c.h>
#include <mspm0l_bsl_i2c.h>
#include <mspm0l_i2c.h>
#include "stdint.h"

#define MCU_ENTER_BOOTLOADER_REG        0x60
#define MCU_GET_FW_VER                  0x03

static struct device *mcu_ota_i2c_dev;
//uint8_t test_d;
uint8_t I2C_writeBuffer(uint8_t *pData, uint8_t ui8Cnt)
{
    int ret;
    union dev_config config = {0};    
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(mcu_ota_i2c_dev, config.raw);

    ret = i2c_write(mcu_ota_i2c_dev, pData, ui8Cnt, I2C_TARGET_ADDRESS);
    if(ret){
        ret = 1;
        printk("[%s,%d] iic_dev write err\n", __FUNCTION__, __LINE__);
    }
    return ret;
}

void I2C_readBuffer(uint8_t *pData, uint8_t ui8Cnt)
{
    union dev_config config = {0};    
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(mcu_ota_i2c_dev, config.raw);

    i2c_read(mcu_ota_i2c_dev,pData,ui8Cnt,I2C_TARGET_ADDRESS);
}

//uint8_t test_d;

uint8_t Status_check(void)
{
    i2c_error_t i2c_ack;
    uint8_t buf[1] = {0xBB};
    i2c_ack        = I2C_writeBuffer(buf, 1);
    I2C_readBuffer(BSL_RX_buffer, 1);
    return BSL_RX_buffer[0];
}

void mcu_mspm0l_notify_enter_bls(void)
{
    union dev_config config = {0};  
    uint8_t data;

    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(mcu_ota_i2c_dev, config.raw);
    data = 1;
    i2c_burst_write(mcu_ota_i2c_dev, I2C_TARGET_ADDRESS, MCU_ENTER_BOOTLOADER_REG, &data, 1);
    printk("[%s,%d] end!!\n",__FUNCTION__, __LINE__);
}

int mcu_get_fw_version(void)
{
    union dev_config config = {0};  
    uint8_t buf[2] = {0};

    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(mcu_ota_i2c_dev, config.raw);    

    if(i2c_burst_read(mcu_ota_i2c_dev, I2C_TARGET_ADDRESS, MCU_GET_FW_VER, buf, 1)<0)
    {
        printk("[%s:%d] MCU_GET_FW_VER: Error!\n", __func__, __LINE__);
        return -EIO;
    }
    printk("[%s:%d] mcu fw ver: %d!\n", __func__, __LINE__,buf[0]);
    return buf[0];
}

int mcu_mspm0l_ota_is_init(void)
{
    if(!mcu_ota_i2c_dev)
    {
        printk("[%s,%d] iic_dev not found\n", __FUNCTION__, __LINE__);
        return 0;
    }
    else{
        return 1;
    }    
}

int mcu_mspm0l_ota_i2c_init(void)
{
    union dev_config config = {0};    
    mcu_ota_i2c_dev = device_get_binding(CONFIG_I2C_0_NAME);

    if(!mcu_ota_i2c_dev)
    {
        printk("[%s,%d] iic_dev not found\n", __FUNCTION__, __LINE__);
        return -EIO;
    }else{
        config.bits.speed = I2C_SPEED_STANDARD;
        i2c_configure(mcu_ota_i2c_dev , config.raw);
    }   
    printk("[%s,%d] end!!\n",__FUNCTION__, __LINE__);
    return 0;
}


#include "hm_gauge_bq28z610_security.h"
#include <zephyr.h>

#include <init.h>
#include <kernel.h>

#include <thread_timer.h>
#include <i2c.h>

#include <gpio.h>
#include <pinmux.h>
#include <sys_event.h>

//#include "hm_charge6.a"

#include "power_supply.h"
#include "power_manager.h"

#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif
#include <msg_manager.h>
#include <soc_pm.h>
#include <pd_manager_supply.h>
#include <property_manager.h>
#include <mbedtls/sha1.h>
#include <stdlib.h>

#define DEBUG_HM_BQ28Z610x
#ifdef DEBUG_HM_BQ28Z610x
#define BQ28Z610_DEBUG printf
#else
#define BQ28Z610_DEBUG(...)
#endif

#define DEBUG_HM_SEC_BQ28Z610x
#ifdef DEBUG_HM_SEC_BQ28Z610x
#define DEBUG_SEC_BQ28Z610 printf
#else
#define DEBUG_SEC_BQ28Z610(...)
#endif


#define AUTHENTICATED

#define SECURITY_AUTH_KEY_LEN 16
#define SECURITY_STRING_LEN 20
#define SECURITY_RSN_LEN    8
#define SECURITY_RSN_OFFSET    12

#define RET_OK (0x00)

#define sec_Register_AltManufacturerAccess_L 0x3e
#define sec_Register_AltManufacturerAccess_M 0x3f
#define sec_Register_MACData_L 0x40
#define sec_Register_MACDatq_checkSum 0x60
#define sec_Register_MACData_length 0x61
#define I2C_BAT_DEV_ADDR    0x55


static se_uint8  bq28z610_i2c_addr = 0x55;

static int i2c_read_data(se_uint8 dev_addr,
				 se_uint8 addr, se_uint8 *buf,
				 se_uint8 num_bytes)
{
    union dev_config config = {0};
    struct device *iic_dev;
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);  
   return i2c_burst_read(iic_dev, dev_addr, addr, buf, num_bytes);
}

static void i2c_write_data(se_uint8 dev_addr,
				 se_uint8 addr, se_uint8 *buf,
				 se_uint8 num_bytes)
{
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);  
    i2c_burst_write(iic_dev, dev_addr, addr, buf, num_bytes);

}


// 320 key （temp key）
#if 1
static se_uint8 battery_auth_key[SECURITY_AUTH_KEY_LEN] =
   // {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
      {0xC2,0xF8,0x1B,0x1C,0x93,0x44,0x59,0x90,0x6C,0x64,0xF5,0xA4,0xEB,0x63,0xFE,0xC1};


static se_uint8 battery_auth_key2[SECURITY_AUTH_KEY_LEN] =
    {0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xca, 0x40, 0xa6, 0xb2, 0x90, 0xef, 0x33, 0x22, 0x11};
#endif

#ifdef DEBUG_SEAL_MODE
static void read_operation_status()
{
    se_uint8 writebuf[] = {0x54, 0x00};
    se_uint8 readbuf[32] = {0};

    hm_i2c_write_buff(I2C_ADDR_BQ28Z610, Register_AltManufacturerAccess_L, writebuf, 2);

    if(RET_OK == hm_i2c_read(I2C_ADDR_BQ28Z610, Register_AltManufacturerAccess_L, readbuf, 32)) {
        se_uint8 i = 0;

        DEBUG_SEC_BQ28Z610("\nace = ");
        for(; i < 32; i++) {
            DEBUG_SEC_BQ28Z610(" 0x%x ", readbuf[i]);
        }
        DEBUG_SEC_BQ28Z610("\n");
    }
}

static void write_security_mode()
{
    se_uint8 writebuf[] = {0x35, 0x00, 0x23, 0x01, 0x67, 0x45, 0xab, 0x89, 0xef, 0xcd};
    // se_uint8 writebuf2[]= {0x00,0x0c};
    se_uint8 checksum = 0;

    hm_i2c_write_buff(I2C_ADDR_BQ28Z610, Register_AltManufacturerAccess_L, writebuf, 10);

    {
        se_uint8 i;
        for(i = 0; i < 10; i++) {
            checksum += writebuf[i];
        }
        checksum = (~checksum);
        DEBUG_SEC_BQ28Z610("\n secchecksum = 0x%x \n", checksum);
    }
    // writebuf2[0]= checksum;
    hm_drvbq28z610_writeReg(Register_MACDatq_checkSum, checksum);
    hm_drvbq28z610_writeReg(Register_MACData_length, 10 + 2);

    // hm_i2c_write_buff(I2C_ADDR_BQ28Z610,Register_AltManufacturerAccess_L, writebuf2, 2);
    // os_msleep(4000);
}

static void write_auth_key()
{
    se_uint8 writeCmd[] = {0x37, 0x00};
    se_uint8 writebuf[SECURITY_AUTH_KEY_LEN] = {0x11, 0x55, 0x34};
    se_uint8 checksum = 0;
    se_uint8 output_string[SECURITY_STRING_LEN] = {0};
    se_uint8 output_string2[SECURITY_STRING_LEN] = {0};

    hm_i2c_write_buff(I2C_ADDR_BQ28Z610, Register_AltManufacturerAccess_L, writeCmd, 2);

    hm_i2c_write_buff(I2C_ADDR_BQ28Z610, Register_MACData_L, writebuf, SECURITY_AUTH_KEY_LEN);

    {
        se_uint8 i;
        for(i = 0; i < SECURITY_AUTH_KEY_LEN; i++) {
            checksum += writebuf[i];
        }
        checksum = (~checksum);
        DEBUG_SEC_BQ28Z610("\n auth checksum = 0x%x \n", checksum);
    }

    hm_drvbq28z610_writeReg(Register_MACDatq_checkSum, checksum);
    hm_drvbq28z610_writeReg(Register_MACData_length, SECURITY_AUTH_KEY_LEN + 4);

    k_sleep(350);

    DEBUG_SEC_BQ28Z610("\n r auth key output = ");
    if(RET_OK == hm_i2c_read(I2C_ADDR_BQ28Z610, Register_MACData_L, output_string, SECURITY_STRING_LEN)) {
        se_uint8 i;
        for(i = 0; i < SECURITY_STRING_LEN; i++) {
            // BQ28Z610_DEBUG(" 0x%x ",output_string[i]);
            output_string2[SECURITY_STRING_LEN - i - 1] = output_string[i];
        }

        for(i = 0; i < SECURITY_STRING_LEN; i++) {
            DEBUG_SEC_BQ28Z610(" 0x%x ", output_string2[i]);
        }
    }
    DEBUG_SEC_BQ28Z610("\n");
}

static void enter_full_acess_mode()
{
    se_uint8 writebuf[] = {0xff, 0xff};
    se_uint8 writebuf2[] = {0xff, 0xff};
    // This full key need ask battery factory first time

    i2c_write_data(bq28z610_i2c_addr, Register_AltManufacturerAccess_L, writebuf, 2);
    i2c_write_data(bq28z610_i2c_addr, Register_AltManufacturerAccess_L, writebuf2, 2);
}

// 0x0414 0x3672
static void enter_unseal_mode()
{
    se_uint8 writebuf[] = {0x14, 0x04};
    se_uint8 writebuf2[] = {0x72, 0x36};
    // This unseal key need ask battery factory first time

    i2c_write_data(bq28z610_i2c_addr, Register_AltManufacturerAccess_L, writebuf, 2);
    i2c_write_data(bq28z610_i2c_addr, Register_AltManufacturerAccess_L, writebuf2, 2);
}

static void full_acess(void)
{
    // write_security_mode();
    read_operation_status();

    enter_unseal_mode();
    enter_full_acess_mode();
    read_operation_status();
    // write_auth_key();
    // write_security_mode();
}
#endif

#ifdef AUTHENTICATED
// extern void sha1( unsigned char *input, int ilen, unsigned char output[20]);
static void battery_sw_key(se_uint8* rnd_string, se_uint8* out)
{
    se_uint8 auth_key[SECURITY_AUTH_KEY_LEN] = {0};
    // This is auth key, this need get from battery factory

    memcpy(auth_key, battery_auth_key, SECURITY_AUTH_KEY_LEN);

    se_uint8 input_string[SECURITY_AUTH_KEY_LEN + SECURITY_STRING_LEN + SECURITY_STRING_LEN + 8] = {0};
    se_uint8 output_string[SECURITY_STRING_LEN] = {0};

    memcpy(input_string, auth_key, SECURITY_AUTH_KEY_LEN);
    memcpy(input_string + SECURITY_AUTH_KEY_LEN, rnd_string, SECURITY_STRING_LEN);
    mbedtls_sha1(input_string, SECURITY_AUTH_KEY_LEN + SECURITY_STRING_LEN, output_string);

    DEBUG_SEC_BQ28Z610("\n Auth output = ");
    {
        se_uint8 i;
        for(i = 0; i < SECURITY_STRING_LEN; i++) {
            DEBUG_SEC_BQ28Z610(" 0x%x ", output_string[i]);
        }
    }
    DEBUG_SEC_BQ28Z610("\n");

    memcpy(input_string, auth_key, SECURITY_AUTH_KEY_LEN);
    memcpy(input_string + SECURITY_AUTH_KEY_LEN, output_string, SECURITY_STRING_LEN);
    mbedtls_sha1(input_string, SECURITY_AUTH_KEY_LEN + SECURITY_STRING_LEN, output_string);

    DEBUG_SEC_BQ28Z610("\n Auth final output = ");
    {
        se_uint8 i;
        for(i = 0; i < SECURITY_STRING_LEN; i++) {
            DEBUG_SEC_BQ28Z610(" 0x%x ", output_string[i]);
        }
    }
    DEBUG_SEC_BQ28Z610("\n");

    memcpy(out, output_string, SECURITY_STRING_LEN);
}



static void battery_sw_key2(se_uint8* rnd_string, se_uint8* out)
{
    se_uint8 auth_key[SECURITY_AUTH_KEY_LEN] = {0};
    // This is auth key, this need get from battery factory

    memcpy(auth_key, battery_auth_key2, SECURITY_AUTH_KEY_LEN);

    se_uint8 input_string[SECURITY_AUTH_KEY_LEN + SECURITY_STRING_LEN + SECURITY_STRING_LEN + 8] = {0};
    se_uint8 output_string[SECURITY_STRING_LEN] = {0};

    memcpy(input_string, auth_key, SECURITY_AUTH_KEY_LEN);
    memcpy(input_string + SECURITY_AUTH_KEY_LEN, rnd_string, SECURITY_STRING_LEN);
    mbedtls_sha1(input_string, SECURITY_AUTH_KEY_LEN + SECURITY_STRING_LEN, output_string);

    DEBUG_SEC_BQ28Z610("\n Auth output = ");
    {
        se_uint8 i;
        for(i = 0; i < SECURITY_STRING_LEN; i++) {
            DEBUG_SEC_BQ28Z610(" 0x%x ", output_string[i]);
        }
    }
    DEBUG_SEC_BQ28Z610("\n");

    memcpy(input_string, auth_key, SECURITY_AUTH_KEY_LEN);
    memcpy(input_string + SECURITY_AUTH_KEY_LEN, output_string, SECURITY_STRING_LEN);
    mbedtls_sha1(input_string, SECURITY_AUTH_KEY_LEN + SECURITY_STRING_LEN, output_string);

    DEBUG_SEC_BQ28Z610("\n Auth final output = ");
    {
        se_uint8 i;
        for(i = 0; i < SECURITY_STRING_LEN; i++) {
            DEBUG_SEC_BQ28Z610(" 0x%x ", output_string[i]);
        }
    }
    DEBUG_SEC_BQ28Z610("\n");

    memcpy(out, output_string, SECURITY_STRING_LEN);
}

static void battery_hw_key(se_uint8* rnd_string, se_uint8* out)
{
    se_uint8 random_string[SECURITY_STRING_LEN] = {0};
    se_uint8 output_string[SECURITY_STRING_LEN] = {0};
    se_uint8 output_string2[SECURITY_STRING_LEN] = {0};
    se_uint8 reg = 0;
    se_uint8 checksum = 0;

    {
        se_uint8 i;
        for(i = 0; i < SECURITY_STRING_LEN; i++) {
            random_string[SECURITY_STRING_LEN - i - 1] = rnd_string[i];
        }
    }

    reg = 0;
    i2c_write_data(bq28z610_i2c_addr, sec_Register_AltManufacturerAccess_L, &reg, 1);
    reg = 0;
    i2c_write_data(bq28z610_i2c_addr, sec_Register_AltManufacturerAccess_M, &reg, 1);

    i2c_write_data(bq28z610_i2c_addr, sec_Register_MACData_L, random_string, SECURITY_STRING_LEN);

    {
        se_uint8 i;
        for(i = 0; i < SECURITY_STRING_LEN; i++) {
            checksum += random_string[i];
        }
        checksum = (~checksum);
        DEBUG_SEC_BQ28Z610("\n checksum = 0x%x \n", checksum);
    }

    reg = checksum;
    i2c_write_data(bq28z610_i2c_addr, sec_Register_MACDatq_checkSum, &reg, 1);
    reg = SECURITY_STRING_LEN + 4;
    i2c_write_data(bq28z610_i2c_addr, sec_Register_MACData_length, &reg, 1);

    k_sleep(300);

    DEBUG_SEC_BQ28Z610("\n Security output = ");
    if(RET_OK == i2c_read_data(bq28z610_i2c_addr, sec_Register_MACData_L, output_string, SECURITY_STRING_LEN)) {
        se_uint8 i;
        for(i = 0; i < SECURITY_STRING_LEN; i++) {
            // BQ28Z610_DEBUG(" 0x%x ",output_string[i]);
            output_string2[SECURITY_STRING_LEN - i - 1] = output_string[i];
        }

        for(i = 0; i < SECURITY_STRING_LEN; i++) {
            DEBUG_SEC_BQ28Z610(" 0x%x ", output_string2[i]);
        }
    }
    DEBUG_SEC_BQ28Z610("\n");
    memcpy(out, output_string2, SECURITY_STRING_LEN);
}

extern u32_t sys_rand32_get(void);

int rand(se_uint32 seed)
{
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;

	return seed;
}

static void battery_rnd_string(se_uint8* out)
{
    se_uint32 rnd_seed = 0;   
    rnd_seed = sys_rand32_get();
	DEBUG_SEC_BQ28Z610("\n battery_rnd_string rnd_seed = %x ", rnd_seed);
    for(se_uint8 i = 0; i < SECURITY_STRING_LEN; i++) {
        out[i] = rand(rnd_seed);
        DEBUG_SEC_BQ28Z610(" 0x%x ", out[i]);
    }
    DEBUG_SEC_BQ28Z610("\n");
}

unsigned char is_authenticated(void)
{
    se_bool ret = 0;
    se_uint8 random_string[SECURITY_STRING_LEN] = {0};
    se_uint8 hw_string[SECURITY_STRING_LEN] = {0};
    se_uint8 sw_string[SECURITY_STRING_LEN] = {0};

    battery_rnd_string(random_string);
    battery_sw_key(random_string, sw_string);
    battery_hw_key(random_string, hw_string);

    if(memcmp(hw_string, sw_string, SECURITY_STRING_LEN) == 0) {
        BQ28Z610_DEBUG("\n is authenticated! \n");
        ret = 1;
    } else {
        BQ28Z610_DEBUG("\n not authenticated! \n");
        ret = 0;
    }

    return ret;
}

se_bool is_backup_authenticated(se_uint8* sn, se_uint8* rsn)
{
    se_bool ret = 0;
    se_uint8 random_string[SECURITY_STRING_LEN] = {0};
    se_uint8 sw_string[SECURITY_STRING_LEN] = {0};

    memcpy(random_string, sn, SECURITY_AUTH_KEY_LEN);

    battery_sw_key2(random_string, sw_string);


    if(memcmp(rsn, sw_string + SECURITY_RSN_OFFSET, SECURITY_RSN_LEN) == 0) {
        BQ28Z610_DEBUG("\n is bk authenticated! \n");
        ret = 1;
    } else {
        BQ28Z610_DEBUG("\n not bk authenticated! \n");
        ret = 0;
    }

    return ret;
}


#endif

void battery_hw_test(void)
{
	//se_uint8 cmd_id[] = {0x01,0x00};
	se_uint8 cmd_Chemical_ID0[] = {0x70,0x00};
    se_uint8 cmd_Chemical_ID1[] = {0x71,0x00};
    se_uint8 cmd_Chemical_ID2[] = {0x72,0x00};
    se_uint8 cmd_Chemical_ID3[] = {0x73,0x00};
    se_uint8 cmd_Chemical_ID4[] = {0x74,0x00};
	static se_uint8 output_string[40] = {0};
	
    i2c_write_data(bq28z610_i2c_addr, sec_Register_AltManufacturerAccess_L, cmd_Chemical_ID0, 2);
	i2c_read_data(bq28z610_i2c_addr,sec_Register_AltManufacturerAccess_L,output_string,32);
	//print_buffer_lazy("output_string:", output_string, 32);

    i2c_write_data(bq28z610_i2c_addr, sec_Register_AltManufacturerAccess_L, cmd_Chemical_ID1, 2);
	i2c_read_data(bq28z610_i2c_addr,sec_Register_AltManufacturerAccess_L,output_string,32);
	//print_buffer_lazy("output_string:", output_string, 32);    

    i2c_write_data(bq28z610_i2c_addr, sec_Register_AltManufacturerAccess_L, cmd_Chemical_ID2, 2);
	i2c_read_data(bq28z610_i2c_addr,sec_Register_AltManufacturerAccess_L,output_string,14);
	//print_buffer_lazy("output_string:", output_string, 14);  

    i2c_write_data(bq28z610_i2c_addr, sec_Register_AltManufacturerAccess_L, cmd_Chemical_ID3, 2);
	i2c_read_data(bq28z610_i2c_addr,sec_Register_AltManufacturerAccess_L,output_string,24);
	//print_buffer_lazy("output_string:", output_string, 24);     

    i2c_write_data(bq28z610_i2c_addr, sec_Register_AltManufacturerAccess_L, cmd_Chemical_ID4, 2);
	i2c_read_data(bq28z610_i2c_addr,sec_Register_AltManufacturerAccess_L,output_string,24);
	//print_buffer_lazy("output_string:", output_string, 24);              
}




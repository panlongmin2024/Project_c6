/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file SoC init for Actions SoC.
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <soc.h>
#include <arch/cpu.h>
#include <string.h>
#include <nvram_config.h>
#include <crypto/rsa.h>
#include <mem_manager.h>
#include <thread_timer.h>
#include <gpio.h>

#define UUID_STR_DATA_LEN       (32)
#define UUID_MSG_SIGNATURE_LEN  (256)
#define UUID_HASH_DATA_LEN      (32)
#define UUID_SIGN_MSG_NAME "uuid_msg"

#define	TIMEOUT_UUID_VERIFY_FAIL_POWEROFF		5000

struct uuid_ctx{
    uint32_t uuid[4];
    uint8_t hash_ctx[128];
    uint8_t uuid_sig_data[UUID_MSG_SIGNATURE_LEN];
	uint8_t uuid_str[UUID_STR_DATA_LEN + 1];
};

extern int hex2bin(uint8_t *dst, const char *src, unsigned long count);
extern char *bin2hex(char *dst, const void *src, unsigned long count);

static const unsigned int g_pubkey[]   = {
    0x54c0ea61,
	0x5e62c65f, 0x21b60366, 0x3426ab42, 0xd154e2d9, 
	0x2fcd5dda, 0x341fc978, 0x62c4a8bc, 0x30327a43, 
	0x854558c6, 0xa8812a52, 0x86144470, 0xb4a4b827, 
	0x3e25f331, 0x90d6712f, 0xa4b376fa, 0xf86f424d, 
	0x0837d001, 0x2fc1afc4, 0xf2c13019, 0x86ab2010, 
	0x948792b9, 0xf374290b, 0x526d980f, 0x42b31237, 
	0x2c6cfee8, 0xe892c8f6, 0xdc1d20c8, 0x322cb3e4, 
	0xac76565f, 0xe56ed637, 0x1d870674, 0x917d4e83, 
	0xc3cddfa2, 0xcc43622e, 0xb0de97cc, 0xa246e8d8, 
	0xf0b2d1fb, 0x8da2e1b7, 0x9bcfe6df, 0x1be209dc, 
	0xf2047e3e, 0xb17370ee, 0x2a2ad4f9, 0x865c6494, 
	0x94a52b72, 0x03a5bd49, 0x7f18479f, 0x9b16f0a6, 
	0x44ec4cc1, 0x71fbf32b, 0x9f529cc4, 0x57001c92, 
	0x8fe59b22, 0x3d4b4300, 0x958edf7c, 0xa69874a3, 
	0x307db7e9, 0xebf8ff58, 0x5d06f0bb, 0xea2af5d9, 
	0x900d9582, 0xfc038e29, 0xf3e6a376, 0x931d2df8, 
	0xed14934d, 0xff0bc412, 0xac742ee5, 0xdffb3406, 
	0x1152a5d3, 0x6221e1fd, 0x74199fc3, 0x62edca3f, 
	0xa5220512, 0x3b5efda0, 0x04bbf4f2, 0xb744badc, 
	0x81d6a389, 0xc0c5a17b, 0xd82eff4a, 0xf707deca, 
	0x1a2e4463, 0xdc775584, 0xe947e52a, 0x22c74024, 
	0x588d6362, 0xb2c672b5, 0xc52da4fe, 0x9eea58e1, 
	0xecd93e90, 0xf4643edb, 0xb6dcfc56, 0x61cbc00a, 
	0xc4e0bffe, 0xb5696432, 0xb513aa73, 0xba8b4360, 
	0xcd884edd, 0x5db4bc4e, 0x93d6e604, 0x47cc586c, 
	0x1be08383, 0xbd15c0f6, 0x2a3d1644, 0x4fbb51d4, 
	0xb09d1542, 0xd3c30625, 0x3c164233, 0x8457dd2e, 
	0xd39ca4bf, 0x3a032aaf, 0xa108b8c5, 0x06512ee2, 
	0x76449b32, 0xf7bdb464, 0x476a3d88, 0xe78033f7, 
	0xc5b6fa6f, 0x04656913, 0x272c99c7, 0x7b125159, 
	0x878809e8, 0xd792e2bb, 0x985e4e3a, 0x401af32b, 
	0x075fad2b, 0x9686f314, 0xde879a9a, 0x77a9d4e9, 
};

struct thread_timer uuid_verify_timer;
/* verify fail delay xs poweroff */
static void timeout_poweroff(struct thread_timer *timer, void* pdata)
{
	printk("----> %s %d\n",__func__,__LINE__);
	sys_pm_poweroff();
}
static void enter_verify_fail_func(void)
{
	printk("----> %s %d\n",__func__,__LINE__);
	if (thread_timer_is_running(&uuid_verify_timer)){
		thread_timer_stop(&uuid_verify_timer);
	}	
	thread_timer_init(&uuid_verify_timer, timeout_poweroff, NULL);
	thread_timer_start(&uuid_verify_timer, TIMEOUT_UUID_VERIFY_FAIL_POWEROFF, 0);

}
static void enter_verify_ok_func(void)
{
	//nothinf to do
	printk("----> %s %d\n",__func__,__LINE__);
}
static uint8_t* sys_get_uuid_hash(struct uuid_ctx *ctx)
{
	rom_sha256_init(ctx->hash_ctx);

	rom_sha256_update(ctx->hash_ctx, ctx->uuid_str, UUID_STR_DATA_LEN);

	return (uint8_t *)rom_sha256_final(ctx->hash_ctx);
}

static int sys_uuid_verify(void)
{
    struct uuid_ctx *ctx;
    uint8_t *hash_data;
    int rlen, ret;

    ctx = mem_malloc(sizeof(struct uuid_ctx));

    if(!ctx){
        return -ENOMEM;
    }

    soc_get_system_uuid(ctx->uuid);
	bin2hex(ctx->uuid_str, ctx->uuid, 16);
	ctx->uuid_str[32] =  0;
    printk("uuid: %x-%x-%x-%x, hex=%s\r\n", ctx->uuid[0], ctx->uuid[1], ctx->uuid[2], ctx->uuid[3], ctx->uuid_str);

    hash_data = sys_get_uuid_hash(ctx);

	print_buffer_lazy("uuid sha256:", hash_data, 32);

	rlen = nvram_config_get_factory(UUID_SIGN_MSG_NAME, ctx->uuid_sig_data, UUID_MSG_SIGNATURE_LEN);
	if( rlen != UUID_MSG_SIGNATURE_LEN){
		printk("uuid nv-r: sign rlen=%d != %d ", rlen, UUID_MSG_SIGNATURE_LEN);
        mem_free(ctx);
		
		extern uint8_t ReadODM(void);
		if(ReadODM() == 1){
			struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
			u32_t val,val_back,i;
			gpio_pin_configure(gpio_dev, 22, GPIO_DIR_IN);
			gpio_pin_read(gpio_dev, 22, &val);
			val_back = val;

			for(i = 0;i<3;i++){
				k_sleep(20);
				gpio_pin_read(gpio_dev, 22, &val);
				if(val != val_back)
					return -1;
			}
			printk("sys_uuid_verify fail DV?=%d\n",val);
			if(val == 1)
				enter_verify_fail_func();
		}
		return -1;
	}


    ret = RSA_verify_hash((const unsigned char *)g_pubkey, ctx->uuid_sig_data, UUID_MSG_SIGNATURE_LEN, hash_data, UUID_HASH_DATA_LEN);

	if(ret != 0){
        printk("uuid verify fail %d\n", ret);
		enter_verify_fail_func();
	}else{
		printk("uuid verify ok\r\n");
		enter_verify_ok_func();
	}

    mem_free(ctx);

	return ret;
}

static int uuid_init(struct device *dev)
{
	//sys_uuid_verify();
	return 0;
}

SYS_INIT(uuid_init, PRE_KERNEL_2, 10);




/*sign msg write to nvram*/
static const char uuid_sign_msg_hex[513] = \
"29484c36343b2ba23c53d919c7403efc2332dae0809e4816c567148a20d4e70ead6d800ab2363dd04d178c9291ce67fd69cc2fd77f1d45f99dc83fc786c227b9070f1a6243b17430a52db2352119ce5762de28484aef72322f2a025bcb1c13dfcc56f907c32e1f9e9b73054e656cdccaca53fcfaaee2bbdeb6191de6b22878a522337ec290c5f05bf57089f471d6feee2c8d99def6244f11353de9459b7a83d30943f3e1006a47390bca1c3e2ce9548016c96216b4e410ee44d23177acfca945cd29f77aa6d90803595d91f4a78f5978c163cf19f58e806328260b9c0eb851c8d7ec5c37e3e442f8eb7a0105f348501575031766eadcc8a3cc9c76a7a2531fa2";

int uuid_sign_msg_write(int is_read)
{
	char  str_msg[33];
	int i,  rlen, len;
    char *sign_msg = mem_malloc(UUID_MSG_SIGNATURE_LEN);

    if(!sign_msg){
        return -ENOMEM;
    }

	if(is_read) {
		rlen = nvram_config_get_factory(UUID_SIGN_MSG_NAME, sign_msg, UUID_MSG_SIGNATURE_LEN);
		printk("uuid nv: rlen= %d\r\n", rlen);
		str_msg[32] = 0;
		i = 0;
		while(rlen > 0){
			if( rlen > 16 )
				len = 16;
			else
				len = rlen;
			bin2hex(str_msg, &sign_msg[i*16], 16);
			printk("%d-hex:%s\r\n", i, str_msg);
			rlen -= len;
			i++;
		}
	} else{
			rlen = hex2bin(sign_msg, uuid_sign_msg_hex, UUID_MSG_SIGNATURE_LEN);
			printk("write sig msg to nv: wlen=%d\r\n", rlen);

			if(rlen == 0){
				nvram_config_set_factory(UUID_SIGN_MSG_NAME, sign_msg , UUID_MSG_SIGNATURE_LEN);
			}
	}
	return 0;
}

void user_uuid_init(void)
{
	sys_uuid_verify();
}

int user_uuid_verify(void)
{
	return sys_uuid_verify();
}

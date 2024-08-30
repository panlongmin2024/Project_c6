/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <zephyr.h>
#include <soc.h>
#include <image.h>
#include <sdfs.h>
#include <logging/sys_log.h>
#include <property_manager.h>


typedef struct{
	u8_t id;
	char* name;
} bin_test_info_t;

//#0:none bin test; 1:fcc; 2:produce; 3:srrc
typedef enum {
	BIN_TEST_ID_NONE = 0,
	BIN_TEST_ID_FCC,
	BIN_TEST_ID_PORDUCE,
	BIN_TEST_ID_SRRC,
	BIN_TEST_ID_MAX,
} bin_test_id_e;

const bin_test_info_t bin_info[]=
{
	{.id = BIN_TEST_ID_NONE,	.name = NULL},
	{.id = BIN_TEST_ID_FCC,		.name = "pt1.bin"},
	{.id = BIN_TEST_ID_PORDUCE,	.name = "pd1.bin"},
	{.id = BIN_TEST_ID_SRRC,	.name = "srrc.bin"},
};



#define FCC_OVER 0xefa55afe
#define RTC_BAK0 0xc0120030

#define WD_CTL_EN		(1 << 4)
#define WD_CTL_CLR		(1 << 0)

void system_reboot(void)
{
	sys_write32(WD_CTL_CLR, WD_CTL);
	sys_write32(WD_CTL_EN, WD_CTL);
	while (1) ;
}

/*
**  Refresh the RTC domain register
*/
#define UPDATE_MAGIC 0xA596
#define UPDATE_OK 0x5A69
static void update_rtc_regs(void)
{
#if 1
	sys_write32(UPDATE_MAGIC, RTC_REGUPDATE);

	while (sys_read32(RTC_REGUPDATE) != UPDATE_OK) {
		;		// wait rtc register update
	}
#endif
}

uint32_t img_code_load(char *file_name)
{
	struct sd_file *file_p;
	image_head_t *p_img;
	uint32_t address = 0;

	file_p = sd_fopen(file_name);

	if (file_p) {
		p_img = (image_head_t *) file_p->start;
		printk
		    ("name %s, load_addr 0x%x, entery 0x%x, size 0x%x\n",
		     p_img->name, (int)p_img->load_addr,
		     (int)p_img->entry, (file_p->size));

		printk("version 0x%04x\n", p_img->version);

		if ((file_p->size)) {
			memcpy((void *)p_img->load_addr,
			       (void *)file_p->start, file_p->size);

		}
		address = (uint32_t) p_img->entry;

		sd_fclose(file_p);
	} else {
		SYS_LOG_INF("cannot open %s.", file_name);
		while(1)
		{
			k_sleep(1000);
		}
	}

	return address;
}

void img_code_manage(void)
{
	void (*img_code_entry)(void);

	char *IMAGE_NAME = NULL;

	int bin_id = property_get_int(CFG_BIN_TEST_ID, -1);
	bin_id = BIN_TEST_ID_SRRC;//BIN_TEST_ID_PORDUCE
	if(bin_id <= BIN_TEST_ID_NONE || bin_id >= BIN_TEST_ID_MAX)
		return;

	u8_t i = 0;
	for( i = 0; i < ARRAY_SIZE(bin_info); i++)
	{
		if(bin_info[i].id == bin_id)
		{
			break;
		}
	}

	if(i >= ARRAY_SIZE(bin_info))
		return;

	IMAGE_NAME = bin_info[i].name;

	if(!IMAGE_NAME)
		return;

#ifndef CONFIG_ACTIONS_IMG_TEST_ALWAYS
	u8_t mode = '0';
	SYS_LOG_INF("reset bin test id\n");
	int ret = property_set(CFG_BIN_TEST_ID, &mode, 1);
	if (ret < 0) {
		return;
	}
	property_flush(CFG_BIN_TEST_ID);
#endif

	SYS_LOG_INF("load %s\n", IMAGE_NAME);

	img_code_entry = (void (*)(void))(img_code_load(IMAGE_NAME));
	if (NULL != img_code_entry) {
		sys_write32(0x0, WD_CTL);
		SYS_LOG_INF("irq_lock, and goto 0x%x", (int)img_code_entry);
		irq_lock();
		img_code_entry();
	} else {
		SYS_LOG_INF("Cannot load %s\n", IMAGE_NAME);
	}
}

int run_test_image(void)
{

	img_code_manage();

	sys_write32(0, RTC_BAK0);
	update_rtc_regs();

	return 0;
}

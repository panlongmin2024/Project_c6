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


#ifdef CONFIG_ACTIONS_IMG_FCC
#define IMAGE_NAME "pt1.bin"
#else
#ifdef CONFIG_ACTIONS_IMG_RF_NS
#define IMAGE_NAME "pd1.bin"
#endif
#endif

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

		if ((file_p->size)) {
			memcpy((void *)p_img->load_addr,
			       (void *)file_p->start, file_p->size);

		}
		address = (uint32_t) p_img->entry;

		sd_fclose(file_p);
	} else {
		SYS_LOG_INF("cannot open %s.", file_name);
	}

	return address;
}

void img_code_manage(void)
{
	void (*img_code_entry)(void);

	SYS_LOG_INF("load %s\n", IMAGE_NAME);

	img_code_entry = (void (*)(void))(img_code_load(IMAGE_NAME));
	if (NULL != img_code_entry) {
		SYS_LOG_INF("irq_lock, and goto 0x%x", (int)img_code_entry);
		irq_lock();
		img_code_entry();
	} else {
		SYS_LOG_INF("Cannot load %s\n", IMAGE_NAME);
	}
}

int run_test_image(void)
{
	u32_t fcc_status = sys_read32(RTC_BAK0);
	SYS_LOG_INF("status 0x%x", fcc_status);
	if (fcc_status != FCC_OVER) {
		sys_write32(0x0, WD_CTL);

		img_code_manage();

		SYS_LOG_INF("System reboot\n");
		system_reboot();
	} else {
		sys_write32(0, RTC_BAK0);
		update_rtc_regs();
	}

	return 0;
}

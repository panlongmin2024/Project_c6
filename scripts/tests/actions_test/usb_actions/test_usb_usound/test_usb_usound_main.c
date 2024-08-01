/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      hujunhua   2017年1月23日-下午2:11:44             1.0             build this file
 ********************************************************************************/
/*!
 * \file     test_usb_stub.c
 * \brief
 * \author
 * \version  1.0
 * \date  2017年1月23日-下午2:11:44
 *******************************************************************************/
#define SYS_LOG_LEVEL 4
#define SYS_LOG_DOMAIN "test-usb-usound-main"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TEST_USB_USOUND_MAIN

#include <zephyr.h>
#include <init.h>
#include <string.h>

#include <usb/usb_device.h>

#include "usb_handler.h"
#include <greatest_api.h>
#include <usb/class/usb_hid.h>
#include <audio_usound_hal.h>



//#define DEBUG_HID

#define REPORT_ID_1		0x01
#define REPORT_ID_2		0x02


//extern int usb_usound_ep_write_dma(void);

/*int test_usb_usound_main(void)
{
    //uint8_t key_value = 0x20;
    //int ret = 0, wrote = 0;

	//u8_t report_1[2] = { REPORT_ID_1, 0x00 };

    	while (true) {

		k_sleep(K_SECONDS(2));

        //usb_usound_ep_write_dma();

		//ret = hid_int_ep_write(report_1, sizeof(report_1), &wrote);
		//ACT_LOG_ID_INF(ALF_STR_XXX__HID_WROTE_D_BYTES_WI, 2, wrote, ret);
	}

    usb_audio_source_exit();
	return 0;
}*/


void test_usb_usound_main(void)
{
    int ret = 0;
    u8_t i = 0;
    //char g_test_data[8] = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0xAA, 0xBB};
    
    char g_test_data[64] = {0};
	ACT_LOG_ID_INF(ALF_STR_test_usb_usound_main__NOW_TEST_USOUND, 0);

    for (i = 0; i < 64; i++)
    {
        g_test_data[i] = i;
    }

    ACT_LOG_ID_INF(ALF_STR_test_usb_usound_main__INT_USOUNDN, 0);
    //usb_stub_init();
    usound_drv_init();

    while (true) {

	if (!usb_audio_source_enabled()) {
		k_sleep(K_SECONDS(2));
		continue;
	}

    ACT_LOG_ID_INF(ALF_STR_test_usb_usound_main__USOUND_READY_OKN, 0);
    ret = usb_hid_ep_write(g_test_data, sizeof(g_test_data),K_FOREVER);
    if (ret != 0)
    {
        ACT_LOG_ID_INF(ALF_STR_test_usb_usound_main__TEST_WRITE_RET_DN, 1, ret);
        break;
    }
 
#ifdef CONFIG_HID_INTERRUPT_OUT
    ret = usb_hid_ep_read(g_test_data, sizeof(g_test_data),K_FOREVER);
    if (ret != 0)
    {
        ACT_LOG_ID_INF(ALF_STR_test_usb_usound_main__READ_RET_DN, 1, ret);
        break;
    }
#endif
    }

     ACT_LOG_ID_INF(ALF_STR_test_usb_usound_main__EXIT_USOUNDN, 0);
     //usb_stub_exit();
     //usb_audio_source_exit();
     usound_drv_exit();
}



TEST tts_usb_usound(void)
{
    test_usb_usound_main();
}


ADD_TEST_CASE("test_usb_usound")
{
    RUN_TEST(tts_usb_usound);
}

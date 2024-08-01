/*
 * Copyright (c) 2018 Actions Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SYS_LOG_LEVEL 4
#define SYS_LOG_DOMAIN "main"

#include <greatest_api.h>
#include <stub_hal2.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TEST_USB_STUB

void test_usb_stub(void)
{
    int ret = 0;
    char g_test_data[8] = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0xAA, 0xBB};
	ACT_LOG_ID_INF(ALF_STR_test_usb_stub__NOW_TEST_USB_STUB, 0);

    ACT_LOG_ID_INF(ALF_STR_test_usb_stub__INT_STUBN, 0);
    usb_stub_init();

    while (true) {

    #ifdef CONFIG_USB_DEVICE_SS
	if (!whether_usb_stub_drv_ready()) {
		k_sleep(K_SECONDS(2));
		continue;
	}
    #endif

    ACT_LOG_ID_INF(ALF_STR_test_usb_stub__STUB_READY_OKOKN, 0);
    ret = usb_stub_ep_write(g_test_data, sizeof(g_test_data),K_FOREVER);
    if (ret != 0)
    {
        ACT_LOG_ID_INF(ALF_STR_test_usb_stub__TEST_WRITE_RET_DN, 1, ret);
        break;
    }
    
    ret = usb_stub_ep_read(g_test_data, sizeof(g_test_data),K_FOREVER);
    if (ret != 0)
    {
        ACT_LOG_ID_INF(ALF_STR_test_usb_stub__READ_RET_DN, 1, ret);
        break;
    }
    }

     ACT_LOG_ID_INF(ALF_STR_test_usb_stub__EXIT_STUBN, 0);
     usb_stub_exit();
}

/*#ifdef CONFIG_USB_COMPOSITE_DEVICE
static int composite_pre_init(struct device *dev)
{
	ss_register_status_cb(stub_cb_usb_status);
	ss_register_ops(stub_in_ready, stub_out_ready);

	stub_init1();

	return 0;
}

SYS_INIT(composite_pre_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
#endif*/


TEST tts_usb_stub(void)
{
    test_usb_stub();
}


ADD_TEST_CASE("test_usbstub")
{
    RUN_TEST(tts_usb_stub);
}


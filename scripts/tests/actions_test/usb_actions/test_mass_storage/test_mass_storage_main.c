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
#define SYS_LOG_DOMAIN "test-mass-storage-main"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TEST_MASS_STORAGE_MAIN

#include <zephyr.h>
#include <greatest_api.h>

int test_mass_storage_main(void)
{
    int ret = 9;
	/* Nothing to be done other than the selecting appropriate build
	 * config options. Everything is driven from the USB host side.
	 */
	ACT_LOG_ID_INF(ALF_STR_test_mass_storage_main__THE_DEVICE_IS_PUT_IN, 0);
    while (true) {

		k_sleep(K_SECONDS(2));

		//report_1[1]++;

        
		//ret = hid_int_ep_write(&key_value, 1, &wrote);

		//ret = hid_int_ep_write(report_1, sizeof(report_1), &wrote);
		ACT_LOG_ID_INF(ALF_STR_test_mass_storage_main__HID_WROTE_BYTES_WITH, 1, ret);
	}
    
	ACT_LOG_ID_INF(ALF_STR_test_mass_storage_main__MASS_STORAGE_ENDN, 0);
    return 0;
}
TEST tts_mass_storage(void)
{
    test_mass_storage_main();
}


RUN_TEST_CASE("test_mass")
{
    RUN_TEST(tts_mass_storage);
}

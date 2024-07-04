/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      hujunhua   2019年9月24日-上午11:15:44             1.0          test_fat_fs_basic_operations
 ********************************************************************************/
/*!
 * \file     test_fat_fs_main.c
 * \brief
 * \author
 * \version  1.0
 * \date  2019年9月24日-上午11:15:44
 ******************************************************************************/
#define SYS_LOG_LEVEL 4
#define SYS_LOG_DOMAIN "test-fat-fs-main"
#include <zephyr.h>
#include <init.h>
#include <greatest_api.h>
#include <test_fat.h>
#include <usb/storage.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TEST_FAT_FS_MAIN


fs_file_t filep;

extern int usb_storage_exit();

int test_detect_disk_uhost(u8_t* pstate)
{
    int res = 0;
    //u8_t state = 0;

    TC_PRINT("\ndisk detect tests:\n");
    //f_chdrive(system_volume_name[DISK_USB]);
    res = fs_disk_detect(TEST_FILE, pstate);
    ACT_LOG_ID_INF(ALF_STR_test_detect_disk_uhost__DETECT_D_STATE_DN, 2, res,*pstate);
    if (res)
    {
        ACT_LOG_ID_INF(ALF_STR_test_detect_disk_uhost__TEST_DETECT_UHOST_ER, 0);
    }
    return res;
}


void fat_fs_main(void)
{
    u8_t state = 0;

    ACT_LOG_ID_INF(ALF_STR_fat_fs_main__START_FAT_FS_MAINN, 0);

    test_detect_disk_uhost(&state);
    if (state == USB_DISK_CONNECTED){
        test_fat_file();
        test_fat_dir();
        test_fat_fs();
    }else if (state == USB_DISK_DISCONNECTED){
        ACT_LOG_ID_INF(ALF_STR_fat_fs_main__NOT_DETECT_UHOST_INN, 0);
    }else{
        ;
    }

    //ACT_LOG_ID_INF(ALF_STR_fat_fs_main__USB_STORAGE_EXITN, 0);
    //usb_storage_exit();
    //fat_fs_basic_test();
	/*ztest_test_suite(fat_fs_basic_test,
			 ztest_unit_test(test_fat_file),
			 ztest_unit_test(test_fat_dir),
			 ztest_unit_test(test_fat_fs));
	ztest_run_test_suite(fat_fs_basic_test);*/
}



TEST tts_fat_fs_main(void)
{
    fat_fs_main();
}


RUN_TEST_CASE("test_fat_fs_api")
{
    RUN_TEST(tts_fat_fs_main);
}

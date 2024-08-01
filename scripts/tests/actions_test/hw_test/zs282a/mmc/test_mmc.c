/********************************************************************************
 *                            USDK(ATS350B_linux)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-8-8-下午2:39:39             1.0             build this file
 ********************************************************************************/
/*!
 * \file     test_mmc_0.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-8-8-下午2:39:39
 *******************************************************************************/
#include <sdk.h>
#include <flash.h>
#include <greatest_api.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TEST_MMC

#define DISK_SPEED_TEST

#ifdef DISK_SPEED_TEST
#include <input_dev.h>
#define DISK_TEST_LEVEL_SD

int disk_test_config(int key);
static void disk_test_init(void);
#endif

#ifdef DISK_SPEED_TEST
#ifdef DISK_TEST_LEVEL_SD
#define SD_SECTOR_SIZE 512
#define SD_SECTOR_OFFSET 16666
#else
static const char* test_file="SD:yingshanhong.wav";
#endif
static int perf_count = 500;
static int perf_len = 512;
#define PERF_LEN_NUM 8
static int perf_lens[PERF_LEN_NUM] = { 35, 230, 511, 512, 1000, 1024, 1536, 4096};
static char disk_buff[4096];

static bool disk_test_enable = false;

#define MY_STACK_SIZE 1024
//#define MY_PRIORITY CONFIG_APP_PRIORITY
#define MY_PRIORITY 10

static struct k_thread my_thread_data_disk;
K_THREAD_STACK_DEFINE(my_stack_area_disk, 1024);

static void disk_test_switch(void)
{
    disk_test_enable = !disk_test_enable;

    if (disk_test_enable) {
        ACT_LOG_ID_INF(ALF_STR_disk_test_switch__TEST_SWITCH_ON_COUNT, 2, perf_count, perf_len);
    } else {
        ACT_LOG_ID_INF(ALF_STR_disk_test_switch__TEST_SWITCH_OFFN, 0);
    }
}

int disk_test_config(int key)
{
    static int mode = 0;
    int ret=0;

    if (key == (0x4000000|KEY_PREVIOUSSONG)) {
        mode++;
        perf_len = perf_lens[mode % PERF_LEN_NUM];
        perf_count=9000;
        ACT_LOG_ID_INF(ALF_STR_disk_test_config__PERF_LENDN, 1, perf_len);
        ret = 1;
    }

    if (key == (0x4000000|KEY_VOLUMEUP)) {
        disk_test_switch();
        ret = 1;
    }

    return ret;
}

static void disk_report(int id, unsigned int ms, unsigned int size)
{
    ACT_LOG_ID_INF(ALF_STR_disk_report__DISK_SUMMARYN, 0);
    if (ms != 0) {
        ACT_LOG_ID_INF(ALF_STR_disk_report__FISHDDONE_D_IN_DMS_S, 4, id, size, ms, size / ms);
    } else {
        ACT_LOG_ID_INF(ALF_STR_disk_report__FISHDDONE_D_IN_0SN, 2, id, size);
    }
}

static void disk_test_thread(void *p1, void *p2, void *p3)
{
    u32_t ms, count = 0;
    static s64_t time_stamp = 0;
    int ret;
    ssize_t i;
#ifdef DISK_TEST_LEVEL_SD
    int sectors;
    struct device * sd_disk = NULL;
    const struct flash_driver_api * api = NULL;
#else
    fs_file_t zfp;
#endif

    while (1) {
        if (disk_test_enable) {
            if(count == 0){
#ifdef DISK_TEST_LEVEL_SD
                sd_disk = device_get_binding(CONFIG_MMC_SDCARD_DEV_NAME);
                if(NULL == sd_disk) {
                    ret = -1;
                } else {
                    ret = 0;
                    api = sd_disk->driver_api;
                }

#else
                printk("fs_open: %s\n", test_file);
                ret = fs_open(&zfp, test_file);
#endif
                if(ret == 0){
                    time_stamp = k_uptime_get_32();
                    count++;
                }else{
                   ACT_LOG_ID_INF(ALF_STR_disk_test_thread__FS_OPEN_RETDN, 1, ret);
                }
            }else if(count >= perf_count){
                ms = k_uptime_delta_32(&time_stamp);
#ifdef DISK_TEST_LEVEL_SD
                ret = 0;
#else
                ret = fs_close(&zfp);
#endif
                disk_test_enable = false;
                if(ret == 0){
                    disk_report(0, ms, perf_len * (count - 1));
                    count=0;
                }else{
                   ACT_LOG_ID_INF(ALF_STR_disk_test_thread__FS_CLOSE_RETDN, 1, ret);
                }
                ACT_LOG_ID_INF(ALF_STR_disk_test_thread__FS_CLOSE_CLOSEDN, 0);
            }else{
#ifdef DISK_TEST_LEVEL_SD
                sectors = perf_len/SD_SECTOR_SIZE;
                if(sectors > 0){
                    i = api->read(sd_disk, SD_SECTOR_OFFSET+count*(sectors+1), disk_buff, sectors);
                    //ACT_LOG_ID_INF(ALF_STR_disk_test_thread__SD_DISK_READ_D_LENDN, 2, count, i);
                }else{
                    ACT_LOG_ID_INF(ALF_STR_disk_test_thread__ERR_SECTORS_INVALIDN, 0);
                }
#else
                i = fs_read(&zfp, disk_buff, perf_len);
                //ACT_LOG_ID_INF(ALF_STR_disk_test_thread__FS_READ_COUNTD_RETDN, 2, count, i);
#endif
                count++;
            }
        } else {
            k_sleep(1000);
        }
    }
}

static void disk_test_init(void)
{
    //k_sem_init(&sem_disk_test, 1, 1);
    k_thread_create(&my_thread_data_disk, my_stack_area_disk,
                    K_THREAD_STACK_SIZEOF(my_stack_area_disk),
                    disk_test_thread, (void *)1, NULL, NULL,
                    MY_PRIORITY, 0, 0);
}
#endif
TEST mmc_init(void){
    struct device *sd_dev;
    char test_buffer[512];
    int ret_val;

    ACT_LOG_ID_INF(ALF_STR_mmc_init__ENTER_MMC_TESTN, 0);

    sd_dev = device_get_binding("sd-storage");

    TEST_ASSERT_NOT_NULL(sd_dev);

    ret_val = flash_init(sd_dev);

    TEST_ASSERT_EQ_FMT(0, ret_val, "%d");

    ret_val = flash_read(sd_dev, 0, test_buffer, 1);

    TEST_ASSERT_EQ_FMT(0, ret_val, "%d");

#ifdef DISK_SPEED_TEST
#if 0
    disk_test_config(0x4000000|KEY_PREVIOUSSONG);
    disk_test_config(0x4000000|KEY_PREVIOUSSONG);
    disk_test_config(0x4000000|KEY_PREVIOUSSONG);
    disk_test_switch();
#endif
    disk_test_init();
#endif

    ACT_LOG_ID_INF(ALF_STR_mmc_init__LEAVE_MMC_TESTN, 0);
}

RUN_TEST_CASE("mmc0 test")
{
    RUN_TEST(mmc_init);
}


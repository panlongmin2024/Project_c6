/********************************************************************************
 *                            USDK(ZS283A_clean)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-11-8-上午10:46:06             1.0             build this file
 ********************************************************************************/
/*!
 * \file     crash_test.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-11-8-上午10:46:06
 *******************************************************************************/
#include <kernel.h>
#include <soc.h>
#include <misc/printk.h>
#include <malloc.h>
#include <string.h>
#include<stdlib.h>
#include <shell/shell.h>
#include <init.h>
#include <debug/object_tracing.h>
#if defined(CONFIG_KALLSYMS)
#include <kallsyms.h>
#endif
#if defined(CONFIG_SYS_LOG)
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_CRASH_TEST
#endif

#include <system_recovery.h>
#include <os_common_api.h>

#define SHELL_CRASH "crash"

static int crash_load_data_error(int argc, char *argv[])
{
    uint32_t *ptr0 = (uint32_t *) strtoul("0x78001", NULL, 16);
    uint32_t *ptr1 = (uint32_t *) strtoul("0x78008", NULL, 16);

    ACT_LOG_ID_INF(ALF_STR_crash_load_data_error__CRASH_SAMPLE_LOAD_DA, 0);

    //这一句话会触发数据读取异常
    *ptr0 = *ptr1;

    return 0;
}

static int crash_save_data_error(int argc, char *argv[])
{
    uint32_t *ptr = (uint32_t *) strtoul("0x78001", NULL, 16);

    ACT_LOG_ID_INF(ALF_STR_crash_save_data_error__CRASH_SAMPLE_SAVE_DA, 0);

    //这一句话会触发数据存取异常
    *ptr = 0;

    return 0;
}

static int crash_jump_function_error(int argc, char *argv[])
{
    void (*jump_func)(void) = (void (*)(void))0x00000000;

    ACT_LOG_ID_INF(ALF_STR_crash_jump_function_error__CRASH_SAMPLE_JUMP_FU, 0);

    //跳转到0地址执行
    jump_func();

    return 0;
}

static int crash_watchdog_overflow(int argc, char *argv[])
{
    ACT_LOG_ID_INF(ALF_STR_crash_watchdog_overflow__CRASH_SAMPLE_WATCHDO, 0);

    //锁上线程调度，看门狗会自动复位触发异常
    os_sched_lock();

    while (1)
        ;

    return 0;
}

static int crash_cpu_mpu_error(int argc, char *argv[])
{
    volatile uint32_t *ptr = (uint32_t *) 0x00000;

    ACT_LOG_ID_INF(ALF_STR_crash_cpu_mpu_error__CRASH_SAMPLE_MPU_ERR, 0);

    //这一句话会触发MPU写cpu 0地址异常
    *ptr = 0;

    //mpu异常属于不精确异常,要延时几个cycle
    delay(10);

    return 0;
}

static int crash_thread_stackerror_mpu_error(int argc, char *argv[])
{
    uint32_t *stack_start;

    struct k_thread *thread = k_current_get();

    stack_start = (uint32_t *)thread->stack_info.start;

    ACT_LOG_ID_INF(ALF_STR_crash_thread_stackerror_mpu_error__CRASH_SAMPLE_STACK_O, 0);

    //往栈底写一段数据
    memset(stack_start, 0, 32);

    //mpu异常属于不精确异常,要延时几个cycle
    delay(10);

    return 0;
}

static int crash_thread_stackoverflow_mpu_error(int argc, char *argv[])
{
    uint8_t static_data[2048];

    memset(static_data, 0, sizeof(static_data));

    return 0;
}


static void crash_test_timer(os_timer *timer)
{
    free(timer);

    crash_load_data_error(0, NULL);
}

static int crash_test_irq(int argc, char *argv[])
{
    os_timer  *test_timer;

    test_timer = (os_timer  *)malloc(sizeof(os_timer));

    os_timer_init(test_timer, crash_test_timer, NULL);

    os_timer_start(test_timer, 10, 0);

    return 0;
}

static int crash_assert(int argc, char *argv[])
{
    int test_value =  strtoul("0x1", NULL, 16);
    __ASSERT(test_value == 0, "failed test val %d\n", test_value);
    return 0;
}

static int crash_panic(int argc, char *argv[])
{
    panic("this is a panic test\n");
    return 0;
}


static const struct shell_cmd crash_commands[] = {
    { "load_error", crash_load_data_error, "simulate load data error" },
    { "store_error", crash_save_data_error, "simulate store data error" },
    { "jump_error", crash_jump_function_error, "simulate jump function error" },
    { "watchdog_flow", crash_watchdog_overflow, "simulate watchdog overflow error" },
    { "cpu_write_error", crash_cpu_mpu_error, "simulate cpu write error" },
    { "stack_error", crash_thread_stackerror_mpu_error, "simulate stack error" },
    { "stack_flow", crash_thread_stackoverflow_mpu_error, "simulate stack flow error" },
    { "irq_error", crash_test_irq, "simulate irq error" },
    { "assert_error", crash_assert, "simulate assert error"},    
    { "panic_error", crash_panic, "simulate panic error"},
    { NULL, NULL, NULL }, };

SHELL_REGISTER(SHELL_CRASH, crash_commands);



/********************************************************************************
 *                            USDK(ATS350B_linux)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-9-22-下午12:21:19             1.0             build this file
 ********************************************************************************/
/*!
 * \file     test_malloc.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-9-22-下午12:21:19
 *******************************************************************************/
#include <sdk.h>
#include <greatest_api.h>
#include "../../../kernel/memory/include/heap.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TEST_MALLOC

TEST test_malloc(void)
{
    int i;
    void *val[64];


    ACT_LOG_ID_INF(ALF_STR_test_malloc__TEST_MALLOC_STARTN, 0);
    val[2] = malloc(128);
    val[1] = malloc(768);
    val[0] = malloc(1024);
    ACT_LOG_ID_INF(ALF_STR_test_malloc__08X_08X_08XN, 3, val[0], val[1], val[2]);

    free(val[0]);
    free(val[1]);
    free(val[2]);

    for(i = 0; i < 64; i++)
    {
        val[i] = malloc(i + 1);
        if(val[i] == NULL)
            ACT_LOG_ID_INF(ALF_STR_test_malloc__MALLOCD_FAILEDN, 1, i);
        else
            ACT_LOG_ID_INF(ALF_STR_test_malloc__MALLOCD_SUCESS_RETUR, 2, i, val[i]);
    }

    for(i = 0; i < 64; i++)
    {
        free(val[i]);
    }

    for(i = 0; i < 32; i++)
    {
        val[i * 2] = malloc(i * 2 + 1);
        if(val[i * 2] == NULL)
            ACT_LOG_ID_INF(ALF_STR_test_malloc__MALLOCD_FAILEDN, 1, i * 2);
        else
            ACT_LOG_ID_INF(ALF_STR_test_malloc__MALLOCD_SUCESS_RETUR, 2,  i * 2, val[i * 2]);
    }

    for(i = 0; i < 32; i++)
    {
        val[i * 2 + 1] = malloc(i * 2 + 1);
        if(val[i * 2 + 1] == NULL)
            ACT_LOG_ID_INF(ALF_STR_test_malloc__MALLOCD_FAILEDN, 1, i * 2 + 1);
        else
            ACT_LOG_ID_INF(ALF_STR_test_malloc__MALLOCD_SUCESS_RETUR, 2,  i * 2 + 1, val[i * 2 + 1]);
    }

    for(i = 0; i < 64; i++)
    {
        free(val[i]);
    }

    //最多只能分30K的连续内存
    for(i = 1; i < 6; i++)
    {
        val[i] = malloc(i*PAGE_SIZE);
        if(val[i] == NULL)
            ACT_LOG_ID_INF(ALF_STR_test_malloc__MALLOCLD_FAILEDN, 1, i*PAGE_SIZE);
        else
            ACT_LOG_ID_INF(ALF_STR_test_malloc__MALLOCLD_SUCESS_RETU, 2,  i*PAGE_SIZE, val[i]);
    }

    for(i = 1; i < 6; i++)
    {
        free(val[i]);
    }
}

ADD_TEST_CASE("test_malloc")
{
    RUN_TEST(test_malloc);
}




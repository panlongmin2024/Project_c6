/********************************************************************************
 *                            USDK(ATS350B_linux)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-9-22-上午10:39:28             1.0             build this file
 ********************************************************************************/
/*!
 * \file     test_alloc_pages.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-9-22-上午10:39:28
 *******************************************************************************/
#include <sdk.h>
#include <greatest_api.h>
#include "../../../kernel/memory/include/heap.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TEST_ALLOC_PAGES

TEST test_page(void)
{
    int i;
    void *val[8];

    ACT_LOG_ID_INF(ALF_STR_test_page__TEST_ALLOC_PAGES_STA, 0);

    for(i = 1; i < 5; i++)
    {
        val[i] = pmalloc(PAGE_SIZE * i);
        if(val[i] == NULL)
            ACT_LOG_ID_INF(ALF_STR_test_page__PAGEPOOL_ALLOC_PAGES, 1, i);
        else
            ACT_LOG_ID_INF(ALF_STR_test_page__PAGEPOOL_ALLOC_PAGES, 2, i, val[i]);
    }

    for(i = 1; i < 5; i++)
    {
        if(pfree(val[i], i) == 0)
            ACT_LOG_ID_INF(ALF_STR_test_page__PAGEPOOL_FREE_PAGES0, 2, val[i], i);
        else
            ACT_LOG_ID_INF(ALF_STR_test_page__PAGEPOOL_FREE_PAGES0, 2, val[i], i);
    }
}

ADD_TEST_CASE("test_page")
{
    RUN_TEST(test_page);
}



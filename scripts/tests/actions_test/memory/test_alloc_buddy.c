/********************************************************************************
 *                            USDK(ATS350B_linux)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-9-22-上午10:39:38             1.0             build this file
 ********************************************************************************/
/*!
 * \file     test_alloc_buddy.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-9-22-上午10:39:38
 *******************************************************************************/
#include <sdk.h>
#include <greatest_api.h>
#include "../../../kernel/memory/include/heap.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TEST_ALLOC_BUDDY

#define TEST_PAGE_SIZE 2048

TEST test_buddy(void)
{
    int8 buddy_no, page_no;
    int i, size;
    void *offset[128];
    void *page;

    page = pmalloc(TEST_PAGE_SIZE);
    page_no = pagepool_convert_addr_to_pageindex(page);

    ACT_LOG_ID_INF(ALF_STR_test_buddy__NEW_BUDDYN, 0);
    buddy_no = rom_new_buddy_no(page_no, &g_rom_buddy_data);
    buddy_dump(buddy_no, true);

    size = 1664;
    offset[0] = buddy_alloc(buddy_no, &size);
    ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_ALLOC_D_RETURN, 2, size, offset[0]);
    buddy_dump(buddy_no, true);
    for(i = 1; i < 64; i++)
    {
        size = i;
        offset[i] = buddy_alloc(buddy_no, &size);
        ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_ALLOC_D_RETURN, 2, i, offset[i]);
        buddy_dump(buddy_no, true);
    }
    for(i = 0; i < 64; i++)
    {
        size = buddy_free(buddy_no, offset[i], NULL);
        ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_FREE_08X_SIZE_, 2, offset[i], size);
        buddy_dump(buddy_no, true);
    }

    size = 580;
    offset[0] = buddy_alloc(buddy_no, &size);
    ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_ALLOC_D_RETURN, 2, size, offset[0]);
    buddy_dump(buddy_no, true);
    for(i = 1; i < 64; i++)
    {
        size = i;
        offset[i] = buddy_alloc(buddy_no, &size);
        ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_ALLOC_D_RETURN, 2, i, offset[i]);
        buddy_dump(buddy_no, true);
    }
    for(i = 0; i < 64; i++)
    {
        size = buddy_free(buddy_no, offset[i], NULL);
        ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_FREE_08X_SIZE_, 2, offset[i], size);
        buddy_dump(buddy_no, true);
    }

    size = 0x340;
    offset[1] = buddy_alloc(buddy_no, &size);
    ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_ALLOC_D_RETURN, 2, size, offset[1]);
    buddy_dump(buddy_no, true);
    for(i = 1; i < 64; i++)
    {
        size = i;
        offset[i] = buddy_alloc(buddy_no, &size);
        ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_ALLOC_D_RETURN, 2, i, offset[i]);
        buddy_dump(buddy_no, true);
    }
    for(i = 0; i < 64; i++)
    {
        size = buddy_free(buddy_no, offset[i], NULL);
        ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_FREE_08X_SIZE_, 2, offset[i], size);
        buddy_dump(buddy_no, true);
    }

    for(i = 0; i < 64; i++)
    {
        size = i;
        offset[i] = buddy_alloc(buddy_no, &size);
        ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_ALLOC_D_RETURN, 2, i, offset[i]);
        buddy_dump(buddy_no, true);
    }
    for(i = 64; i < 128; i++)
    {
        size = i-64;
        offset[i] = buddy_alloc(buddy_no, &size);
        ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_ALLOC_D_RETURN, 2, i-64, offset[i]);
        buddy_dump(buddy_no, true);
    }
    for(i = 0; i < 128; i++)
    {
        size = buddy_free(buddy_no, offset[i], NULL);
        ACT_LOG_ID_INF(ALF_STR_test_buddy__BUDDY_FREE_08X_SIZE_, 2, offset[i], size);
        buddy_dump(buddy_no, true);
    }

    pfree(page, 1);
}

ADD_TEST_CASE("test_buddy")
{
    RUN_TEST(test_buddy);
}


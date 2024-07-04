/********************************************************************************
 *                            USDK(ATS350B_linux)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-8-13-下午1:13:05             1.0             build this file
 ********************************************************************************/
/*!
 * \file     test_dma.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-8-13-下午1:13:05
 *******************************************************************************/
#include <kernel.h>
#include <greatest_api.h>
#include <dma.h>
#include <soc.h>
#include <malloc.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TEST_DMA
extern void unistall_dma_phy_channel(unsigned int channel_num);
extern void install_dma_phy_channel(unsigned int channel_num);

typedef struct
{
    int dma;
    volatile int complete;
}nested1_dma_trans_param_t;

typedef struct
{
    char *src;
    char *dst;
    int vdma_index;
    int vdma_handle;
    int len;
    volatile int complete;
    nested1_dma_trans_param_t nested_param;
}nested_dma_trans_param_t;

//虚拟dma中断回调函数（在中断中被调用）
void test_dma_handle(struct device *dev, u32_t priv_data, int reson)
{
    ACT_LOG_ID_INF(ALF_STR_test_dma_handle__DMA_NO__D_DMA_IRQ_TY, 2, _arch_irq_vector_read() - IRQ_ID_DMA0, reson);
}

void test_dma_reload_irq(struct device *dev, u32_t priv_data, int reson)
{
    nested_dma_trans_param_t *trans_param = (nested_dma_trans_param_t *)priv_data;

    uint32_t dma_no = (_arch_irq_vector_read() - IRQ_ID_DMA0);

    ACT_LOG_ID_INF(ALF_STR_test_dma_reload_irq__DMA_RELOAD_NO__D_DMA, 2, dma_no, reson);

    if(reson == DMA_IRQ_TC)
    {
        //比较传输结果
        TEST_ASSERT_MEM_EQ(trans_param->src, trans_param->dst, trans_param->len);

        //清目的地址数据
        memset(trans_param->dst, 0x5a, trans_param->len);

        ACT_LOG_ID_INF(ALF_STR_test_dma_reload_irq__FORCE_FREE_REQUEST_D, 1, trans_param->vdma_index);

        //暂停DMA传输，否则多路同时做reload传输会导致其他路DMA无法使用
        //但这里模拟虽然DMA传输停止，但物理DMA资源不释放
        //强制释放一路DMA
        dma_stop(dev, trans_param->vdma_handle);

        //测试接口是否可多次abort
        dma_stop(dev, trans_param->vdma_handle);

        dma_free(dev, trans_param->vdma_handle);

        trans_param->complete = 1;
    }
}

void test_dma_nested_1_irq(struct device *dev, u32_t priv_data, int reson)
{
    ACT_LOG_ID_INF(ALF_STR_test_dma_nested_1_irq__DMA_NESTED1_NO__D_DM, 2, _arch_irq_vector_read() - IRQ_ID_DMA0, reson);

    nested1_dma_trans_param_t *nested1_param = (nested1_dma_trans_param_t *)priv_data;

    if(reson == DMA_IRQ_TC)
    {
        dma_stop(dev, nested1_param->dma);
        dma_free(dev, nested1_param->dma);
        nested1_param->complete = 1;
    }
}

void test_dma_nested_0_irq(struct device *dev, u32_t pdata, int reson)
{
    int dma;

    struct dma_config config_data;

    struct dma_block_config head_block;

    ACT_LOG_ID_INF(ALF_STR_test_dma_nested_0_irq__DMA_NESTED0_NO__D_DM, 2, _arch_irq_vector_read() - IRQ_ID_DMA0, reson);

    nested_dma_trans_param_t *trans_param = (nested_dma_trans_param_t *)pdata;

    if(reson == DMA_IRQ_TC){
        if(trans_param->complete == 0){
            dma_stop(dev, trans_param->vdma_handle);
            dma_free(dev, trans_param->vdma_handle);

            dma = trans_param->nested_param.dma;
            trans_param->nested_param.complete = 0;

            config_data.channel_direction = MEMORY_TO_MEMORY;
            config_data.source_data_size = 1;
            config_data.complete_callback_en = 1;
            config_data.dma_callback = test_dma_nested_1_irq;
            config_data.callback_data = (void *)(&(trans_param->nested_param));

            head_block.source_address = (unsigned int)trans_param->src;
            head_block.dest_address = (unsigned int)trans_param->dst;
            head_block.block_size = trans_param->len;
            head_block.source_reload_en = 1;

            config_data.head_block = &head_block;

            dma_config(dev, dma, &config_data);

            dma_start(dev, dma);

            trans_param->complete = 1;
        }
    }
}

int test_dmac(char *source_buf, char *dest_buf)
{
    int dma;

    int i;

    struct device *dma_dev;

    struct dma_config config_data;

    struct dma_block_config head_block;

    memset(&config_data, 0, sizeof(struct dma_config));

    memset(&head_block, 0, sizeof(struct dma_block_config));

	dma_dev = device_get_binding(CONFIG_DMA_0_NAME);

	TEST_ASSERT_NOT_NULL(dma_dev);

    memset(source_buf, 0xab, 512);
    memset(dest_buf, 0x5a, 512);

    dma = dma_request(dma_dev, 0xff); //申请虚拟dma

    TEST_ASSERT_NOT_EQ(0, dma);

    config_data.channel_direction = MEMORY_TO_MEMORY;
    config_data.source_data_size = 1;
    config_data.complete_callback_en = 1;
    config_data.dma_callback = test_dma_handle;
    config_data.callback_data = NULL;

    head_block.source_address = (unsigned int)source_buf;
    head_block.dest_address = (unsigned int)dest_buf;
    head_block.block_size = 512;
    head_block.source_reload_en = 0;

    config_data.head_block = &head_block;

    dma_config(dma_dev, dma, &config_data);

    for(i = 0; i < 50; i++){
        dma_start(dma_dev, dma); //开始dma传输

        dma_wait_finished(dma_dev, dma); //等待dma传输完成

        TEST_ASSERT_MEM_EQ((const void *)source_buf, (const void *)dest_buf, 512);
    }

    dma_start(dma_dev, dma); //第二次开始dma传输

    dma_wait_finished(dma_dev, dma); //等待dma传输完成

    dma_start(dma_dev, dma); //第N次开始dma传输

    dma_wait_finished(dma_dev, dma); //等待dma传输完成

    dma_start(dma_dev, dma); //第N+1次开始dma传输

    dma_stop(dma_dev, dma); //立即终止传输

    dma_free(dma_dev, dma); //释放虚拟dma

    ACT_LOG_ID_INF(ALF_STR_test_dmac__TEST_DMA_OVERN, 0);

    return 0;
}


#define DMA_TRANS_TEST_LEN 512


int test_dmac_reload(char *source_buf, char *dest_buf)
{
    int i;
    int dma[CONFIG_VDMA_ACTS_VCHAN_NUM];

    uint32_t irq_flag[CONFIG_VDMA_ACTS_VCHAN_NUM];

    uint32_t reload_count = 0;

    nested_dma_trans_param_t trans_param[CONFIG_VDMA_ACTS_VCHAN_NUM];

    struct device *dma_dev;

    struct dma_config config_data;

    struct dma_block_config head_block;

    memset(&config_data, 0, sizeof(struct dma_config));

    memset(&head_block, 0, sizeof(struct dma_block_config));    

	dma_dev = device_get_binding(CONFIG_DMA_0_NAME);

	TEST_ASSERT_NOT_NULL(dma_dev);

    for(i = 0; i < CONFIG_VDMA_ACTS_VCHAN_NUM; i++)
    {
        dma[i] = 0;
        irq_flag[i] = 0;
    }

    memset(source_buf, 0xab, DMA_TRANS_TEST_LEN);
    memset(dest_buf, 0x5a, DMA_TRANS_TEST_LEN);

    memset(trans_param, 0, sizeof(trans_param));

    //同时启动多路DMA进行传输
    for(i = 0; i < CONFIG_VDMA_ACTS_VCHAN_NUM; i++)
    {
        dma[i] = dma_request(dma_dev, 0xff); //申请虚拟dma

        TEST_ASSERT_NOT_NULL(dma[i]);

        trans_param[i].src = source_buf;
        trans_param[i].dst = dest_buf;
        trans_param[i].len = DMA_TRANS_TEST_LEN;
        trans_param[i].complete = 0;
        trans_param[i].vdma_index = i;
        trans_param[i].vdma_handle = dma[i];

        config_data.channel_direction = MEMORY_TO_MEMORY;
        config_data.source_data_size = 1;
        config_data.complete_callback_en = 1;
        config_data.dma_callback = test_dma_reload_irq;
        config_data.callback_data = (void *)&trans_param[i];

        head_block.source_address = (unsigned int)source_buf;
        head_block.dest_address = (unsigned int)dest_buf;
        head_block.block_size = DMA_TRANS_TEST_LEN;
        head_block.source_reload_en = 1;

        config_data.head_block = &head_block;

        dma_config(dma_dev, dma[i], &config_data);

        dma_start(dma_dev, dma[i]);
    }

    while(1)
    {
        for(i = 0; i < CONFIG_VDMA_ACTS_VCHAN_NUM; i++){
            if(dma[i] != 0 && trans_param[i].complete == 1)
            {
                reload_count++;
                dma[i] = 0;

                continue;
            }
        }

        if(reload_count == CONFIG_VDMA_ACTS_VCHAN_NUM){
            break;
        }
    }

    //释放所有的dma资源
    for(i = 0; i < CONFIG_VDMA_ACTS_VCHAN_NUM; i++)
    {
        if(dma[i] != 0)
        {
            dma_free(dma_dev, dma[i]); //释放虚拟dma
        }
    }

    ACT_LOG_ID_INF(ALF_STR_test_dmac_reload__TEST_RELOAD_OVERN, 0);

    return 0;
}



int test_dma_nested_trans(char *source_buf, char *dest_buf)
{
    int dma;

    nested_dma_trans_param_t trans_param;

    struct device *dma_dev;

    struct dma_config config_data;

    struct dma_block_config head_block;

    memset(&config_data, 0, sizeof(struct dma_config));

    memset(&head_block, 0, sizeof(struct dma_block_config));    

	dma_dev = device_get_binding(CONFIG_DMA_0_NAME);

	TEST_ASSERT_NOT_NULL(dma_dev);

    memset(&trans_param, 0, sizeof(nested_dma_trans_param_t));

    dma = dma_request(dma_dev, 0xff);

    //中断程序不能申请内存，提前申请
    trans_param.nested_param.dma = dma_request(dma_dev, 0xff);

    trans_param.src = source_buf;
    trans_param.dst = dest_buf;
    trans_param.len = DMA_TRANS_TEST_LEN;
    trans_param.complete = 0;
    trans_param.vdma_index = 0;
    trans_param.vdma_handle = dma;

    config_data.channel_direction = MEMORY_TO_MEMORY;
    config_data.source_data_size = 1;
    config_data.complete_callback_en = 1;
    config_data.dma_callback = test_dma_nested_0_irq;
    config_data.callback_data = (void *)&trans_param;

    head_block.source_address = (unsigned int)source_buf;
    head_block.dest_address = (unsigned int)dest_buf;
    head_block.block_size = DMA_TRANS_TEST_LEN;
    head_block.source_reload_en = 1;

    config_data.head_block = &head_block;

    dma_config(dma_dev, dma, &config_data);

    dma_start(dma_dev, dma);

    while(1){
        if(trans_param.complete == 1
            && trans_param.nested_param.complete == 1){
            break;
        }
    }

    ACT_LOG_ID_INF(ALF_STR_test_dma_nested_trans__NESTED_TRANS_OVERN, 0);

    return 0;
}

void free_dma_controller_all(void)
{
    int i;

	for (i = CONFIG_VDMA_ACTS_PCHAN_START; 
	     i < (CONFIG_VDMA_ACTS_PCHAN_NUM + CONFIG_VDMA_ACTS_PCHAN_START);
	     i++) {
		unistall_dma_phy_channel(i);
	}    
}

void install_dma_controller_one(void)
{
    install_dma_phy_channel(CONFIG_VDMA_ACTS_PCHAN_START);
}

void free_dma_controller_one(void)
{
    unistall_dma_phy_channel(CONFIG_VDMA_ACTS_PCHAN_START);
}

void install_dma_controller_all(void)
{
    int i;

	for (i = CONFIG_VDMA_ACTS_PCHAN_START; 
	     i < (CONFIG_VDMA_ACTS_PCHAN_NUM + CONFIG_VDMA_ACTS_PCHAN_START);
	     i++) {
		install_dma_phy_channel(i);
	}  

}

TEST dma_channel_test(void){
    char *source_buf;
    char *dest_buf;

    int run_loop = 5;

    source_buf = (char *)malloc(512); //申请内存用作dma数据源

    TEST_ASSERT_NOT_NULL(source_buf);

    dest_buf = (char *)malloc(512); //dma目标地址

    TEST_ASSERT_NOT_NULL(dest_buf);

    //先卸载所有的dma控制器
    free_dma_controller_all();

    while(run_loop){
        install_dma_controller_one();

        ACT_LOG_ID_INF(ALF_STR_dma_channel_test__TEST_ONE_LOOP_START_, 0);

        test_dmac(source_buf, dest_buf);

        test_dmac_reload(source_buf, dest_buf);

        test_dma_nested_trans(source_buf, dest_buf);

        ACT_LOG_ID_INF(ALF_STR_dma_channel_test__TEST_ONE_LOOP_OK__N, 0);

        free_dma_controller_one();

        ACT_LOG_ID_INF(ALF_STR_dma_channel_test__TEST_ALL_LOOP_START_, 0);

        install_dma_controller_all();

        test_dmac(source_buf, dest_buf);

        test_dmac_reload(source_buf, dest_buf);

        test_dma_nested_trans(source_buf, dest_buf);

        free_dma_controller_all();

        ACT_LOG_ID_INF(ALF_STR_dma_channel_test__TEST_ALL_LOOP_OK_N, 0);

        run_loop--;
    }

    //再装载所有的dma控制器
    install_dma_controller_all();

    free(source_buf);
    free(dest_buf);
}



ADD_TEST_CASE("dma_test")
{
    RUN_TEST(dma_channel_test);
}





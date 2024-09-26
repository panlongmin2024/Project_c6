
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zephyr.h>
#include <mem_manager.h>
#include <logging/sys_log.h>
#include <os_common_api.h>

#include "../include/dolphin_firmware.h"
#include "../include/dolphin_rw.h"
#include "../include/dolphin_com.h"
#include "../include/ats3615_reg.h"
#include "../include/hmdsp_tuning_hdr.h"

#define DOLPHIN_SLAVE_CTRL  ATS3615_CMU_SLAVE_CTRL
#define DOLPHIN_SPISLV_SEED ATS3615_SPISLV_SEED
#define DOLPHIN_MRCR0  ATS3615_MRCR0
#define dolphin_Q31(x) (int)((long long)x*0x7FFFFFFF/100)

extern const unsigned char charge6_3nod[];
extern const unsigned char charge6_ggec[];
extern const unsigned char charge6_ggec_PB[];
extern const unsigned char charge6_tcl[];
extern const unsigned char charge6_tcl_PB[];

extern u32_t flip7_get_odm_id(void);
extern int ats3615_comm_set_dsp_run(void);

uint32_t com_addr = 0;

int dolphin_demo_exit_flag = 1;
static int comm_init_ok = 0;

const unsigned char * g_dsp_tuning_info = NULL;
int g_dsp_tuning_info_valid = 0;
unsigned int g_tuning_data_address = 0;
unsigned int g_tuning_data_size = 0;
// unsigned int g_product_id_address = 0;
// unsigned int g_hardware_id_address = 0;
unsigned int g_dolphin_com_struct_address = 0;
extern unsigned int charge6_3nod_size;
extern unsigned int charge6_ggec_size;
extern unsigned int charge6_ggec_PB_size;
extern unsigned int charge6_tcl_size;
extern unsigned int charge6_tcl_PB_size;

extern u8_t selfapp_config_get_PB_state(void);
int get_tuning_info_by_odm(void)
{
    u32_t odm_id = ReadODM();

    const unsigned char * tuning_info;
    
    if(HW_3NOD_BOARD == odm_id){
        tuning_info = charge6_3nod;
        g_tuning_data_size = charge6_3nod_size;

    }else if(HW_TONGLI_BOARD == odm_id){

         if (selfapp_config_get_PB_state() == 0){
            tuning_info = charge6_tcl;
            g_tuning_data_size = charge6_tcl_size;
         }else{
            tuning_info = charge6_tcl_PB;
            g_tuning_data_size = charge6_tcl_PB_size;
         }
    }else{
        if (selfapp_config_get_PB_state() == 0){
            tuning_info = charge6_ggec;
            g_tuning_data_size = charge6_ggec_size;
        }else{
            tuning_info = charge6_ggec_PB;
            g_tuning_data_size = charge6_ggec_PB_size;
        }
    }

    g_dsp_tuning_info = (unsigned char * )tuning_info;
    return 0;
}

int dolphin_send_tuning_info(void)
{
    const unsigned char * tuning_info = g_dsp_tuning_info;

    if (!tuning_info) {
        SYS_LOG_ERR("tuning_info is NULL \n");
        return -1;
    }

    if (!g_dsp_tuning_info_valid) {
        SYS_LOG_ERR("DSP Tuning file check error\n");
        return -1;
    }

    if(dsp_3615_mutex_ptr){
        os_mutex_lock(dsp_3615_mutex_ptr, OS_FOREVER);
        printk("dolphin_send_tuning_info  get mutex  \n");
	}else{
        printk("dsp_3615_mutex_ptr == NULL  \n");
        return -1;
    }    

    printk("dolphin_comm_send_tuning  enter \n");

    printf("\rtuning address = %08x\n", g_tuning_data_address);
    printf("\rtuning size    = %d bytes\n",g_tuning_data_size);
	
    printk("tuning_info = %08x \n", (unsigned int)tuning_info);
    int buffer_size = 0x1F0;
    int * buffer = malloc(buffer_size);
    if (!buffer)
    {
        printk("malloc fail %s:%d\n", __FUNCTION__, __LINE__);
        return -1;
    }
    // write all changes to DSP
    // 1. for simplicity host can transmit the whole structure in one SPI transfer (if not too big)
    // 2. otherwise it can only transmit what members of the structures have changed.
    // Here 1. is used :)
    for (uint32_t write_num = 0; write_num < g_tuning_data_size;) {
        uint32_t once_num = ((g_tuning_data_size - write_num) >= buffer_size) ? buffer_size : (g_tuning_data_size - write_num);
        memcpy(buffer, tuning_info + write_num, once_num);

        Dolphin_Write_Mem(0, g_tuning_data_address + write_num, (unsigned int*)buffer, once_num);//com->tuning_size);
        write_num += once_num;
    }
    free(buffer);
    // printk("[pengfei] 1111 \n");
    // finally, host alerts the DSP that it has changed the structure
    // (an interrupt might be triggered on the DSP side or pooling method will be used)
    // Dolphin_Write_Reg(0, DOLPHIN_SPISLV_SEED, 0xac285555);
    // printk("[pengfei] 222 \n");

    printk("dolphin_comm_send_tuning  exit \n");
    // printk("dolphin_comm_send_tuning  OK \n");
    os_mutex_unlock(dsp_3615_mutex_ptr);
    return 0;
}


int dolphin_comm_init(void)
{
    // keep a local structure 
    dolphin_com_t com;
    memset(&com, 0, sizeof(com));

    printk("dolphin_comm_init  enter \n");
    
    if(dsp_3615_mutex_ptr){
        os_mutex_lock(dsp_3615_mutex_ptr, OS_FOREVER);
        printk("dolphin_comm_init  get mutex  \n");
	}else{
        printk("dsp_3615_mutex_ptr == NULL  \n");
        return -1;
    }
    com_addr = g_dolphin_com_struct_address;

    // read the header part of the com structure
    Dolphin_Read_Mem(0, com_addr + offsetof(dolphin_com_t, hdr), (uint32_t*)&com.hdr, sizeof(com.hdr));
    printf("[dolphin test] DSP Struct MAGIC = 0x%08X Struct Version = %d VERSION = %d.%d\n", com.hdr.magic, com.hdr.struct_version, com.hdr.dsp_version >> 16, com.hdr.dsp_version & 0xFFFF);

    if (com.hdr.magic != DOLPHIN_COM_STRUCT_MAGIC)
    {
        printf("[dolphin test] Wrong DSP struct magic number : got 0x%08x should be 0x%08x\n",com.hdr.magic,DOLPHIN_COM_STRUCT_MAGIC);
        goto _err;
    }

    if (com.hdr.struct_version != DOLPHIN_COM_STRUCT_VERSION)
    {
        printf("[dolphin test] Warning ! DSP Com Struct version don't match : got 0x%08x should be 0x%08x\n",com.hdr.struct_version, DOLPHIN_COM_STRUCT_VERSION);
        goto _err;
    }

    comm_init_ok = 1;
    os_mutex_unlock(dsp_3615_mutex_ptr);
    
    printk("dolphin_comm_init  exit \n");
    printk("dolphin_comm_init  OK\n");
    return 0;

_err:
    printk("dolphin_comm_init  exit \n");
    printk("dolphin_comm_init  FAIL\n");
    comm_init_ok = 0;
    os_mutex_unlock(dsp_3615_mutex_ptr);
    return -1;
}


int dolphin_comm_deinit(void)
{
    comm_init_ok = 0;
    return 0;
}


int dolphin_comm_send_wait_finish(dolphin_com_t * p_comm)
{
    if (!comm_init_ok)
    {
        printk("ATS3615 comm not init \n");
        return -1;
    }

    printk("dolphin_comm_send  enter \n");

    if(dsp_3615_mutex_ptr){
        os_mutex_lock(dsp_3615_mutex_ptr, OS_FOREVER);
        printk("dolphin_comm_send  get mutex \n");
	}else{
        printk("dsp_3615_mutex_ptr == NULL  \n");
        return -1;
    }
	
    // write all changes to DSP
    // 1. for simplicity host can transmit the whole structure in one SPI transfer (if not too big)
    // 2. otherwise it can only transmit what members of the structures have changed.
    // Here 1. is used :)
    Dolphin_Write_Mem(0, com_addr + offsetof(dolphin_com_t, host), (uint32_t*)&p_comm->host, sizeof(p_comm->host));
    
    // finally, host alerts the DSP that it has changed the structure
    // (an interrupt might be triggered on the DSP side or pooling method will be used)
    Dolphin_Write_Reg(0, DOLPHIN_SPISLV_SEED, 0xac285555);

#if 1
    // this is optional : wait for DSP to handle the command (it will clear bit #5 of the DOLPHIN_SLAVE_CTRL)
    for (int i = 0; ; i++)
    {
        uint32_t reg;
        MASTER_Read_ATS3615_Reg(DOLPHIN_SLAVE_CTRL, &reg);
        if (!(reg & (1 << 5)))
            break;

        k_sleep(10);
        if (i == 10)
        {
            printk("timeout : DSP did not handled the change\n");
            goto _err;
        }
    }
#endif

    printk("dolphin_comm_send  exit \n");
    // printk("ats3615_comm  OK \n");
    os_mutex_unlock(dsp_3615_mutex_ptr);
    return 0;

_err:
    printk("dolphin_comm_send  exit \n");
    printk("ats3615_comm  FAIL \n");
    os_mutex_unlock(dsp_3615_mutex_ptr);
    return -1;
}

int dolphin_comm_send_ex(dolphin_com_t * p_comm)
{
    int retry_cnt = 3;
    int i, send_result = -1;

    if (comm_init_ok)
    {

        for (i=0; i<retry_cnt; i++)
        {
            send_result = dolphin_comm_send_wait_finish(p_comm);
            if (!send_result)
                return send_result;

            SYS_LOG_ERR("Send Fail, wait and retry (%d) ...\n", i+1);
            k_sleep(200);
        }

        extern int external_dsp_ats3615_reset(void);
        external_dsp_ats3615_reset();
    }

    return send_result;
}

#if 0
int dolphin_set_vol(int argc, char *argv[])
{
    //float vol = fabsf(atof(argv[1]));
    int vol = atoi(argv[1]);
    if(vol < 0) vol = -vol;

    // convert to fixed-point q1.31
    com.host.volume = vol >= 100 ? 0x7fffffff : dolphin_Q31(vol);
    com.host.change_flags |= FLAG_CHANGE_VOLUME;

    printf("[dolphin test]change volume -> %d/100 = (0x%08x)\n", vol, com.host.volume);
    //printf("change volume -> %g = %g dB (0x%08x)\n", vol, 20.0f * log10f(vol), com.host.volume);
    
    // write all changes to DSP
    // 1. for simplicity host can transmit the whole structure in one SPI transfer (if not too big)
    // 2. otherwise it can only transmit what members of the structures have changed.
    // Here 1. is used :)
    Dolphin_Write_Mem(0, com_addr + offsetof(dolphin_com_t, host), (uint32_t*)&com.host, sizeof(com.host));
    
    // finally, host alerts the DSP that it has changed the structure
    // (an interrupt might be triggered on the DSP side or pooling method will be used)
    Dolphin_Write_Reg(0, DOLPHIN_SPISLV_SEED, 0xac285555);

#if 0
    // this is optional : wait for DSP to handle the command (it will clear bit #5 of the DOLPHIN_SLAVE_CTRL)
    for (int i = 0; ; i++)
    { 
        uint32_t reg;
        Dolphin_Read_Reg(0, DOLPHIN_SLAVE_CTRL, &reg);
        if (!(reg & (1 << 5)))
            break;

        k_busy_wait(10*1000);
        if (i == 10)
        {
            printf("timeout : DSP did not handled the change\n");
            return -2;
        }
    }
#endif

    printf("[dolphin test] set_dolphin_vol OK\n");
    
    return 0;
}

char *dolphin_stack;
int count = 1;

static void get_dolphin_params(void *p1, void *p2, void *p3)
{
    while (dolphin_demo_exit_flag == 0)
    {
        // read the dsp part of the com structure
        Dolphin_Read_Mem(0, com_addr + offsetof(dolphin_com_t, dsp), (uint32_t*)&com.dsp, sizeof(com.dsp));
        if(count%100==0)
            printf("[dolphin test] framecount = %d\n", com.dsp.framecount);
                
        count++;
        if(count>10000) count = 1;
        os_sleep(20);
    }
    
    dolphin_demo_exit_flag = 1;
    printf("[dolphin test] get_dolphin_params task exit\n");

}

int dolphin_read_frames_start(int argc, char *argv[])
{
    if (dolphin_stack)
        return -EALREADY;    
    dolphin_stack = app_mem_malloc(CONFIG_TOOL_STACK_SIZE);
    if (!dolphin_stack) {
	SYS_LOG_ERR("dolphin stack mem_malloc failed");
	return -ENOMEM;
    }
    
    dolphin_demo_exit_flag = 0;

    os_thread_create(dolphin_stack, CONFIG_TOOL_STACK_SIZE,
    		 get_dolphin_params, NULL, NULL, NULL, 8, 0, OS_NO_WAIT);

    return 0;
}

int dolphin_read_frames_stop(int argc,char *argv[])
{
    if (!dolphin_stack)
        return 0;
    dolphin_demo_exit_flag = 1;
    
    app_mem_free(dolphin_stack);
    dolphin_stack = NULL;
    return 0;
}

#endif

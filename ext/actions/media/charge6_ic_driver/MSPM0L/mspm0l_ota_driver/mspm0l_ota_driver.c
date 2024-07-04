/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <init.h>
#include <kernel.h>

#include <thread_timer.h>
#include <mem_manager.h>
#include <os_common_api.h>
#include <application_image.h>

#include <mspm0l_bsl_i2c.h>
#include "mspm0l_i2c.h"
#ifdef CONFIG_LED_MANAGER
#include <led_manager.h>
#endif
#include <os_common_api.h>
#include "board.h"

static char *wlt_mspm0l_stack;
void ToggleLeds(void);
void TurnonsucessLED(void);

BSL_error_t bsl_err;
uint8_t status;
uint8_t rxData2;
/*=============================================================================*/
/* Here is password of the boot code for update. The last two bytes if the start address of the boot code.*/
const uint8_t BSL_PW_RESET[32] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF};

static int mspm0l_ota_driver_fn(void)
{
    uint8_t section;
    printk("[%s,%d] start!!\n",__FUNCTION__, __LINE__);
    //delay_cycles(INIT_DELAY);
    ToggleLeds();  /*To show code start*/

   //while (1) {
        delay_cycles(SWITCH_DELAY);
        bsl_err = eBSL_success;
        ToggleLeds();  /* Show we are starting BSL*/
        #ifdef Hardware_Invoke
        Host_BSL_entry_sequence();  /*PLACE TARGET INTO BSL MODE by hardware invoke*/
        delay_cycles(ENTRY_DELAY);
        #endif
        bsl_err = Host_BSL_Connection();
        delay_cycles(BSL_DELAY);
        status = Status_check();  /*Check the status of the target: BSL mode or application mode*/
        if (status == 0x51)  /*BSL mode 0x51; application mode 0x22*/
        {
            delay_cycles(ACK_DELAY);
            bsl_err = Host_BSL_GetID();
            if (BSL_MAX_BUFFER_SIZE >= MAX_PACKET_SIZE) {
                bsl_err =
                    Host_BSL_loadPassword((uint8_t*) BSL_PW_RESET);
                if (bsl_err == eBSL_success) {
                    bsl_err = Host_BSL_MassErase();
                    if (bsl_err == eBSL_success) {
                        delay_cycles(10);
                        /*WRITE THE ENTIRE PROGRAM MEMORY SECTION TO TARGET*/
                        for (section = 0;
                                section < (sizeof(App1_Addr) /
                                            sizeof(App1_Addr[0]));
                                section++) {
                            bsl_err = Host_BSL_writeMemory(
                                App1_Addr[section], App1_Ptr[section],
                                App1_Size[section]);
                            if (bsl_err != eBSL_success) break;
                        }
                        printk("[%s,%d] MCU UPDAT OK !!!\n", __FUNCTION__, __LINE__);
                        TurnonsucessLED();
                        /*Start the application*/
                        //Host_BSL_StartApp();
                        return 1;
                    } else {
                        TurnOnErrorLED();  /* Mass Erase failed error*/
                        printk("[%s,%d] Mass Erase failed error !!!\n", __FUNCTION__, __LINE__);
                    }
                } else {
                    TurnOnErrorLED();  /* Password failed error*/
                    printk("[%s,%d] Password failed error !!!\n", __FUNCTION__, __LINE__);
                }

            } else {
                TurnOnErrorLED();  /*Buffer less than MAX_PACKET_SIZE error*/
                printk("[%s,%d] Buffer less than MAX_PACKET_SIZE error !!!\n", __FUNCTION__, __LINE__);
            }
        }
    //}
    printk("[%s,%d] exit!!\n",__FUNCTION__, __LINE__);
    return 0;
}
#if 1
static int mspm0l_is_bootloader_ota(void)
{
    uint8_t section;
    printk("[%s,%d] start!!\n",__FUNCTION__, __LINE__);

    delay_cycles(ACK_DELAY);
    bsl_err = Host_BSL_GetID();
    if (BSL_MAX_BUFFER_SIZE >= MAX_PACKET_SIZE) {
        bsl_err =
            Host_BSL_loadPassword((uint8_t*) BSL_PW_RESET);
        if (bsl_err == eBSL_success) {
            bsl_err = Host_BSL_MassErase();
            if (bsl_err == eBSL_success) {
                delay_cycles(10);
                /*WRITE THE ENTIRE PROGRAM MEMORY SECTION TO TARGET*/
                for (section = 0;
                        section < (sizeof(App1_Addr) /
                                    sizeof(App1_Addr[0]));
                        section++) {
                    bsl_err = Host_BSL_writeMemory(
                        App1_Addr[section], App1_Ptr[section],
                        App1_Size[section]);
                    if (bsl_err != eBSL_success) break;
                }
                printk("[%s,%d] MCU UPDAT OK !!!\n", __FUNCTION__, __LINE__);
                TurnonsucessLED();
                /*Start the application*/
                for(uint8_t i = 0; i < 5; i++){
                    Host_BSL_StartApp();
                    delay_cycles(10);
                }
            } else {
                TurnOnErrorLED();  /* Mass Erase failed error*/
                printk("[%s,%d] Mass Erase failed error !!!\n", __FUNCTION__, __LINE__);
            }
        } else {
            TurnOnErrorLED();  /* Password failed error*/
            printk("[%s,%d] Password failed error !!!\n", __FUNCTION__, __LINE__);
        }

    } else {
        TurnOnErrorLED();  /*Buffer less than MAX_PACKET_SIZE error*/
        printk("[%s,%d] Buffer less than MAX_PACKET_SIZE error !!!\n", __FUNCTION__, __LINE__);
    }  
    printk("[%s,%d] end!!\n",__FUNCTION__, __LINE__);
    return 0;  
}
#endif
/*
 * Toggle the LEDs to show we are going to BSL the target
 */
void ToggleLeds(void)
{
#ifdef CONFIG_LED_MANAGER
    pwm_breath_ctrl_t ctrl;		
	ctrl.rise_time_ms = 0;
	ctrl.down_time_ms = 0;
	ctrl.high_time_ms = 400;
	ctrl.low_time_ms = 400;
    //led_manager_set_breath(128, &ctrl, OS_FOREVER, NULL);

#endif
}

void TurnonsucessLED(void)
{
#ifdef CONFIG_LED_MANAGER

	//led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);

#endif

}
/*
static void  wlt_mspm0l_proceess_thread(void *p1, void *p2, void *p3)
{
    mspm0l_ota_driver_fn();
}
*/
void mcu_mspm0l_notify_enter_bls(void);
int mcu_mspm0l_ota_i2c_init(void);
int mcu_get_fw_version(void);
int mcu_mspm0l_ota_is_init(void);

void mspm0l_ota_init(void)
{
    //int mcu_ver;
    //if (key_bt_status_read() /*&& key_vol_down_status_read()*/ && key_broadcast_status_read())
    {
        //delay_cycles(100);
        printk("[%s,%d] start!!\n",__FUNCTION__, __LINE__);
        mcu_mspm0l_ota_i2c_init();
    //is bootloader statue
        bsl_err = Host_BSL_Connection();
        delay_cycles(BSL_DELAY);
        status = Status_check();  /*Check the status of the target: BSL mode or application mode*/
        printk("[%s,%d] status: %x !!\n",__FUNCTION__, __LINE__,status);
        if (status == 0x51)  /*BSL mode 0x51; application mode 0x22*/
        {  
            mspm0l_is_bootloader_ota();
        }
        #if 0
        else if(status == 0x22){
            //if (key_bt_status_read() /*&& key_vol_down_status_read()*/ && key_broadcast_status_read())
            {            
                mcu_ver = mcu_get_fw_version();
                //if(mcu_ver < 0)
                //return;

                if(mcu_ver != APP_FW_VER){

                    mcu_mspm0l_notify_enter_bls();
                    delay_cycles(100);
                    mspm0l_ota_driver_fn();
                    /*
                    if(!wlt_mspm0l_stack)
                    {
                        wlt_mspm0l_stack = app_mem_malloc(1024);
                        if (wlt_mspm0l_stack) {
                            os_thread_create(wlt_mspm0l_stack, 1024, wlt_mspm0l_proceess_thread,
                                    NULL, NULL, NULL, 8, 0, 0);
                        }else{
                            printk("[%s,%d] wlt_mspm0l_stack mem_malloc failed\n\n",__FUNCTION__, __LINE__);
                        }
                    }  
                    */
                } 
            } 
        }
        else{

        }
        #endif
        printk("[%s,%d] end!!\n",__FUNCTION__, __LINE__);
    }
}

int user_mspm0l_ota_process(void)
{
    int mcu_ver;
    int ret = 0;
    //delay_cycles(100);
    if(!mcu_mspm0l_ota_is_init()){
        mcu_mspm0l_ota_i2c_init();
    }
    printk("[%s,%d] start!!\n",__FUNCTION__, __LINE__);
//is bootloader statue
    bsl_err = Host_BSL_Connection();
    delay_cycles(BSL_DELAY);
    status = Status_check();  /*Check the status of the target: BSL mode or application mode*/
    printk("[%s,%d] status: %x !!\n",__FUNCTION__, __LINE__,status);
    if (status == 0x51)  /*BSL mode 0x51; application mode 0x22*/
    {  
        mspm0l_is_bootloader_ota();
    }
    else if(status == 0x22){
        mcu_ver = mcu_get_fw_version();
        //if(mcu_ver < 0)
        //return;

        if(mcu_ver != APP_FW_VER){

            mcu_mspm0l_notify_enter_bls();
            delay_cycles(100);
            ret = mspm0l_ota_driver_fn();

        }
        else{
            printk("mcu ver is %d ,== update ver\n\n",mcu_ver);
            ret = 2;
        } 
    } 
    else{

    }
    printk("[%s,%d] end!!\n",__FUNCTION__, __LINE__);
    return ret;
}

void user_mspm0l_ota_sucess_startapp(void)
{
     printk("[%s,%d] !!\n",__FUNCTION__, __LINE__);
        /*Start the application*/
    Host_BSL_StartApp();
}

void mspm0l_ota_deinit(void)
{
    if (!wlt_mspm0l_stack)
        return;

    app_mem_free(wlt_mspm0l_stack);
    wlt_mspm0l_stack = NULL;
	
}
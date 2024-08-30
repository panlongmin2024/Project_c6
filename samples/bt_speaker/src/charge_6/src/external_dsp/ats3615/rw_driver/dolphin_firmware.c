/*******************************************************************************
 * Project        Dolphin DSP ATS3615 NTI
 * Copyright      2023
 * Company        Harman
 *                All rights reserved
 ******************************************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/dolphin_firmware.h"
#include "../include/dolphin_rw.h"
#include <mem_manager.h>
#define dbg_dolphin printk
//#define dbg_dolphin(...)

#include <zephyr.h>
#define Dolphin_Firmware_Sleep_ms(ms) k_busy_wait(ms*1000)

extern int __ram_dsp_start, __ram_dsp_size;

firmware_prog_status_t Dolphin_Firmware_Send(void* spidev, const void* firmware_data)
{
    const uint32_t* firmware = (const uint32_t*)firmware_data;
    int write_size,one_write_size,max_size = 0x1F0;
    int ret = FIRMWARE_PROG_STATUS_OK;

    int * buffer = malloc(max_size);
    if (!buffer)
    {
        printk("malloc(%d) fail %s:%d\n", max_size, __FUNCTION__, __LINE__);
        ret = FIRMWARE_PROG_STATUS_MALLOC_FAIL;
        goto exit;
    }
    
    if(max_size > 0x8000)
    {
        max_size = 0X8000;
    }

    for (int i = 0;;)
    {
        uint32_t cmd = firmware[i++];

        if (cmd == FIRMWARE_PROG_CMD_DELAY_MS){ // command : delay
        
            uint32_t delayms = firmware[i++];
            dbg_dolphin("Delay %d ms\n", delayms);
            Dolphin_Firmware_Sleep_ms(delayms);
        }else if (cmd == FIRMWARE_PROG_CMD_END){ // command : end
            break;
        }else if (cmd >= 0x50000000 && cmd < 0x54000000){ // System Memory
            uint32_t data = firmware[i++];
            uint32_t tmp_data = 0;
            dbg_dolphin("Write Dolphin Cmd @ %08x : %08x\n", cmd, data);
            if (Dolphin_Write_Reg(spidev, cmd, data)){
                ret = FIRMWARE_PROG_STATUS_DSP_WRONG_CRC;
                goto exit;
            }
            
            Dolphin_Read_Reg(spidev, cmd, (unsigned int*)&tmp_data);
            if(data != tmp_data)
                dbg_dolphin("Read Dolphin Cmd @ %08x : %08x\n", cmd, data);
        }else if (cmd >= 0x30000000 && cmd < 0x50000000){ // IRAM & DRAM
            uint32_t num_w32 = firmware[i++];
            uint32_t num_bytes = num_w32 * 4;
            dbg_dolphin("Write Dolphin Mem @ %08x : [%d bytes]\n", cmd, num_bytes);
            for(write_size = 0;write_size < num_bytes;){
                one_write_size = ((num_bytes - write_size) >= max_size)?max_size:(num_bytes - write_size);
                memcpy(buffer, &firmware[i] + write_size/4, one_write_size);
                
                if(MASTER_Write_ATS3615_Mem(cmd + write_size, (unsigned int*)buffer, one_write_size)){
                    ret = FIRMWARE_PROG_STATUS_DSP_WRONG_CRC;
                    goto exit;
                }
                write_size += one_write_size;
	        }
            i += num_w32;
        }
        else{
            dbg_dolphin("Error : Unknown Command 0x%08x\n",cmd);
            ret = FIRMWARE_PROG_STATUS_WRONG_COMMAND;
            goto exit;
        }
    }
exit:
    if (buffer){
        free(buffer);
    }
    return ret;
}

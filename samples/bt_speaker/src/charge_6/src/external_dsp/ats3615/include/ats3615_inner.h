#ifndef __ATS3615_INNER_H__
#define __ATS3615_INNER_H__
#include <os_common_api.h>
#include "ats3615_reg.h"
#include <wltmcu_manager_supply.h>

extern int __ram_dsp_start, __ram_dsp_size;
extern unsigned int MASTER_Read_ATS3615_Reg(unsigned int ATS3615_addr,unsigned int* data_addr);
extern unsigned int MASTER_Write_ATS3615_Reg(unsigned int ATS3615_addr, unsigned int data_val);
extern unsigned int MASTER_Write_ATS3615_Mem(unsigned int ATS3615_addr, unsigned int* data_addr,unsigned int byte_count);

extern unsigned int ATS3615_SPI_Master_Write_Slave(unsigned int register_addr, unsigned int *r_w_buffer, unsigned int  n/*word count*/);
extern unsigned int ATS3615_SPI_Master_Read_Slave(unsigned int register_addr, unsigned int *r_w_buffer, unsigned int  n/*word count*/);
extern unsigned int chksum_crc32(const unsigned char *block, unsigned int length);
extern int dolphin_comm_deinit(void);
extern os_mutex *dsp_3615_mutex_ptr;
#endif

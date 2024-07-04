/*******************************************************************************
 * Project        Dolphin DSP ATS3615 NTI
 * Copyright      2023
 * Company        Harman
 *                All rights reserved
 ******************************************************************************/
#ifndef Dolphin_RW__H
#define Dolphin_RW__H

#if 1
#include "../include/ats3615_inner.h"
extern unsigned int MASTER_Write_ATS3615_Reg(unsigned int ATS3615_addr, unsigned int data_val);
extern unsigned int MASTER_Read_ATS3615_Reg(unsigned int ATS3615_addr, unsigned int* data_addr);
extern unsigned int MASTER_Write_ATS3615_Mem(unsigned int ATS3615_addr, unsigned int* data_addr, unsigned int byte_count);
extern unsigned int MASTER_Read_ATS3615_Mem(unsigned int ATS3615_addr, unsigned int* data_addr, unsigned int byte_count);

#define	Dolphin_Read_Reg(dev, dolphin_addr, data_addr)				MASTER_Read_ATS3615_Reg( dolphin_addr, data_addr)
#define	Dolphin_Read_Mem(dev, dolphin_addr, data_addr, byte_count)		MASTER_Read_ATS3615_Mem(dolphin_addr, data_addr, byte_count)

#define	Dolphin_Write_Reg(dev, dolphin_addr, data)				MASTER_Write_ATS3615_Reg( dolphin_addr, data)
#define	Dolphin_Write_Mem(dev, dolphin_addr, data_addr, byte_count)		MASTER_Write_ATS3615_Mem(dolphin_addr, data_addr, byte_count)

#else
	
#include <stdint.h>

/* these functions return 0 if no error */
uint32_t Dolphin_Read_Reg(void* dev, uint32_t dolphin_addr,uint32_t* data_addr);
uint32_t Dolphin_Read_Mem(void* dev, uint32_t dolphin_addr,uint32_t* data_addr,uint32_t byte_count);

uint32_t Dolphin_Write_Reg(void* dev, uint32_t dolphin_addr,const uint32_t data_val);
uint32_t Dolphin_Write_Mem(void* dev, uint32_t dolphin_addr,const uint32_t* data_addr,uint32_t byte_count);
#endif

#endif

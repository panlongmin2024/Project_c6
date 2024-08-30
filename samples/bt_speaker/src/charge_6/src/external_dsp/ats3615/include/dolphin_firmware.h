/*******************************************************************************
 * Project        Dolphin DSP ATS3615 NTI
 * Copyright      2023
 * Company        Harman
 *                All rights reserved
 ******************************************************************************/
#ifndef Dolphin_Firmware__H
#define Dolphin_Firmware__H

#include <stdint.h>

#define TUNING_SECTION_NAME ".hmpp_tuning"
#define FIRMWARE_HDR_SECTION_NAME ".dsp_firmware_info"

enum
{
    FIRMWARE_PROG_CMD_DELAY_MS = 0xFFFFFFFE,
    FIRMWARE_PROG_CMD_END = 0xFFFFFFFF
};

typedef enum
{
    FIRMWARE_PROG_STATUS_OK = 0,
    FIRMWARE_PROG_STATUS_DSP_WRONG_CRC = 1,
    FIRMWARE_PROG_STATUS_WRONG_COMMAND = 2,
    FIRMWARE_PROG_STATUS_MALLOC_FAIL = 3
} firmware_prog_status_t;

typedef struct
{
	union
    {
        struct
        {
        	char 	 firmware_info[64];
        	uint32_t start_address;

        	uint32_t code_version;
            uint32_t signalflow_version;

            uint32_t dolphin_com_struct_address;
            uint32_t tuning_data_address;
        };

        char firmware_hdr_data[256];
    };

	char firmware_data[];

} dolphin_dsp_code_hdr_t;

firmware_prog_status_t Dolphin_Firmware_Send(void* spidev, const void* firmware_data);
extern void Dolphin_Firmware_Sleep_ms(int ms);

#endif
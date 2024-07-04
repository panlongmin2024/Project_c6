// Generic MSP430 Device Include
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <init.h>
#include <kernel.h>

#include <thread_timer.h>
#include <mspm0l_bsl_i2c.h>
#include <mspm0l_i2c.h>

#include "stdio.h"
#include "string.h"
#ifdef CONFIG_LED_MANAGER
#include <led_manager.h>
#endif
#include <os_common_api.h>

uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
uint8_t BSL_RX_buffer[MAX_PACKET_SIZE + 2];
uint16_t BSL_MAX_BUFFER_SIZE;

void delay_cycles(unsigned int us)
{
    k_sleep(us);
    //k_busy_wait(us);
}
//*****************************************************************************
//
// ! BSL Entry Sequence
// ! Forces target to enter BSL mode
//
//*****************************************************************************
void Host_BSL_entry_sequence()
{
    /* BSL reset sequence
     *
     *                   H                        +-------------+
     * PB0 NRST          L -----------------------+
     *
     *                   H        +------------------------------+
     * PB16 Invoke GPIO  L--------+                              +----
     *
     */
    /* NRST low, TEST low */
#if 0
    DL_GPIO_clearPins(GPIO_BSL_PORT, GPIO_BSL_NRST_PIN);
    DL_GPIO_clearPins(GPIO_BSL_PORT, GPIO_BSL_Invoke_PIN);
    delay_cycles(BSL_DELAY);

    /* Invoke GPIO high*/
    DL_GPIO_setPins(GPIO_BSL_PORT, GPIO_BSL_Invoke_PIN);
    delay_cycles(BSL_DELAY);
    delay_cycles(33000000);
    /* NRST high*/
    DL_GPIO_setPins(GPIO_BSL_PORT, GPIO_BSL_NRST_PIN);
    delay_cycles(BSL_DELAY);
    delay_cycles(BSL_DELAY);
    delay_cycles(BSL_DELAY);
    DL_GPIO_clearPins(GPIO_BSL_PORT, GPIO_BSL_Invoke_PIN);
#endif    
}

/*
 * Turn on the error LED
 */
void TurnOnErrorLED(void)
{
#ifdef CONFIG_LED_MANAGER
    pwm_breath_ctrl_t ctrl;		
	ctrl.rise_time_ms = 0;
	ctrl.down_time_ms = 0;
	ctrl.high_time_ms = 2000;
	ctrl.low_time_ms = 1000;
    //led_manager_set_breath(128, &ctrl, OS_FOREVER, NULL);

#endif
}
//*****************************************************************************
//
// ! Host_BSL_Connection
// ! Need to send first to build connection with target
//
//*****************************************************************************
//uint8_t test_G;
BSL_error_t Host_BSL_Connection(void)
{
    BSL_error_t bsl_err = eBSL_success;
    i2c_error_t i2c_ack;
    uint32_t ui32CRC;
    printk("[%s,%d] start!!\n",__FUNCTION__, __LINE__);
    BSL_TX_buffer[0] = (uint8_t) PACKET_HEADER;
    BSL_TX_buffer[1] = LSB(CMD_BYTE);
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_CONNECTION;

    // Calculate CRC on the PAYLOAD (CMD + Data)
    ui32CRC = softwareCRC(&BSL_TX_buffer[3], CMD_BYTE);
    // Insert the CRC into the packet
    //*(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES] = ui32CRC;
    memcpy(&BSL_TX_buffer[HDR_LEN_CMD_BYTES],&ui32CRC,sizeof(uint32_t));

    i2c_ack = I2C_writeBuffer(BSL_TX_buffer, HDR_LEN_CMD_BYTES + CRC_BYTES);
    I2C_readBuffer(BSL_RX_buffer, 1);
    if (i2c_ack != uart_noError) {
        printk("[%s,%d] iic_dev  err\n", __FUNCTION__, __LINE__);
    }
    return (bsl_err);
}

//*****************************************************************************
// ! Host_BSL_GetID
// ! Need to send when build connection to get RAM BSL_RX_buffer size and other information
//
//*****************************************************************************
BSL_error_t Host_BSL_GetID(void)
{
    BSL_error_t bsl_err = eBSL_success;
    i2c_error_t i2c_ack;
    uint32_t ui32CRC;
    printk("[%s,%d] start!!\n",__FUNCTION__, __LINE__);
    BSL_TX_buffer[0] = (uint8_t) PACKET_HEADER;
    BSL_TX_buffer[1] = LSB(CMD_BYTE);
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_GET_ID;

    // Calculate CRC on the PAYLOAD (CMD + Data)
    ui32CRC = softwareCRC(&BSL_TX_buffer[3], CMD_BYTE);
    // Insert the CRC into the packet
    //*(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES] = ui32CRC;
    memcpy(&BSL_TX_buffer[HDR_LEN_CMD_BYTES],&ui32CRC,sizeof(uint32_t));

    // Write the packet to the target
    i2c_ack = I2C_writeBuffer(BSL_TX_buffer, HDR_LEN_CMD_BYTES + CRC_BYTES);
    if (i2c_ack != uart_noError) {
        printk("[%s,%d] iic_dev  err\n", __FUNCTION__, __LINE__);
    }

    I2C_readBuffer(BSL_RX_buffer, HDR_LEN_CMD_BYTES + ID_BACK + CRC_BYTES + 1);
    BSL_MAX_BUFFER_SIZE = 0;
   // BSL_MAX_BUFFER_SIZE =
   //     *(uint16_t *) &BSL_RX_buffer[HDR_LEN_CMD_BYTES + ID_BACK - 14];
    //BSL_MAX_BUFFER_SIZE = (uint16_t)(BSL_RX_buffer[HDR_LEN_CMD_BYTES + ID_BACK - 14 + 1] + 
    //    (BSL_RX_buffer[HDR_LEN_CMD_BYTES + ID_BACK - 14 + 0] << 8));
    memcpy(&BSL_MAX_BUFFER_SIZE,&BSL_RX_buffer[HDR_LEN_CMD_BYTES + ID_BACK - 14],sizeof(uint16_t));    
    printk("[%s,%d] BSL_MAX_BUFFER_SIZE 0x%x \n\n", __FUNCTION__, __LINE__,BSL_MAX_BUFFER_SIZE);
    printk(" %x ,%x \n\n",BSL_RX_buffer[HDR_LEN_CMD_BYTES + ID_BACK - 14],BSL_RX_buffer[HDR_LEN_CMD_BYTES + ID_BACK - 14 + 1]);    
    return (bsl_err);
}
//*****************************************************************************
// ! Unlock BSL for programming
// ! If first time, assume blank device.
// ! This will cause a mass erase and destroy previous password.
// ! When programming complete, issue BSL_readPassword to retrieve new one.
//
//*****************************************************************************
BSL_error_t Host_BSL_loadPassword(uint8_t *pPassword)
{
    BSL_error_t bsl_err = eBSL_success;
    i2c_error_t i2c_ack;
    uint32_t ui32CRC;
    printk("[%s,%d] start!!\n",__FUNCTION__, __LINE__);
    BSL_TX_buffer[0] = (uint8_t) PACKET_HEADER;
    BSL_TX_buffer[1] = LSB(PASSWORD_SIZE + CMD_BYTE);
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_RX_PASSWORD;

    memcpy(&BSL_TX_buffer[4], pPassword, PASSWORD_SIZE);

    // Calculate CRC on the PAYLOAD (CMD + Data)
    ui32CRC = softwareCRC(&BSL_TX_buffer[3], PASSWORD_SIZE + CMD_BYTE);

    // Insert the CRC into the packet
    //*(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES + PASSWORD_SIZE] = ui32CRC;
    memcpy(&BSL_TX_buffer[HDR_LEN_CMD_BYTES + PASSWORD_SIZE],&ui32CRC,sizeof(uint32_t));

    // Write the packet to the target
    i2c_ack = I2C_writeBuffer(
        BSL_TX_buffer, HDR_LEN_CMD_BYTES + PASSWORD_SIZE + CRC_BYTES);
    if (i2c_ack != uart_noError) {
        printk("[%s,%d] iic_dev  err\n", __FUNCTION__, __LINE__);
    }

    bsl_err = Host_BSL_getResponse();

    return (bsl_err);
}

//*****************************************************************************
// ! Host_BSL_MassErase
// ! Need to do mess erase before write new image
//
//*****************************************************************************
BSL_error_t Host_BSL_MassErase(void)
{
    BSL_error_t bsl_err = eBSL_success;
    i2c_error_t i2c_ack;
    uint32_t ui32CRC;
    printk("[%s,%d] start!!\n",__FUNCTION__, __LINE__);
    BSL_TX_buffer[0] = (uint8_t) PACKET_HEADER;
    BSL_TX_buffer[1] = LSB(CMD_BYTE);
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_MASS_ERASE;

    // Calculate CRC on the PAYLOAD (CMD + Data)
    ui32CRC = softwareCRC(&BSL_TX_buffer[3], CMD_BYTE);
    // Insert the CRC into the packet
    //*(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES] = ui32CRC;
    memcpy(&BSL_TX_buffer[HDR_LEN_CMD_BYTES],&ui32CRC,sizeof(uint32_t));

    // Write the packet to the target
    i2c_ack = I2C_writeBuffer(BSL_TX_buffer, HDR_LEN_CMD_BYTES + CRC_BYTES);
    if (i2c_ack != uart_noError) {
        printk("[%s,%d] iic_dev  err\n", __FUNCTION__, __LINE__);
    }

    I2C_readBuffer(BSL_RX_buffer, 1);
    delay_cycles(1);
    I2C_readBuffer(BSL_RX_buffer, HDR_LEN_CMD_BYTES + MSG + CRC_BYTES);
    //  bsl_err = Host_BSL_getResponse();
    return (bsl_err);
}

//*****************************************************************************
//
// ! Host_BSL_writeMemory
// ! Writes memory section to target
//
//*****************************************************************************
BSL_error_t Host_BSL_writeMemory(
    uint32_t addr, const uint8_t *data, uint32_t len)
{
    BSL_error_t bsl_err = eBSL_success;
    i2c_error_t i2c_ack;
    uint16_t ui16DataLength;
    uint16_t ui16PayloadSize;
    uint16_t ui16PacketSize;
    uint32_t ui32CRC;
    uint16_t ui16BytesToWrite = len;
    uint32_t TargetAddress    = addr;
    printk("[%s,%d] start len:%d!!\n",__FUNCTION__, __LINE__,ui16BytesToWrite);
    //  pSection->checksum = softwareCRC(pSection->pMemory, pSection->mem_size);

    while (ui16BytesToWrite > 0) {
        //        delay_cycles(1000000); //allow target deal with the packet send before

        if (ui16BytesToWrite >= MAX_PAYLOAD_DATA_SIZE)
            ui16DataLength = MAX_PAYLOAD_DATA_SIZE;
        else
            ui16DataLength = ui16BytesToWrite;

        ui16BytesToWrite = ui16BytesToWrite - ui16DataLength;

        // Add (1byte) command + (4 bytes)ADDRS = 5 bytes to the payload
        ui16PayloadSize = (CMD_BYTE + ADDRS_BYTES + ui16DataLength);

        BSL_TX_buffer[0] = PACKET_HEADER;
        BSL_TX_buffer[1] =
            LSB(ui16PayloadSize);  // typically 4 + MAX_PAYLOAD SIZE
        BSL_TX_buffer[2] = MSB(ui16PayloadSize);
        BSL_TX_buffer[3] = (uint8_t) CMD_PROGRAMDATA;
        //*(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES] = TargetAddress;
        memcpy(&BSL_TX_buffer[HDR_LEN_CMD_BYTES],&TargetAddress,sizeof(uint32_t));

        // Bump up the target address by 2x the number of bytes sent for the next packet
        TargetAddress += ui16DataLength;

        // Copy the data into the BSL_RX_buffer
        memcpy(&BSL_TX_buffer[HDR_LEN_CMD_BYTES + ADDRS_BYTES], data,
            ui16DataLength);

        data += ui16DataLength;

        // Calculate CRC on the PAYLOAD
        ui32CRC = softwareCRC(&BSL_TX_buffer[3], ui16PayloadSize);

        // Calculate the packet length
        ui16PacketSize = HDR_LEN_CMD_BYTES + ADDRS_BYTES + ui16DataLength;

        // Insert the CRC into the packet at the end
        //*(uint32_t *) &BSL_TX_buffer[ui16PacketSize] = ui32CRC;
        memcpy(&BSL_TX_buffer[ui16PacketSize],&ui32CRC,sizeof(uint32_t));

        // Write the packet to the target
        i2c_ack = I2C_writeBuffer(BSL_TX_buffer, ui16PacketSize + CRC_BYTES);
        if (i2c_ack != uart_noError) {
            printk("[%s,%d] iic_dev  err\n", __FUNCTION__, __LINE__);
        }

        // Check operation was complete
        //       bsl_err = Host_BSL_getResponse();
        I2C_readBuffer(BSL_RX_buffer, 1);
        delay_cycles(1);
        I2C_readBuffer(BSL_RX_buffer, (HDR_LEN_CMD_BYTES + MSG + CRC_BYTES));

        if (bsl_err != eBSL_success) break;

    }  // end while

    return (bsl_err);
}

//*****************************************************************************
// ! Host_BSL_StartApp
// ! Start the new application
//
//*****************************************************************************
BSL_error_t Host_BSL_StartApp(void)
{
    BSL_error_t bsl_err = eBSL_success;
    i2c_error_t i2c_ack;
    uint32_t ui32CRC;

    BSL_TX_buffer[0] = (uint8_t) PACKET_HEADER;
    BSL_TX_buffer[1] = LSB(CMD_BYTE);
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_START_APP;

    // Calculate CRC on the PAYLOAD (CMD + Data)
    ui32CRC = softwareCRC(&BSL_TX_buffer[3], CMD_BYTE);
    // Insert the CRC into the packet
    //*(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES] = ui32CRC;
    memcpy(&BSL_TX_buffer[HDR_LEN_CMD_BYTES],&ui32CRC,sizeof(uint32_t));

    // Write the packet to the target
    i2c_ack = I2C_writeBuffer(BSL_TX_buffer, HDR_LEN_CMD_BYTES + CRC_BYTES);
    I2C_readBuffer(BSL_RX_buffer, 1);
    if (i2c_ack != uart_noError) {
        printk("[%s,%d] iic_dev  err\n", __FUNCTION__, __LINE__);
    }
    return (bsl_err);
}

//*****************************************************************************
//
// ! softwareCRC
// ! Can be used on MSP430 and non-MSP platforms
// ! This functions computes the 16-bit CRC (same as BSL on MSP target)
//
//*****************************************************************************
#define CRC32_POLY 0xEDB88320
uint32_t softwareCRC(const uint8_t *data, uint8_t length)
{
    uint32_t ii, jj, byte, crc, mask;
    ;

    crc = 0xFFFFFFFF;

    for (ii = 0; ii < length; ii++) {
        byte = data[ii];
        crc  = crc ^ byte;

        for (jj = 0; jj < 8; jj++) {
            mask = -(crc & 1);
            crc  = (crc >> 1) ^ (CRC32_POLY & mask);
        }
    }

    return crc;
}

//*****************************************************************************
//
// ! Host_BSL_getResponse
// ! For those function calls that don't return specific data.
// ! Returns errors.
//
//*****************************************************************************
BSL_error_t Host_BSL_getResponse(void)
{
    BSL_error_t bsl_err = eBSL_success;

    I2C_readBuffer(
        BSL_RX_buffer, (ACK_BYTE + HDR_LEN_CMD_BYTES + MSG + CRC_BYTES));
    //   Get ACK value
    bsl_err = BSL_RX_buffer[ACK_BYTE + HDR_LEN_CMD_BYTES + MSG - 1];
    //   Return ACK value
    return (bsl_err);
}

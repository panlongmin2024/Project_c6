#include "stdint.h"

#define BSL_DELAY (10)
#define INIT_DELAY      (100)
#define SWITCH_DELAY    (100)
#define ENTRY_DELAY     (100)
#define ACK_DELAY       (10)
#define LOOP_DELAY      (10)
#define LED_DELAY       (10)

#define MAX_PAYLOAD_DATA_SIZE (128)
//MAX_PACKET_SIZE = MAX_PAYLOAD_DATA_SIZE + HDR_LEN_CMD_BYTES + CRC_BYTES = 128 + 8 = 136
#define MAX_PACKET_SIZE (136)

#define Hardware_Invoke

//uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
//uint8_t BSL_RX_buffer[MAX_PACKET_SIZE + 2];
// ! Define BSL CORE commands
#define CMD_CONNECTION (0x12)
#define CMD_GET_ID (0x19)
#define CMD_RX_PASSWORD (0x21)
#define CMD_MASS_ERASE (0x15)
#define CMD_PROGRAMDATA (0x20)
#define CMD_START_APP (0x40)

// ! Other useful macros
#define PACKET_HEADER (0x80)

#define CMD_BYTE (1)
#define HDR_LEN_CMD_BYTES (4)
#define CRC_BYTES (4)
#define PASSWORD_SIZE (uint8_t)(32)
#define ACK_BYTE (1)
#define MSG (1)
#define ID_BACK (24)
#define ADDRS_BYTES (4)

//================================================================================
// ! Conversion MACROS
#define LSB(x) ((x) & 0x00FF)
#define MSB(x) (((x) & 0xFF00) >> 8)

enum {
    //! No Error Occurred! The operation was successful.
    eBSL_success = 0,

    //! Flash write check failed. After programming, a CRC is run on the programmed data
    //! If the CRC does not match the expected result, this error is returned.
    eBSL_flashWriteCheckFailed = 1,

    //! BSL locked.  The correct password has not yet been supplied to unlock the BSL.
    eBSL_locked = 4,

    //! BSL password error. An incorrect password was supplied to the BSL when attempting an unlock.
    eBSL_passwordError = 5,

    //! Unknown error.  The command given to the BSL was not recognized
    eBSL_unknownError = 7,

    eBSL_responseCommand = 0x3B

};
typedef uint8_t BSL_error_t;

enum {
    uart_noError   = 0,     //normal ACK
    header_Error   = 0x51,  //Header incorrect
    checksum_Error = 0x52,  //Checksum incorrect.
    //   packetsize0_Error = 0x53,   //Packet size zero.
    //   packetsizemax_Error = 0x54, //Packet size exceeds buffer.
    unknown_Error = 0x55,  //Unknown error
    //   baudrate_Error = 0x56,      //Unknown baud rate.
    packetsize_Error = 0x57,  //Packet Size Error.

};

typedef uint8_t i2c_error_t;

//uint16_t BSL_MAX_BUFFER_SIZE;
extern uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
extern uint8_t BSL_RX_buffer[MAX_PACKET_SIZE + 2];
extern uint16_t BSL_MAX_BUFFER_SIZE;

void Host_BSL_entry_sequence(void);

void TurnOnErrorLED(void);

BSL_error_t Host_BSL_Connection(void);
BSL_error_t Host_BSL_GetID(void);
BSL_error_t Host_BSL_loadPassword(uint8_t* pPassword);
BSL_error_t Host_BSL_MassErase(void);
BSL_error_t Host_BSL_writeMemory(
    uint32_t addr, const uint8_t* data, uint32_t len);
BSL_error_t Host_BSL_StartApp(void);

uint32_t softwareCRC(const uint8_t* data, uint8_t length);
BSL_error_t Host_BSL_getResponse(void);
void delay_cycles(unsigned int us);
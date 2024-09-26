#ifndef __TAS5828M_H__
#define __TAS5828M_H__

typedef unsigned char cfg_u8;

#if 0
typedef union {
    struct {
        cfg_u8 offset;
        cfg_u8 value;
    };
    struct {
        cfg_u8 command;
        cfg_u8 param;
    };
} cfg_reg;
#else
typedef struct {
    cfg_u8 command;
    cfg_u8 param;
} cfg_reg;
#endif

#define CFG_META_SWITCH (255)
#define CFG_META_DELAY  (254)
#define CFG_META_BURST  (253)

/* Example C code */
/*
    // Externally implemented function that can write n-bytes to the device
    // PCM51xx and TAS5766 targets require the high bit (0x80) of the I2C register to be set on multiple writes.
    // Refer to the device data sheet for more information.
    extern int i2c_write(unsigned char *data, int n);
    // Externally implemented function that delays execution by n milliseconds
    extern int delay(int n);
    // Example implementation.  Call like:
    //     transmit_registers(registers, sizeof(registers)/sizeof(registers[0]));
    void transmit_registers(cfg_reg *r, int n)
    {
        int i = 0;
        while (i < n) {
            switch (r[i].command) {
            case CFG_META_SWITCH:
                // Used in legacy applications.  Ignored here.
                break;
            case CFG_META_DELAY:
                delay(r[i].param);
                break;
            case CFG_META_BURST:
                i2c_write((unsigned char *)&r[i+1], r[i].param);
                i +=  (r[i].param / 2) + 1;
                break;
            default:
                i2c_write((unsigned char *)&r[i], 2);
                break;
            }
            i++;
        }
    }
 */

extern const cfg_reg registers[];
extern const int registers_cnt;
extern const cfg_reg ti_registers[];
extern const int ti_registers_cnt;
extern const int ti_play_registers_cnt;
extern const cfg_reg ti_play_registers[];

#endif

#ifndef HMDSP_TUNING_HDR__H
#define HMDSP_TUNING_HDR__H

/*
    This header is meant to be shared with the firmware team when they own the tuning data.
    
    Firmware team should first check that magic & tuning_format_version match the below enums.
    Then they should display the tuning_name, tuning_file_version and signalflow_version in their logs.
*/
#define HMDSP_MAKE_DEVICE_ID(PRODUCT_ID, MANUFACTURER_ID, HARDWARE_ID) ((((PRODUCT_ID) & 0xff) << 16) | (((MANUFACTURER_ID) & 0xff) << 8) | ((HARDWARE_ID) & 0xff))

#define HMDSP_PRODUCT_ID(DEVICE_ID) (((DEVICE_ID) >> 16)&0xff)
#define HMDSP_MANUFACTURER_ID(DEVICE_ID) (((DEVICE_ID) >> 8)&0xff)
#define HMDSP_HARDWARE_ID(DEVICE_ID) (((DEVICE_ID) >> 0)&0xff)

enum
{
    HMDSP_TUNING_HDR_MAGIC = 0x70736D68, /* hmsp (little endian) */
    HMDSP_TUNING_HDR_TUNING_FORMAT_VERSION = 0x00000004,
    HMDSP_TUNING_HDR_TUNING_NAME_LEN = 64
};

typedef struct
{
    int magic; /* must match HMDSP_TUNING_HDR_MAGIC */
    int version; /* must match HMDSP_TUNING_HDR_TUNING_FORMAT_VERSION */

    char tuning_name[HMDSP_TUNING_HDR_TUNING_NAME_LEN];
    int tuning_file_version;
    int signalflow_version;

    int device_id;

    int default_sigflow_index;
    int default_tuning_index;
} hmdsp_tuning_hdr_t;

#endif

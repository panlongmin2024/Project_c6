#ifndef _HID_DESC_H_
#define _HID_DESC_H_

#define REPORT_ID_1		0x01
#define REPORT_ID_2		0x02
#define REPORT_ID_3		0x03
#define REPORT_ID_4		0x04

/*
 * Report descriptor for keys, button and so on
 */

#if 1
/* Some HID sample Report Descriptor */
static const u8_t hid_report_desc[] = {
	/* 0x05, 0x01,		USAGE_PAGE (Generic Desktop)		*/
	HID_GI_USAGE_PAGE, USAGE_GEN_CONSUMER,
	/* 0x09, 0x00,		USAGE (Undefined)			*/
	HID_LI_USAGE, USAGE_GEN_CONSUMER_CONTROL,
	/* 0xa1, 0x01,		COLLECTION (Application)		*/
	HID_MI_COLLECTION, COLLECTION_APPLICATION,
	/* 0x15, 0x00,			LOGICAL_MINIMUM one-byte (0)	*/
	HID_GI_LOGICAL_MIN(1), 0x00,
	/* 0x26, 0xff, 0x00,		LOGICAL_MAXIMUM two-bytes (255)	*/
	HID_GI_LOGICAL_MAX(1), 0x01,
	/* 0x85, 0x01,			REPORT_ID (1)			*/
	/*HID_GI_REPORT_ID, REPORT_ID_1,*/
	/* 0x75, 0x08,			REPORT_SIZE (8) in bits		*/
	/*HID_GI_REPORT_SIZE, 0x08,*/
	/* 0x95, 0x01,			REPORT_COUNT (1)		*/
	/*HID_GI_REPORT_COUNT, 0x01,*/
	/* 0x09, 0x00,			USAGE (Undefined)		*/
	HID_LI_USAGE, USAGE_GEN_CONSUMER_VOLUME_UP,
	0x09,0xea,
	0x09,0xe2,
	0x09, 0xcd, //USAGE (Play/Pause)
    0x09, 0xb5, //USAGE (Scan Next Track)
    0x09, 0xb6, //USAGE (Scan Previous Track)
    0x09, 0xb3, //USAGE (Fast Forward)
    0x09, 0xb7, //USAGE (Stop)
    0x75, 0x01, //Report Size (0x01)
    0x95, 0x08, //Report Count (0x08),报告的个数为8，即总共有8个bits
	/* v0x81, 0x82,			INPUT (Data,Var,Abs,Vol)	*/
	HID_MI_INPUT, 0x42,
	/* 0x85, 0x02,			REPORT_ID (2)			*/
	/*HID_GI_REPORT_ID, REPORT_ID_2,*/
	/* 0x75, 0x08,			REPORT_SIZE (8) in bits		*/
	/*HID_GI_REPORT_SIZE, 0x08,*/
	/* 0x95, 0x01,			REPORT_COUNT (1)		*/
	/*HID_GI_REPORT_COUNT, 0x01,*/
	/* 0x09, 0x00,			USAGE (Undefined)		*/
	/*HID_LI_USAGE, USAGE_GEN_DESKTOP_UNDEFINED,*/
	/* 0x91, 0x82,			OUTPUT (Data,Var,Abs,Vol)	*/
	/*HID_MI_OUTPUT, 0x82,*/
	/* 0xc0			END_COLLECTION			*/
	HID_MI_COLLECTION_END,
};
#endif

#if 0
static const u8_t hid_report_desc[] =
{
    0x05, 0x0c, //USAGE_PAGE (Consumer)
    0x09, 0x01, //USAGE (Consumer Control)
    0xa1, 0x01, //COLLECTION (Application)
    0x15, 0x00, //Logical Minimum (0x00)
    0x25, 0x01, //Logical Maximum (0x01)
    0x09, 0xe9, //USAGE (Volume Up)
    0x09, 0xea, //USAGE (Volume Down)
    0x09, 0xe2, //USAGE (Mute)
    0x09, 0xcd, //USAGE (Play/Pause)
    0x09, 0xb5, //USAGE (Scan Next Track)
    0x09, 0xb6, //USAGE (Scan Previous Track)
    0x09, 0xb3, //USAGE (Fast Forward)
    0x09, 0xb7, //USAGE (Stop)
    0x75, 0x01, //Report Size (0x01)
    0x95, 0x08, //Report Count (0x08),报告的个数为8，即总共有8个bits
    0x81, 0x42, //Input (Data,Variable,Absolute,No Wrap,Linear,Preferred State,Null state)
    0xc0, //END_COLLECTION           //END_COLLECTION
    /*0xa1, 0x01, //COLLECTION (Application)
    0x15, 0x00, //Logical Minimum (0x00)
    0x25, 0x01, //Logical Maximum (0x01)
    0x09, 0x08,
    0x09, 0x0f,
    0x09, 0x18,
    0x09, 0x1f,
    0xc0, //END_COLLECTION */          //END_COLLECTION
};
#endif

#endif /* _HID_DESC_H_ */

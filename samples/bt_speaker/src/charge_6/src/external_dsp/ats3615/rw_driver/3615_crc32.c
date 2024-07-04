#include <zephyr.h>

static unsigned int crc_tab1[256];

static void chksum_crc32gentab1(void)
{
   unsigned long crc, poly;
   int i, j;

   poly = 0xEDB88320L;
   for (i = 0; i < 256; i++)
   {
		crc = i;
		for (j = 8; j > 0; j--)
		{
			if (crc & 1)
			{
				crc = (crc >> 1) ^ poly;
			}
			else
			{
				crc >>= 1;
			}
		}
		
		crc_tab1[i] = crc;
		
	}
}

unsigned int chksum_crc32(const unsigned char *block, unsigned int length)
{
	register unsigned long crc;
	unsigned long i;

	chksum_crc32gentab1();
	crc = 0xFFFFFFFF;
   
	for (i = 0; i < length; i++)
	{
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_tab1[(crc ^ *block++) & 0xFF];
	}
	
	return (crc ^ 0xFFFFFFFF);
}

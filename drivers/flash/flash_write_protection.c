#include <errno.h>
#include <kernel.h>
#include <linker/sections.h>
#include <flash.h>
#include <soc.h>
#include "xspi_mem_acts.h"
#include "xspi_nor_acts.h"


#define NOR_STATUS1_MASK (0x1f<<2)  /*bp4-bp0  status1 bit6-bit2*/
#define NOR_STATUS2_MASK (0x1<<6)  /*cmp   status2  bit6*/

/**********NOR GD25Q32E  2MB  ****************/
#define GD25Q32E_NOR_CHIPID	0x1640c8  	/*GD25Q32E*/
#define GD25Q32E_CMP0_VAL		0
#define GD25Q32E_CMP0_PROTECT_16KB	0x1b /*0-16KB*/
#define GD25Q32E_CMP0_PROTECT_64KB	0x9  /*0-64KB*/
#define GD25Q32E_CMP0_PROTECT_128KB	0xa  /*0-128KB*/
#define GD25Q32E_CMP0_PROTECT_256KB	0xb  /*0-256KB*/
#define GD25Q32E_CMP0_PROTECT_512KB	0xc  /*0-512KB*/
#define GD25Q32E_CMP0_PROTECT_1MB 	0xd  /*0-1MB*/
#define GD25Q32E_CMP0_PROTECT_2MB 	0xe  /*0-2MB*/
#define GD25Q32E_CMP0_PROTECT_4MB 	0xf  /*0-4MB*/

#define GD25Q32E_NOR_BP_STATUS1 (GD25Q32E_CMP0_PROTECT_16KB<<2)
#define GD25Q32E_NOR_CMP_STATUS2 (GD25Q32E_CMP0_VAL<<6)

struct nor_wp_info {
	unsigned int chipid;
	unsigned char wp_status1_val;
	unsigned char wp_status1_mask;
	unsigned char wp_status2_val;
	unsigned char wp_status2_mask;
};
const struct nor_wp_info  g_nor_wp_info[] = {
	{GD25Q32E_NOR_CHIPID, GD25Q32E_NOR_BP_STATUS1, NOR_STATUS1_MASK, GD25Q32E_NOR_CMP_STATUS2, NOR_STATUS2_MASK},
};

static __ramfunc void nor_write_status(struct xspi_nor_info *sni, u8_t status1, u8_t status2)
{
	u8_t status[2];
	//unsigned int flag;
	status[0] = status1;
	status[1] = status2;
	//flag = sni->spi.flag;
	//sni->spi.flag &= ~SPI_FLAG_NO_IRQ_LOCK;  //wait ready
	xspi_nor_write_status(sni, XSPI_NOR_CMD_WRITE_STATUS2, &status2, 1);
	xspi_nor_write_status(sni, XSPI_NOR_CMD_WRITE_STATUS,  &status1,  1);
	xspi_nor_write_status(sni, XSPI_NOR_CMD_WRITE_STATUS,  status, 2);
	//sni->spi.flag =  flag;
}

static __ramfunc void nor_read_status(struct xspi_nor_info *sni, u8_t *status1, u8_t *status2)
{
	*status1 = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS);
	*status2 = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS2);
}

static void xspi_nor_protect_handle(struct xspi_nor_info *sni, const struct nor_wp_info *wp, bool benable)
{
	u32_t flags;
	u8_t status1, status2;
	u8_t val1;
	flags = irq_lock();
	nor_read_status(sni, &status1, &status2);

	printk("status1-2=0x%x,0x%x\n", status1, status2);

	val1 = wp->wp_status1_mask & status1;
	if(benable){
		if(val1 !=  wp->wp_status1_val){// enable protect
			status1 =  ((~wp->wp_status1_mask) & status1) | wp->wp_status1_val;
			status2 =  ((~wp->wp_status2_mask) & status2) | wp->wp_status2_val;
			printk("enable status1-2=0x%x,0x%x\n", status1, status2);
			nor_write_status(sni, status1, status2);
		}
	}else{
		if(val1 !=	0){// diable protect
			status1 =  (~wp->wp_status1_mask) & status1;
			status2 =  (~wp->wp_status2_mask) & status2;
			printk("disable status1-2=0x%x,0x%x\n", status1, status2);
			nor_write_status(sni, status1, status2);
		}
	}
	nor_read_status(sni, &status1, &status2);
	printk("read again status1-2=0x%x,0x%x\n", status1, status2);
	irq_unlock(flags);

}


int nor_write_protection(const struct device *dev, bool enable)
{
	struct xspi_nor_info *sni = (struct xspi_nor_info *)dev->driver_data;
	const struct nor_wp_info *wp;
	int i;
	for(i = 0; i < ARRAY_SIZE(g_nor_wp_info); i++){
		wp = &g_nor_wp_info[i];
		if(wp->chipid == sni->chipid)
			xspi_nor_protect_handle(sni, wp, enable);
	}
	return 0;
}



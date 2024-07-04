#include <zephyr.h>
#include "../include/ats3615_reg.h"
#include <device.h>
#include <spi.h>
#include <logging/sys_log.h>

struct spi_config dsp_3615_config ={
    .frequency = 40000,
   	.operation = (SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(32) | SPI_LINES_SINGLE),
    .vendor    = 0,
	.slave     = 0,
	.cs        = NULL,
};


unsigned int ATS3615_SPI_Master_Write_Slave(unsigned int register_addr,unsigned int *r_w_buffer, unsigned int  n/*word count*/)
{
    struct device *spi_device = device_get_binding(CONFIG_SPI_1_NAME);
    u32_t addr[1];
    struct spi_buf spi_bufs[2];
    int ret;

    addr[0] = register_addr;
    spi_bufs[0].buf = addr;
    spi_bufs[0].len = 4;

    spi_bufs[1].buf = r_w_buffer;
    spi_bufs[1].len = n*4;

    dsp_3615_config.dev = spi_device;
    ret = spi_transceive(&dsp_3615_config,spi_bufs,2,NULL,0);

    return ret?1:0;
}

unsigned int ATS3615_SPI_Master_Read_Slave(unsigned int register_addr, unsigned int *r_w_buffer, unsigned int  n/*word count*/)
{
    struct device *spi_device = device_get_binding(CONFIG_SPI_1_NAME);
    u32_t addr[1];
    struct spi_buf spi_bufs[1];
    struct spi_buf spi_bufsrx[2];
    int ret;

    dsp_3615_config.dev = spi_device;

    addr[0] = register_addr;
    spi_bufs[0].buf = addr;
    spi_bufs[0].len = 4;
  
   // spi_transceive(&dsp_3615_config,spi_bufs,1,NULL,0);

    *r_w_buffer = 0;
    spi_bufsrx[0].buf = NULL;
    spi_bufsrx[0].len = 0;
    spi_bufsrx[1].buf = r_w_buffer;
    spi_bufsrx[1].len = n*4;
    ret = spi_transceive(&dsp_3615_config,spi_bufs,1,spi_bufsrx,2);
    
     return ret?1:0;
}

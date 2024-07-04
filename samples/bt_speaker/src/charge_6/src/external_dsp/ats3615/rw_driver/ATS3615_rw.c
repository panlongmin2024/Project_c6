#include <zephyr.h>
#include <i2c.h>
#include <spi.h>
#include "../include/ats3615_inner.h"

#define USE_I2C                 	0  //1=I2C,0=SPI
#define DEFAULT_I2C_SLV_ADDR0		0xCC

void interface_init()
{
#if USE_I2C
	mfp_cfg_i2c0(2);
	i2c_init(S100K, 0); 
	// pull up select I2C Slave
#else
  	// sSPI_init();
	// pull down select SPI Slave
#endif
}

unsigned int MASTER_Write_ATS3615_Reg(unsigned int ATS3615_addr, unsigned int data_val)
{
	unsigned char RAM_REG = 0, RAM_ROM = 0, I_D = 0;
  	unsigned int cmd;
	unsigned int addr_offset;
	unsigned int data_val_cmp[1];
	unsigned int ret;

	data_val_cmp[0] = data_val;
	addr_offset = ATS3615_addr & (0xfffff); // 20bit address

	switch(ATS3615_addr >> 28)
	{
	case 3: // DRAM
		break;

	case 4:	// IRAM
		I_D = 1;	
		if ((ATS3615_addr & (1<<20)) == (1<<20)) // IROM
			RAM_ROM = 1;
		break;

	case 5:		//REG or ASRCRAM
		RAM_REG = 1;
		break;

	default:
		printk("write reg addr err\n");
		return 1;
	}

#if USE_I2C
	cmd = (0<<23) | (RAM_REG<<22) | (RAM_ROM<<21) | (I_D<<20) | (addr_offset);
	ret = ATS3615_I2C_Master_Write_Slave(DEFAULT_I2C_SLV_ADDR0, cmd, &data_val, 1);
#else
	cmd=(0<<31) | (RAM_REG<<30) | (RAM_ROM<<29) | (I_D<<28) | (addr_offset<<8);
	ret = ATS3615_SPI_Master_Write_Slave(cmd, data_val_cmp, 1);
#endif

	return ret;
}

unsigned int MASTER_Read_ATS3615_Reg(unsigned int ATS3615_addr, unsigned int* data_addr)
{
	unsigned char RAM_REG = 0, RAM_ROM = 0, I_D = 0;
	unsigned int cmd;
	unsigned int addr_offset;
	unsigned int ret;

	addr_offset = ATS3615_addr & (0xfffff); // 20bit address

	switch(ATS3615_addr >> 28)
	{
	case 3: // DRAM
		break;

	case 4: // IRAM
		I_D = 1;	
		if((ATS3615_addr & (1<<20)) == (1<<20))	// IROM
			RAM_ROM = 1;
		break;

	case 5: // REG or ASRCRAM 
		RAM_REG = 1;
		break;

	default:
		printk("read reg addr err\n");
		return 1;
	}

#if USE_I2C
	cmd = (1<<23) | (RAM_REG<<22) | (RAM_ROM<<21) | (I_D<<20) | (addr_offset);
	ret = ATS3615_I2C_Master_Read_Slave(DEFAULT_I2C_SLV_ADDR0, cmd, data_addr, 1);
#else
	cmd=(1ul<<31) | (RAM_REG<<30) | (RAM_ROM<<29) | (I_D<<28) | (addr_offset<<8);
	ret = ATS3615_SPI_Master_Read_Slave(cmd, data_addr, 1);
#endif

	return ret;
}


unsigned int MASTER_Write_ATS3615_Mem(unsigned int ATS3615_addr, unsigned int* data_addr, unsigned int byte_count)
{
	unsigned char RAM_REG=0, RAM_ROM=0, I_D=0;
	unsigned int cmd;
	unsigned int addr_offset;
	unsigned int crc_value_cal,crc_value_reg;

#if USE_I2C
	unsigned int read_cnt;
#endif	

	if((byte_count == 0) || (byte_count % 4))
		return 1;

	addr_offset = ATS3615_addr & (0xfffff); // 20bit address

	switch(ATS3615_addr >> 28)
	{
	case 3: // DRAM
			break;
	case 4: // IRAM
		I_D = 1;	
		if((ATS3615_addr & (1<<20)) == (1<<20))	//IROM
			RAM_ROM = 1;
		break;
		
	case 5:	// REG or ASRCRAM 
		RAM_REG = 1;
		if(byte_count > 4)//continue read/write
			RAM_ROM = 1;
		break;
	default:
		printk("write mem addr err\n");
		return 1;
	}

#if USE_I2C
	cmd = (0<<23) | (RAM_REG<<22) | (RAM_ROM<<21) | (I_D<<20) | (addr_offset);
	read_cnt=0;

	while(read_cnt < byte_count)
	{
		//我们的i2c控制器不支持连续读写大量数据，所以拆成128B每一帧，拆开后每一帧都要算一次CRC
		if(byte_count - read_cnt > 128)
		{
			ATS3615_I2C_Master_Write_Slave(DEFAULT_I2C_SLV_ADDR0, cmd+read_cnt, data_addr+read_cnt/4, 128/4);
			crc_value_cal=chksum_crc32((const unsigned char *)data_addr+read_cnt, 128);
			ATS3615_I2C_Master_Read_Slave(DEFAULT_I2C_SLV_ADDR0, (1<<23) | (1<<22) | (0<<21) | (0<<20) | (ATS3615_I2CSLV_CRC & 0xfffff), &crc_value_reg, 1);
			if(crc_value_cal != crc_value_reg)
				return 1;
		}
		else
		{
			ATS3615_I2C_Master_Write_Slave(DEFAULT_I2C_SLV_ADDR0, cmd+read_cnt, data_addr+read_cnt/4, (byte_count-read_cnt)/4);
			crc_value_cal=chksum_crc32((const unsigned char *)data_addr+read_cnt, (byte_count-read_cnt));
			ATS3615_I2C_Master_Read_Slave(DEFAULT_I2C_SLV_ADDR0, (1<<23) | (1<<22) | (0<<21) | (0<<20) | (ATS3615_I2CSLV_CRC & 0xfffff), &crc_value_reg, 1);
			if(crc_value_cal != crc_value_reg)
				return 1;
		}
		
		read_cnt += 128;
	}
#else
	cmd=(0ul<<31) | (RAM_REG<<30) | (RAM_ROM<<29) | (I_D<<28) | (addr_offset<<8);
    ATS3615_SPI_Master_Write_Slave(cmd, data_addr, byte_count/4);
	//write complete, check crc
	//k_sleep(50);
	ATS3615_SPI_Master_Read_Slave((1ul<<31) | (1<<30) | (0<<29) | (0<<28) | ((ATS3615_SPISLV_CRC & 0xfffff) << 8), &crc_value_reg, 1);
	crc_value_cal = chksum_crc32((const unsigned char *)data_addr, byte_count);
	if(crc_value_cal != crc_value_reg){
		printk("write mem crc_value_cal = %x  crc_value_reg = %x\n", crc_value_cal, crc_value_reg);
		return 1;
	}
#endif

	return 0;
}

unsigned int MASTER_Read_ATS3615_Mem(unsigned int ATS3615_addr, unsigned int* data_addr, unsigned int byte_count)
{
	unsigned char RAM_REG=0, RAM_ROM=0, I_D=0;
	unsigned int cmd;
	unsigned int addr_offset;
	unsigned int crc_value_cal,crc_value_reg;

#if USE_I2C
	unsigned int read_cnt;
#endif	

  	if((byte_count == 0) || (byte_count % 4))
		return 1;

	addr_offset = ATS3615_addr & (0xfffff); // 20bit address

	switch(ATS3615_addr >> 28)
	{
	case 3: // DRAM
			break;

	case 4: // IRAM
			I_D = 1;	
			if((ATS3615_addr&(1<<20))==(1<<20))	//IROM
				RAM_ROM = 1;
			break;

	case 5:		//REG or ASRCRAM
		RAM_REG = 1;
		if(byte_count>4)//continue read/write
			RAM_ROM = 1;
		break;

	default:
		printk("read mem addr err\n");
		return 1;
	}

#if USE_I2C
	cmd = (1<<23) | (RAM_REG<<22) | (RAM_ROM<<21) | (I_D<<20) | (addr_offset);
	read_cnt = 0;

	while(read_cnt < byte_count)
	{
		//我们的i2c控制器不支持连续读写大量数据，所以拆成128B每一帧，拆开后每一帧都要算一次CRC
		if(byte_count - read_cnt > 128)
		{
			ATS3615_I2C_Master_Read_Slave(DEFAULT_I2C_SLV_ADDR0, cmd+read_cnt, data_addr+read_cnt/4, 128/4);
			crc_value_cal = chksum_crc32((const unsigned char *)data_addr + read_cnt, 128);
			ATS3615_I2C_Master_Read_Slave(DEFAULT_I2C_SLV_ADDR0, (1<<23) | (1<<22) | (0<<21) | (0<<20) | (ATS3615_I2CSLV_CRC & 0xfffff), &crc_value_reg, 1);
			if(crc_value_cal != crc_value_reg)
				return 1;
		}
		else
		{
			ATS3615_I2C_Master_Read_Slave(DEFAULT_I2C_SLV_ADDR0, cmd+read_cnt, data_addr+read_cnt/4, (byte_count-read_cnt) / 4);
			crc_value_cal = chksum_crc32((const unsigned char *)data_addr + read_cnt, (byte_count - read_cnt));
			ATS3615_I2C_Master_Read_Slave(DEFAULT_I2C_SLV_ADDR0, (1<<23) | (1<<22) | (0<<21) | (0<<20) | (ATS3615_I2CSLV_CRC & 0xfffff), &crc_value_reg, 1);
			if(crc_value_cal != crc_value_reg)
				return 1;
		}
		read_cnt += 128;
	}
#else
	cmd = (1ul<<31) | (RAM_REG<<30) | (RAM_ROM<<29) | (I_D<<28) | (addr_offset << 8);
	ATS3615_SPI_Master_Read_Slave(cmd, data_addr, byte_count / 4);
	crc_value_cal = chksum_crc32((const unsigned char *)data_addr, byte_count);
	//read complete, check crc
	ATS3615_SPI_Master_Read_Slave((1ul<<31) | (1<<30) | (0<<29) | (0<<28) | ((ATS3615_SPISLV_CRC & 0xfffff) << 8), &crc_value_reg, 1);
	if(crc_value_cal != crc_value_reg){
		printk("read mem crc_value_cal = %x  crc_value_reg = %x\n", crc_value_cal, crc_value_reg);
		return 1;
	}
#endif

	return 0;
}

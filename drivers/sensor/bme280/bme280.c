/* bmp280.c - Driver for Bosch BMP280 temperature and pressure sensor */

/*
 * Copyright (c) 2016, 2017 Intel Corporation
 * Copyright (c) 2017 IpTronix S.r.l.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <sensor.h>
#include <init.h>
#include <gpio.h>
#include <misc/byteorder.h>
#include <misc/__assert.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_BME280

#ifdef CONFIG_BME280_DEV_TYPE_I2C
#include <i2c.h>
#elif defined CONFIG_BME280_DEV_TYPE_SPI
#include <spi.h>
#endif

#include "bme280.h"

static int bm280_reg_read(struct bme280_data *data,
			  u8_t start, u8_t *buf, int size)
{
#ifdef CONFIG_BME280_DEV_TYPE_I2C
	return i2c_burst_read(data->i2c_master, data->i2c_slave_addr,
			      start, buf, size);
#elif defined CONFIG_BME280_DEV_TYPE_SPI
	u8_t tx_buf[2];
	u8_t rx_buf[2];
	int i;
	int ret;

	for (i = 0; i < size; i++) {

		ret = spi_slave_select(data->spi, data->spi_slave);
		if (ret) {
			ACT_LOG_ID_DBG(ALF_STR_bm280_reg_read__SPI_SLAVE_SELECT_FAI, 1, ret);
			return ret;
		}

		tx_buf[0] = (start + i) | 0x80;
		ret = spi_transceive(data->spi, tx_buf, 2, rx_buf, 2);
		if (ret) {
			ACT_LOG_ID_DBG(ALF_STR_bm280_reg_read__SPI_TRANSCEIVE_FAIL_, 1, ret);
			return ret;
		}

		buf[i] = rx_buf[1];
	}
#endif
	return 0;
}

static int bm280_reg_write(struct bme280_data *data, u8_t reg, u8_t val)
{
#ifdef CONFIG_BME280_DEV_TYPE_I2C
	return i2c_reg_write_byte(data->i2c_master, data->i2c_slave_addr,
				  reg, val);
#elif defined CONFIG_BME280_DEV_TYPE_SPI
	u8_t tx_buf[2];
	u8_t rx_buf[2];
	int ret;

	ret = spi_slave_select(data->spi, data->spi_slave);
	if (ret) {
		ACT_LOG_ID_DBG(ALF_STR_bm280_reg_write__SPI_SLAVE_SELECT_FAI, 1, ret);
		return ret;
	}

	reg &= 0x7F;
	tx_buf[0] = reg;
	tx_buf[1] = val;
	ret = spi_transceive(data->spi, tx_buf, 2, rx_buf, 2);
	if (ret) {
		ACT_LOG_ID_DBG(ALF_STR_bm280_reg_write__SPI_TRANSCEIVE_FAIL_, 1, ret);
		return ret;
	}

#endif
	return 0;
}

/*
 * Compensation code taken from BME280 datasheet, Section 4.2.3
 * "Compensation formula".
 */
static void bme280_compensate_temp(struct bme280_data *data, s32_t adc_temp)
{
	s32_t var1, var2;

	var1 = (((adc_temp >> 3) - ((s32_t)data->dig_t1 << 1)) *
		((s32_t)data->dig_t2)) >> 11;
	var2 = (((((adc_temp >> 4) - ((s32_t)data->dig_t1)) *
		  ((adc_temp >> 4) - ((s32_t)data->dig_t1))) >> 12) *
		((s32_t)data->dig_t3)) >> 14;

	data->t_fine = var1 + var2;
	data->comp_temp = (data->t_fine * 5 + 128) >> 8;
}

static void bme280_compensate_press(struct bme280_data *data, s32_t adc_press)
{
	s64_t var1, var2, p;

	var1 = ((s64_t)data->t_fine) - 128000;
	var2 = var1 * var1 * (s64_t)data->dig_p6;
	var2 = var2 + ((var1 * (s64_t)data->dig_p5) << 17);
	var2 = var2 + (((s64_t)data->dig_p4) << 35);
	var1 = ((var1 * var1 * (s64_t)data->dig_p3) >> 8) +
		((var1 * (s64_t)data->dig_p2) << 12);
	var1 = (((((s64_t)1) << 47) + var1)) * ((s64_t)data->dig_p1) >> 33;

	/* Avoid exception caused by division by zero. */
	if (var1 == 0) {
		data->comp_press = 0;
		return;
	}

	p = 1048576 - adc_press;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = (((s64_t)data->dig_p9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((s64_t)data->dig_p8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + (((s64_t)data->dig_p7) << 4);

	data->comp_press = (u32_t)p;
}

static void bme280_compensate_humidity(struct bme280_data *data,
				       s32_t adc_humidity)
{
	s32_t h;

	h = (data->t_fine - ((s32_t)76800));
	h = ((((adc_humidity << 14) - (((s32_t)data->dig_h4) << 20) -
		(((s32_t)data->dig_h5) * h)) + ((s32_t)16384)) >> 15) *
		(((((((h * ((s32_t)data->dig_h6)) >> 10) * (((h *
		((s32_t)data->dig_h3)) >> 11) + ((s32_t)32768))) >> 10) +
		((s32_t)2097152)) * ((s32_t)data->dig_h2) + 8192) >> 14);
	h = (h - (((((h >> 15) * (h >> 15)) >> 7) *
		((s32_t)data->dig_h1)) >> 4));
	h = (h > 419430400 ? 419430400 : h);

	data->comp_humidity = (u32_t)(h >> 12);
}

static int bme280_sample_fetch(struct device *dev, enum sensor_channel chan)
{
	struct bme280_data *data = dev->driver_data;
	u8_t buf[8];
	s32_t adc_press, adc_temp, adc_humidity;
	int size = 6;
	int ret;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	if (data->chip_id == BME280_CHIP_ID) {
		size = 8;
	}
	ret = bm280_reg_read(data, BME280_REG_PRESS_MSB, buf, size);
	if (ret < 0) {
		return ret;
	}

	adc_press = (buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);
	adc_temp = (buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4);

	bme280_compensate_temp(data, adc_temp);
	bme280_compensate_press(data, adc_press);

	if (data->chip_id == BME280_CHIP_ID) {
		adc_humidity = (buf[6] << 8) | buf[7];
		bme280_compensate_humidity(data, adc_humidity);
	}

	return 0;
}

static int bme280_channel_get(struct device *dev,
			      enum sensor_channel chan,
			      struct sensor_value *val)
{
	struct bme280_data *data = dev->driver_data;

	switch (chan) {
	case SENSOR_CHAN_TEMP:
		/*
		 * data->comp_temp has a resolution of 0.01 degC.  So
		 * 5123 equals 51.23 degC.
		 */
		val->val1 = data->comp_temp / 100;
		val->val2 = data->comp_temp % 100 * 10000;
		break;
	case SENSOR_CHAN_PRESS:
		/*
		 * data->comp_press has 24 integer bits and 8
		 * fractional.  Output value of 24674867 represents
		 * 24674867/256 = 96386.2 Pa = 963.862 hPa
		 */
		val->val1 = (data->comp_press >> 8) / 1000;
		val->val2 = (data->comp_press >> 8) % 1000 * 1000 +
			(((data->comp_press & 0xff) * 1000) >> 8);
		break;
	case SENSOR_CHAN_HUMIDITY:
		/*
		 * data->comp_humidity has 22 integer bits and 10
		 * fractional.  Output value of 47445 represents
		 * 47445/1024 = 46.333 %RH
		 */
		val->val1 = (data->comp_humidity >> 10);
		val->val2 = (((data->comp_humidity & 0x3ff) * 1000 * 1000) >> 10);
		val->val1 = val->val1 * 1000 + (val->val2 * 1000) / 1000000;
		val->val2 = (val->val2 * 1000) % 1000000;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct sensor_driver_api bme280_api_funcs = {
	.sample_fetch = bme280_sample_fetch,
	.channel_get = bme280_channel_get,
};

static int bme280_read_compensation(struct bme280_data *data)
{
	u16_t buf[12];
	u8_t hbuf[7];
	int err = 0;

	err = bm280_reg_read(data, BME280_REG_COMP_START,
			     (u8_t *)buf, sizeof(buf));

	if (err < 0) {
		return err;
	}

	data->dig_t1 = sys_le16_to_cpu(buf[0]);
	data->dig_t2 = sys_le16_to_cpu(buf[1]);
	data->dig_t3 = sys_le16_to_cpu(buf[2]);

	data->dig_p1 = sys_le16_to_cpu(buf[3]);
	data->dig_p2 = sys_le16_to_cpu(buf[4]);
	data->dig_p3 = sys_le16_to_cpu(buf[5]);
	data->dig_p4 = sys_le16_to_cpu(buf[6]);
	data->dig_p5 = sys_le16_to_cpu(buf[7]);
	data->dig_p6 = sys_le16_to_cpu(buf[8]);
	data->dig_p7 = sys_le16_to_cpu(buf[9]);
	data->dig_p8 = sys_le16_to_cpu(buf[10]);
	data->dig_p9 = sys_le16_to_cpu(buf[11]);

	if (data->chip_id == BME280_CHIP_ID) {
		err = bm280_reg_read(data, BME280_REG_HUM_COMP_PART1,
				     &data->dig_h1, 1);
		if (err < 0) {
			return err;
		}

		err = bm280_reg_read(data, BME280_REG_HUM_COMP_PART2, hbuf, 7);
		if (err < 0) {
			return err;
		}

		data->dig_h2 = (hbuf[1] << 8) | hbuf[0];
		data->dig_h3 = hbuf[2];
		data->dig_h4 = (hbuf[3] << 4) | (hbuf[4] & 0x0F);
		data->dig_h5 = ((hbuf[4] >> 4) & 0x0F) | (hbuf[5] << 4);
		data->dig_h6 = hbuf[6];
	}

	return 0;
}

static int bme280_chip_init(struct device *dev)
{
	struct bme280_data *data = (struct bme280_data *) dev->driver_data;
	int err;

	err = bm280_reg_read(data, BME280_REG_ID, &data->chip_id, 1);
	if (err < 0) {
		return err;
	}

	if (data->chip_id == BME280_CHIP_ID) {
		ACT_LOG_ID_DBG(ALF_STR_bme280_chip_init__BME280_CHIP_DETECTED, 0);
	} else if (data->chip_id == BMP280_CHIP_ID_MP ||
		   data->chip_id == BMP280_CHIP_ID_SAMPLE_1) {
		ACT_LOG_ID_DBG(ALF_STR_bme280_chip_init__BMP280_CHIP_DETECTED, 0);
	} else {
		ACT_LOG_ID_DBG(ALF_STR_bme280_chip_init__BAD_CHIP_ID_0XX, 1, data->chip_id);
		return -ENOTSUP;
	}

	err = bme280_read_compensation(data);
	if (err < 0) {
		return err;
	}

	if (data->chip_id == BME280_CHIP_ID) {
		err = bm280_reg_write(data, BME280_REG_CTRL_HUM,
				      BME280_HUMIDITY_OVER);
		if (err < 0) {
			return err;
		}
	}

	err = bm280_reg_write(data, BME280_REG_CTRL_MEAS, BME280_CTRL_MEAS_VAL);
	if (err < 0) {
		return err;
	}

	err = bm280_reg_write(data, BME280_REG_CONFIG, BME280_CONFIG_VAL);
	if (err < 0) {
		return err;
	}

	return 0;
}

#ifdef CONFIG_BME280_DEV_TYPE_SPI
static inline int bme280_spi_init(struct bme280_data *data)
{
	struct spi_config spi_config;
	int ret;

	data->spi = device_get_binding(CONFIG_BME280_SPI_DEV_NAME);
	if (!data->spi) {
		SYS_LOG_DBG("spi device not found: %s",
			    CONFIG_BME280_SPI_DEV_NAME);
		return -EINVAL;
	}

	spi_config.config = SPI_WORD(8) | SPI_TRANSFER_MSB |
			    SPI_MODE_CPOL | SPI_MODE_CPHA;
	spi_config.max_sys_freq = 4;

	ret = spi_configure(data->spi, &spi_config);
	if (ret) {
		SYS_LOG_DBG("SPI configuration error %s %d\n",
			    CONFIG_BME280_SPI_DEV_NAME, ret);
		return ret;
	}

	return 0;
}
#endif

int bme280_init(struct device *dev)
{
	struct bme280_data *data = dev->driver_data;

#ifdef CONFIG_BME280_DEV_TYPE_I2C
	data->i2c_master = device_get_binding(CONFIG_BME280_I2C_MASTER_DEV_NAME);
	if (!data->i2c_master) {
		SYS_LOG_DBG("i2c master not found: %s",
			    CONFIG_BME280_I2C_MASTER_DEV_NAME);
		return -EINVAL;
	}

	data->i2c_slave_addr = BME280_I2C_ADDR;
#elif defined CONFIG_BME280_DEV_TYPE_SPI
	if (bme280_spi_init(data) < 0) {
		SYS_LOG_DBG("spi master not found: %s",
			    CONFIG_BME280_SPI_DEV_NAME);
		return -EINVAL;
	}

	data->spi_slave = CONFIG_BME280_SPI_DEV_SLAVE;
#endif

	if (bme280_chip_init(dev) < 0) {
		return -EINVAL;
	}

	return 0;
}

static struct bme280_data bme280_data;

DEVICE_AND_API_INIT(bme280, CONFIG_BME280_DEV_NAME, bme280_init, &bme280_data,
		    NULL, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
		    &bme280_api_funcs);

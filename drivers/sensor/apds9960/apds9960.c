/*
 *
 *Copyright (c) 2017 Intel Corporation
 *
 *SPDX-License-Identifier: Apache-2.0
 *
 */

/* @file
 * @brief driver for APDS9960 ALS/RGB/gesture/proximity sensor
 */

#include <device.h>
#include <sensor.h>
#include <i2c.h>
#include <misc/__assert.h>
#include <init.h>
#include <kernel.h>

#include "apds9960.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_APDS9960

static int apds9960_sample_fetch(struct device *dev, enum sensor_channel chan)
{
	struct apds9960_data *data = dev->driver_data;
	u8_t cmsb, clsb, rmsb, rlsb, blsb, bmsb, glsb, gmsb;
	u8_t pdata;
	int temp = 0;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	/* Read lower byte following by MSB  Ref: Datasheet : RGBC register */
	temp = i2c_reg_read_byte(data->i2c, APDS9960_I2C_ADDRESS,
			APDS9960_CDATAL_REG, &clsb);
	if (temp < 0) {
		return temp;
	}

	temp = i2c_reg_read_byte(data->i2c, APDS9960_I2C_ADDRESS,
			APDS9960_CDATAH_REG, &cmsb);
	if (temp < 0) {
		return temp;
	}

	temp = i2c_reg_read_byte(data->i2c, APDS9960_I2C_ADDRESS,
			APDS9960_RDATAL_REG, &rlsb);
	if (temp < 0) {
		return temp;
	}

	temp = i2c_reg_read_byte(data->i2c, APDS9960_I2C_ADDRESS,
			APDS9960_RDATAH_REG, &rmsb);
	if (temp < 0) {
		return temp;
	}

	temp = i2c_reg_read_byte(data->i2c, APDS9960_I2C_ADDRESS,
			APDS9960_GDATAL_REG, &glsb);
	if (temp < 0) {
		return temp;
	}

	temp = i2c_reg_read_byte(data->i2c, APDS9960_I2C_ADDRESS,
			APDS9960_GDATAH_REG, &gmsb);
	if (temp < 0) {
		return temp;
	}

	temp = i2c_reg_read_byte(data->i2c, APDS9960_I2C_ADDRESS,
			APDS9960_BDATAL_REG, &blsb);
	if (temp < 0) {
		return temp;
	}

	temp = i2c_reg_read_byte(data->i2c, APDS9960_I2C_ADDRESS,
			APDS9960_BDATAH_REG, &bmsb);
	if (temp < 0) {
		return temp;
	}

	temp = i2c_reg_read_byte(data->i2c, APDS9960_I2C_ADDRESS,
			APDS9960_PDATA_REG, &pdata);
	if (temp < 0) {
		return temp;
	}

	data->sample_c = (cmsb << 8) + clsb;
	data->sample_r = (rmsb << 8) + rlsb;
	data->sample_g = (gmsb << 8) + glsb;
	data->sample_b = (bmsb << 8) + blsb;
	data->pdata    = pdata;

	return 0;
}

static int apds9960_channel_get(struct device *dev,
				enum sensor_channel chan,
				struct sensor_value *val)
{
	struct apds9960_data *data = dev->driver_data;

	switch (chan) {
	case SENSOR_CHAN_LIGHT:
		val->val1 = data->sample_c;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_RED:
		val->val1 = data->sample_r;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_GREEN:
		val->val1 = data->sample_g;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_BLUE:
		val->val1 = data->sample_b;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_PROX:
		val->val1 = data->pdata;
		val->val2 = 0;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static int apds9960_setproxint_lowthresh(struct device *dev, u8_t threshold)
{
	struct apds9960_data *data = dev->driver_data;
	int temp = 0;

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_PILT_REG, threshold);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_setproxint_lowthresh___FAILED, 0);
		return temp;
	}

	return 0;
}

static int apds9960_setproxint_highthresh(struct device *dev, u8_t threshold)
{
	struct apds9960_data *data = dev->driver_data;
	int temp = 0;

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_PIHT_REG, threshold);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_setproxint_highthresh___FAILED, 0);
		return temp;
	}

	return 0;
}

static int apds9960_setlightint_lowthresh(struct device *dev, u16_t threshold)
{
	struct apds9960_data *data = dev->driver_data;
	int temp = 0;
	u8_t val_low;
	u8_t val_high;

	/* Break 16-bit threshold into 2 8-bit values */
	val_low = threshold & 0x00FF;
	val_high = (threshold & 0xFF00) >> 8;

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_INT_AILTL_REG, val_low);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_setlightint_lowthresh___FAILED, 0);
		return temp;
	}

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_INT_AILTH_REG, val_high);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_setlightint_lowthresh___FAILED, 0);
		return temp;
	}

	return 0;
}

static int apds9960_setlightint_highthresh(struct device *dev, u16_t threshold)
{
	struct apds9960_data *data = dev->driver_data;
	int temp = 0;
	u8_t val_low;
	u8_t val_high;

	/* Break 16-bit threshold into 2 8-bit values */
	val_low = threshold & 0x00FF;
	val_high = (threshold & 0xFF00) >> 8;

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_INT_AIHTL_REG, val_low);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_setlightint_highthresh___FAILED, 0);
		return temp;
	}

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_INT_AIHTH_REG, val_high);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_setlightint_highthresh___FAILED, 0);
		return temp;
	}

	return 0;
}

static int apds9960_proxy_setup(struct device *dev, int gain)
{
	struct apds9960_data *data = dev->driver_data;
	int temp = 0;
	u8_t mask = APDS9960_ENABLE_PROXY | APDS9960_ENABLE_PIEN;
	u8_t val = APDS9960_PROXY_ON;

	/* Power ON */
	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_ENABLE_REG, BIT(0), APDS9960_POWER_ON);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_proxy_setup__POWER_ON_BIT_NOT_SET, 0);
		return temp;
	}

	/* ADC value */
	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_ATIME_REG, APDS9960_ATIME_WRTIE, APDS9960_ADC_VALUE);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_proxy_setup__ADC_BITS_ARE_NOT_WRI, 0);
		return temp;
	}

	/* proxy Gain */
	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_CONTROL_REG, APDS9960_CONTROL_PGAIN,
		(gain & APDS9960_PGAIN_8X));

	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_proxy_setup__PROXY_GAIN_IS_NOT_SE, 0);
		return temp;
	}

	/* Enable proxy */
	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_ENABLE_REG, mask, val);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_proxy_setup__PROXY_IS_NOT_ENABLED, 0);
		return temp;
	}

	return temp;
}

static int apds9960_ambient_setup(struct device *dev, int gain)
{
	struct apds9960_data *data = dev->driver_data;
	int temp = 0;

	/* Power ON */
	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_ENABLE_REG, BIT(0), APDS9960_POWER_ON);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_ambient_setup__POWER_ON_BIT_NOT_SET, 0);
		return -EIO;
	}

	/* ADC value */
	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_ATIME_REG, APDS9960_ATIME_WRTIE, APDS9960_ADC_VALUE);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_ambient_setup__ADC_BITS_ARE_NOT_WRI, 0);
		return temp;
	}

	/* ALE Gain */
	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_CONTROL_REG, APDS9960_CONTROL_AGAIN,
		(gain & APDS9960_AGAIN_64X));
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_ambient_setup__AMBIENT_GAIN_IS_NOT_, 0);
		return temp;
	}

	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_ENABLE_REG, BIT(4), 0x00);
	if (temp < 0) {
		return temp;
	}

	/* Enable ALE */
	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_ENABLE_REG, APDS9960_ENABLE_ALE, APDS9960_RGB_ON);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_ambient_setup__PROXY_IS_NOT_ENABLED, 0);
		return temp;
	}

	return 0;
}

static int apds9960_sensor_setup(struct device *dev, int gain)
{
	struct apds9960_data *data = dev->driver_data;
	int temp = 0;
	u8_t chip_id;

	temp = i2c_reg_read_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_ID_REG, &chip_id);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__FAILED_READING_CHIP_, 0);
		return temp;
	}

	if (!((chip_id == APDS9960_ID_1) || (chip_id == APDS9960_ID_2))) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__INVALID_CHIP_ID_0XX, 1, chip_id);
		return -EIO;
	}

	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_ENABLE_REG, APDS9960_ALL_BITS,
		APDS9960_MODE_OFF);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__ENABLE_REGISTERED_IS, 0);
		return temp;
	}

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_ATIME_REG, APDS9960_DEFAULT_ATIME);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__DEFAULT_INTEGRATION_, 0);
		return temp;
	}

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_WTIME_REG, APDS9960_DEFAULT_WTIME);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__DEFAULT_WAIT_TIME_NO, 0);
		return temp;
	}

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_PPULSE_REG, APDS9960_DEFAULT_PROX_PPULSE);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__DEFAULT_PROXIMITY_PP, 0);
		return temp;
	}

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_POFFSET_UR_REG, APDS9960_DEFAULT_POFFSET_UR);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__DEFAULT_POFFSET_UR_N, 0);
		return temp;
	}

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_POFFSET_DL_REG, APDS9960_DEFAULT_POFFSET_DL);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__DEFAULT_POFFSET_DL_N, 0);
		return temp;
	}

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_CONFIG1_REG, APDS9960_DEFAULT_CONFIG1);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__DEFAULT_CONFIG1_NOT_, 0);
		return temp;
	}

	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_CONTROL_REG, APDS9960_CONTROL_LDRIVE,
		APDS9960_DEFAULT_LDRIVE);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__LEDDRIVE_NOT_SET, 0);
		return -EIO;
	}

	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_CONTROL_REG, APDS9960_CONTROL_PGAIN,
		(gain & APDS9960_DEFAULT_PGAIN));
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__PROXY_GAIN_IS_NOT_SE, 0);
		return temp;
	}

	temp = i2c_reg_update_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_CONTROL_REG, APDS9960_CONTROL_AGAIN,
		(gain & APDS9960_DEFAULT_AGAIN));
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__AMBIENT_GAIN_IS_NOT_, 0);
		return temp;
	}

	temp = apds9960_setproxint_lowthresh(dev, APDS9960_DEFAULT_PILT);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__PROX_LOW_THRESHOLD_N, 0);
		return temp;
	}

	temp = apds9960_setproxint_highthresh(dev, APDS9960_DEFAULT_PIHT);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__PROX_HIGH_THRESHOLD_, 0);
		return temp;
	}

	temp = apds9960_setlightint_lowthresh(dev, APDS9960_DEFAULT_AILT);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__LIGHT_LOW_THRESHOLD_, 0);
		return temp;
	}

	temp = apds9960_setlightint_highthresh(dev, APDS9960_DEFAULT_AIHT);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__LIGHT_HIGH_THRESHOLD, 0);
		return temp;
	}

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_PERS_REG, APDS9960_DEFAULT_PERS);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__ALS_INTERRUPT_PERSIS, 0);
		return temp;
	}

	temp = i2c_reg_write_byte(data->i2c, APDS9960_I2C_ADDRESS,
		APDS9960_CONFIG2_REG, APDS9960_DEFAULT_CONFIG2);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__CLEAR_DIODE_SATURATI, 0);
		return temp;
	}

	temp = apds9960_proxy_setup(dev, gain);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__FAILED_TO_SETUP_PROX, 0);
		return temp;
	}

	temp = apds9960_ambient_setup(dev, gain);
	if (temp < 0) {
		ACT_LOG_ID_ERR(ALF_STR_apds9960_sensor_setup__FAILED_TO_SETUP_AMBI, 0);
		return temp;
	}

	return 0;
}

static const struct sensor_driver_api apds9960_driver_api = {
	.sample_fetch = &apds9960_sample_fetch,
	.channel_get = &apds9960_channel_get,
};

static int apds9960_init(struct device *dev)
{
	struct apds9960_data *data = dev->driver_data;
	int als_gain = 0;

	data->i2c = device_get_binding(CONFIG_APDS9960_I2C_DEV_NAME);

	if (data->i2c == NULL) {
		SYS_LOG_ERR("Failed to get pointer to %s device!",
		CONFIG_APDS9960_I2C_DEV_NAME);
		return -EINVAL;
	}

	data->sample_c = 0;
	data->sample_r = 0;
	data->sample_g = 0;
	data->sample_b = 0;
	data->pdata = 0;

	apds9960_sensor_setup(dev, als_gain);

	return 0;
}

static struct apds9960_data apds9960_data;

DEVICE_AND_API_INIT(apds9960, CONFIG_APDS9960_DRV_NAME, &apds9960_init,
		&apds9960_data, NULL, POST_KERNEL,
		CONFIG_SENSOR_INIT_PRIORITY, &apds9960_driver_api);

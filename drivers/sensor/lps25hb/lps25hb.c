/* lps25hb.c - Driver for LPS25HB pressure and temperature sensor */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sensor.h>
#include <kernel.h>
#include <device.h>
#include <init.h>
#include <misc/byteorder.h>
#include <misc/__assert.h>

#include "lps25hb.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_LPS25HB

static inline int lps25hb_power_ctrl(struct device *dev, u8_t value)
{
	struct lps25hb_data *data = dev->driver_data;
	const struct lps25hb_config *config = dev->config->config_info;

	return i2c_reg_update_byte(data->i2c_master, config->i2c_slave_addr,
				   LPS25HB_REG_CTRL_REG1,
				   LPS25HB_MASK_CTRL_REG1_PD,
				   value << LPS25HB_SHIFT_CTRL_REG1_PD);
}

static inline int lps25hb_set_odr_raw(struct device *dev, u8_t odr)
{
	struct lps25hb_data *data = dev->driver_data;
	const struct lps25hb_config *config = dev->config->config_info;

	return i2c_reg_update_byte(data->i2c_master, config->i2c_slave_addr,
				   LPS25HB_REG_CTRL_REG1,
				   LPS25HB_MASK_CTRL_REG1_ODR,
				   odr << LPS25HB_SHIFT_CTRL_REG1_ODR);
}

static int lps25hb_sample_fetch(struct device *dev,
				enum sensor_channel chan)
{
	struct lps25hb_data *data = dev->driver_data;
	const struct lps25hb_config *config = dev->config->config_info;
	u8_t out[5];
	int offset;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	for (offset = 0; offset < sizeof(out); ++offset) {
		if (i2c_reg_read_byte(data->i2c_master, config->i2c_slave_addr,
				      LPS25HB_REG_PRESS_OUT_XL + offset,
				      out + offset) < 0) {
			ACT_LOG_ID_DBG(ALF_STR_lps25hb_sample_fetch__FAILED_TO_READ_SAMPL, 0);
			return -EIO;
		}
	}

	data->sample_press = (s32_t)((u32_t)(out[0]) |
					((u32_t)(out[1]) << 8) |
					((u32_t)(out[2]) << 16));
	data->sample_temp = (s16_t)((u16_t)(out[3]) |
					((u16_t)(out[4]) << 8));

	return 0;
}

static inline void lps25hb_press_convert(struct sensor_value *val,
					 s32_t raw_val)
{
	/* val = raw_val / 40960 */
	val->val1 = raw_val / 40960;
	val->val2 = ((s32_t)raw_val * 1000000 / 40960) % 1000000;
}

static inline void lps25hb_temp_convert(struct sensor_value *val,
					s16_t raw_val)
{
	s32_t uval;

	/* val = raw_val / 480 + 42.5 */
	uval = (s32_t)raw_val * 1000000 / 480 + 42500000;
	val->val1 = (raw_val * 10 / 480 + 425) / 10;
	val->val2 = uval % 1000000;
}

static int lps25hb_channel_get(struct device *dev,
			       enum sensor_channel chan,
			       struct sensor_value *val)
{
	struct lps25hb_data *data = dev->driver_data;

	if (chan == SENSOR_CHAN_PRESS) {
		lps25hb_press_convert(val, data->sample_press);
	} else if (chan == SENSOR_CHAN_TEMP) {
		lps25hb_temp_convert(val, data->sample_temp);
	} else {
		return -ENOTSUP;
	}

	return 0;
}

static const struct sensor_driver_api lps25hb_api_funcs = {
	.sample_fetch = lps25hb_sample_fetch,
	.channel_get = lps25hb_channel_get,
};

static int lps25hb_init_chip(struct device *dev)
{
	struct lps25hb_data *data = dev->driver_data;
	const struct lps25hb_config *config = dev->config->config_info;
	u8_t chip_id;

	lps25hb_power_ctrl(dev, 0);
	k_busy_wait(50 * USEC_PER_MSEC);

	if (lps25hb_power_ctrl(dev, 1) < 0) {
		ACT_LOG_ID_DBG(ALF_STR_lps25hb_init_chip__FAILED_TO_POWER_ON_D, 0);
		return -EIO;
	}

	k_busy_wait(20 * USEC_PER_MSEC);

	if (i2c_reg_read_byte(data->i2c_master, config->i2c_slave_addr,
			      LPS25HB_REG_WHO_AM_I, &chip_id) < 0) {
		ACT_LOG_ID_DBG(ALF_STR_lps25hb_init_chip__FAILED_READING_CHIP_, 0);
		goto err_poweroff;
	}
	if (chip_id != LPS25HB_VAL_WHO_AM_I) {
		ACT_LOG_ID_DBG(ALF_STR_lps25hb_init_chip__INVALID_CHIP_ID_0XX, 1, chip_id);
		goto err_poweroff;
	}

	ACT_LOG_ID_DBG(ALF_STR_lps25hb_init_chip__CHIP_ID_0XX, 1, chip_id);

	if (lps25hb_set_odr_raw(dev, LPS25HB_DEFAULT_SAMPLING_RATE)
				< 0) {
		ACT_LOG_ID_DBG(ALF_STR_lps25hb_init_chip__FAILED_TO_SET_SAMPLI, 0);
		goto err_poweroff;
	}

	if (i2c_reg_update_byte(data->i2c_master, config->i2c_slave_addr,
				LPS25HB_REG_CTRL_REG1,
				LPS25HB_MASK_CTRL_REG1_BDU,
				(1 << LPS25HB_SHIFT_CTRL_REG1_BDU)) < 0) {
		ACT_LOG_ID_DBG(ALF_STR_lps25hb_init_chip__FAILED_TO_SET_BDU, 0);
		goto err_poweroff;
	}

	return 0;

err_poweroff:
	lps25hb_power_ctrl(dev, 0);
	return -EIO;
}

static int lps25hb_init(struct device *dev)
{
	const struct lps25hb_config * const config = dev->config->config_info;
	struct lps25hb_data *data = dev->driver_data;

	data->i2c_master = device_get_binding(config->i2c_master_dev_name);
	if (!data->i2c_master) {
		SYS_LOG_DBG("i2c master not found: %s",
			    config->i2c_master_dev_name);
		return -EINVAL;
	}

	if (lps25hb_init_chip(dev) < 0) {
		ACT_LOG_ID_DBG(ALF_STR_lps25hb_init__FAILED_TO_INITIALIZE, 0);
		return -EIO;
	}

	dev->driver_api = &lps25hb_api_funcs;

	return 0;
}

static const struct lps25hb_config lps25hb_config = {
	.i2c_master_dev_name = CONFIG_LPS25HB_I2C_MASTER_DEV_NAME,
	.i2c_slave_addr = CONFIG_LPS25HB_I2C_ADDR,
};

static struct lps25hb_data lps25hb_data;

DEVICE_INIT(lps25hb, CONFIG_LPS25HB_DEV_NAME, lps25hb_init,
	    &lps25hb_data, &lps25hb_config, POST_KERNEL,
	    CONFIG_SENSOR_INIT_PRIORITY);

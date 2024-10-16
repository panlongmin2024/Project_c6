/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lis2dh.h"

#include <misc/util.h>
#include <kernel.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_LIS2DH_TRIGGER

#define START_TRIG_INT1			BIT(0)
#define START_TRIG_INT2			BIT(1)
#define TRIGGED_INT1			BIT(4)
#define TRIGGED_INT2			BIT(5)

static int lis2dh_trigger_drdy_set(struct device *dev, enum sensor_channel chan,
				   sensor_trigger_handler_t handler)
{
	struct lis2dh_data *lis2dh = dev->driver_data;
	int status;

	gpio_pin_disable_callback(lis2dh->gpio, CONFIG_LIS2DH_INT1_GPIO_PIN);

	/* cancel potentially pending trigger */
	atomic_clear_bit(&lis2dh->trig_flags, TRIGGED_INT1);

	status = lis2dh_reg_field_update(lis2dh->bus, LIS2DH_REG_CTRL3,
					 LIS2DH_EN_DRDY1_INT1_SHIFT,
					 LIS2DH_EN_DRDY1_INT1, 0);

	lis2dh->handler_drdy = handler;
	if ((handler == NULL) || (status < 0)) {
		return status;
	}

	lis2dh->chan_drdy = chan;

	/* serialize start of int1 in thread to synchronize output sampling
	 * and first interrupt. this avoids concurrent bus context access.
	 */
	atomic_set_bit(&lis2dh->trig_flags, START_TRIG_INT1);
#if defined(CONFIG_LIS2DH_TRIGGER_OWN_THREAD)
	k_sem_give(&lis2dh->gpio_sem);
#elif defined(CONFIG_LIS2DH_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&lis2dh->work);
#endif

	return 0;
}

static int lis2dh_start_trigger_int1(const struct lis2dh_data *lis2dh)
{
	int status;
	u8_t raw[LIS2DH_BUF_SZ];
	u8_t ctrl1 = 0;

	/* power down temporarly to align interrupt & data output sampling */
	status = lis2dh_reg_read_byte(lis2dh->bus, LIS2DH_REG_CTRL1, &ctrl1);
	if (unlikely(status < 0)) {
		return status;
	}
	status = lis2dh_reg_write_byte(lis2dh->bus, LIS2DH_REG_CTRL1,
				       ctrl1 & ~LIS2DH_ODR_MASK);
	if (unlikely(status < 0)) {
		return status;
	}

	ACT_LOG_ID_DBG(ALF_STR_lis2dh_start_trigger_int1__CTRL10XX_TICKU, 2, ctrl1, k_cycle_get_32());

	/* empty output data */
	status = lis2dh_burst_read(lis2dh->bus, LIS2DH_REG_STATUS, raw,
				   sizeof(raw));
	if (unlikely(status < 0)) {
		return status;
	}

	gpio_pin_enable_callback(lis2dh->gpio, CONFIG_LIS2DH_INT1_GPIO_PIN);

	/* re-enable output sampling */
	status = lis2dh_reg_write_byte(lis2dh->bus, LIS2DH_REG_CTRL1, ctrl1);
	if (unlikely(status < 0)) {
		return status;
	}

	return lis2dh_reg_field_update(lis2dh->bus, LIS2DH_REG_CTRL3,
				       LIS2DH_EN_DRDY1_INT1_SHIFT,
				       LIS2DH_EN_DRDY1_INT1, 1);
}

#define LIS2DH_ANYM_CFG (LIS2DH_INT_CFG_ZHIE_ZUPE | LIS2DH_INT_CFG_YHIE_YUPE |\
			 LIS2DH_INT_CFG_XHIE_XUPE)

static int lis2dh_trigger_anym_set(struct device *dev,
				   sensor_trigger_handler_t handler)
{
	struct lis2dh_data *lis2dh = dev->driver_data;
	int status;
	u8_t reg_val;

	gpio_pin_disable_callback(lis2dh->gpio, CONFIG_LIS2DH_INT2_GPIO_PIN);

	/* cancel potentially pending trigger */
	atomic_clear_bit(&lis2dh->trig_flags, TRIGGED_INT2);

	/* disable all interrupt 2 events */
	status = lis2dh_reg_write_byte(lis2dh->bus, LIS2DH_REG_INT2_CFG, 0);

	/* make sure any pending interrupt is cleared */
	status = lis2dh_reg_read_byte(lis2dh->bus, LIS2DH_REG_INT2_SRC,
				      &reg_val);

	lis2dh->handler_anymotion = handler;
	if ((handler == NULL) || (status < 0)) {
		return status;
	}

	/* serialize start of int2 in thread to synchronize output sampling
	 * and first interrupt. this avoids concurrent bus context access.
	 */
	atomic_set_bit(&lis2dh->trig_flags, START_TRIG_INT2);
#if defined(CONFIG_LIS2DH_TRIGGER_OWN_THREAD)
	k_sem_give(&lis2dh->gpio_sem);
#elif defined(CONFIG_LIS2DH_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&lis2dh->work);
#endif
	return 0;
}

static int lis2dh_start_trigger_int2(const struct lis2dh_data *lis2dh)
{
	int status;

	status = gpio_pin_enable_callback(lis2dh->gpio,
					  CONFIG_LIS2DH_INT2_GPIO_PIN);
	if (unlikely(status < 0)) {
		ACT_LOG_ID_ERR(ALF_STR_lis2dh_start_trigger_int2__ENABLE_CALLBACK_FAIL, 1, status);
	}

	return lis2dh_reg_write_byte(lis2dh->bus, LIS2DH_REG_INT2_CFG,
				     LIS2DH_ANYM_CFG);
}

int lis2dh_trigger_set(struct device *dev,
		       const struct sensor_trigger *trig,
		       sensor_trigger_handler_t handler)
{
	if (trig->type == SENSOR_TRIG_DATA_READY &&
	    trig->chan == SENSOR_CHAN_ACCEL_XYZ) {
		return lis2dh_trigger_drdy_set(dev, trig->chan, handler);
	} else if (trig->type == SENSOR_TRIG_DELTA) {
		return lis2dh_trigger_anym_set(dev, handler);
	}

	return -ENOTSUP;
}

int lis2dh_acc_slope_config(struct device *dev, enum sensor_attribute attr,
			    const struct sensor_value *val)
{
	struct lis2dh_data *lis2dh = dev->driver_data;
	int status;

	if (attr == SENSOR_ATTR_SLOPE_TH) {
		u8_t range_g, reg_val;
		u32_t slope_th_ums2;

		status = lis2dh_reg_read_byte(lis2dh->bus, LIS2DH_REG_CTRL4,
					      &reg_val);
		if (status < 0) {
			return status;
		}

		/* fs reg value is in the range 0 (2g) - 3 (16g) */
		range_g = 2 * (1 << ((LIS2DH_FS_MASK & reg_val)
				      >> LIS2DH_FS_SHIFT));

		slope_th_ums2 = val->val1 * 1000000 + val->val2;

		/* make sure the provided threshold does not exceed range */
		if ((slope_th_ums2 - 1) > (range_g * SENSOR_G)) {
			return -EINVAL;
		}

		/* 7 bit full range value */
		reg_val = 128 / range_g * (slope_th_ums2 - 1) / SENSOR_G;

		SYS_LOG_INF("int2_ths=0x%x range_g=%d ums2=%u", reg_val,
			    range_g, slope_th_ums2 - 1);

		status = lis2dh_reg_write_byte(lis2dh->bus, LIS2DH_REG_INT2_THS,
					       reg_val);
	} else { /* SENSOR_ATTR_SLOPE_DUR */
		/*
		 * slope duration is measured in number of samples:
		 * N/ODR where N is the register value
		 */
		if (val->val1 < 0 || val->val1 > 127) {
			return -ENOTSUP;
		}

		ACT_LOG_ID_INF(ALF_STR_lis2dh_acc_slope_config__INT2_DUR0XX, 1, val->val1);

		status = lis2dh_reg_write_byte(lis2dh->bus, LIS2DH_REG_INT2_DUR,
					       val->val1);
	}

	return status;
}

static void lis2dh_gpio_int1_callback(struct device *dev,
				      struct gpio_callback *cb, u32_t pins)
{
	struct lis2dh_data *lis2dh =
		CONTAINER_OF(cb, struct lis2dh_data, gpio_int1_cb);

	ARG_UNUSED(pins);

	atomic_set_bit(&lis2dh->trig_flags, TRIGGED_INT1);

#if defined(CONFIG_LIS2DH_TRIGGER_OWN_THREAD)
	k_sem_give(&lis2dh->gpio_sem);
#elif defined(CONFIG_LIS2DH_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&lis2dh->work);
#endif
}

static void lis2dh_gpio_int2_callback(struct device *dev,
				      struct gpio_callback *cb, u32_t pins)
{
	struct lis2dh_data *lis2dh =
		CONTAINER_OF(cb, struct lis2dh_data, gpio_int2_cb);

	ARG_UNUSED(pins);

	atomic_set_bit(&lis2dh->trig_flags, TRIGGED_INT2);

#if defined(CONFIG_LIS2DH_TRIGGER_OWN_THREAD)
	k_sem_give(&lis2dh->gpio_sem);
#elif defined(CONFIG_LIS2DH_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&lis2dh->work);
#endif
}

static void lis2dh_thread_cb(void *arg)
{
	struct device *dev = arg;
	struct lis2dh_data *lis2dh = dev->driver_data;

	if (unlikely(atomic_test_and_clear_bit(&lis2dh->trig_flags,
		     START_TRIG_INT1))) {
		int status = lis2dh_start_trigger_int1(lis2dh);

		if (unlikely(status < 0)) {
			ACT_LOG_ID_ERR(ALF_STR_lis2dh_thread_cb__LIS2DH_START_TRIGGER, 1, status);
		}
		return;
	}

	if (unlikely(atomic_test_and_clear_bit(&lis2dh->trig_flags,
		     START_TRIG_INT2))) {
		int status = lis2dh_start_trigger_int2(lis2dh);

		if (unlikely(status < 0)) {
			ACT_LOG_ID_ERR(ALF_STR_lis2dh_thread_cb__LIS2DH_START_TRIGGER, 1, status);
		}
		return;
	}

	if (atomic_test_and_clear_bit(&lis2dh->trig_flags,
				      TRIGGED_INT1)) {
		struct sensor_trigger drdy_trigger = {
			.type = SENSOR_TRIG_DATA_READY,
			.chan = lis2dh->chan_drdy,
		};

		if (likely(lis2dh->handler_drdy != NULL)) {
			lis2dh->handler_drdy(dev, &drdy_trigger);
		}

		return;
	}

	if (atomic_test_and_clear_bit(&lis2dh->trig_flags,
				      TRIGGED_INT2)) {
		struct sensor_trigger anym_trigger = {
			.type = SENSOR_TRIG_DELTA,
			.chan = lis2dh->chan_drdy,
		};
		u8_t reg_val;

		/* clear interrupt 2 to de-assert int2 line */
		lis2dh_reg_read_byte(lis2dh->bus, LIS2DH_REG_INT2_SRC,
				     &reg_val);

		if (likely(lis2dh->handler_anymotion != NULL)) {
			lis2dh->handler_anymotion(dev, &anym_trigger);
		}

		SYS_LOG_DBG("@tick=%u int2_src=0x%x", k_cycle_get_32(),
			    reg_val);

		return;
	}
}

#ifdef CONFIG_LIS2DH_TRIGGER_OWN_THREAD
static void lis2dh_thread(void *arg1, void *unused2, void *unused3)
{
	struct device *dev = arg1;
	struct lis2dh_data *lis2dh = dev->driver_data;

	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);

	while (1) {
		k_sem_take(&lis2dh->gpio_sem, K_FOREVER);
		lis2dh_thread_cb(dev);
	}
}
#endif

#ifdef CONFIG_LIS2DH_TRIGGER_GLOBAL_THREAD
static void lis2dh_work_cb(struct k_work *work)
{
	struct lis2dh_data *lis2dh =
		CONTAINER_OF(work, struct lis2dh_data, work);

	lis2dh_thread_cb(lis2dh->dev);
}
#endif

#define LIS2DH_INT1_CFG			(GPIO_DIR_IN | GPIO_INT |\
					 GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH)

#define LIS2DH_INT2_CFG			(GPIO_DIR_IN | GPIO_INT |\
					 GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH)

int lis2dh_init_interrupt(struct device *dev)
{
	struct lis2dh_data *lis2dh = dev->driver_data;
	int status;
	u8_t raw[LIS2DH_DATA_OFS + 2];

	/* setup data ready gpio interrupt */
	lis2dh->gpio = device_get_binding(CONFIG_LIS2DH_GPIO_DEV_NAME);
	if (lis2dh->gpio == NULL) {
		SYS_LOG_ERR("Cannot get pointer to %s device",
			    CONFIG_LIS2DH_GPIO_DEV_NAME);
		return -EINVAL;
	}

	/* data ready int1 gpio configuration */
	status = gpio_pin_configure(lis2dh->gpio, CONFIG_LIS2DH_INT1_GPIO_PIN,
				    LIS2DH_INT1_CFG);
	if (status < 0) {
		SYS_LOG_ERR("Could not configure gpio %d",
			     CONFIG_LIS2DH_INT1_GPIO_PIN);
		return status;
	}

	gpio_init_callback(&lis2dh->gpio_int1_cb,
			   lis2dh_gpio_int1_callback,
			   BIT(CONFIG_LIS2DH_INT1_GPIO_PIN));

	status = gpio_add_callback(lis2dh->gpio, &lis2dh->gpio_int1_cb);
	if (status < 0) {
		ACT_LOG_ID_ERR(ALF_STR_lis2dh_init_interrupt__COULD_NOT_ADD_GPIO_I, 0);
		return status;
	}

	/* any motion int2 gpio configuration */
	status = gpio_pin_configure(lis2dh->gpio, CONFIG_LIS2DH_INT2_GPIO_PIN,
				    LIS2DH_INT2_CFG);
	if (status < 0) {
		SYS_LOG_ERR("Could not configure gpio %d",
			     CONFIG_LIS2DH_INT2_GPIO_PIN);
		return status;
	}

	gpio_init_callback(&lis2dh->gpio_int2_cb,
			   lis2dh_gpio_int2_callback,
			   BIT(CONFIG_LIS2DH_INT2_GPIO_PIN));

	/* callback is going to be enabled by trigger setting function */
	status = gpio_add_callback(lis2dh->gpio, &lis2dh->gpio_int2_cb);
	if (status < 0) {
		ACT_LOG_ID_ERR(ALF_STR_lis2dh_init_interrupt__COULD_NOT_ADD_GPIO_I, 1, status);
		return status;
	}

#if defined(CONFIG_LIS2DH_TRIGGER_OWN_THREAD)
	k_sem_init(&lis2dh->gpio_sem, 0, UINT_MAX);

	k_thread_create(&lis2dh->thread, lis2dh->thread_stack,
			CONFIG_LIS2DH_THREAD_STACK_SIZE,
			(k_thread_entry_t)lis2dh_thread, dev, NULL, NULL,
			K_PRIO_COOP(CONFIG_LIS2DH_THREAD_PRIORITY), 0, 0);
#elif defined(CONFIG_LIS2DH_TRIGGER_GLOBAL_THREAD)
	lis2dh->work.handler = lis2dh_work_cb;
	lis2dh->dev = dev;
#endif
	/* disable interrupt 2 in case of warm (re)boot */
	status = lis2dh_reg_write_byte(lis2dh->bus, LIS2DH_REG_INT2_CFG, 0);
	if (status < 0) {
		SYS_LOG_ERR("Interrupt 2 disable reg write failed (%d)",
			    status);
		return status;
	}

	memset(raw, 0, sizeof(raw));
	status = lis2dh_burst_write(lis2dh->bus, LIS2DH_REG_INT2_THS, raw,
				    sizeof(raw));
	if (status < 0) {
		ACT_LOG_ID_ERR(ALF_STR_lis2dh_init_interrupt__BURST_WRITE_TO_INT2_, 1, status);
		return status;
	}

	/* latch int2 line interrupt */
	status = lis2dh_reg_write_byte(lis2dh->bus, LIS2DH_REG_CTRL5,
				       LIS2DH_EN_LIR_INT2);
	if (status < 0) {
		ACT_LOG_ID_ERR(ALF_STR_lis2dh_init_interrupt__INT2_LATCH_ENABLE_RE, 1, status);
		return status;
	}

	SYS_LOG_INF("int1 on pin=%d cfg=0x%x, int2 on pin=%d cfg=0x%x",
		    CONFIG_LIS2DH_INT1_GPIO_PIN, LIS2DH_INT1_CFG,
		    CONFIG_LIS2DH_INT2_GPIO_PIN, LIS2DH_INT2_CFG);

	/* enable interrupt 2 on int2 line */
	return lis2dh_reg_write_byte(lis2dh->bus, LIS2DH_REG_CTRL6,
				     LIS2DH_EN_INT2_INT2);
}

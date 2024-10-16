/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for FLASH drivers
 */

#ifndef __FLASH_H__
#define __FLASH_H__

/**
 * @brief FLASH Interface
 * @defgroup flash_interface FLASH Interface
 * @ingroup io_interfaces
 * @{
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/types.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	FLASH_PROTECT_REGION_NONE,
	FLASH_PROTECT_REGION_BOOT,
	FLASH_PROTECT_REGION_BOOT_AND_SYS,
	FLASH_PROTECT_REGION_ALL_EXCLUDE_NVRAM,
}flash_protect_region_id_e;

typedef int (*flash_api_init)(struct device *dev);
typedef int (*flash_api_read)(struct device *dev, off_t offset, void *data,
			      size_t len);
typedef int (*flash_api_write)(struct device *dev, off_t offset,
			       const void *data, size_t len);
typedef int  (*flash_api_ioctl)(struct device *dev, uint8_t cmd, void *buff);
typedef int (*flash_api_erase)(struct device *dev, off_t offset, size_t size);
typedef int (*flash_api_write_protection)(struct device *dev, bool enable);
typedef int (*flash_api_write_protection_region)(struct device *dev, int region_id);

struct flash_driver_api {
	flash_api_init init;
	flash_api_read read;
	flash_api_write write;
	flash_api_ioctl ioctl;
	flash_api_erase erase;
	flash_api_write_protection write_protection;
	flash_api_write_protection_region write_protection_region;
};

/**
 *  @brief  init flash
 *
 *  @param  dev             : flash dev
 *
 *  @return  0 on success, negative errno code on fail.
 */
static inline int flash_init(struct device *dev)
{
	const struct flash_driver_api *api = dev->driver_api;

	return api->init(dev);
}

/**
 *  @brief  Read data from flash
 *
 *  @param  dev             : flash dev
 *  @param  offset          : Offset (byte aligned) to read
 *  @param  data            : Buffer to store read data
 *  @param  len             : Number of bytes to read.
 *
 *  @return  0 on success, negative errno code on fail.
 */
static inline int flash_read(struct device *dev, off_t offset, void *data,
			     size_t len)
{
	const struct flash_driver_api *api = dev->driver_api;

	return api->read(dev, offset, data, len);
}

/**
 *  @brief  Write buffer into flash memory.
 *
 *  Prior to the invocation of this API, the flash_write_protection_set needs
 *  to be called first to disable the write protection.
 *
 *  @param  dev             : flash device
 *  @param  offset          : starting offset for the write
 *  @param  data            : data to write
 *  @param  len             : Number of bytes to write
 *
 *  @return  0 on success, negative errno code on fail.
 */
static inline int flash_write(struct device *dev, off_t offset,
			      const void *data, size_t len)
{
	const struct flash_driver_api *api = dev->driver_api;

	return api->write(dev, offset, data, len);
}

/**
 *  @brief flash ioctl cmd
 *
 *  @param  dev             : flash dev
 *  @param  cmd            : command
 *  @param  buff            : ioctl command operation buffer
 *
 *  @return  0 on success, negative errno code on fail.
 */
static inline int flash_ioctl(struct device *dev, uint8_t cmd, void *buff)
{
	const struct flash_driver_api *api = dev->driver_api;

	return api->ioctl(dev, cmd, buff);
}

/**
 *  @brief  Erase part or all of a flash memory
 *
 *  Acceptable values of erase size and offset are subject to
 *  hardware-specific multiples of sector size and offset. Please check the
 *  API implemented by the underlying sub driver.
 *
 *  Prior to the invocation of this API, the flash_write_protection_set needs
 *  to be called first to disable the write protection.
 *
 *  @param  dev             : flash device
 *  @param  offset          : erase area starting offset
 *  @param  size            : size of area to be erased
 *
 *  @return  0 on success, negative errno code on fail.
 */
static inline int flash_erase(struct device *dev, off_t offset, size_t size)
{
	const struct flash_driver_api *api = dev->driver_api;

	return api->erase(dev, offset, size);
}

/**
 *  @brief  Enable or disable write protection for a flash memory
 *
 *  This API is required to be called before the invocation of write or erase
 *  API. Please note that on some flash components, the write protection is
 *  automatically turned on again by the device after the completion of each
 *  write or erase calls. Therefore, on those flash parts, write protection needs
 *  to be disabled before each invocation of the write or erase API. Please refer
 *  to the sub-driver API or the data sheet of the flash component to get details
 *  on the write protection behavior.
 *
 *  @param  dev             : flash device
 *  @param  enable          : enable or disable flash write protection
 *
 *  @return  0 on success, negative errno code on fail.
 */
static inline int flash_write_protection_set(struct device *dev, bool enable)
{
	const struct flash_driver_api *api = dev->driver_api;

	return api->write_protection(dev, enable);
}



/**
 *	@brief	Set write protection region for a flash memory
 *
 *	This API is required to be called before the invocation of write or erase
 *	API. Please note that on some flash components, the write protection is
 *	automatically turned on again by the device after the completion of each
 *	write or erase calls. Therefore, on those flash parts, write protection needs
 *	to be disabled before each invocation of the write or erase API. Please refer
 *	to the sub-driver API or the data sheet of the flash component to get details
 *	on the write protection behavior.
 *
 *	@param	dev 			: flash device
 *	@param	region_id		: region id for flash write protection
 *
 *	@return  0 on success, negative errno code on fail.
 */
static inline int flash_write_protection_region_set(struct device *dev, int region_id)
{
	const struct flash_driver_api *api = dev->driver_api;

	return api->write_protection_region(dev, region_id);
}


#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _FLASH_H_ */

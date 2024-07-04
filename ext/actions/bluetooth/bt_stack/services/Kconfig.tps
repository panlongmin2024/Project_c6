# Bluetooth GATT TX Power Service

# Copyright (c) 2020 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

menuconfig BT_TPS
	bool "Enable GATT TX Power service"

if BT_TPS

source "subsys/logging/Kconfig.template.log_config"

endif # BT_TPS

# Copyright (c) 2020 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
subdir-ccflags-y += -I${ZEPHYR_BASE}/samples/bt_speaker/src/charge_6/include
subdir-ccflags-y += -I${ZEPHYR_BASE}/samples/bt_speaker/src/include/
#subdir-ccflags-y += -I${ZEPHYR_BASE}/ext/lib/crypto/mbedtls/include/
subdir-ccflags-$(CONFIG_BATTERY_SECURITY) += -I${ZEPHYR_BASE}/ext/lib/crypto/mbedtls/include/
#subdir-ccflags-y += -I${ZEPHYR_BASE}/samples/bt_speaker/src/charge_6/include/driver/

MEDIA_LIB_OUT_DIR=$(ZEPHYR_BASE)/ext/actions/media
MEDIA_LIB_OBJ_DIR=$(O)/ext/actions/media
MEDIA_LIB=

obj-$(CONFIG_MEDIA) += media_effect_param.o
obj-$(CONFIG_MEDIA) += media_mem.o
obj-$(CONFIG_MEDIA) += media_player.o
obj-$(CONFIG_MEDIA) += codec_config.o
obj-$(CONFIG_MEDIA) += ex_mic_stream.o

ifdef CONFIG_MEDIA_SERVICE
ifneq ($(wildcard $(srctree)/ext/actions/media/libmedia/Makefile),)
obj-y += libmedia/
MEDIA_LIB += libmedia
else
LINK_SYS_LIBS += $(MEDIA_LIB_OUT_DIR)/libmedia.a
endif
endif

ifneq ($(wildcard $(srctree)/ext/actions/media/libwltmcu/Makefile),)
obj-y += libwltmcu/
MEDIA_LIB += libwltmcu
else
LINK_SYS_LIBS += $(MEDIA_LIB_OUT_DIR)/libwltmcu.a
endif
obj-y += charge6_ic_driver/
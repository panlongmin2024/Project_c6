ifeq ($(CONFIG_WIFI_RTL8189FTV),y)
obj-$(CONFIG_WIFI_RTL8189FTV) += app/
obj-$(CONFIG_WIFI_RTL8189FTV) += netif/
obj-$(CONFIG_WIFI_RTL8189FTV) += osdep/
obj-$(CONFIG_WIFI_RTL8189FTV) += sdio/
#obj-$(CONFIG_WIFI_RTL8189FTV) += wlan/

subdir-ccflags-y += -I$(srctree)/drivers/wifi/rtl8189ftv/app
subdir-ccflags-y += -I$(srctree)/drivers/wifi/rtl8189ftv/netif
endif
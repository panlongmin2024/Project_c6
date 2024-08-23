subdir-ccflags-y += -I${ZEPHYR_BASE}/samples/bt_speaker/src/include
subdir-ccflags-y += -I${ZEPHYR_BASE}/include/drivers

ifdef CONFIG_OTA_RECOVERY_APP
obj-y += ota_recovery/
else
subdir-ccflags-y += -I${ZEPHYR_BASE}/samples/bt_speaker/src/tool/
subdir-ccflags-$(CONFIG_OTA_APP) += -I${ZEPHYR_BASE}/samples/bt_speaker/src/ota/

obj-y += main.o
obj-y += common/
obj-y += main/
obj-y += btmusic/
obj-y += lemusic/
obj-$(CONFIG_AURACAST) += bmr/
obj-$(CONFIG_TOOL) += tool/
obj-$(CONFIG_OTA_APP) += ota/
obj-$(CONFIG_USOUND_APP) += usound/
obj-$(CONFIG_DEMO_APP) += demo/
obj-$(CONFIG_AUDIO_INPUT_APP) += audio_input/
obj-$(CONFIG_CHARGER_APP) += charger/
obj-$(CONFIG_BT_SELF_APP) += selfapp/
obj-$(CONFIG_DATA_ANALY) += data_analy/
obj-y += desktop/
obj-y += system_app/
obj-$(CONFIG_LCMUSIC_APP) += lcmusic/

obj-$(CONFIG_BUILD_PROJECT_HM_DEMAND_CODE) += charge_6/

endif

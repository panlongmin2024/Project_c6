subdir-ccflags-$(CONFIG_BUILD_PROJECT_HM_DEMAND_CODE) += -I$(ZEPHYR_BASE)/samples/bt_speaker/src/charge_6/include/
subdir-ccflags-y += -I${ZEPHYR_BASE}/samples/bt_speaker/src/charge_6/include

obj-y += app_switcher.o
obj-$(CONFIG_ESD_MANAGER) += esd_manager.o
obj-$(CONFIG_SYSTEM_SHELL) += sys_shell.o
obj-y += sys_monitor.o
obj-y += sys_event.o
obj-y += sys_manager.o
obj-y += sys_power_off.o
obj-y += sys_wakelock.o
obj-$(CONFIG_SYS_STANDBY) += sys_standby.o
obj-y += system_init.o
obj-$(CONFIG_SYS_SOC_UUID) += sys_soc_uuid.o

obj-$(CONFIG_MUTIPLE_VOLUME_MANAGER) += fs_manager/
obj-$(CONFIG_HOTPLUG) += hotplug/
obj-$(CONFIG_INPUT) += input/
obj-$(CONFIG_FM) += fm/
obj-$(CONFIG_PLAYTTS) += tts/
obj-$(CONFIG_POWER) += power/
obj-$(CONFIG_ACT_EVENT) += act_event/ 
obj-$(CONFIG_SERIAL_FLASHER) += serial_flasher/

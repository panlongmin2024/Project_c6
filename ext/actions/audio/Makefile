subdir-ccflags-y += -I$(srctree)/drivers/audio/andes
subdir-ccflags-y += -I$(ZEPHYR_BASE)/samples/bt_speaker/src/charge_6/include/
subdir-ccflags-y += -I$(ZEPHYR_BASE)/samples/bt_speaker/src/charge_6/include/driver/

obj-$(CONFIG_VOLUME_MANAGER) += volume_manager.o
# obj-$(CONFIG_TWS) += audio_tws_aps.o
obj-$(CONFIG_TWS) += audio_tws_aps_snoop.o
obj-$(CONFIG_TWS) += libaudio/
obj-y +=  audio_aps.o
obj-y +=  audio_policy.o
obj-y +=  audio_system.o
obj-y +=  audio_record.o
obj-y +=  audio_track.o
obj-y +=  audio_shell.o
obj-y +=  pcm_data.o
#obj-y += audio_stream.o
obj-$(CONFIG_AUDIO_MULTICORE_SYNC_PLAY) += audio_multicore_sync.o


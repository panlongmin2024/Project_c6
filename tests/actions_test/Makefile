CUR_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

#OBJ_C := $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/sample/*.c))
OBJ_C := $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/hw_test/zs282a/mmc/*.c))

ifdef CONFIG_VDMA_ACTS_ANDES
OBJ_C += $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/hw_test/zs282a/dma/*.c))
endif

OBJ_C += $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/hw_test/zs282a/uart_console/*.c))
OBJ_C += $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/hw_test/zs282a/dsp/*.c))
OBJ_C += $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/hw_test/zs282a/crash/*.c))
#OBJ_C += $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/memory/*.c))

SRCS := $(basename $(foreach dir,$(CUR_DIR),$(wildcard $(dir)/*.[csS])))

OBJS := $(addsuffix .o,$(SRCS))

ifdef CONFIG_ACTIONS_TEST
obj-y := $(OBJ_C:.c=.o) $(OBJ_S:.S=.o) 
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/utils/unit_test/include
endif

obj-y += player/tts_player/ 
# obj-y += player/tts_test/ 
obj-y += player/tts_mix/
#obj-y += usb_actions/test_fat_fs_api/
#obj-$(CONFIG_ACTIONS_TEST) += usb_actions/test_usbstub/
#obj-y += usb_actions/hid/
#obj-y += usb_actions/test_mass_storage/
#obj-$(CONFIG_USB_AUDIO_SOURCE) += usb_actions/test_usb_usound/
#obj-y += usb_actions/test_cdc_acm/

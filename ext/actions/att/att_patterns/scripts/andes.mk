ROOT ?= ${CURDIR}

# Build verbosity
V			:= 0
# Debug build
DEBUG			:= 0
# Build platform
PLAT			:= ATS283X


CROSS_COMPILE ?= /opt/csky-elfabiv2/bin/csky-abiv2-elf-

CC	:= ${CROSS_COMPILE}gcc
CPP	:= ${CROSS_COMPILE}cpp
AS	:= ${CROSS_COMPILE}gcc
AR	:= ${CROSS_COMPILE}ar
LD	:= ${CROSS_COMPILE}ld
OC	:= ${CROSS_COMPILE}objcopy
OD	:= ${CROSS_COMPILE}objdump
NM	:= ${CROSS_COMPILE}nm
PP	:= ${CROSS_COMPILE}gcc -E ${CFLAGS}

READELF := $(CROSS_COMPILE)readelf
STRIP   := $(CROSS_COMPILE)strip
OBJSIZE := $(CROSS_COMPILE)size


INCLUDES	+=


# 编译选项
# 编译选项修改说明 -g改成-g3,g3按照编译文档说明表示产生尽可能多的调试信息,暂时还使用-G0
# mlittle-endian表示使用小端模式
# mno-multiple-stld表示是否使用stm指令，默认是不使用的
# fomit-frame-pointer表示不需要frame pointer register时编译器不需要保留栈帧类指令
CC_OPTS_BASE = \
	-c \
	-g \
	-EL \
	-MD \
	-Wall \
	-mno-multiple-stld \
	-fno-builtin \
	-fomit-frame-pointer \
	-frename-registers \
	-Werror \
	-msoft-float \
	-fsigned-char \
	-fshort-enums \
	-mcpu=ck802 \
	-nostdinc \
	-nostdlib \
	-fno-builtin \
	-fno-common \
	-gdwarf-2 -gstrict-dwarf


ASFLAGS			+= 	$(CC_OPTS_BASE) -D_ASSEMBLER_  \
				${DEFINES} ${INCLUDES} -DDEBUG=${DEBUG} -g
CFLAGS			+= 	$(CC_OPTS_BASE)   \
				-Os  \
				${DEFINES} ${INCLUDES} -DDEBUG=${DEBUG} -g

CFLAGS			+=	-ffunction-sections -fdata-sections


# 增加工具链库搜索路径
TOOLCHAIN_LIB_DIR := $(shell dirname `$(CC) $(CFLAGS) -print-libgcc-file-name`)
LIB_DIR += $(TOOLCHAIN_LIB_DIR)
# 生成库文件搜索路径
LIB_INCLUDE := $(foreach n, $(LIB_DIR), -L$(n))

LDFLAGS 		:= -G0 \
				-EL \
				-gc-section  $(LIB_INCLUDE)



# Checkpatch rules
CKPATCH := ${ROOT}/scripts/codestyle/checkpatch.pl
CKFLAGS := --mailback --no-tree -f --emacs --summary-file --show-types
CKFLAGS += --ignore BRACES,PRINTK_WITHOUT_KERN_LEVEL,SPLIT_STRING,CONST_STRUCT
CKFLAGS += --ignore NEW_TYPEDEFS,VOLATILE,DATE_TIME,ARRAY_SIZE,PREFER_KERNEL_TYPES,PREFER_PACKED
CKFLAGS += --max-line-length=100

#-pedantic

ifeq (${V},0)
	Q=@
else
	Q=
endif

# Default build string (git branch and commit)
ifeq (${BUILD_STRING},)
	BUILD_STRING	:=	$(shell git log -n 1 --pretty=format:"%h")
endif

all: msg_start

msg_start:
	@echo "Building ${PLAT}"

clean:
	@echo "  CLEAN"
	@-rm -f *.o *.map *.lst *.elf *.bin *.ld *.a *.d $(OBJS) $(PREREQUISITES)

define MAKE_C

$(eval OBJ := $(1)/$(patsubst %.c,%.o,$(2)))
$(eval PREREQUISITES := $(patsubst %.o,%.d,$(OBJ)))
$(eval CHK_C := $(1)/$(patsubst %.c,%.c.chk,$(2)))

$(OBJ) : $(2)
	@echo "  CC      $$<"
	$$(Q)$$(CC) $$(CFLAGS) -c $$< -o $$@

$(PREREQUISITES) : $(2)
	@echo "  DEPS    $$@"
	@mkdir -p $(1)
	$$(Q)$$(CC) $$(CFLAGS) -M -MT $(OBJ) -MF $$@ $$<

ifdef IS_ANYTHING_TO_BUILD
-include $(PREREQUISITES)
endif

$(CHK_C) : $(2)
	@echo "  CHK     $$<"
	$$(Q)$$(CKPATCH) $$(CKFLAGS) $$<

check : $(CHK_C)

endef


define MAKE_H

$(eval CHK_H := $(patsubst %.h,%.h.chk,$(1)))

$(CHK_H) : $(1)
	@echo "  CHK     $$<"
	$$(Q)$$(CKPATCH) $$(CKFLAGS) $$<

check : $(CHK_H)

endef


define MAKE_S

$(eval OBJ := $(1)/$(patsubst %.S,%.o,$(2)))
$(eval PREREQUISITES := $(patsubst %.o,%.d,$(OBJ)))

$(OBJ) : $(2)
	@echo "  AS      $$<"
	$$(Q)$$(AS) $$(ASFLAGS) -c $$< -o $$@

$(PREREQUISITES) : $(2)
	@echo "  DEPS    $$@"
	@mkdir -p $(1)
	$$(Q)$$(AS) $$(ASFLAGS) -M -MT $(OBJ) -MF $$@ $$<

ifdef IS_ANYTHING_TO_BUILD
-include $(PREREQUISITES)
endif

endef


define MAKE_LD

$(eval PREREQUISITES := $(1).d)

$(1) : $(2)
	@echo "  PP      $$<"
	$$(Q)$$(AS) $$(ASFLAGS) -P -E -o $$@ $$<

$(PREREQUISITES) : $(2)
	@echo "  DEPS    $$@"
	mkdir -p $$(dir $$@)
	$$(Q)$$(AS) $$(ASFLAGS) -M -MT $(1) -MF $$@ $$<

ifdef IS_ANYTHING_TO_BUILD
-include $(PREREQUISITES)
endif

endef


define MAKE_OBJS
	$(eval C_OBJS := $(filter %.c,$(2)))
	$(eval REMAIN := $(filter-out %.c,$(2)))
	$(eval $(foreach obj,$(C_OBJS),$(call MAKE_C,$(1),$(obj))))

	$(eval S_OBJS := $(filter %.S,$(REMAIN)))
	$(eval REMAIN := $(filter-out %.S,$(REMAIN)))
	$(eval $(foreach obj,$(S_OBJS),$(call MAKE_S,$(1),$(obj))))

	$(and $(REMAIN),$(error Unexpected source files present: $(REMAIN)))

	$(eval H_DIRS := $(subst -I,,$(subst $(CURDIR),.,$(INCLUDES))))
	$(eval H_FILES := $(foreach d,$(H_DIRS),$(wildcard $(d)/*.h)))
	$(eval $(foreach h,$(H_FILES),$(call MAKE_H,$(h))))
endef

# NOTE: The line continuation '\' is required in the next define otherwise we
# end up with a line-feed characer at the end of the last c filename.
# Also bare this issue in mind if extending the list of supported filetypes.
define SOURCES_TO_OBJS
	$(patsubst %.c,%.o,$(filter %.c,$(1))) \
	$(patsubst %.S,%.o,$(filter %.S,$(1)))
endef


define MAKE_LIB
	$(eval BUILD_DIR  := .)
	$(eval SOURCES    := $(2))
	$(eval OBJS       := $(addprefix $(BUILD_DIR)/,$(call SOURCES_TO_OBJS,$(SOURCES))))
	$(eval LIB        := $(1))
	$(eval CFLAGS     += -pie)
	$(eval $(call MAKE_OBJS,$(BUILD_DIR),$(SOURCES)))

$(BUILD_DIR) :
	$$(Q)mkdir -p "$$@"

$(LIB) : $(OBJS)
	@echo "  AR      $$@"
	$$(Q)$$(AR) rcs $$@ $(OBJS)
	@echo "Built $$@ successfully"
	@echo

.PHONY : $(LIB)

all :  $(BUILD_DIR) $(LIB)

endef

define MAKE_PROG
	$(eval BUILD_DIR  := .)
	$(eval SOURCES    := $(2))
	$(eval OBJS       := $(addprefix $(BUILD_DIR)/,$(call SOURCES_TO_OBJS,$(SOURCES))))
	$(eval LINKER_SFILE := $(filter %.S,$(3)))
	$(eval LINKERFILE := $(basename $(LINKER_SFILE)))
	$(eval MAPFILE    := $(1).map)
	$(eval ELF        := $(1).elf)
	$(eval DUMP       := $(1).lst)
	$(eval BIN        := $(1).bin)

	$(eval $(call MAKE_OBJS,$(BUILD_DIR),$(SOURCES)))

	$(if $(LINKER_SFILE), 							\
		$(eval $(call MAKE_LD,$(LINKERFILE),$(LINKER_SFILE))),		\
		$(eval LINKERFILE = $(3)))

$(BUILD_DIR) :
	$$(Q)mkdir -p "$$@"

$(ELF) : $(OBJS) $(LINKERFILE)
	@echo "  LD      $$@"

	$$(Q)$$(LD) -o $$@ $$(LDFLAGS) -Map=$(MAPFILE) --script $(LINKERFILE) \
					$(OBJS) $(LIBS) -lgcc

$(DUMP) : $(ELF)
	@echo "  OD      $$@"
	$${Q}$${OD} -dx $$< > $$@

$(BIN) : $(ELF)
	@echo "  BIN     $$@"
	$$(Q)$$(OC) -O binary $$< $$@
	@echo
	@echo "Built $$@ successfully"
	@echo

.PHONY : prog_$(1)
prog_$(1) : $(BUILD_DIR) $(BIN) $(DUMP)

all : prog_$(1)

endef

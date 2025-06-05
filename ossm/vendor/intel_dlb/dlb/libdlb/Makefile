# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

VERSION := 1

BASE_DIR ?= $(CURDIR)/..
BUILD_DIR := build

ifneq ($(wildcard $(BASE_DIR)/linux/.*),)
  DLB2_USER_HEADER_DIR = $(BASE_DIR)/linux/uapi/linux
else
  DLB2_USER_HEADER_DIR = $(BASE_DIR)/driver/dlb2/uapi/linux
endif

CFLAGS  := -g -fPIC -DVERSION_$(VERSION) -Wall -O3 -pthread
CFLAGS  += -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security \
           -Werror=format-security -fstack-protector-strong
CFLAGS  += -fno-delete-null-pointer-checks -fno-strict-overflow -fwrapv
CFLAGS  += $(EXTRA_CFLAGS)

ifeq ($(DLB_DISABLE_DOMAIN_SERVER), TRUE)
CFLAGS  += -DDLB_NOT_USE_DOMAIN_SERVER
endif

LDFLAGS := $(EXTRA_LDFLAGS) -pthread -Wl,-z,now,-z,relro,-z,noexecstack

ifneq ($(DEV_FILENAME_OVERRIDE),)
  CFLAGS += -DDEV_NAME=$(DEV_FILENAME_OVERRIDE)
endif

ifdef CROSS_COMPILE
  SYSROOT ?= $(SDKTARGETSYSROOT)
  CC = $(CROSS_COMPILE)gcc --sysroot=$(SYSROOT)
endif

SRCS :=         \
  dlb.c         \
  dlb2_ioctl.c \

OBJS := $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))
DEPS := $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.d))
LIB  := libdlb.so
SLIB := libdlb.a

default: lib examples cli doc

debug: CFLAGS +=
debug: default

release: CFLAGS += -DDISABLE_CHECK
release: default

lib: $(LIB) $(SLIB)

$(BUILD_DIR):
	@mkdir $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo Compiling $@
	@$(CC) -c $< $(CFLAGS) -o $@
	@$(CC) -c $< $(CFLAGS) -E -M $< -MT $@ -MF $(BUILD_DIR)/$*.d

$(LIB): $(OBJS)
	@echo Linking $@
	@$(CC) $^ -shared $(LDFLAGS) -o $@ -Wl,--version-script=version.map

$(SLIB): $(OBJS)
	@echo Generating $@
	@$(AR) -rc $(SLIB) $^

examples cli: $(LIB)
	@$(MAKE) -C $@/

doc:
	@$(MAKE) -C $@/

clean:
	@rm -rf $(BUILD_DIR) $(LIB) $(SLIB)
	@$(MAKE) -C examples/ clean
	@$(MAKE) -C cli/ clean
	@$(MAKE) -C doc/ clean

.PHONY: examples clean cli doc

-include $(DEPS)

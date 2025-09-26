# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

LIBDIR ?= $(CURDIR)/..

LDFLAGS := -L$(LIBDIR) -pthread
LDLIBS := -ldlb -lrt
CFLAGS := -I$(LIBDIR)/ -g -fPIC $(EXTRA_CFLAGS) -pthread

ifdef CROSS_COMPILE
  SYSROOT ?= $(SDKTARGETSYSROOT)
  CC = $(CROSS_COMPILE)gcc --sysroot=$(SYSROOT)
endif

SRCS :=         \
  ldb_traffic.c \
  dir_traffic.c \

OBJS := $(SRCS:.c=.o)
BINS := $(SRCS:.c=)
DEPS := $(SRCS:.c=.d)

LIB := $(BASE_DIR)/lib/libdlb.so

default: $(BINS)

%: %.c $(LIB)
	@echo Compiling $@
	@$(CC) $< $(LDFLAGS) $(LDLIBS) $(CFLAGS) -o $@
	@$(CC) $< $(LDFLAGS) $(LDLIBS) $(CFLAGS) -E -M $< -MT $@ -MF $*.d

clean:
	@rm -rf $(OBJS) $(DEPS) $(BINS)

-include $(DEPS)

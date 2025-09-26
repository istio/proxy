# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

DOXYGEN := $(shell command -v doxygen 2>/dev/null)

DOC_OUTPUT_DIR = gen

SRCS :=           \
  ../dlb.h        \
  ../dlb_adv.h    \
  ../dlb_common.h \

TOP=$(DOC_OUTPUT_DIR)/html/index.html

all: $(TOP)

$(TOP): $(SRCS)
ifneq ($(DOXYGEN),)
	doxygen Doxyfile
endif

clean:
	$(RM) -r $(DOC_OUTPUT_DIR)

.PHONY: all clean

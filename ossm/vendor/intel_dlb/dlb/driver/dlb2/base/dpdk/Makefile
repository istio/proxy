#
# Makefile for the Intel(R) Dynamic Load Balancer 2.0 (dlb2.ko) driver
#

GENERATED_BASE_FILES := \
  ../dlb2_regs.h        \
  ../dlb2_5_regs.h      \
  ../dlb2_resource.h    \
  ../dlb2_resource.c    \
  ../dlb2_hw_types.h    \
  ../dlb2_mbox.h        \

GENERATED_UAPI_FILES := \
  ../dlb2_user.h        \

GENERATED_FILES := $(GENERATED_BASE_FILES) $(GENERATED_UAPI_FILES)

default: $(GENERATED_FILES)

../dlb2_user.h: ../dlb2_user.h.in dlb2_user_ioctl.h.in dpdk-ify.sh
	@./dpdk-ify.sh $< $@

%.h: %.h.in dpdk-ify.sh
	@./dpdk-ify.sh $< $@

%.c: %.c.in dpdk-ify.sh
	@./dpdk-ify.sh $< $@

clean:
	@rm -rf $(GENERATED_FILES)

.PHONY: default clean

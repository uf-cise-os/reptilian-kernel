#
# Copyright (C) 2009-2011 The Android-x86 Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#

ifeq ($(TARGET_PREBUILT_KERNEL),)

KERNEL_DIR ?= $(call my-dir)

ifeq ($(TARGET_ARCH),x86)
KERNEL_TARGET := bzImage
TARGET_KERNEL_CONFIG ?= android-x86_defconfig
endif
ifeq ($(TARGET_ARCH),arm)
KERNEL_TARGET := zImage
TARGET_KERNEL_CONFIG ?= goldfish_defconfig
endif

KBUILD_OUTPUT := $(CURDIR)/$(TARGET_OUT_INTERMEDIATES)/kernel
mk_kernel := + $(hide) $(MAKE) -C $(KERNEL_DIR) O=$(KBUILD_OUTPUT) ARCH=$(TARGET_ARCH) $(if $(SHOW_COMMANDS),V=1)
ifneq ($(TARGET_ARCH),$(HOST_ARCH))
mk_kernel += CROSS_COMPILE=$(CURDIR)/$(TARGET_TOOLS_PREFIX)
endif

KERNEL_CONFIG_FILE := $(if $(wildcard $(TARGET_KERNEL_CONFIG)),$(TARGET_KERNEL_CONFIG),$(KERNEL_DIR)/arch/$(TARGET_ARCH)/configs/$(TARGET_KERNEL_CONFIG))

MOD_ENABLED := $(shell grep ^CONFIG_MODULES=y $(KERNEL_CONFIG_FILE))
FIRMWARE_ENABLED := $(shell grep ^CONFIG_FIRMWARE_IN_KERNEL=y $(KERNEL_CONFIG_FILE))

# I understand Android build system discourage to use submake,
# but I don't want to write a complex Android.mk to build kernel.
# This is the simplest way I can think.
KERNEL_DOTCONFIG_FILE := $(KBUILD_OUTPUT)/.config
$(KERNEL_DOTCONFIG_FILE): $(KERNEL_CONFIG_FILE) | $(ACP)
	$(copy-file-to-new-target)

BUILT_KERNEL_TARGET := $(KBUILD_OUTPUT)/arch/$(TARGET_ARCH)/boot/$(KERNEL_TARGET)
$(INSTALLED_KERNEL_TARGET): $(KERNEL_DOTCONFIG_FILE)
	$(mk_kernel) oldnoconfig
	$(mk_kernel) $(KERNEL_TARGET) $(if $(MOD_ENABLED),modules)
	$(hide) $(ACP) -fp $(BUILT_KERNEL_TARGET) $@
	$(if $(FIRMWARE_ENABLED),$(mk_kernel) INSTALL_MOD_PATH=$(CURDIR)/$(TARGET_OUT) firmware_install)

ifneq ($(MOD_ENABLED),)
ifeq ($(wildcard $(INSTALLED_KERNEL_TARGET)),)
KERNEL_MODULES_DEP := $(TARGET_OUT)/lib/modules
else
KERNELRELEASE := $(shell $(MAKE) -sC $(KERNEL_DIR) O=$(KBUILD_OUTPUT) ARCH=$(TARGET_ARCH) kernelrelease)
KERNEL_MODULES_DEP := $(TARGET_OUT)/lib/modules/$(KERNELRELEASE)/modules.dep
endif
$(TARGET_OUT_INTERMEDIATES)/%.kmodule: $(INSTALLED_KERNEL_TARGET)
	$(hide) cp -an $(EXTRA_KERNEL_MODULE_PATH_$*) $(TARGET_OUT_INTERMEDIATES)/$*.kmodule
	@echo Building additional kernel module $*
	$(mk_kernel) M=$(CURDIR)/$@ modules

$(KERNEL_MODULES_DEP): $(INSTALLED_KERNEL_TARGET) $(patsubst %,$(TARGET_OUT_INTERMEDIATES)/%.kmodule,$(TARGET_EXTRA_KERNEL_MODULES))
	$(hide) rm -rf $(TARGET_OUT)/lib/modules
	$(mk_kernel) INSTALL_MOD_PATH=$(CURDIR)/$(TARGET_OUT) modules_install
	+ $(hide) for kmod in $(TARGET_EXTRA_KERNEL_MODULES) ; do \
	        echo Installing additional kernel module $${kmod} ; \
	        $(subst +,,$(subst $(hide),,$(mk_kernel))) INSTALL_MOD_PATH=$(CURDIR)/$(TARGET_OUT) M=$(CURDIR)/$(TARGET_OUT_INTERMEDIATES)/$${kmod}.kmodule modules_install ; \
	done
	$(hide) rm -f $(TARGET_OUT)/lib/modules/*/{build,source}
endif

# rules to get source of Broadcom 802.11a/b/g/n hybrid device driver
# based on broadcomsetup.sh of Kyle Evans
WL_ENABLED := $(shell grep ^CONFIG_WL=[my] $(KERNEL_CONFIG_FILE))
WL_PATH := drivers/net/wireless/wl
WL_SRC := $(KERNEL_DIR)/$(WL_PATH)/hybrid-portsrc_x86_32-v5_100_82_112.tar.gz
$(WL_SRC):
	@echo Downloading $(@F)...
	$(hide) curl http://www.broadcom.com/docs/linux_sta/$(@F) > $@ && tar zxf $@ -C $(@D) --overwrite && \
	(cd kernel/$(WL_PATH); patch Makefile < Makefile.patch; patch src/wl/sys/wl_iw.h < wl_iw.h.patch)
$(INSTALLED_KERNEL_TARGET): $(if $(WL_ENABLED),$(WL_SRC))

installclean: FILES += $(KBUILD_OUTPUT) $(INSTALLED_KERNEL_TARGET)

TARGET_PREBUILT_KERNEL := $(INSTALLED_KERNEL_TARGET)

.PHONY: kernel
kernel: $(TARGET_PREBUILT_KERNEL)

else

$(INSTALLED_KERNEL_TARGET): $(TARGET_PREBUILT_KERNEL) | $(ACP)
	$(copy-file-to-new-target)
ifdef TARGET_PREBUILT_MODULES
	mkdir -p $(TARGET_OUT)/lib
	$(hide) cp -r $(TARGET_PREBUILT_MODULES) $(TARGET_OUT)/lib
endif

endif # TARGET_PREBUILT_KERNEL

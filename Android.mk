#
# Copyright (C) 2009-2013 The Android-x86 Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#

ifeq ($(KBUILD_OUTPUT),)
ifeq ($(TARGET_PREBUILT_KERNEL),)

KERNEL_DIR ?= $(call my-dir)

ifeq ($(TARGET_ARCH),x86)
TARGET_KERNEL_ARCH ?= x86
KERNEL_TARGET := bzImage
TARGET_KERNEL_CONFIG ?= android-$(TARGET_KERNEL_ARCH)_defconfig
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
KERNEL_ARCH_CHANGED := $(if $(filter 0,$(shell grep -s ^$(if $(filter x86,$(TARGET_KERNEL_ARCH)),\#.)CONFIG_64BIT $(KERNEL_DOTCONFIG_FILE) | wc -l)),FORCE)
$(KERNEL_DOTCONFIG_FILE): $(KERNEL_CONFIG_FILE) $(KERNEL_ARCH_CHANGED) | $(ACP)
	$(copy-file-to-new-target)

# bison is needed to build kernel and external modules from source
BISON := $(HOST_OUT_EXECUTABLES)/bison$(HOST_EXECUTABLE_SUFFIX)

BUILT_KERNEL_TARGET := $(KBUILD_OUTPUT)/arch/$(TARGET_ARCH)/boot/$(KERNEL_TARGET)
$(INSTALLED_KERNEL_TARGET): $(KERNEL_DOTCONFIG_FILE) $(BISON)
	$(mk_kernel) oldnoconfig
	$(mk_kernel) $(KERNEL_TARGET) $(if $(MOD_ENABLED),modules)
	$(hide) $(ACP) -fp $(BUILT_KERNEL_TARGET) $@
	$(if $(FIRMWARE_ENABLED),$(mk_kernel) INSTALL_MOD_PATH=$(CURDIR)/$(TARGET_OUT) firmware_install)

ifneq ($(MOD_ENABLED),)
KERNEL_MODULES_DEP := $(firstword $(wildcard $(TARGET_OUT)/lib/modules/*/modules.dep))
KERNEL_MODULES_DEP := $(if $(KERNEL_MODULES_DEP),$(KERNEL_MODULES_DEP),$(TARGET_OUT)/lib/modules)

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
WL_PATH := $(KERNEL_DIR)/drivers/net/wireless/wl
WL_ENABLED := $(if $(wildcard $(WL_PATH)),$(shell grep ^CONFIG_WL=[my] $(KERNEL_CONFIG_FILE)))
ifeq (,$(shell grep ^CONFIG_X86_32=y $(KERNEL_CONFIG_FILE)))
WL_SRC := $(WL_PATH)/hybrid-v35_64-nodebug-pcoem-6_30_223_141.tar.gz
else
WL_SRC := $(WL_PATH)/hybrid-v35-nodebug-pcoem-6_30_223_141.tar.gz
endif
$(WL_SRC):
	@echo Downloading $(@F)...
	$(hide) curl http://www.broadcom.com/docs/linux_sta/$(@F) > $@
$(WL_PATH)/Makefile : $(WL_SRC) $(wildcard $(WL_PATH)/*.patch) $(KERNEL_ARCH_CHANGED)
	$(hide) tar zxf $< -C $(@D) --overwrite && \
		patch -p5 -d $(@D) -i wl.patch && \
		patch -p1 -d $(@D) -i linux-recent.patch
$(INSTALLED_KERNEL_TARGET): $(if $(WL_ENABLED),$(WL_PATH)/Makefile)

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
endif # KBUILD_OUTPUT

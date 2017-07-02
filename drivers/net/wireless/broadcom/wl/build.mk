# parts of build/core/tasks/kernel.mk

WL_ENABLED := $(if $(wildcard $(WL_PATH)),$(shell grep ^CONFIG_WL=[my] $(KERNEL_CONFIG_FILE)))
WL_SRC := $(WL_PATH)/hybrid-v35$(if $(filter x86,$(TARGET_KERNEL_ARCH)),,_64)-nodebug-pcoem-6_30_223_271.tar.gz
WL_LIB := $(WL_PATH)/lib$(if $(filter x86,$(TARGET_KERNEL_ARCH)),32,64)

WL_PATCHES := \
	wl.patch \
	linux-recent.patch \
	linux-48.patch \
	linux-411.patch \

$(WL_SRC):
	@echo Downloading $(@F)...
	$(hide) curl -k https://docs.broadcom.com/docs-and-downloads/docs/linux_sta/$(@F) > $@

$(WL_LIB): $(WL_SRC) $(addprefix $(WL_PATH)/,$(WL_PATCHES))
	$(hide) tar zxf $< -C $(@D) --overwrite -m && \
		rm -rf $@ && mv $(@D)/lib $@ && touch $@ && \
		cat $(filter %.patch,$^) | patch -p1 -d $(@D)

$(INSTALLED_KERNEL_TARGET): $(if $(WL_ENABLED),$(WL_LIB))

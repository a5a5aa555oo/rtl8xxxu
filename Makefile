# SPDX-License-Identifier: GPL-2.0-only

ifneq ($(KERNELRELEASE),)

obj-m	:= rtl8xxxu_git.o

rtl8xxxu_git-y	:= core.o 8192e.o 8723b.o \
		   8723a.o 8192c.o 8188f.o \
		   8188e.o 8710b.o 8192f.o

ccflags-y += -DCONFIG_RTL8XXXU_UNTESTED -std=gnu11

else

KVER ?= `uname -r`
KDIR ?= /lib/modules/$(KVER)/build
MODDIR ?= /lib/modules/$(KVER)/extra
FWDIR := /lib/firmware/rtlwifi

.PHONY: modules clean install install_fw uninstall

modules:
	$(MAKE) -j`nproc` -C $(KDIR) M=$$PWD modules

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

install:
	strip -g rtl8xxxu_git.ko
	@install -Dvm 644 -t $(MODDIR) rtl8xxxu_git.ko
	depmod -a $(KVER)
	
install_fw:
ifeq ($(wildcard $(FWDIR)), )
	@install -Dvm 644 -t $(FWDIR) firmware/*.bin
else
	@cp -r firmware tmp
ifneq ($(wildcard $(FWDIR)/*.zst), )
	@zstd -fq --rm tmp/*.bin
endif
ifneq ($(wildcard $(FWDIR)/*.xz), )
	@xz -f -C crc32 tmp/*.bin
endif
ifneq ($(wildcard $(FWDIR)/*.gz), )
	@gzip -f tmp/*.bin
endif
	@install -Dvm 644 -t $(FWDIR) tmp/rtl*
	@rm -rf tmp
endif

uninstall:
	@rm -vf $(MODDIR)/rtl8xxxu_git.ko
	@rmdir --ignore-fail-on-non-empty $(MODDIR) || true
	depmod -a $(KVER)

endif

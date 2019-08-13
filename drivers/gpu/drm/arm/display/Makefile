# SPDX-License-Identifier: GPL-2.0
# linux build system bootstrap for out-of-tree module

# default to building for the host
ARCH ?= $(shell uname -m)
KDIR ?= /lib/modules/$(shell uname -r)/build

default: module

module:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(PWD) CONFIG_DRM_KOMEDA=m C=1 CF='-fdiagnostic-prefix -D__CHECK_ENDIAN'
clean:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(PWD) clean

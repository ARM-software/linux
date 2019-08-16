# SPDX-License-Identifier: GPL-2.0
# linux build system bootstrap for out-of-tree module

# default to building for the host
ARCH ?= $(shell uname -m)
KDIR ?= /lib/modules/$(shell uname -r)/build

default: strict

strict:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) CONFIG_SPLIT_CONNECTOR=m W=1 C=1 CF='-fdiagnostic-prefix -D__CHECK_ENDIAN'

module:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) CONFIG_SPLIT_CONNECTOR=m

clean:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) clean

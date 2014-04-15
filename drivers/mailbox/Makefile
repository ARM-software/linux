# Generic MAILBOX API

obj-$(CONFIG_MAILBOX)		+= mailbox.o

obj-$(CONFIG_ARM_MHU_MBOX)	+= arm_mhu.o
obj-$(CONFIG_ARM_SCPI_PROTOCOL)	+= scpi_protocol.o
obj-$(CONFIG_PL320_MBOX)	+= pl320.o

obj-$(CONFIG_OMAP_MBOX)		+= omap-mailbox.o
obj-$(CONFIG_OMAP1_MBOX)	+= mailbox_omap1.o
mailbox_omap1-objs		:= mailbox-omap1.o
obj-$(CONFIG_OMAP2PLUS_MBOX)	+= mailbox_omap2.o
mailbox_omap2-objs		:= mailbox-omap2.o

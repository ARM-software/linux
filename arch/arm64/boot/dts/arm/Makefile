dtb-$(CONFIG_ARCH_VEXPRESS) += foundation-v8.dtb foundation-v8-gicv3.dtb
dtb-$(CONFIG_ARCH_VEXPRESS) += fvp-base-aemv8a-aemv8a.dtb
dtb-$(CONFIG_ARCH_VEXPRESS) += juno-r0.dtb juno-r1.dtb juno-r2.dtb
dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb
dtb-$(CONFIG_ARCH_VEXPRESS) += \
	juno-r0-midgard.dtb \
	juno-r0-bifrost.dtb \
	juno-r1-midgard.dtb \
	juno-r1-midgard-acelite.dtb \
	juno-r1-bifrost.dtb \
	juno-r1-bifrost-acelite.dtb \
	juno-r1-bifrost-ace.dtb

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb

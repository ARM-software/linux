#
# Makefile for Secure OS control sysfs
#
obj-$(CONFIG_SECURE_OS_CONTROL) += sec_os_ctrl.o

# Choose Exynos SoC
ifdef CONFIG_SOC_EXYNOS5422
EXYNOS_SOC := exynos5422
else ifdef CONFIG_SOC_EXYNOS5430
EXYNOS_SOC := exynos5430
endif

# MobiCore kernel driver path
GUD_ROOT_FOLDER := drivers/gud/gud-$(EXYNOS_SOC)

# Includes MobiCore kernel driver
ccflags-y += -I$(GUD_ROOT_FOLDER)/MobiCoreDriver
ccflags-y += -I$(GUD_ROOT_FOLDER)/MobiCoreDriver/public

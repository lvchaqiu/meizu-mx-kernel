# arch/arm/mach-exynos/Makefile
#
# Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:= #reserve_mem-exynos4.o
obj-m				:=
obj-n				:=
obj-				:=

# Core support for EXYNOS system

obj-y				+= init.o irq-combiner.o dma.o irq-eint.o
obj-y	+= irq-sgi.o
obj-$(CONFIG_ARCH_EXYNOS4)	+= cpu-exynos4.o clock-exynos4.o pmu-exynos4.o
obj-$(CONFIG_ARCH_EXYNOS5)	+= cpu-exynos5.o clock-exynos5.o pmu-exynos5.o

obj-y				+= clock-domain.o
obj-$(CONFIG_CPU_EXYNOS4210)	+= clock-exynos4210.o
obj-$(CONFIG_CPU_EXYNOS4212)	+= clock-exynos4212.o
obj-$(CONFIG_EXYNOS4_PM)	+= pm-exynos4.o sleep-exynos4.o
obj-$(CONFIG_EXYNOS5_PM)	+= pm-exynos5.o sleep-exynos5.o
obj-y				+= asv.o asv-4210.o asv-4x12.o
obj-$(CONFIG_EXYNOS4_CPUIDLE)	+= cpuidle-exynos4.o idle-exynos4.o
obj-$(CONFIG_EXYNOS5_CPUIDLE)	+= cpuidle-exynos5.o idle-exynos5.o

obj-$(CONFIG_EXYNOS4_DEV_PPMU)	+= dev-ppmu.o exynos4_ppmu.o
obj-$(CONFIG_BUSFREQ)		+= ppc.o busfreq.o

obj-$(CONFIG_BUSFREQ_OPP) += busfreq_opp_exynos4.o dev.o
obj-$(CONFIG_EXYNOS4210_BUSFREQ_OPP) += busfreq_opp_4210.o
obj-$(CONFIG_EXYNOS4X12_BUSFREQ_OPP) += busfreq_opp_4x12.o

obj-$(CONFIG_SMP)		+= platsmp.o headsmp.o

obj-$(CONFIG_EXYNOS_MCT)	+= mct.o

obj-$(CONFIG_HOTPLUG_CPU)	+= hotplug.o

obj-$(CONFIG_STAND_ALONE_POLICY)	+= stand-hotplug.o
obj-$(CONFIG_WITH_DVFS_POLICY)		+= dvfs-hotplug.o
obj-$(CONFIG_DVFS_NR_RUNNING_POLICY)	+= dynamic-dvfs-nr_running-hotplug.o
obj-$(CONFIG_NR_RUNNING_POLICY)		+= dynamic-nr_running-hotplug.o

obj-$(CONFIG_EXYNOS_FIQ_DEBUGGER)	+= exynos_fiq_debugger.o

# machine support

obj-$(CONFIG_MACH_SMDKC210)		+= mach-smdkv310.o
obj-$(CONFIG_MACH_SMDKV310)		+= mach-smdkv310.o
obj-$(CONFIG_MACH_ARMLEX4210)		+= mach-armlex4210.o
obj-$(CONFIG_MACH_UNIVERSAL_C210)	+= mach-universal_c210.o
obj-$(CONFIG_MACH_NURI)			+= mach-nuri.o
obj-$(CONFIG_MACH_SMDK4X12)		+= mach-smdk4x12.o
obj-$(CONFIG_MACH_SMDK5210)		+= mach-smdk5210.o
obj-$(CONFIG_MACH_SMDK5250)		+= mach-smdk5250.o
obj-$(CONFIG_MX_SERIAL_TYPE)		+= usb-detect.o mx_reboot.o
obj-$(CONFIG_MACH_M032)			+= mach-m032.o gpio-m032.o
obj-$(CONFIG_MACH_M030)			+= mach-m030.o gpio-m030.o

# device support

obj-$(CONFIG_ARCH_EXYNOS)		+= dev-audio.o
obj-$(CONFIG_EXYNOS5_DEV_AHCI)		+= dev_dwc_ahsata.o
obj-$(CONFIG_EXYNOS4_DEV_AHCI)		+= dev-ahci.o
obj-$(CONFIG_EXYNOS_DEV_PD)		+= dev-pd.o
ifeq ($(CONFIG_EXYNOS_DEV_PD),y)
obj-$(CONFIG_ARCH_EXYNOS4)		+= dev-pd-exynos4.o
obj-$(CONFIG_ARCH_EXYNOS5)		+= dev-pd-exynos5.o
endif
obj-$(CONFIG_EXYNOS_DEV_CPUFREQ)	+= dev-cpufreq.o
obj-$(CONFIG_EXYNOS_DEV_BUSFREQ)	+= dev-busfreq.o
obj-$(CONFIG_EXYNOS_DEV_TMU)		+= dev-tmu.o
obj-$(CONFIG_EXYNOS_IOMMU)		+= dev-sysmmu.o
obj-$(CONFIG_EXYNOS4_DEV_DWMCI)		+= dev-dwmci.o
obj-$(CONFIG_EXYNOS4_DEV_FIMC_IS)	+= dev-fimc-is.o
obj-$(CONFIG_EXYNOS4_DEV_FIMC_LITE)	+= dev-fimc-lite.o
obj-$(CONFIG_EXYNOS_DEV_C2C)		+= dev-c2c.o
obj-$(CONFIG_EXYNOS5_DEV_GSC)		+= dev-gsc.o

obj-$(CONFIG_EXYNOS4_SETUP_FIMC)	+= setup-fimc.o
obj-$(CONFIG_EXYNOS4_SETUP_FIMC0)	+= setup-fimc0.o
obj-$(CONFIG_EXYNOS4_SETUP_FIMC1)	+= setup-fimc1.o
obj-$(CONFIG_EXYNOS4_SETUP_FIMC2)	+= setup-fimc2.o
obj-$(CONFIG_EXYNOS4_SETUP_FIMC3)	+= setup-fimc3.o
obj-$(CONFIG_EXYNOS4_SETUP_CSIS)	+= setup-csis.o
obj-$(CONFIG_EXYNOS4_SETUP_FIMD0)	+= setup-fimd0.o
obj-$(CONFIG_EXYNOS4_SETUP_FIMD)	+= setup-fimd.o
obj-$(CONFIG_EXYNOS4_SETUP_DP)		+= setup-dp.o
obj-$(CONFIG_EXYNOS4_SETUP_FIMC_IS)	+= setup-fimc-is.o
obj-$(CONFIG_EXYNOS5_SETUP_GSC)		+= setup-gsc.o
obj-y					+= setup-i2c0.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C1)	+= setup-i2c1.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C2)	+= setup-i2c2.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C3)	+= setup-i2c3.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C4)	+= setup-i2c4.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C5)	+= setup-i2c5.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C6)	+= setup-i2c6.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C7)	+= setup-i2c7.o
obj-$(CONFIG_EXYNOS4_SETUP_KEYPAD)	+= setup-keypad.o
obj-$(CONFIG_EXYNOS4_SETUP_MFC)		+= setup-mfc.o
obj-$(CONFIG_EXYNOS4_SETUP_SDHCI)	+= setup-sdhci.o
obj-$(CONFIG_EXYNOS4_SETUP_SDHCI_GPIO)	+= setup-sdhci-gpio.o
obj-$(CONFIG_EXYNOS4_SETUP_MSHCI)	+= setup-mshci.o
obj-$(CONFIG_EXYNOS4_SETUP_MSHCI_GPIO)	+= setup-mshci-gpio.o

obj-$(CONFIG_S3C64XX_DEV_SPI)		+= dev-spi.o

obj-$(CONFIG_EXYNOS4_SETUP_USB_PHY)	+= setup-usb-phy.o
obj-$(CONFIG_EXYNOS4_SETUP_FB_S5P)	+= setup-fb-s5p.o
obj-$(CONFIG_EXYNOS4_SETUP_TVOUT)	+= setup-tvout.o
obj-$(CONFIG_EXYNOS4_SETUP_JPEG)	+= setup-jpeg.o
obj-$(CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION) += secmem-allocdev.o

obj-$(CONFIG_WAKEUP_ASSIST)		+= wakeup_assist.o

obj-$(CONFIG_ION_EXYNOS)		+= dev-ion.o
obj-$(CONFIG_EXYNOS_C2C)		+= setup-c2c.o
obj-$(CONFIG_UBLOX_GPS)			+= ublox-gps.o
obj-$(CONFIG_BT)			+= mx-rfkill.o
obj-$(CONFIG_BCM4329)			+= mx-wifi.o
obj-$(CONFIG_MX_SERIAL_TYPE)		+= mx_fb.o mx_camera.o mx_cma.o mx_tsp.o mx_mshci.o mx_tvout.o
obj-$(CONFIG_MX_SERIAL_TYPE)		+= mx_factory.o mx_debug.o

obj-$(CONFIG_LEDS_GPIO_REGISTER)	+= mx_leds.o
obj-$(CONFIG_MX_DEV_JACK)		+= mx_jack.o
obj-$(CONFIG_TOUCH_BOOSTER)		+= mx_ts_booster.o
obj-$(CONFIG_CPUPOWER)			+= mx_cpu_power.o

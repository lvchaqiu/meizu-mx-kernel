#
# linux/arch/arm/vfp/Makefile
#
# Copyright (C) 2001 ARM Limited
#

# ccflags-y := -DDEBUG
# asflags-y := -DDEBUG
#__GNUC_PATCHLEVEL__

GCCVERSION = $(shell $(CC) --version | grep gcc | sed 's/^.* //g')

ifeq ("$(GCCVERSION)", "4.6.3")
KBUILD_AFLAGS	:=$(KBUILD_AFLAGS:-mhard-float=-Wa,-mfpu=neon)
else
KBUILD_AFLAGS	:=$(KBUILD_AFLAGS:-msoft-float=-Wa,-mfpu=softvfp+vfp)
endif

LDFLAGS		+=--no-warn-mismatch

obj-y			+= vfp.o

vfp-$(CONFIG_VFP)	+= vfpmodule.o entry.o vfphw.o vfpsingle.o vfpdouble.o

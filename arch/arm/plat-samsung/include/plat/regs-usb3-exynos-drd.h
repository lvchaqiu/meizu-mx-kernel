/* arch/arm/plat-samsung/include/plat/regs-usb3-exynos-drd.h
 *
 * Copyright (c) 2011 Samsung Electronics Co. Ltd
 * Author: Anton Tikhomirov <av.tikhomirov@samsung.com>
 *
 * Exynos SuperSpeed USB 3.0 DRD Controller registers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_PLAT_REGS_USB3_EXYNOS_DRD_H
#define __SAMSUNG_PLAT_REGS_USB3_EXYNOS_DRD_H __FILE__

#define EXYNOS_USB3_REG(x) (x)

/* xHCI registers */

/* Global registers */
#define EXYNOS_USB3_GSBUSCFG0		EXYNOS_USB3_REG(0xC100)
#define EXYNOS_USB3_GSBUSCFG0_SBusStoreAndForward	(1 << 12)
#define EXYNOS_USB3_GSBUSCFG0_DatBigEnd			(1 << 11)
#define EXYNOS_USB3_GSBUSCFG0_INCR256BrstEna		(1 << 7)
#define EXYNOS_USB3_GSBUSCFG0_INCR128BrstEna		(1 << 6)
#define EXYNOS_USB3_GSBUSCFG0_INCR64BrstEna		(1 << 5)
#define EXYNOS_USB3_GSBUSCFG0_INCR32BrstEna		(1 << 4)
#define EXYNOS_USB3_GSBUSCFG0_INCR16BrstEna		(1 << 3)
#define EXYNOS_USB3_GSBUSCFG0_INCR8BrstEna		(1 << 2)
#define EXYNOS_USB3_GSBUSCFG0_INCR4BrstEna		(1 << 1)
#define EXYNOS_USB3_GSBUSCFG0_INCRBrstEna		(1 << 0)


#define EXYNOS_USB3_GSBUSCFG1		EXYNOS_USB3_REG(0xC104)
#define EXYNOS_USB3_GSBUSCFG1_EN1KPAGE			(1 << 12)

#define EXYNOS_USB3_GSBUSCFG1_BREQLIMIT_MASK		(0xf << 8)
#define EXYNOS_USB3_GSBUSCFG1_BREQLIMIT_SHIFT		(8)
#define EXYNOS_USB3_GSBUSCFG1_BREQLIMIT_LIMIT		(0xf)
#define EXYNOS_USB3_GSBUSCFG1_BREQLIMIT(_x)		((_x) << 8)


#define EXYNOS_USB3_GTXTHRCFG		EXYNOS_USB3_REG(0xC108)
#define EXYNOS_USB3_GTXTHRCFG_USBTxPktCntSel		(1 << 29)

#define EXYNOS_USB3_GTXTHRCFG_USBTxPktCnt_MASK		(0xf << 24)
#define EXYNOS_USB3_GTXTHRCFG_USBTxPktCnt_SHIFT		(24)
#define EXYNOS_USB3_GTXTHRCFG_USBTxPktCnt_LIMIT		(0xf)
#define EXYNOS_USB3_GTXTHRCFG_USBTxPktCnt(_x)		((_x) << 24)

#define EXYNOS_USB3_GTXTHRCFG_USBMaxTxBurstSize_MASK	(0xff << 16)
#define EXYNOS_USB3_GTXTHRCFG_USBMaxTxBurstSize_SHIFT	(16)
#define EXYNOS_USB3_GTXTHRCFG_USBMaxTxBurstSize_LIMIT	(0xff)
#define EXYNOS_USB3_GTXTHRCFG_USBMaxTxBurstSize(_x)	((_x) << 16)


#define EXYNOS_USB3_GRXTHRCFG		EXYNOS_USB3_REG(0xC10C)
#define EXYNOS_USB3_GRXTHRCFG_USBRxPktCntSel		(1 << 29)

#define EXYNOS_USB3_GRXTHRCFG_USBRxPktCnt_MASK		(0xf << 24)
#define EXYNOS_USB3_GRXTHRCFG_USBRxPktCnt_SHIFT		(24)
#define EXYNOS_USB3_GRXTHRCFG_USBRxPktCnt_LIMIT		(0xf)
#define EXYNOS_USB3_GRXTHRCFG_USBRxPktCnt(_x)		((_x) << 24)

#define EXYNOS_USB3_GRXTHRCFG_USBMaxRxBurstSize_MASK	(0x1f << 19)
#define EXYNOS_USB3_GRXTHRCFG_USBMaxRxBurstSize_SHIFT	(19)
#define EXYNOS_USB3_GRXTHRCFG_USBMaxRxBurstSize_LIMIT	(0x1f)
#define EXYNOS_USB3_GRXTHRCFG_USBMaxRxBurstSize(_x)	((_x) << 19)


#define EXYNOS_USB3_GCTL		EXYNOS_USB3_REG(0xC110)

#define EXYNOS_USB3_GCTL_PwrDnScale_MASK		(0x1fff << 19)
#define EXYNOS_USB3_GCTL_PwrDnScale_SHIFT		(19)
#define EXYNOS_USB3_GCTL_PwrDnScale_LIMIT		(0x1fff)
#define EXYNOS_USB3_GCTL_PwrDnScale(_x)			((_x) << 19)

#define EXYNOS_USB3_GCTL_U2RSTECN			(1 << 16)

#define EXYNOS_USB3_GCTL_FRMSCLDWN_MASK			(0x3 << 14)
#define EXYNOS_USB3_GCTL_FRMSCLDWN_SHIFT		(14)
#define EXYNOS_USB3_GCTL_FRMSCLDWN_LIMIT		(0x3)
#define EXYNOS_USB3_GCTL_FRMSCLDWN(_x)			((_x) << 14)

#define EXYNOS_USB3_GCTL_PrtCapDir_MASK			(0x3 << 12)
#define EXYNOS_USB3_GCTL_PrtCapDir_SHIFT		(12)
#define EXYNOS_USB3_GCTL_PrtCapDir_LIMIT		(0x3)
#define EXYNOS_USB3_GCTL_PrtCapDir(_x)			((_x) << 12)

#define EXYNOS_USB3_GCTL_CoreSoftReset			(1 << 11)
#define EXYNOS_USB3_GCTL_LocalLpBkEn			(1 << 10)
#define EXYNOS_USB3_GCTL_LpbkEn				(1 << 9)
#define EXYNOS_USB3_GCTL_DebugAttach			(1 << 8)

#define EXYNOS_USB3_GCTL_RAMClkSel_MASK			(0x3 << 6)
#define EXYNOS_USB3_GCTL_RAMClkSel_SHIFT		(6)
#define EXYNOS_USB3_GCTL_RAMClkSel_LIMIT		(0x3)
#define EXYNOS_USB3_GCTL_RAMClkSel(_x)			((_x) << 6)

#define EXYNOS_USB3_GCTL_ScaleDown_MASK			(0x3 << 4)
#define EXYNOS_USB3_GCTL_ScaleDown_SHIFT		(4)
#define EXYNOS_USB3_GCTL_ScaleDown_LIMIT		(0x3)
#define EXYNOS_USB3_GCTL_ScaleDown(_x)			((_x) << 4)

#define EXYNOS_USB3_GCTL_DisScramble			(1 << 3)
#define EXYNOS_USB3_GCTL_SsPwrClmp			(1 << 2)
#define EXYNOS_USB3_GCTL_HsFsLsPwrClmp			(1 << 1)
#define EXYNOS_USB3_GCTL_DsblClkGtng			(1 << 0)


#define EXYNOS_USB3_GEVTEN		EXYNOS_USB3_REG(0xC114)
#define EXYNOS_USB3_GEVTEN_I2CEvtEn			(1 << 1)
#define EXYNOS_USB3_GEVTEN_ULPICKEvtEn			(1 << 0)
#define EXYNOS_USB3_GEVTEN_I2CCKEvtEn			(1 << 0)


#define EXYNOS_USB3_GSTS		EXYNOS_USB3_REG(0xC118)

#define EXYNOS_USB3_GSTS_CBELT_MASK			(0xfff << 20)
#define EXYNOS_USB3_GSTS_CBELT_SHIFT			(20)
#define EXYNOS_USB3_GSTS_CBELT_LIMIT			(0xfff)
#define EXYNOS_USB3_GSTS_CBELT(_x)			((_x) << 20)

#define EXYNOS_USB3_GSTS_OTG_IP				(1 << 10)
#define EXYNOS_USB3_GSTS_BC_IP				(1 << 9)
#define EXYNOS_USB3_GSTS_ADP_IP				(1 << 8)
#define EXYNOS_USB3_GSTS_Host_IP			(1 << 7)
#define EXYNOS_USB3_GSTS_Device_IP			(1 << 6)
#define EXYNOS_USB3_GSTS_CSRTimeout			(1 << 5)
#define EXYNOS_USB3_GSTS_BusErrAddrVld			(1 << 4)

#define EXYNOS_USB3_GSTS_CurMod_MASK			(0x3 << 0)
#define EXYNOS_USB3_GSTS_CurMod_SHIFT			(0)
#define EXYNOS_USB3_GSTS_CurMod_LIMIT			(0x3)
#define EXYNOS_USB3_GSTS_CurMod(_x)			((_x) << 0)


#define EXYNOS_USB3_GSNPSID		EXYNOS_USB3_REG(0xC120)


#define EXYNOS_USB3_GGPIO		EXYNOS_USB3_REG(0xC124)

#define EXYNOS_USB3_GGPIO_GPO_MASK			(0xffff << 16)
#define EXYNOS_USB3_GGPIO_GPO_SHIFT			(16)
#define EXYNOS_USB3_GGPIO_GPO_LIMIT			(0xffff)
#define EXYNOS_USB3_GGPIO_GPO(_x)			((_x) << 16)

#define EXYNOS_USB3_GGPIO_GPI_MASK			(0xffff << 0)
#define EXYNOS_USB3_GGPIO_GPI_SHIFT			(0)
#define EXYNOS_USB3_GGPIO_GPI_LIMIT			(0xffff)
#define EXYNOS_USB3_GGPIO_GPI(_x)			((x) << 0)


#define EXYNOS_USB3_GUID		EXYNOS_USB3_REG(0xC128)


#define EXYNOS_USB3_GUCTL		EXYNOS_USB3_REG(0xC12C)
#define EXYNOS_USB3_GUCTL_SprsCtrlTransEn		(1 << 17)
#define EXYNOS_USB3_GUCTL_ResBwHSEPS			(1 << 16)
#define EXYNOS_USB3_GUCTL_CMdevAddr			(1 << 15)
#define EXYNOS_USB3_GUCTL_USBHstInAutoRetryEn		(1 << 14)

#define EXYNOS_USB3_GUCTL_USBHstInMaxBurst_MASK		(0x7 << 11)
#define EXYNOS_USB3_GUCTL_USBHstInMaxBurst_SHIFT	(11)
#define EXYNOS_USB3_GUCTL_USBHstInMaxBurst_LIMIT	(0x7)
#define EXYNOS_USB3_GUCTL_USBHstInMaxBurst(_x)		((_x) << 11)

#define EXYNOS_USB3_GUCTL_DTCT_MASK			(0x3 << 9)
#define EXYNOS_USB3_GUCTL_DTCT_SHIFT			(9)
#define EXYNOS_USB3_GUCTL_DTCT_LIMIT			(0x3)
#define EXYNOS_USB3_GUCTL_DTCT(_x)			((_x) << 9)

#define EXYNOS_USB3_GUCTL_DTFT_MASK			(0x1ff << 0)
#define EXYNOS_USB3_GUCTL_DTFT_SHIFT			(0)
#define EXYNOS_USB3_GUCTL_DTFT_LIMIT			(0x1ff)
#define EXYNOS_USB3_GUCTL_DTFT(_x)			((_x) << 0)

/* TODO: Not finished */
#define EXYNOS_USB3_GBUSERRADDR_31_0	EXYNOS_USB3_REG(0xC130)
#define EXYNOS_USB3_GBUSERRADDR_63_32	EXYNOS_USB3_REG(0xC134)
#define EXYNOS_USB3_GPRTBIMAP_31_0	EXYNOS_USB3_REG(0xC138)
#define EXYNOS_USB3_GPRTBIMAP_63_32	EXYNOS_USB3_REG(0xC13C)

#define EXYNOS_USB3_GHWPARAMS0		EXYNOS_USB3_REG(0xC140)
#define EXYNOS_USB3_GHWPARAMS1		EXYNOS_USB3_REG(0xC144)
#define EXYNOS_USB3_GHWPARAMS2		EXYNOS_USB3_REG(0xC148)
#define EXYNOS_USB3_GHWPARAMS3		EXYNOS_USB3_REG(0xC14C)
#define EXYNOS_USB3_GHWPARAMS4		EXYNOS_USB3_REG(0xC150)
#define EXYNOS_USB3_GHWPARAMS5		EXYNOS_USB3_REG(0xC154)
#define EXYNOS_USB3_GHWPARAMS6		EXYNOS_USB3_REG(0xC158)
#define EXYNOS_USB3_GHWPARAMS7		EXYNOS_USB3_REG(0xC15C)

#define EXYNOS_USB3_GDBGFIFOSPACE	EXYNOS_USB3_REG(0xC160)
#define EXYNOS_USB3_GDBGLTSSM		EXYNOS_USB3_REG(0xC164)

#define EXYNOS_USB3_GDBGLSPMUX		EXYNOS_USB3_REG(0xC170)
#define EXYNOS_USB3_GDBGLSP		EXYNOS_USB3_REG(0xC174)
#define EXYNOS_USB3_GDBGEPINFO0		EXYNOS_USB3_REG(0xC178)
#define EXYNOS_USB3_GDBGEPINFO1		EXYNOS_USB3_REG(0xC17C)

#define EXYNOS_USB3_GPRTBIMAP_HS_31_0	EXYNOS_USB3_REG(0xC180)
#define EXYNOS_USB3_GPRTBIMAP_HS_63_32	EXYNOS_USB3_REG(0xC184)
#define EXYNOS_USB3_GPRTBIMAP_FS_31_0	EXYNOS_USB3_REG(0xC188)
#define EXYNOS_USB3_GPRTBIMAP_FS_63_32	EXYNOS_USB3_REG(0xC18C)
/****************/

#define EXYNOS_USB3_GUSB2PHYCFG(_a)	EXYNOS_USB3_REG(0xC200 + ((_a) * 0x04))
#define EXYNOS_USB3_GUSB2PHYCFGx_PHYSoftRst		(1 << 31)

#define EXYNOS_USB3_GUSB2PHYCFGx_PhyIntrNum_MASK	(0x3f << 19)
#define EXYNOS_USB3_GUSB2PHYCFGx_PhyIntrNum_SHIFT	(19)
#define EXYNOS_USB3_GUSB2PHYCFGx_PhyIntrNum_LIMIT	(0x3f)
#define EXYNOS_USB3_GUSB2PHYCFGx_PhyIntrNum(_x)		((_x) << 19)

#define EXYNOS_USB3_GUSB2PHYCFGx_ULPIExtVbusIndicator	(1 << 18)
#define EXYNOS_USB3_GUSB2PHYCFGx_ULPIExtVbusDrv		(1 << 17)
#define EXYNOS_USB3_GUSB2PHYCFGx_ULPIClkSusM		(1 << 16)
#define EXYNOS_USB3_GUSB2PHYCFGx_ULPIAutoRes		(1 << 15)
#define EXYNOS_USB3_GUSB2PHYCFGx_PhyLPwrClkSel		(1 << 14)

#define EXYNOS_USB3_GUSB2PHYCFGx_USBTrdTim_MASK		(0xf << 10)
#define EXYNOS_USB3_GUSB2PHYCFGx_USBTrdTim_SHIFT	(10)
#define EXYNOS_USB3_GUSB2PHYCFGx_USBTrdTim_LIMIT	(0xf)
#define EXYNOS_USB3_GUSB2PHYCFGx_USBTrdTim(_x)		((_x) << 10)

#define EXYNOS_USB3_GUSB2PHYCFGx_EnblSlpM		(1 << 8)
#define EXYNOS_USB3_GUSB2PHYCFGx_PHYSel			(1 << 7)
#define EXYNOS_USB3_GUSB2PHYCFGx_SusPHY			(1 << 6)
#define EXYNOS_USB3_GUSB2PHYCFGx_FSIntf			(1 << 5)
#define EXYNOS_USB3_GUSB2PHYCFGx_ULPI_UTMI_Sel		(1 << 4)
#define EXYNOS_USB3_GUSB2PHYCFGx_PHYIf			(1 << 3)

#define EXYNOS_USB3_GUSB2PHYCFGx_TOutCal_MASK		(0x7 << 0)
#define EXYNOS_USB3_GUSB2PHYCFGx_TOutCal_SHIFT		(0)
#define EXYNOS_USB3_GUSB2PHYCFGx_TOutCal_LIMIT		(0x7)
#define EXYNOS_USB3_GUSB2PHYCFGx_TOutCal(_x)		((_x) << 0)


/* Reserved for future use */
#define EXYNOS_USB3_GUSB2I2CCTL(_a)	EXYNOS_USB3_REG(0xC240 + ((_a) * 0x04))


#define EXYNOS_USB3_GUSB2PHYACC(_a)	EXYNOS_USB3_REG(0xC280 + ((_a) * 0x04))
#define EXYNOS_USB3_GUSB2PHYACCx_DisUlpiDrvr		(1 << 26)
#define EXYNOS_USB3_GUSB2PHYACCx_NewRegReq		(1 << 25)
#define EXYNOS_USB3_GUSB2PHYACCx_VStsDone		(1 << 24)
#define EXYNOS_USB3_GUSB2PHYACCx_VStsBsy		(1 << 23)
#define EXYNOS_USB3_GUSB2PHYACCx_RegWr			(1 << 22)

#define EXYNOS_USB3_GUSB2PHYACCx_RegAddr_MASK		(0x3f << 16)
#define EXYNOS_USB3_GUSB2PHYACCx_RegAddr_SHIFT		(16)
#define EXYNOS_USB3_GUSB2PHYACCx_RegAddr_LIMIT		(0x3f)
#define EXYNOS_USB3_GUSB2PHYACCx_RegAddr(_x)		((_x) << 16)

/* Next 2 fields are overlaping. Is it error in user manual? */
#define EXYNOS_USB3_GUSB2PHYACCx_VCtrl_MASK		(0xff << 8)
#define EXYNOS_USB3_GUSB2PHYACCx_VCtrl_SHIFT		(8)
#define EXYNOS_USB3_GUSB2PHYACCx_VCtrl_LIMIT		(0xff)
#define EXYNOS_USB3_GUSB2PHYACCx_VCtrl(_x)		((_x) << 8)

#define EXYNOS_USB3_GUSB2PHYACCx_ExtRegAddr_MASK	(0x3f << 8)
#define EXYNOS_USB3_GUSB2PHYACCx_ExtRegAddr_SHIFT	(8)
#define EXYNOS_USB3_GUSB2PHYACCx_ExtRegAddr_LIMIT	(0x3f)
#define EXYNOS_USB3_GUSB2PHYACCx_ExtRegAddr(_x)		((_x) << 8)

#define EXYNOS_USB3_GUSB2PHYACCx_RegData_MASK		(0xff << 0)
#define EXYNOS_USB3_GUSB2PHYACCx_RegData_SHIFT		(0)
#define EXYNOS_USB3_GUSB2PHYACCx_RegData_LIMIT		(0xff)
#define EXYNOS_USB3_GUSB2PHYACCx_RegData(_x)		((_x) << 0)


#define EXYNOS_USB3_GUSB3PIPECTL(_a)	EXYNOS_USB3_REG(0xC2C0 + ((_a) * 0x04))
#define EXYNOS_USB3_GUSB3PIPECTLx_PHYSoftRst		(1 << 31)
#define EXYNOS_USB3_GUSB3PIPECTLx_request_p1p2p3	(1 << 24)
#define EXYNOS_USB3_GUSB3PIPECTLx_StartRxdetU3RxDet	(1 << 23)
#define EXYNOS_USB3_GUSB3PIPECTLx_DisRxDetU3RxDet	(1 << 22)

#define EXYNOS_USB3_GUSB3PIPECTLx_delay_p1p2p3_MASK	(0x7 << 19)
#define EXYNOS_USB3_GUSB3PIPECTLx_delay_p1p2p3_SHIFT	(19)
#define EXYNOS_USB3_GUSB3PIPECTLx_delay_p1p2p3_LIMIT	(0x7)
#define EXYNOS_USB3_GUSB3PIPECTLx_delay_p1p2p3(_x)	((_x) << 19)

/* TODO: Check naming for the next 2 fields */
#define EXYNOS_USB3_GUSB3PIPECTLx_delay_phy_pwr_p1p2p3	(1 << 18)
#define EXYNOS_USB3_GUSB3PIPECTLx_SuspSSPhy		(1 << 17)

#define EXYNOS_USB3_GUSB3PIPECTLx_DatWidth_MASK		(0x3 << 15)
#define EXYNOS_USB3_GUSB3PIPECTLx_DatWidth_SHIFT	(15)
#define EXYNOS_USB3_GUSB3PIPECTLx_DatWidth_LIMIT	(0x3)
#define EXYNOS_USB3_GUSB3PIPECTLx_DatWidth(_x)		((_x) << 15)

#define EXYNOS_USB3_GUSB3PIPECTLx_AbortRxDetInU2	(1 << 14)
#define EXYNOS_USB3_GUSB3PIPECTLx_SkipRxDet		(1 << 13)
#define EXYNOS_USB3_GUSB3PIPECTLx_LFPSP0Algn		(1 << 12)
#define EXYNOS_USB3_GUSB3PIPECTLx_P3P2TranOK		(1 << 11)
#define EXYNOS_USB3_GUSB3PIPECTLx_LFPSFilt		(1 << 9)
#define EXYNOS_USB3_GUSB3PIPECTLx_TxSwing		(1 << 6)

#define EXYNOS_USB3_GUSB3PIPECTLx_TxMargin_MASK		(0x7 << 3)
#define EXYNOS_USB3_GUSB3PIPECTLx_TxMargin_SHIFT	(3)
#define EXYNOS_USB3_GUSB3PIPECTLx_TxMargin_LIMIT	(0x7)
#define EXYNOS_USB3_GUSB3PIPECTLx_TxMargin(_x)		((_x) << 3)

#define EXYNOS_USB3_GUSB3PIPECTLx_TxDeemphasis_MASK	(0x3 << 1)
#define EXYNOS_USB3_GUSB3PIPECTLx_TxDeemphasis_SHIFT	(1)
#define EXYNOS_USB3_GUSB3PIPECTLx_TxDeemphasis_LIMIT	(0x3)
#define EXYNOS_USB3_GUSB3PIPECTLx_TxDeemphasis(_x)	((_x) << 1)

#define EXYNOS_USB3_GUSB3PIPECTLx_ElasticBufferMode	(1 << 0)


#define EXYNOS_USB3_GTXFIFOSIZ(_a)	EXYNOS_USB3_REG(0xC300 + ((_a) * 0x04))

#define EXYNOS_USB3_GTXFIFOSIZx_TxFStAddr_n_MASK	(0xffff << 16)
#define EXYNOS_USB3_GTXFIFOSIZx_TxFStAddr_n_SHIFT	(16)
#define EXYNOS_USB3_GTXFIFOSIZx_TxFStAddr_n_LIMIT	(0xffff)
#define EXYNOS_USB3_GTXFIFOSIZx_TxFStAddr_n(_x)		((_x) << 16)

#define EXYNOS_USB3_GTXFIFOSIZx_TxFDep_n_MASK		(0xffff << 0)
#define EXYNOS_USB3_GTXFIFOSIZx_TxFDep_n_SHIFT		(0)
#define EXYNOS_USB3_GTXFIFOSIZx_TxFDep_n_LIMIT		(0xffff)
#define EXYNOS_USB3_GTXFIFOSIZx_TxFDep_n(_x)		((_x) << 0)


#define EXYNOS_USB3_GRXFIFOSIZ(_a)	EXYNOS_USB3_REG(0xC380 + ((_a) * 0x04))

#define EXYNOS_USB3_GRXFIFOSIZx_RxFStAddr_n_MASK	(0xffff << 16)
#define EXYNOS_USB3_GRXFIFOSIZx_RxFStAddr_n_SHIFT	(16)
#define EXYNOS_USB3_GRXFIFOSIZx_RxFStAddr_n_LIMIT	(0xffff)
#define EXYNOS_USB3_GRXFIFOSIZx_RxFStAddr_n(_x)		((_x) << 16)

#define EXYNOS_USB3_GRXFIFOSIZx_RxFDep_n_MASK		(0xffff << 0)
#define EXYNOS_USB3_GRXFIFOSIZx_RxFDep_n_SHIFT		(0)
#define EXYNOS_USB3_GRXFIFOSIZx_RxFDep_n_LIMIT		(0xffff)
#define EXYNOS_USB3_GRXFIFOSIZx_RxFDep_n(_x)		((_x) << 0)


#define EXYNOS_USB3_GEVNTADR_31_0(_a)	EXYNOS_USB3_REG(0xC400 + ((_a) * 0x10))
#define EXYNOS_USB3_GEVNTADR_63_32(_a)	EXYNOS_USB3_REG(0xC404 + ((_a) * 0x10))


#define EXYNOS_USB3_GEVNTSIZ(_a)	EXYNOS_USB3_REG(0xC408 + ((_a) * 0x10))
#define EXYNOS_USB3_GEVNTSIZx_EvntIntMask		(1 << 31)

#define EXYNOS_USB3_GEVNTSIZx_EVNTSiz_MASK		(0xffff << 0)
#define EXYNOS_USB3_GEVNTSIZx_EVNTSiz_SHIFT		(0)
#define EXYNOS_USB3_GEVNTSIZx_EVNTSiz_LIMIT		(0xffff)
#define EXYNOS_USB3_GEVNTSIZx_EVNTSiz(x)		((_x) << 0)


#define EXYNOS_USB3_GEVNTCOUNT(_a)	EXYNOS_USB3_REG(0xC40C + ((_a) * 0x10))

#define EXYNOS_USB3_GEVNTCOUNTx_EVNTCount_MASK		(0xffff << 0)
#define EXYNOS_USB3_GEVNTCOUNTx_EVNTCount_SHIFT		(0)
#define EXYNOS_USB3_GEVNTCOUNTx_EVNTCount_LIMIT		(0xffff)
#define EXYNOS_USB3_GEVNTCOUNTx_EVNTCount(_x)		((_x) << 0)

/* Event Buffer Content for Device Endpoint-Specific Events (DEPEVT) */
#define EXYNOS_USB3_DEPEVT_EventParam_MASK		(0xffff << 16)
#define EXYNOS_USB3_DEPEVT_EventParam_SHIFT		(16)
#define EXYNOS_USB3_DEPEVT_EventParam_LIMIT		(0xffff)
#define EXYNOS_USB3_DEPEVT_EventParam(_x)		((_x) << 16)

#define EXYNOS_USB3_DEPEVT_EventStatus_MASK		(0xf << 12)
#define EXYNOS_USB3_DEPEVT_EventStatus_SHIFT		(12)
#define EXYNOS_USB3_DEPEVT_EventStatus_LIMIT		(0xf)

#define EXYNOS_USB3_DEPEVT_EVENT_MASK			(0xf << 6)
#define EXYNOS_USB3_DEPEVT_EVENT_SHIFT			(6)
#define EXYNOS_USB3_DEPEVT_EVENT_LIMIT			(0xf)
#define EXYNOS_USB3_DEPEVT_EVENT_EPCmdCmplt		(7 << 6)
#define EXYNOS_USB3_DEPEVT_EVENT_StreamEvt		(6 << 6)
#define EXYNOS_USB3_DEPEVT_EVENT_RxTxfifoEvt		(4 << 6)
#define EXYNOS_USB3_DEPEVT_EVENT_XferNotReady		(3 << 6)
#define EXYNOS_USB3_DEPEVT_EVENT_XferInProgress		(2 << 6)
#define EXYNOS_USB3_DEPEVT_EVENT_XferComplete		(1 << 6)

#define EXYNOS_USB3_DEPEVT_EPNUM_MASK			(0x1f << 1)
#define EXYNOS_USB3_DEPEVT_EPNUM_SHIFT			(1)
#define EXYNOS_USB3_DEPEVT_EPNUM_LIMIT			(0x1f)
#define EXYNOS_USB3_DEPEVT_EPNUM(_x)			((_x) << 1)

/* Event Buffer Content for Device-Specific Events (DEVT) */
#define EXYNOS_USB3_DEVT_EventParam_MASK		(0xf << 16)
#define EXYNOS_USB3_DEVT_EventParam_SHIFT		(16)
#define EXYNOS_USB3_DEVT_EventParam_LIMIT		(0xf)
#define EXYNOS_USB3_DEVT_EventParam_SS			(1 << 20)
#define EXYNOS_USB3_DEVT_EventParam(_x)			((_x) << 16)

#define EXYNOS_USB3_DEVT_EVENT_MASK			(0xf << 8)
#define EXYNOS_USB3_DEVT_EVENT_SHIFT			(8)
#define EXYNOS_USB3_DEVT_EVENT_LIMIT			(0xf)
#define EXYNOS_USB3_DEVT_EVENT_VndrDevTstRcved		(12 << 8)
#define EXYNOS_USB3_DEVT_EVENT_EvntOverflow		(11 << 8)
#define EXYNOS_USB3_DEVT_EVENT_CmdCmplt			(10 << 8)
#define EXYNOS_USB3_DEVT_EVENT_ErrticErr		(9 << 8)
#define EXYNOS_USB3_DEVT_EVENT_Sof			(7 << 8)
#define EXYNOS_USB3_DEVT_EVENT_EOPF			(6 << 8)
#define EXYNOS_USB3_DEVT_EVENT_WkUpEvt			(4 << 8)
#define EXYNOS_USB3_DEVT_EVENT_ULStChng			(3 << 8)
#define EXYNOS_USB3_DEVT_EVENT_ConnectDone		(2 << 8)
#define EXYNOS_USB3_DEVT_EVENT_USBRst			(1 << 8)
#define EXYNOS_USB3_DEVT_EVENT_DisconnEvt		(0 << 8)


#define EXYNOS_USB3_GHWPARAMS8		EXYNOS_USB3_REG(0xC600)


/* Device registers */
#define EXYNOS_USB3_DCFG		EXYNOS_USB3_REG(0xC700)
#define EXYNOS_USB3_DCFG_IgnoreStreamPP			(1 << 23)
#define EXYNOS_USB3_DCFG_LPMCap				(1 << 22)

#define EXYNOS_USB3_DCFG_NumP_MASK			(0x1f << 17)
#define EXYNOS_USB3_DCFG_NumP_SHIFT			(17)
#define EXYNOS_USB3_DCFG_NumP_LIMIT			(0x1f)
#define EXYNOS_USB3_DCFG_NumP(_x)			((_x) << 17)

#define EXYNOS_USB3_DCFG_IntrNum_MASK			(0x1f << 12)
#define EXYNOS_USB3_DCFG_IntrNum_SHIFT			(12)
#define EXYNOS_USB3_DCFG_IntrNum_LIMIT			(0x1f)
#define EXYNOS_USB3_DCFG_IntrNum(_x)			(0x1f << 12)

#define EXYNOS_USB3_DCFG_PerFrInt_MASK			(0x3 << 10)
#define EXYNOS_USB3_DCFG_PerFrInt_SHIFT			(10)
#define EXYNOS_USB3_DCFG_PerFrInt_LIMIT			(0x3)
#define EXYNOS_USB3_DCFG_PerFrInt(_x)			((_x) << 10)

#define EXYNOS_USB3_DCFG_DevAddr_MASK			(0x7f << 3)
#define EXYNOS_USB3_DCFG_DevAddr_SHIFT			(3)
#define EXYNOS_USB3_DCFG_DevAddr_LIMIT			(0x7f)
#define EXYNOS_USB3_DCFG_DevAddr(_x)			((_x) << 3)

#define EXYNOS_USB3_DCFG_DevSpd_MASK			(0x7 << 0)
#define EXYNOS_USB3_DCFG_DevSpd_SHIFT			(0)
#define EXYNOS_USB3_DCFG_DevSpd_LIMIT			(0x7)
#define EXYNOS_USB3_DCFG_DevSpd(_x)			((_x) << 0)


#define EXYNOS_USB3_DCTL		EXYNOS_USB3_REG(0xC704)
#define EXYNOS_USB3_DCTL_Run_Stop			(1 << 31)
#define EXYNOS_USB3_DCTL_CSftRst			(1 << 30)
#define EXYNOS_USB3_DCTL_LSftRst			(1 << 29)

#define EXYNOS_USB3_DCTL_HIRD_Thres_MASK		(0x1f << 24)
#define EXYNOS_USB3_DCTL_HIRD_Thres_SHIFT		(24)
#define EXYNOS_USB3_DCTL_HIRD_Thres_LIMIT		(0x1f)
#define EXYNOS_USB3_DCTL_HIRD_Thres(_x)			((_x) << 24)

#define EXYNOS_USB3_DCTL_AppL1Res			(1 << 23)

#define EXYNOS_USB3_DCTL_TrgtULSt_MASK			(0xf << 17)
#define EXYNOS_USB3_DCTL_TrgtULSt_SHIFT			(17)
#define EXYNOS_USB3_DCTL_TrgtULSt_LIMIT			(0xf)
#define EXYNOS_USB3_DCTL_TrgtULSt(_x)			((_x) << 17)

#define EXYNOS_USB3_DCTL_InitU2Ena			(1 << 12)
#define EXYNOS_USB3_DCTL_AcceptU2Ena			(1 << 11)
#define EXYNOS_USB3_DCTL_InitU1Ena			(1 << 10)
#define EXYNOS_USB3_DCTL_AcceptU1Ena			(1 << 9)

#define EXYNOS_USB3_DCTL_ULStChngReq_MASK		(0xf << 5)
#define EXYNOS_USB3_DCTL_ULStChngReq_SHIFT		(5)
#define EXYNOS_USB3_DCTL_ULStChngReq_LIMIT		(0xf)
#define EXYNOS_USB3_DCTL_ULStChngReq(_x)		((_x) << 5)

#define EXYNOS_USB3_DCTL_TstCtl_MASK			(0xf << 1)
#define EXYNOS_USB3_DCTL_TstCtl_SHIFT			(1)
#define EXYNOS_USB3_DCTL_TstCtl_LIMIT			(0xf)
#define EXYNOS_USB3_DCTL_TstCtl(_x)			((_x) << 1)


#define EXYNOS_USB3_DEVTEN		EXYNOS_USB3_REG(0xC708)
#define EXYNOS_USB3_DEVTEN_VndrDevTstRcvedEn		(1 << 12)
#define EXYNOS_USB3_DEVTEN_EvntOverflowEn		(1 << 11)
#define EXYNOS_USB3_DEVTEN_CmdCmpltEn			(1 << 10)
#define EXYNOS_USB3_DEVTEN_ErrticErrEn			(1 << 9)
#define EXYNOS_USB3_DEVTEN_SofEn			(1 << 7)
#define EXYNOS_USB3_DEVTEN_EOPFEn			(1 << 6)
#define EXYNOS_USB3_DEVTEN_WkUpEvtEn			(1 << 4)
#define EXYNOS_USB3_DEVTEN_ULStCngEn			(1 << 3)
#define EXYNOS_USB3_DEVTEN_ConnectDoneEn		(1 << 2)
#define EXYNOS_USB3_DEVTEN_USBRstEn			(1 << 1)
#define EXYNOS_USB3_DEVTEN_DisconnEvtEn			(1 << 0)


#define EXYNOS_USB3_DSTS		EXYNOS_USB3_REG(0xC70C)
#define EXYNOS_USB3_DSTS_PwrUpReq			(1 << 24)
#define EXYNOS_USB3_DSTS_CoreIdle			(1 << 23)
#define EXYNOS_USB3_DSTS_DevCtrlHlt			(1 << 22)

#define EXYNOS_USB3_DSTS_USBLnkSt_MASK			(0xf << 18)
#define EXYNOS_USB3_DSTS_USBLnkSt_SHIFT			(18)
#define EXYNOS_USB3_DSTS_USBLnkSt_LIMIT			(0xf)
#define EXYNOS_USB3_DSTS_USBLnkSt(_x)			((_x) << 18)
#define EXYNOS_USB3_LnkSt_LPBK				(0xb)
#define EXYNOS_USB3_LnkSt_CMPLY				(0xa)
#define EXYNOS_USB3_LnkSt_HRESET			(0x9)
#define EXYNOS_USB3_LnkSt_RECOV				(0x8)
#define EXYNOS_USB3_LnkSt_POLL				(0x7)
#define EXYNOS_USB3_LnkSt_SS_INACT			(0x6)
#define EXYNOS_USB3_LnkSt_RX_DET			(0x5)
#define EXYNOS_USB3_LnkSt_SS_DIS			(0x4)
#define EXYNOS_USB3_LnkSt_U3				(0x3)
#define EXYNOS_USB3_LnkSt_U2				(0x2)
#define EXYNOS_USB3_LnkSt_U1				(0x1)
#define EXYNOS_USB3_LnkSt_U0				(0x0)

#define EXYNOS_USB3_DSTS_RxFIFOEmpty			(1 << 17)

#define EXYNOS_USB3_DSTS_SOFFN_MASK			(0x3fff << 3)
#define EXYNOS_USB3_DSTS_SOFFN_SHIFT			(3)
#define EXYNOS_USB3_DSTS_SOFFN_LIMIT			(0x3fff)
#define EXYNOS_USB3_DSTS_SOFFN(_x)			((_x) << 3)

#define EXYNOS_USB3_DSTS_ConnectSpd_MASK		(0x7 << 0)
#define EXYNOS_USB3_DSTS_ConnectSpd_SHIFT		(0)
#define EXYNOS_USB3_DSTS_ConnectSpd_LIMIT		(0x7)
#define EXYNOS_USB3_DSTS_ConnectSpd(_x)			((_x) << 0)


#define EXYNOS_USB3_DGCMDPAR		EXYNOS_USB3_REG(0xC710)


#define EXYNOS_USB3_DGCMD		EXYNOS_USB3_REG(0xC714)
#define EXYNOS_USB3_DGCMD_CmdStatus			(1 << 15)
#define EXYNOS_USB3_DGCMD_CmdAct			(1 << 10)
#define EXYNOS_USB3_DGCMD_CmdIOC			(1 << 8)

#define EXYNOS_USB3_DGCMD_CmdTyp_MASK			(0xff << 0)
#define EXYNOS_USB3_DGCMD_CmdTyp_SHIFT			(0)
#define EXYNOS_USB3_DGCMD_CmdTyp_LIMIT			(0xff)
#define EXYNOS_USB3_DGCMD_CmdTyp(_x)			((_x) << 0)
/* TODO: add device generic command descriptions */

/* TODO: not finished */
#define EXYNOS_USB3_DALEPENA		EXYNOS_USB3_REG(0xC720)


#define EXYNOS_USB3_DEPCMDPAR2(_a)	EXYNOS_USB3_REG(0xC800 + ((_a) * 0x10))

#define EXYNOS_USB3_DEPCMDPAR1(_a)	EXYNOS_USB3_REG(0xC804 + ((_a) * 0x10))
/* DEPCFG command parameter 1 */
#define EXYNOS_USB3_DEPCMDPAR1x_FIFO_based		(1 << 31)
#define EXYNOS_USB3_DEPCMDPAR1x_BULK_based		(1 << 30)

#define EXYNOS_USB3_DEPCMDPAR1x_EpNum_MASK		(0xf << 26)
#define EXYNOS_USB3_DEPCMDPAR1x_EpNum_SHIFT		(26)
#define EXYNOS_USB3_DEPCMDPAR1x_EpNum_LIMIT		(0xf)
#define EXYNOS_USB3_DEPCMDPAR1x_EpNum(_x)		((_x) << 26)

#define EXYNOS_USB3_DEPCMDPAR1x_EpDir			(1 << 25)
#define EXYNOS_USB3_DEPCMDPAR1x_StrmCap			(1 << 24)

#define EXYNOS_USB3_DEPCMDPAR1x_bInterval_m1_MASK	(0xff << 16)
#define EXYNOS_USB3_DEPCMDPAR1x_bInterval_m1_SHIFT	(16)
#define EXYNOS_USB3_DEPCMDPAR1x_bInterval_m1_LIMIT	(0xff)
#define EXYNOS_USB3_DEPCMDPAR1x_bInterval_m1(_x)	((_x) << 16)

#define EXYNOS_USB3_DEPCMDPAR1x_StreamEvtEn		(1 << 13)
#define EXYNOS_USB3_DEPCMDPAR1x_RxTxfifoEvtEn		(1 << 11)
#define EXYNOS_USB3_DEPCMDPAR1x_XferNRdyEn		(1 << 10)
#define EXYNOS_USB3_DEPCMDPAR1x_XferInProgEn		(1 << 9)
#define EXYNOS_USB3_DEPCMDPAR1x_XferCmplEn		(1 << 8)

#define EXYNOS_USB3_DEPCMDPAR1x_IntrNum_MASK		(0x1f << 0)
#define EXYNOS_USB3_DEPCMDPAR1x_IntrNum_SHIFT		(0)
#define EXYNOS_USB3_DEPCMDPAR1x_IntrNum_LIMIT		(0x1f)
#define EXYNOS_USB3_DEPCMDPAR1x_IntrNum(_x)		((_x) << 0)


#define EXYNOS_USB3_DEPCMDPAR0(_a)	EXYNOS_USB3_REG(0xC808 + ((_a) * 0x10))
/* DEPCFG command parameter 0 */
#define EXYNOS_USB3_DEPCMDPAR0x_IgnrSeqNum		(1 << 31)

#define EXYNOS_USB3_DEPCMDPAR0x_DataSeqNum_MASK		(0x1f << 26)
#define EXYNOS_USB3_DEPCMDPAR0x_DataSeqNum_SHIFT	(26)
#define EXYNOS_USB3_DEPCMDPAR0x_DataSeqNum_LIMIT	(0x1f)
#define EXYNOS_USB3_DEPCMDPAR0x_DataSeqNum(_x)		((_x) << 26)

#define EXYNOS_USB3_DEPCMDPAR0x_BrstSiz_MASK		(0xf << 22)
#define EXYNOS_USB3_DEPCMDPAR0x_BrstSiz_SHIFT		(22)
#define EXYNOS_USB3_DEPCMDPAR0x_BrstSiz_LIMIT		(0xf)
#define EXYNOS_USB3_DEPCMDPAR0x_BrstSiz(_x)		((_x) << 22)

#define EXYNOS_USB3_DEPCMDPAR0x_FIFONum_MASK		(0x1f << 17)
#define EXYNOS_USB3_DEPCMDPAR0x_FIFONum_SHIFT		(17)
#define EXYNOS_USB3_DEPCMDPAR0x_FIFONum_LIMIT		(0x1f)
#define EXYNOS_USB3_DEPCMDPAR0x_FIFONum(_x)		((_x) << 17)

#define EXYNOS_USB3_DEPCMDPAR0x_MPS_MASK		(0x7ff << 3)
#define EXYNOS_USB3_DEPCMDPAR0x_MPS_SHIFT		(3)
#define EXYNOS_USB3_DEPCMDPAR0x_MPS_LIMIT		(0x7ff)
#define EXYNOS_USB3_DEPCMDPAR0x_MPS(_x)			((_x) << 3)

#define EXYNOS_USB3_DEPCMDPAR0x_EPType_MASK		(0x3 << 1)
#define EXYNOS_USB3_DEPCMDPAR0x_EPType_SHIFT		(1)
#define EXYNOS_USB3_DEPCMDPAR0x_EPType_LIMIT		(0x3)
#define EXYNOS_USB3_DEPCMDPAR0x_EPType(_x)		((_x) << 1)

/* DEPXFERCFG command parameter 0 */
#define EXYNOS_USB3_DEPCMDPAR0x_NumXferRes_MASK		(0xff << 0)
#define EXYNOS_USB3_DEPCMDPAR0x_NumXferRes_SHIFT	(0)
#define EXYNOS_USB3_DEPCMDPAR0x_NumXferRes_LIMIT	(0xff)
#define EXYNOS_USB3_DEPCMDPAR0x_NumXferRes(_x)		((_x) << 0)


#define EXYNOS_USB3_DEPCMD(_a)		EXYNOS_USB3_REG(0xC80C + ((_a) * 0x10))

#define EXYNOS_USB3_DEPCMDx_CommandParam_MASK		(0xffff << 16)
#define EXYNOS_USB3_DEPCMDx_CommandParam_SHIFT		(16)
#define EXYNOS_USB3_DEPCMDx_CommandParam_LIMIT		(0xffff)
#define EXYNOS_USB3_DEPCMDx_CommandParam(_x)		((_x) << 16)

#define EXYNOS_USB3_DEPCMDx_EventParam_MASK		(0xffff << 16)
#define EXYNOS_USB3_DEPCMDx_EventParam_SHIFT		(16)
#define EXYNOS_USB3_DEPCMDx_EventParam_LIMIT		(0xffff)
#define EXYNOS_USB3_DEPCMDx_EventParam(_x)		((_x) << 16)

#define EXYNOS_USB3_DEPCMDx_XferRscIdx_LIMIT		(0x7f)

#define EXYNOS_USB3_DEPCMDx_CmdStatus_MASK		(0xf << 12)
#define EXYNOS_USB3_DEPCMDx_CmdStatus_SHIFT		(12)
#define EXYNOS_USB3_DEPCMDx_CmdStatus_LIMIT		(0xf)
#define EXYNOS_USB3_DEPCMDx_CmdStatus(_x)		((_x) << 12)

#define EXYNOS_USB3_DEPCMDx_HiPri_ForceRM		(1 << 11)
#define EXYNOS_USB3_DEPCMDx_CmdAct			(1 << 10)
#define EXYNOS_USB3_DEPCMDx_CmdIOC			(1 << 8)

#define EXYNOS_USB3_DEPCMDx_CmdTyp_MASK			(0xf << 0)
#define EXYNOS_USB3_DEPCMDx_CmdTyp_SHIFT		(0)
#define EXYNOS_USB3_DEPCMDx_CmdTyp_LIMIT		(0xf)
#define EXYNOS_USB3_DEPCMDx_CmdTyp(_x)			((_x) << 0)
/* Physical Endpoint commands */
#define EXYNOS_USB3_DEPCMDx_CmdTyp_DEPCFG		(0x1 << 0)
#define EXYNOS_USB3_DEPCMDx_CmdTyp_DEPXFERCFG		(0x2 << 0)
#define EXYNOS_USB3_DEPCMDx_CmdTyp_DEPGETDSEQ		(0x3 << 0)
#define EXYNOS_USB3_DEPCMDx_CmdTyp_DEPSSTALL		(0x4 << 0)
#define EXYNOS_USB3_DEPCMDx_CmdTyp_DEPCSTALL		(0x5 << 0)
#define EXYNOS_USB3_DEPCMDx_CmdTyp_DEPSTRTXFER		(0x6 << 0)
#define EXYNOS_USB3_DEPCMDx_CmdTyp_DEPUPDXFER		(0x7 << 0)
#define EXYNOS_USB3_DEPCMDx_CmdTyp_DEPENDXFER		(0x8 << 0)
#define EXYNOS_USB3_DEPCMDx_CmdTyp_DEPSTARTCFG		(0x9 << 0)


/* USB 2.0 OTG and Battery Charger registers */
#define EXYNOS_USB3_OCFG		EXYNOS_USB3_REG(0xCC00)
#define EXYNOS_USB3_OCFG_OTG_Version			(1 << 2)
#define EXYNOS_USB3_OCFG_HNPCap				(1 << 1)
#define EXYNOS_USB3_OCFG_SRPCap				(1 << 0)


#define EXYNOS_USB3_OCTL		EXYNOS_USB3_REG(0xCC04)
#define EXYNOS_USB3_OCTL_PeriMode			(1 << 6)
#define EXYNOS_USB3_OCTL_PrtPwrCtl			(1 << 5)
#define EXYNOS_USB3_OCTL_HNPReq				(1 << 4)
#define EXYNOS_USB3_OCTL_SesReq				(1 << 3)
#define EXYNOS_USB3_OCTL_TermSelDLPulse			(1 << 2)
#define EXYNOS_USB3_OCTL_DevSetHNPEn			(1 << 1)
#define EXYNOS_USB3_OCTL_HstSetHNPEn			(1 << 0)


#define EXYNOS_USB3_OEVT		EXYNOS_USB3_REG(0xCC08)
#define EXYNOS_USB3_OEVT_DeviceMode			(1 << 31)
#define EXYNOS_USB3_OEVT_OTGConIDStsChngEvnt		(1 << 24)
#define EXYNOS_USB3_OEVT_OTGADevBHostEndEvnt		(1 << 20)
#define EXYNOS_USB3_OEVT_OTGADevHostEvnt		(1 << 19)
#define EXYNOS_USB3_OEVT_OTGADevHNPChngEvnt		(1 << 18)
#define EXYNOS_USB3_OEVT_OTGADevSRPDetEvnt		(1 << 17)
#define EXYNOS_USB3_OEVT_OTGADevSessEndDetEvnt		(1 << 16)
#define EXYNOS_USB3_OEVT_OTGBDevBHostEndEvnt		(1 << 11)
#define EXYNOS_USB3_OEVT_OTGBDevHNPChngEvnt		(1 << 10)
#define EXYNOS_USB3_OEVT_OTGBDevSessVldDetEvnt		(1 << 9)
#define EXYNOS_USB3_OEVT_OTGBDevVBUSChngEvnt		(1 << 8)
#define EXYNOS_USB3_OEVT_BSesVld			(1 << 3)
#define EXYNOS_USB3_OEVT_HstNegSts			(1 << 2)
#define EXYNOS_USB3_OEVT_SesReqSts			(1 << 1)
#define EXYNOS_USB3_OEVT_OEVTError			(1 << 0)


#define EXYNOS_USB3_OEVTEN		EXYNOS_USB3_REG(0xCC0C)
#define EXYNOS_USB3_OEVTEN_OTGConIDStsChngEvntEn	(1 << 24)
#define EXYNOS_USB3_OEVTEN_OTGADevBHostEndEvntEn	(1 << 20)
#define EXYNOS_USB3_OEVTEN_OTGADevHostEvntEn		(1 << 19)
#define EXYNOS_USB3_OEVTEN_OTGADevHNPChngEvntEn		(1 << 18)
#define EXYNOS_USB3_OEVTEN_OTGADevSRPDetEvntEn		(1 << 17)
#define EXYNOS_USB3_OEVTEN_OTGADevSessEndDetEvntEn	(1 << 16)
#define EXYNOS_USB3_OEVTEN_OTGBDevBHostEndEvntEn	(1 << 11)
#define EXYNOS_USB3_OEVTEN_OTGBDevHNPChngEvntEn		(1 << 10)
#define EXYNOS_USB3_OEVTEN_OTGBDevSessVldDetEvntEn	(1 << 9)
#define EXYNOS_USB3_OEVTEN_OTGBDevVBUSChngEvntEn	(1 << 8)


#define EXYNOS_USB3_OSTS		EXYNOS_USB3_REG(0xCC10)

#define EXYNOS_USB3_OSTS_OTG_state_MASK			(0xf << 8)
#define EXYNOS_USB3_OSTS_OTG_state_SHIFT		(8)
#define EXYNOS_USB3_OSTS_OTG_state_LIMIT		(0xf)
#define EXYNOS_USB3_OSTS_OTG_state(_x)			((_x) << 8)

#define EXYNOS_USB3_OSTS_PeripheralState		(1 << 4)
#define EXYNOS_USB3_OSTS_xHCIPrtPower			(1 << 3)
#define EXYNOS_USB3_OSTS_BSesVld			(1 << 2)
#define EXYNOS_USB3_OSTS_VbusVld			(1 << 1)
#define EXYNOS_USB3_OSTS_ConIDSts			(1 << 0)


#define EXYNOS_USB3_ADPCFG		EXYNOS_USB3_REG(0xCC20)

#define EXYNOS_USB3_ADPCFG_PrbPer_MASK			(0x3 << 30)
#define EXYNOS_USB3_ADPCFG_PrbPer_SHIFT			(30)
#define EXYNOS_USB3_ADPCFG_PrbPer_LIMIT			(0x3)
#define EXYNOS_USB3_ADPCFG_PrbPer(_x)			((_x) << 30)

#define EXYNOS_USB3_ADPCFG_PrbDelta_MASK		(0x3 << 28)
#define EXYNOS_USB3_ADPCFG_PrbDelta_SHIFT		(28)
#define EXYNOS_USB3_ADPCFG_PrbDelta_LIMIT		(0x3)
#define EXYNOS_USB3_ADPCFG_PrbDelta(_x)			((_x) << 28)

#define EXYNOS_USB3_ADPCFG_PrbDschg_MASK		(0x3 << 26)
#define EXYNOS_USB3_ADPCFG_PrbDschg_SHIFT		(26)
#define EXYNOS_USB3_ADPCFG_PrbDschg_LIMIT		(0x3)
#define EXYNOS_USB3_ADPCFG_PrbDschg(_x)			((_x) << 26)


#define EXYNOS_USB3_ADPCTL		EXYNOS_USB3_REG(0xCC24)
#define EXYNOS_USB3_ADPCTL_EnaPrb			(1 << 28)
#define EXYNOS_USB3_ADPCTL_EnaSns			(1 << 27)
#define EXYNOS_USB3_ADPCTL_ADPEn			(1 << 26)
#define EXYNOS_USB3_ADPCTL_ADPRes			(1 << 25)
#define EXYNOS_USB3_ADPCTL_WB				(1 << 24)


#define EXYNOS_USB3_ADPEVT		EXYNOS_USB3_REG(0xCC28)
#define EXYNOS_USB3_ADPEVT_AdpPrbEvnt			(1 << 28)
#define EXYNOS_USB3_ADPEVT_AdpSnsEvnt			(1 << 27)
#define EXYNOS_USB3_ADPEVT_AdpTmoutEvnt			(1 << 26)
#define EXYNOS_USB3_ADPEVT_ADPRstCmpltEvnt		(1 << 25)

#define EXYNOS_USB3_ADPEVT_RTIM_MASK			(0x7ff << 0)
#define EXYNOS_USB3_ADPEVT_RTIM_SHIFT			(0)
#define EXYNOS_USB3_ADPEVT_RTIM_LIMIT			(0x7ff)
#define EXYNOS_USB3_ADPEVT_RTIM(_x)			((_x) << 0)


#define EXYNOS_USB3_ADPEVTEN		EXYNOS_USB3_REG(0xCC2C)
#define EXYNOS_USB3_ADPEVTEN_AdpPrbEvntEn		(1 << 28)
#define EXYNOS_USB3_ADPEVTEN_AdpSnsEvntEn		(1 << 27)
#define EXYNOS_USB3_ADPEVTEN_AdpTmoutEvntEn		(1 << 26)
#define EXYNOS_USB3_ADPEVTEN_ADPRstCmpltEvntEn		(1 << 25)


#define EXYNOS_USB3_BCFG		EXYNOS_USB3_REG(0xCC30)
#define EXYNOS_USB3_BCFG_IDDIG_SEL			(1 << 1)
#define EXYNOS_USB3_BCFG_CHIRP_EN			(1 << 0)


#define EXYNOS_USB3_BCEVT		EXYNOS_USB3_REG(0xCC38)
#define EXYNOS_USB3_BCEVT_MV_ChngEvnt			(1 << 24)

#define EXYNOS_USB3_BCEVT_MultValIdBc_MASK		(0x1f << 0)
#define EXYNOS_USB3_BCEVT_MultValIdBc_SHIFT		(0)
#define EXYNOS_USB3_BCEVT_MultValIdBc_LIMIT		(0x1f)
#define EXYNOS_USB3_BCEVT_MultValIdBc(_x)		((_x) << 0)


#define EXYNOS_USB3_BCEVTEN		EXYNOS_USB3_REG(0xCC3C)
#define EXYNOS_USB3_BCEVTEN_MV_ChngEvntEn		(1 << 24)

/* Transfer Request Block */
#define EXYNOS_USB3_TRB_TRBSTS_MASK			(0xf << 28)
#define EXYNOS_USB3_TRB_TRBSTS_SHIFT			(28)
#define EXYNOS_USB3_TRB_TRBSTS_LIMIT			(0xf)
#define EXYNOS_USB3_TRB_TRBSTS(_x)			((_x) << 28)

#define EXYNOS_USB3_TRB_PCM1_MASK			(0x3 << 24)
#define EXYNOS_USB3_TRB_PCM1_SHIFT			(24)
#define EXYNOS_USB3_TRB_PCM1_LIMIT			(0x3)
#define EXYNOS_USB3_TRB_PCM1(_x)			((_x) << 24)

#define EXYNOS_USB3_TRB_BUFSIZ_MASK			(0xffffff << 0)
#define EXYNOS_USB3_TRB_BUFSIZ_SHIFT			(0)
#define EXYNOS_USB3_TRB_BUFSIZ_LIMIT			(0xffffff)
#define EXYNOS_USB3_TRB_BUFSIZ(_x)			((_x) << 0)

#define EXYNOS_USB3_TRB_StreamID_SOFNumber_MASK		(0xffff << 14)
#define EXYNOS_USB3_TRB_StreamID_SOFNumber_SHIFT	(14)
#define EXYNOS_USB3_TRB_StreamID_SOFNumber_LIMIT	(0xffff)
#define EXYNOS_USB3_TRB_StreamID_SOFNumber(_x)		((_x) << 14)

#define EXYNOS_USB3_TRB_IOC				(1 << 11)
#define EXYNOS_USB3_TRB_ISP_IMI				(1 << 10)

#define EXYNOS_USB3_TRB_TRBCTL_MASK			(0x3f << 4)
#define EXYNOS_USB3_TRB_TRBCTL_SHIFT			(4)
#define EXYNOS_USB3_TRB_TRBCTL_LIMIT			(0x3f)
#define EXYNOS_USB3_TRB_TRBCTL(_x)			((_x) << 4)

#define EXYNOS_USB3_TRB_CSP				(1 << 3)
#define EXYNOS_USB3_TRB_CHN				(1 << 2)
#define EXYNOS_USB3_TRB_LST				(1 << 1)
#define EXYNOS_USB3_TRB_HWO				(1 << 0)

#endif /* __SAMSUNG_PLAT_REGS_USB3_EXYNOS_DRD_H */

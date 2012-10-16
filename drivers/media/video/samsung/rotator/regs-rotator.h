/* rotator/regs-rotator.h*/

#ifndef __REGS_ROT_H
#define __REGS_ROT_H __FILE__


/*************************************************************************
 *  * Macro part
*************************************************************************/
#define S5P_ROT_WIDTH(x)		((x) << 0)
#define S5P_ROT_HEIGHT(x)		((x) << 16)
#define S5P_ROT_SRC_FMT(x)		((x) << 8)
#define S5P_ROT_DEGREE(x)		((x) << 4)
#define S5P_ROT_FLIP(x)			((x) << 6)
#define S5P_ROT_LEFT(x)			((x) << 0)
#define S5P_ROT_TOP(x)			((x) << 16)
/*************************************************************************
 *  * Register part
*************************************************************************/
#define S5P_ROT(x)			((x))
#define S5P_ROT_CONFIG			S5P_ROT(0x00)
#define S5P_ROT_CTRL			S5P_ROT(0x10)
#define S5P_ROT_STATUS			S5P_ROT(0x20)
#define S5P_ROT_SRCBASEADDR0		S5P_ROT(0x30)
#define S5P_ROT_SRCBASEADDR1		S5P_ROT(0x34)
#define S5P_ROT_SRCBASEADDR2		S5P_ROT(0x38)
#define S5P_ROT_SRCIMGSIZE		S5P_ROT(0x3C)
#define S5P_ROT_SRC_XY			S5P_ROT(0x40)
#define S5P_ROT_SRCROTSIZE		S5P_ROT(0x44)
#define S5P_ROT_DSTBASEADDR0		S5P_ROT(0x50)
#define S5P_ROT_DSTBASEADDR1		S5P_ROT(0x54)
#define S5P_ROT_DSTBASEADDR2		S5P_ROT(0x58)
#define S5P_ROT_DSTIMGSIZE		S5P_ROT(0x5C)
#define S5P_ROT_DST_XY			S5P_ROT(0x60)

/*************************************************************************
 *  * Bit definition part
*************************************************************************/

#define S5P_ROT_CONFIG_ENABLE_INT 	(0x1 << 8)
#define S5P_ROT_STATREG_INT_PENDING 	(0x1 << 8)
#define S5P_ROT_CONTROL_PATWRITING	(1 << 16)
#define S5P_ROT_CTRL_START_ROTATE	(1)

#define S5P_ROT_CONFIG_STATUS_MASK		(0x3 << 0)
#define S5P_ROT_CTRL_INPUT_FMT_MASK		(0x7 << 8)
#define S5P_ROT_CTRL_DEGREE_MASK		(0x3 << 4)
#define S5P_ROT_STATREG_STATUS_IDLE		0
#define S5P_ROT_STATREG_STATUS_BUSY		2
#define S5P_ROT_STATREG_STATUS_MORE_BUSY	3
#endif /* __REGS_ROT_H */

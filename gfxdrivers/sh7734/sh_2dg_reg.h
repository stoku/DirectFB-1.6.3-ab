/*
 * Copyright (C) 2013 Sosuke Tokunaga <sosuke.tokunaga@courier-systems.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the Lisence, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be helpful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MARCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 *
 */

#ifndef __SH_2DG_REG_H__
#define __SH_2DG_REG_H__

/* register offsets */

#define SH_2DG_SCLR	(0x0000)
#define SH_2DG_SR	(0x0004)
#define SH_2DG_SRCR	(0x0008)
#define SH_2DG_IER	(0x000C)
#define SH_2DG_ICIDR	(0x0010)
#define SH_2DG_RTN0R	(0x0040)
#define SH_2DG_RTN1R	(0x0044)
#define SH_2DG_DLSAR	(0x0048)
#define SH_2DG_SSAR	(0x004C)
#define SH_2DG_RSAR	(0x0050)
#define SH_2DG_WSAR	(0x0054)
#define SH_2DG_SSTRR	(0x0058)
#define SH_2DG_DSTRR	(0x005C)
#define SH_2DG_ENDCVR	(0x0060)
#define SH_2DG_ADREXTR	(0x006C)
#define SH_2DG_STCR	(0x0080)
#define SH_2DG_DTCR	(0x0084)
#define SH_2DG_ALPHR	(0x0088)
#define SH_2DG_COFSR	(0x008C)
#define SH_2DG_RCLR	(0x00C0)
#define SH_2DG_CSTR	(0x00C4)
#define SH_2DG_CURR	(0x00C8)
#define SH_2DG_LCOR	(0x00CC)
#define SH_2DG_SCLMAR	(0x00D0)
#define SH_2DG_UCLMIR	(0x00D4)
#define SH_2DG_UCLMAR	(0x00D8)
#define SH_2DG_RUCLMIR	(0x00DC)
#define SH_2DG_RUCLMAR	(0x00E0)
#define SH_2DG_RCL2R	(0x00F0)
#define SH_2DG_POFSR	(0x00F8)

/* register values */

#define SH_2DG_SCLR_SRES	(0x01 << 31)
#define SH_2DG_SCLR_HRES	(0x01 << 30)
#define SH_2DG_SCLR_RS		(0x01 << 0)

#define SH_2DG_SR_VER		(0x0F << 28)
#define SH_2DG_SR_CER		(0x01 << 2)
#define SH_2DG_SR_INT		(0x01 << 1)
#define SH_2DG_SR_TRA		(0x01 << 0)

#define SH_2DG_SRCR_CECL	(SH_2DG_SR_CER)
#define SH_2DG_SRCR_INCL	(SH_2DG_SR_INT)
#define SH_2DG_SRCR_TRCL	(SH_2DG_SR_TRA)

#define SH_2DG_IER_CEE		(SH_2DG_SR_CER)
#define SH_2DG_IER_INE		(SH_2DG_SR_INT)
#define SH_2DG_IER_TRE		(SH_2DG_SR_TRA)

#define SH_2DG_ICIDR_ICID	(0xFF << 0)

#define SH_2DG_RTN0R_RTN0  	(0x1FFFFFFC)

#define SH_2DG_RTN1R_RTN1	(0x1FFFFFFC)

#define SH_2DG_DLSAR_DLSA	(0x1FFFFFF0)

#define SH_2DG_RSAR_RSA		(0x1FFFFFF0)

#define SH_2DG_WSAR_WSA		(0x1FFFFFF0)

#define SH_2DG_SSTRR_SSTR	(0x00001FF8)

#define SH_2DG_DSTRR_DSTR	(0x00001FC0)

#define SH_2DG_ENDCVR_LWSWAP	(0x01 << 3)
#define SH_2DG_ENDCVR_WSWAP	(0x01 << 2)
#define SH_2DG_ENDCVR_BYTESWAP	(0x01 << 1)
#define SH_2DG_ENDCVR_BITSWAP	(0x01 << 0)

#define SH_2DG_ADREXTR_ADREXT	(0xE0000000)

#define SH_2DG_STCR_STC1	(0x01 << 24)
#define SH_2DG_STCR_STC8	(0xFF << 16)
#define SH_2DG_STCR_STC16	(0xFFFF << 0)

#define SH_2DG_DTCR_DTC8	(0xFF << 16)
#define SH_2DG_DTCR_DTC16	(0xFFFF << 0)

#define SH_2DG_ALPHR_ALPH	(0x000000FC)

#define SH_2DG_COFSR_COR	(0x00F80000)
#define SH_2DG_COFSR_COG	(0x0000FE00)
#define SH_2DG_COFSR_COB	(0x000000F8)

#define SH_2DG_RCLR_STP		(0x01 << 25)
#define SH_2DG_RCLR_DTP		(0x01 << 24)
#define SH_2DG_RCLR_SPF		(0x01 << 21)
#define SH_2DG_RCLR_DPF		(0x01 << 20)
#define SH_2DG_RCLR_GBM		(0x01 << 18)
#define SH_2DG_RCLR_SAU		(0x01 << 17)
#define SH_2DG_RCLR_AVALUE	(0x01 << 16)
#define SH_2DG_RCLR_RESV	(0x01 << 2)
#define SH_2DG_RCLR_LPCE	(0x01 << 1)
#define SH_2DG_RCLR_COM		(0x01 << 0)

#define SH_2DG_CSTR_CST		(0x1FFFFFFC)

#define SH_2DG_CURR_XC		(0xFFFF << 16)
#define SH_2DG_CURR_YC		(0xFFFF << 0)

#define SH_2DG_LCOR_XO		(0xFFFF << 16)
#define SH_2DG_LCOR_YO		(0xFFFF << 0)

#define SH_2DG_SCLMAR_SXMAX	(0x0FFF << 16)
#define SH_2DG_SCLMAR_SYMAX	(0x0FFF << 0)

#define SH_2DG_UCLMIR_UXMIN	(0x0FFF << 16)
#define SH_2DG_UCLMIR_UYMIN	(0x0FFF << 0)

#define SH_2DG_UCLMAR_UXMAX	(0x0FFF << 16)
#define SH_2DG_UCLMAR_UYMAX	(0x0FFF << 0)

#define SH_2DG_RUCLMIR_RUXMIN	(0x0FFF << 16)
#define SH_2DG_RUCLMIR_RUYMIN	(0x0FFF << 0)

#define SH_2DG_RUCLMAR_RUXMAX	(0x0FFF << 16)
#define SH_2DG_RUCLMAR_RUYMAX	(0x0FFF << 0)

#define SH_2DG_RCL2R_DAE	(0x01 << 21)
#define SH_2DG_RCL2R_PSTYLE	(0x01 << 20)
#define SH_2DG_RCL2R_PXSIZE8	(0x00 << 18)
#define SH_2DG_RCL2R_PXSIZE16	(0x01 << 18)
#define SH_2DG_RCL2R_PXSIZE32	(0x10 << 18)
#define SH_2DG_RCL2R_PXSIZE64	(0x11 << 18)
#define SH_2DG_RCL2R_PYSIZE8	(0x00 << 16)
#define SH_2DG_RCL2R_PYSIZE16	(0x01 << 16)
#define SH_2DG_RCL2R_PYSIZE32	(0x10 << 16)
#define SH_2DG_RCL2R_PYSIZE64	(0x11 << 16)

#define SH_2DG_POFSR_POFSX	(0x0FFF << 16)
#define SH_2DG_POFSR_POFSY	(0x0FFF << 0)

/* commands */
#define SH_2DG_WPR		(0x18000000)
#define SH_2DG_NOP		(0x08000000)
#define SH_2DG_INT(n)		(0x01008000 | (n))
#define SH_2DG_TRAP		(0x00000000)
#define SH_2DG_SYNC(mode)	(0x12000000 | (mode))
#define SH_2DG_POLYGON4A(mode)	(0x82000000 | ((mode) & 0x3BFF))
#define SH_2DG_POLYGON4C(mode)	(0x80000000 | ((mode) & 0x323E))
#define SH_2DG_LINEC(mode)	(0xB0000000 | ((mode) & 0x361E))
#define SH_2DG_FTRAPC(mode)	(0xD0000020 | ((mode) & 0x3618))
#define SH_2DG_CLRWC(mode)	(0xE0000020 | ((mode) & 0x3000))
#define SH_2DG_BITBLTA(mode)	(0xA2000000 | ((mode) & 0x3FFF))
#define SH_2DG_BITBLTC(mode)	(0xA0000000 | ((mode) & 0x361E))
#define SH_2DG_MOVE		(0x48000000)
#define SH_2DG_LCOFS		(0x40000000)

#define SH_2DG_SYNC_WCLR	(0x01 << 9)
#define SH_2DG_SYNC_WFLSH	(0x01 << 8)
#define SH_2DG_SYNC_TCLR	(0x01 << 4)
#define SH_2DG_SYNC_DCLR	(0x01 << 1)
#define SH_2DG_SYNC_DFLSH	(0x01 << 0)

#define SH_2DG_DRAWMODE_CLIP	(0x01 << 13)
#define SH_2DG_DRAWMODE_RCLIP	(0x01 << 12)
#define SH_2DG_DRAWMODE_STRANS	(0x01 << 11)
#define SH_2DG_DRAWMODE_DTRANS	(0x01 << 10)
#define SH_2DG_DRAWMODE_LINKE	(0x01 << 10)
#define SH_2DG_DRAWMODE_WORK	(0x01 <<  9)
#define SH_2DG_DRAWMODE_LREL	(0x01 <<  9)
#define SH_2DG_DRAWMODE_SS	(0x01 <<  8)
#define SH_2DG_DRAWMODE_REL	(0x01 <<  7)
#define SH_2DG_DRAWMODE_STYLE	(0x01 <<  6)
#define SH_2DG_DRAWMODE_SRCDIRX	(0x01 <<  6)
#define SH_2DG_DRAWMODE_BLKE	(0x01 <<  5)
#define SH_2DG_DRAWMODE_SRCDIRY	(0x01 <<  5)
#define SH_2DG_DRAWMODE_NET	(0x01 <<  4)
#define SH_2DG_DRAWMODE_EDG	(0x01 <<  4)
#define SH_2DG_DRAWMODE_DSTDIRX	(0x01 <<  4)
#define SH_2DG_DRAWMODE_EOS	(0x01 <<  3)
#define SH_2DG_DRAWMODE_DSTDIRY	(0x01 <<  3)
#define SH_2DG_DRAWMODE_COOF	(0x01 <<  2)
#define SH_2DG_DRAWMODE_AE	(0x01 <<  1)
#define SH_2DG_DRAWMODE_AA	(0x01 <<  1)
#define SH_2DG_DRAWMODE_SAE	(0x01 <<  0)
#define SH_2DG_DRAWMODE_CLKW	(0x01 <<  0)

#define SH_2DG_ROP_CLEAR		(0x00)  /* 0        */
#define SH_2DG_ROP_NOR			(0x11)  /* !(s | d) */
#define SH_2DG_ROP_AND_INVERTED		(0x22)  /* !s & d   */
#define SH_2DG_ROP_COPY_INVERTED	(0x33)  /* !s       */
#define SH_2DG_ROP_AND_REVERSE		(0x44)  /* s & !d   */
#define SH_2DG_ROP_INVERT		(0x55)  /* !d       */
#define SH_2DG_ROP_XOR			(0x66)  /* s ^ d    */
#define SH_2DG_ROP_NAND			(0x77)  /* !(s & d) */
#define SH_2DG_ROP_AND			(0x88)  /* s & d    */
#define SH_2DG_ROP_EQUIV		(0x99)  /* !(s ^ d) */
#define SH_2DG_ROP_NOOP			(0xAA)  /* d        */
#define SH_2DG_ROP_OR_INVERTED		(0xBB)  /* !s | d   */
#define SH_2DG_ROP_COPY			(0xCC)  /* s        */
#define SH_2DG_ROP_OR_REVERSE		(0xDD)  /* s | !d   */
#define SH_2DG_ROP_OR			(0xEE)  /* s | d    */
#define SH_2DG_ROP_SET			(0xFF)  /* 1        */

#endif /* __SH_2DG_REG_H__ */


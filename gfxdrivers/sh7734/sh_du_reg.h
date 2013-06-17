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

#ifndef __SH_DU_REG_H__
#define __SH_DU_REG_H__

/* register offset */

#define SH_DU_DSYSR		(0x0000)
#define SH_DU_DSMR		(0x0004)
#define SH_DU_DSSR		(0x0008)
#define SH_DU_DSRCR		(0x000C)
#define SH_DU_DIER		(0x0010)
#define SH_DU_CPCR		(0x0014)
#define SH_DU_DPPR		(0x0018)
#define SH_DU_DEFR		(0x0020)
#define SH_DU_DCPCR		(0x0028)
#define SH_DU_DEFR2		(0x0034)
#define SH_DU_DEFR3		(0x0038)
#define SH_DU_DEFR4		(0x003C)
#define SH_DU_DVCSR		(0x00D0)

#define SH_DU_HDSR		(0x0040)
#define SH_DU_HDER		(0x0044)
#define SH_DU_VDSR		(0x0048)
#define SH_DU_VDER		(0x004C)
#define SH_DU_HCR		(0x0050)
#define SH_DU_HSWR		(0x0054)
#define SH_DU_VCR		(0x0058)
#define SH_DU_VSPR		(0x005C)
#define SH_DU_EQWR		(0x0060)
#define SH_DU_SPWR		(0x0064)
#define SH_DU_CLAMPSR		(0x0070)
#define SH_DU_CLAMPWR		(0x0074)
#define SH_DU_DESR		(0x0078)
#define SH_DU_DEWR		(0x007C)

#define SH_DU_CP1TR		(0x0080)
#define SH_DU_CP2TR		(0x0084)
#define SH_DU_CP3TR		(0x0088)
#define SH_DU_CP4TR		(0x008C)
#define SH_DU_DOOR		(0x0090)
#define SH_DU_CDER		(0x0094)
#define SH_DU_BPOR		(0x0098)
#define SH_DU_RINTOFSR		(0x009C)
#define SH_DU_DSHPR		(0x00C8)

#define SH_DU_PBASE(i)		(0x0100 * ((i) + 1))
#define SH_DU_PMR(i)		(SH_DU_PBASE(i) + 0x00)
#define SH_DU_PMWR(i)		(SH_DU_PBASE(i) + 0x04)
#define SH_DU_PALPHAR(i)	(SH_DU_PBASE(i) + 0x08)
#define SH_DU_PDSXR(i)		(SH_DU_PBASE(i) + 0x10)
#define SH_DU_PDSYR(i)		(SH_DU_PBASE(i) + 0x14)
#define SH_DU_PDPXR(i)		(SH_DU_PBASE(i) + 0x18)
#define SH_DU_PDPYR(i)		(SH_DU_PBASE(i) + 0x1C)
#define SH_DU_PDSA0R(i)		(SH_DU_PBASE(i) + 0x20)
#define SH_DU_PDSA1R(i)		(SH_DU_PBASE(i) + 0x24)
#define SH_DU_PDSA2R(i)		(SH_DU_PBASE(i) + 0x28)
#define SH_DU_PSPXR(i)		(SH_DU_PBASE(i) + 0x30)
#define SH_DU_PSPYR(i)		(SH_DU_PBASE(i) + 0x34)
#define SH_DU_PWASPR(i)		(SH_DU_PBASE(i) + 0x38)
#define SH_DU_PWAMWR(i)		(SH_DU_PBASE(i) + 0x3C)
#define SH_DU_PBTR(i)		(SH_DU_PBASE(i) + 0x40)
#define SH_DU_PTC1R(i)		(SH_DU_PBASE(i) + 0x44)
#define SH_DU_PTC2R(i)		(SH_DU_PBASE(i) + 0x48)
#define SH_DU_PMLR(i)		(SH_DU_PBASE(i) + 0x50)
#define SH_DU_PSWAPR(i)		(SH_DU_PBASE(i) + 0x80)
#define SH_DU_PDDCR(i)		(SH_DU_PBASE(i) + 0x84)
#define SH_DU_PDDCR2(i)		(SH_DU_PBASE(i) + 0x88)

#define SH_DU_DC1MWR		(0xC104)
#define SH_DU_DC1SAR		(0xC120)
#define SH_DU_DC1MLR		(0xC150)

#define SH_DU_CPBASE(i)		(0x1000 * ((i) + 1))
#define SH_DU_CPR(i, j)		(SH_DU_CPBASE(i) + (4 * (j)))

#define SH_DU_ESCR		(0x10000)
#define SH_DU_OTAR		(0x10004)

#define SH_DU_DORCR		(0x11000)
#define SH_DU_DS1PR		(0x11020)

#define SH_DU_YNCR		(0x11080)
#define SH_DU_YNOR		(0x11084)
#define SH_DU_CRNOR		(0x11088)
#define SH_DU_CBNOR		(0x1108C)
#define SH_DU_RCRCR		(0x11090)
#define SH_DU_GCRCR		(0x11094)
#define SH_DU_GCBCR		(0x11098)
#define SH_DU_BCBCR		(0x1109C)

/* register values */

#define SH_DU_DSYSR_IUPD		(0x01 << 16)

#define SH_DU_DSSR_VCFB                 (0x02 << 28)
#define SH_DU_DSSR_DFB(i)               (0x01 << (16 + (i)))
#define SH_DU_DSSR_TVR                  (0x01 << 15)
#define SH_DU_DSSR_FRM                  (0x01 << 14)
#define SH_DU_DSSR_VBK                  (0x01 << 11)
#define SH_DU_DSSR_CMPI                 (0x01 << 10)
#define SH_DU_DSSR_RINT                 (0x01 << 9)
#define SH_DU_DSSR_HBK                  (0x01 << 8)
#define SH_DU_DSSR_ADC(i)               (0x01 << (i))

#define SH_DU_DSRCR_TVCL                SH_DU_DSSR_TVR
#define SH_DU_DSRCR_FRCL                SH_DU_DSSR_FRM
#define SH_DU_DSRCR_VBCL                SH_DU_DSSR_VBK
#define SH_DU_DSRCR_CMPCL               SH_DU_DSSR_CMPCL
#define SH_DU_DSRCR_RICL                SH_DU_DSSR_RINT
#define SH_DU_DSRCR_HBCL                SH_DU_DSSR_HBK
#define SH_DU_DSRCR_ADCL(i)             SH_DU_DSSR_ADC(i)

#define SH_DU_DIER_TVE                  SH_DU_DSSR_TVR
#define SH_DU_DIER_FRE                  SH_DU_DSSR_FRM
#define SH_DU_DIER_VBE                  SH_DU_DSSR_VBK
#define SH_DU_DIER_RIE                  SH_DU_DSSR_RINT
#define SH_DU_DIER_HBE                  SH_DU_DSSR_HBE
#define SH_DU_DIER_ADCE(i)              SH_DU_DSSR_ADC(i)

#define SH_DU_DPPR_DPE_MASK     	(0x88888888)
#define SH_DU_DPPR_DPS_MASK     	(0x77777777)

#define SH_DU_DEFR2_CODE        	(0x7775 << 16)
#define SH_DU_DEFR2_DEFE2G      	(0x01 << 0)

#define SH_DU_PMR_NOSRCKEY      	(0x01 << 14)
#define SH_DU_PMR_XOR           	(0x01 << 13)
#define SH_DU_PMR_BLEND         	(0x01 << 12)
#define SH_DU_PMR_CPSL(i)       	((i) << 8)
#define SH_DU_PMR_DDF565        	(0x01 << 0)
#define SH_DU_PMR_DDF1555       	(0x02 << 0)
#define SH_DU_PMR_DDF8          	(0x00 << 0)
#define SH_DU_PMR_DDF_MASK      	(0x03 << 0)

#define SH_DU_PALPHAR_ABIT_IGNORE	(0x01 << 13)
#define SH_DU_PALPHAR_PALETTE           (0x01 << 9)
#define SH_DU_PALPHAR_ALPHA_MASK	(0xFF << 0)

#define SH_DU_ESCR_DCLKSEL		(0x01 << 20)
#define SH_DU_ESCR_FRQSEL(i)		((i) - 1)

#endif /* __SH_DU_REG_H__ */


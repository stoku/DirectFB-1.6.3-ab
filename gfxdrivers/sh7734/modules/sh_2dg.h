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

#ifndef __SH_2DG_H__
#define __SH_2DG_H__

#include <linux/types.h>

struct sh_2dg_reg {
	__u32	offset;
	__u32	value;
};

#define SH_2DG_IOC_TYPE		(0xAF)
#define SH_2DG_IOC_NR(i)	(0xC0 + (i))
#define SH_2DG_IO(i)		_IO(SH_2DG_IOC_TYPE, SH_2DG_IOC_NR(i))
#define SH_2DG_IOR(i, size)	_IOR(SH_2DG_IOC_TYPE, SH_2DG_IOC_NR(i), size)
#define SH_2DG_IOW(i, size)	_IOW(SH_2DG_IOC_TYPE, SH_2DG_IOC_NR(i), size)

#define SH_2DG_IOC_RESET	SH_2DG_IO(0)
#define SH_2DG_IOC_GETREG	SH_2DG_IOR(1, struct sh_2dg_reg)
#define SH_2DG_IOC_SETREG	SH_2DG_IOW(1, struct sh_2dg_reg)

#endif /* __SH_2DG_H__ */


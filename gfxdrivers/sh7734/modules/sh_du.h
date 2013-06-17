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

#ifndef __SH_DU_H__
#define __SH_DU_H__

#include <linux/types.h>

struct sh_du_reg {
	__u32	offset;
	__u32	value;
};

struct sh_du_status {
	__u32	flags;
	__u32	mode;
#define SH_DU_WAIT_AND		(0x00)
#define SH_DU_WAIT_OR		(0x01)
#define SH_DU_CLEAR_BEFORE	(0x02)
#define SH_DU_CLEAR_AFTER	(0x04)
};

#define SH_DU_IOC_TYPE		(0xAF)
#define SH_DU_IOC_NR(i)		(0xA0 + (i))
#define SH_DU_IO(i)		_IO(SH_DU_IOC_TYPE, SH_DU_IOC_NR(i))
#define SH_DU_IOR(i, size)	_IOR(SH_DU_IOC_TYPE, SH_DU_IOC_NR(i), size)
#define SH_DU_IOW(i, size)	_IOW(SH_DU_IOC_TYPE, SH_DU_IOC_NR(i), size)

#define SH_DU_IOC_GETREG	SH_DU_IOR(1, struct sh_du_reg)
#define SH_DU_IOC_SETREG	SH_DU_IOW(1, struct sh_du_reg)
#define SH_DU_IOC_WAIT		SH_DU_IOR(2, struct sh_du_status)

#endif /* __SH_DU_H__ */


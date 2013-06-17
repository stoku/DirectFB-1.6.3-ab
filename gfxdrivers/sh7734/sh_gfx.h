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

#ifndef __SH_GFX_H__
#define __SH_GFX_H__

#define SH_GFX_VERSION_MAJOR	1
#define SH_GFX_VERSION_MINOR	0
#define SH_GFX_NUM_LAYERS       4

typedef struct {
     CoreDFB    *dfb;
     CoreScreen *screen;
     CoreLayer  *layers[SH_GFX_NUM_LAYERS];
     int         gfx_fd;
     int         dpy_fd;
} SHGfxDriverData;

#endif /* __SH_GFX_H__ */

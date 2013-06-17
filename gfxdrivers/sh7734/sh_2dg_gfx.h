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

#ifndef __SH_2DG_GFX_H__
#define __SH_2DG_GFX_H__

unsigned int sh_2dg_device_data_size( void );

void sh_2dg_set_funcs( GraphicsDeviceFuncs *funcs );

DFBResult sh_2dg_init( CoreGraphicsDevice *device,
                       GraphicsDeviceInfo *info,
                       void               *driver_data,
                       void               *device_data );

void sh_2dg_exit( CoreGraphicsDevice *device,
                  void               *driver_data,
                  void               *device_data );

#endif /* __SH_2DG_GFX_H__ */


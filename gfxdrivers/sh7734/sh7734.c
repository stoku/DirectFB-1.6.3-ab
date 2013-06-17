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

#include <config.h>
#include <direct/util.h>
#include <direct/debug.h>
#include <core/graphics_driver.h>
#include "sh_gfx.h"
#include "sh_2dg_gfx.h"
#include "sh_du_screen.h"
#include "sh_du_layer.h"

D_DEBUG_DOMAIN( SH7734, "SH7734", "Renesas SH7734 Driver" );

DFB_GRAPHICS_DRIVER( sh7734 )

static int
driver_probe( CoreGraphicsDevice *device )
{
     return dfb_gfxcard_get_accelerator( device ) == 7734;
}

static void
driver_get_info( CoreGraphicsDevice *device,
                 GraphicsDriverInfo *info )
{
     D_DEBUG_AT( SH7734, "%s()\n", __FUNCTION__ );
     D_UNUSED_P( device );

     /* fill driver info structure */

     snprintf( info->name, DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "Renesas SH7734 Driver" );
     snprintf( info->vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Courier Systems" );
     snprintf( info->license, DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH,
               "LGPL" );

     info->version.major    = SH_GFX_VERSION_MAJOR;
     info->version.minor    = SH_GFX_VERSION_MINOR;
     info->driver_data_size = sizeof(SHGfxDriverData);
     info->device_data_size = sh_2dg_device_data_size();
}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     SHGfxDriverData *shdrv;
     int i;

     D_DEBUG_AT( SH7734, "%s()\n", __FUNCTION__ );
     D_UNUSED_P( device_data );

     shdrv = (SHGfxDriverData *)driver_data;

     shdrv->dfb = core;

     shdrv->gfx_fd = open( "/dev/sh_2dg", O_RDWR );
     if (shdrv->gfx_fd < 0)
          return DFB_INIT;

     shdrv->dpy_fd = open( "/dev/sh_du", O_RDWR );
     if (shdrv->dpy_fd < 0) {
          close(shdrv->gfx_fd);
          shdrv->gfx_fd = 0;
          return DFB_INIT;
     }

     sh_2dg_set_funcs(funcs);
     shdrv->screen = sh_du_screen_register( device, driver_data );
     for (i = 0; i < SH_GFX_NUM_LAYERS; i++)
          shdrv->layers[i] = sh_du_layer_register( shdrv->screen,
                                                   driver_data );

     return DFB_OK;
}

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     D_DEBUG_AT( SH7734, "%s()\n", __FUNCTION__ );
     return sh_2dg_init( device, device_info, driver_data, device_data );
}

static void
driver_close_device( CoreGraphicsDevice *device,
                     void               *driver_data,
                     void               *device_data )
{
     D_DEBUG_AT( SH7734, "%s()\n", __FUNCTION__ );
     sh_2dg_exit( device, driver_data, device_data );
}

static void
driver_close_driver( CoreGraphicsDevice *device,
                     void               *driver_data )
{
     SHGfxDriverData *shdrv;

     D_DEBUG_AT( SH7734, "%s()\n", __FUNCTION__ );
     D_UNUSED_P( device );

     shdrv = (SHGfxDriverData *)driver_data;
     close(shdrv->dpy_fd);
     close(shdrv->gfx_fd);
}


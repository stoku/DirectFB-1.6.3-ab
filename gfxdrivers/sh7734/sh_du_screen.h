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

#ifndef __SH_DU_SCREEN_H__
#define __SH_DU_SCREEN_H__

CoreScreen *sh_du_screen_register( CoreGraphicsDevice *device,
                                   void *driver_data );

void sh_du_get_max_screen_size( int *ret_width,
                                int *ret_height );

DFBResult sh_du_add_layer( CoreScreen *screen,
                           void *driver_data,
                           DFBDisplayLayerID  id,
                           u8 plane,
                           int *level);

DFBResult sh_du_remove_layer( CoreScreen *screen,
                              void *driver_data,
                              DFBDisplayLayerID id );

DFBResult sh_du_change_layer_level( CoreScreen *screen,
                                    void *driver_data,
                                    DFBDisplayLayerID id,
                                    int level );

#endif /* __SH_DU_SCREEN_H__ */


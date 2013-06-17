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

#include <sys/ioctl.h>
#include <direct/util.h>
#include <direct/debug.h>
#include <core/layers.h>
#include <core/layer_control.h>
#include <core/screen.h>
#include <core/palette.h>
#include "modules/sh_du.h"
#include "sh_gfx.h"
#include "sh_du_screen.h"
#include "sh_du_layer.h"
#include "sh_du_reg.h"

#define SH_DU_READ(fd, reg)            ioctl((fd), SH_DU_IOC_GETREG, &(reg))
#define SH_DU_WRITE(fd, reg)           ioctl((fd), SH_DU_IOC_SETREG, &(reg))

#define SH_DU_MASK_PMR                 (0x1C1377B3)
#define SH_DU_MASK_PMWR                (0x00001FF0)
#define SH_DU_MASK_PALPHAR             (0x000037FF)
#define SH_DU_MASK_PDSXR               (0x000007FF)
#define SH_DU_MASK_PDSYR               (0x000003FF)
#define SH_DU_MASK_PDPXR               (0x000007FF)
#define SH_DU_MASK_PDPYR               (0x000003FF)
#define SH_DU_MASK_PDSAR               (0xFFFFFFF0)
#define SH_DU_MASK_PSPXR               (0x00000FFF)
#define SH_DU_MASK_PSPYR               (0x0000FFFF)
#define SH_DU_MASK_PTC1R               (0x000000FF)
#define SH_DU_MASK_PTC2R               (0x0000FFFF)

#define SH_DU_CHECK_REG_VALUE(name, value) \
        D_ASSUME( D_FLAGS_ARE_IN( value, SH_DU_MASK_##name ) )

#define SH_DU_PLANE_SET_DIRTY(flags, f) \
        D_FLAGS_SET( flags, PLANE_##f )

#define SH_DU_PLANE_IS_DIRTY(flags, f) \
        D_FLAGS_IS_SET( flags, PLANE_##f )

#define SH_DU_COLORKEY_TO_RGB565(k)    ((((u32)(k).r & 0xF8) << 8) | \
                                        (((u32)(k).g & 0xFC) << 3) | \
                                        (((u32)(k).b & 0xF8) >> 3))

#define SH_DU_COLORKEY_TO_ARGB1555(k)  ((((u32)(k).r & 0xF8) << 7) | \
                                        (((u32)(k).g & 0xF8) << 2) | \
                                        (((u32)(k).b & 0xF8) >> 3))

#define SH_DU_COLOR_TO_PALETTE(c)      ((((u32)(c).a & 0xFF) << 24) | \
                                        (((u32)(c).r & 0xFC) << 16) | \
                                        (((u32)(c).g & 0xFC) <<  8) | \
                                        (((u32)(c).b & 0xFC) <<  0))

D_DEBUG_DOMAIN( SH_DU_LAYER, "SH-DU/Layer", "Renesas DU Layer" );

enum SHDUPlaneReg {
     PLANE_MODE    = (1 << 0),
     PLANE_PA0     = (1 << 1),
     PLANE_PA1     = (1 << 2),
     PLANE_PA2     = (1 << 3),
     PLANE_STRIDE  = (1 << 4),
     PLANE_SX      = (1 << 5),
     PLANE_SY      = (1 << 6),
     PLANE_DX      = (1 << 7),
     PLANE_DY      = (1 << 8),
     PLANE_WIDTH   = (1 << 9),
     PLANE_HEIGHT  = (1 << 10),
     PLANE_OPACITY = (1 << 11),
     PLANE_CKEY8   = (1 << 12),
     PLANE_CKEY16  = (1 << 13),
     PLANE_ALL     = (1 << 14) - 1,
};

typedef struct {
     DFBDisplayLayerID id;
     int               level;
     int               sx;
     int               sy;
     int               dx;
     int               dy;
     int               width;
     int               height;
     u8                opacity;
     DFBColorKey       ckey;
     struct {
          u8  id;
          u32 mode;
          u32 pa0;
          u32 pa1;
          u32 pa2;
          u32 stride;
          u32 sx;
          u32 sy;
          u32 dx;
          u32 dy;
          u32 width;
          u32 height;
          u32 opacity;
          u32 ckey8;
          u32 ckey16;
     } plane;
} SHDuLayerData;

static void
init_layer( const SHGfxDriverData *drv,
            SHDuLayerData *layer,
            DFBDisplayLayerID id,
            const CoreLayerRegionConfig *config )
{
     struct sh_du_reg reg;

     layer->id            = id;
     /* NOTE: layer->level is assigned after added */
     layer->level         = -1;
     layer->sx            = 0;
     layer->sy            = 0;
     layer->dx            = 0;
     layer->dy            = 0;
     layer->width         = config->width;
     layer->height        = config->height;
     layer->opacity       = 0xFF;
     layer->ckey.index    = 0;
     layer->ckey.r        = 0;
     layer->ckey.g        = 0;
     layer->ckey.b        = 0;

     layer->plane.id      = (u8)(2 * id);
     layer->plane.mode    = SH_DU_PMR_CPSL(id);
     layer->plane.pa0     = 0;
     layer->plane.pa1     = 0;
     layer->plane.pa2     = 0;
     layer->plane.stride  = config->width;
     layer->plane.sx      = 0;
     layer->plane.sy      = 0;
     layer->plane.dx      = 0;
     layer->plane.dy      = 0;
     layer->plane.width   = config->width;
     layer->plane.height  = config->height;
     layer->plane.opacity = 0xFF;
     layer->plane.ckey8   = 0;
     layer->plane.ckey16  = 0;

     if (D_FLAGS_IS_SET( config->options, DLOP_ALPHACHANNEL | DLOP_OPACITY )) {
          layer->plane.mode |= SH_DU_PMR_BLEND;
     }

     if (!D_FLAGS_IS_SET( config->options, DLOP_SRC_COLORKEY )) {
          layer->plane.mode |= SH_DU_PMR_NOSRCKEY;
     }

     switch (config->format) {
     case DSPF_RGB16:
          layer->plane.mode |= SH_DU_PMR_DDF565;
          break;
     case DSPF_ARGB1555:
          layer->plane.mode |= SH_DU_PMR_DDF1555;
          if (D_FLAGS_IS_SET( config->options, DLOP_OPACITY ))
               layer->plane.opacity |= SH_DU_PALPHAR_ABIT_IGNORE;
          break;
     default:
          D_ASSERT( config->format == DSPF_LUT8 );
          layer->plane.mode |= SH_DU_PMR_DDF8;
          break;
     }

     /* initialize undefined registers */

     reg.offset = SH_DU_PWASPR( layer->plane.id );
     reg.value  = 0;
     (void)SH_DU_WRITE( drv->dpy_fd, reg );

     reg.offset = SH_DU_PWAMPR( layer->plane.id );
     reg.value  = 4095; /* max value */
     (void)SH_DU_WRITE( drv->dpy_fd, reg );
}

static DFBResult
update_layer( const SHGfxDriverData *drv,
              const SHDuLayerData *layer,
              u32 dirty,
              const u32 *palette )
{
     int fd;
     u8 i;
     u32 dsys;
     struct sh_du_reg reg;

     fd = drv->dpy_fd;
     i  = layer->plane.id;

     reg.offset = SH_DU_DSYSR;
     if (SH_DU_READ( fd, reg ))
          goto err_out;
     dsys      = reg.value;
     reg.value = dsys | SH_DU_DSYSR_IUPD;
     if (SH_DU_WRITE( fd, reg ))
          goto err_out;

     if (SH_DU_PLANE_IS_DIRTY( dirty, MODE )) {
          reg.offset = SH_DU_PMR( i );
          reg.value  = layer->plane.mode;
          SH_DU_CHECK_REG_VALUE( PMR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, PA0 )) {
          reg.offset = SH_DU_PDSA0R( i );
          reg.value  = layer->plane.pa0;
          SH_DU_CHECK_REG_VALUE( PDSAR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, PA1 )) {
          reg.offset = SH_DU_PDSA1R( i );
          reg.value  = layer->plane.pa1;
          SH_DU_CHECK_REG_VALUE( PDSAR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, PA2 )) {
          reg.offset = SH_DU_PDSA2R( i );
          reg.value  = layer->plane.pa2;
          SH_DU_CHECK_REG_VALUE( PDSAR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, STRIDE )) {
          reg.offset = SH_DU_PMWR( i );
          reg.value  = layer->plane.stride;
          SH_DU_CHECK_REG_VALUE( PMWR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, SX )) {
          reg.offset = SH_DU_PSPXR( i );
          reg.value  = layer->plane.sx;
          SH_DU_CHECK_REG_VALUE( PSPXR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, SY )) {
          reg.offset = SH_DU_PSPYR( i );
          reg.value  = layer->plane.sy;
          SH_DU_CHECK_REG_VALUE( PSPYR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, DX )) {
          reg.offset = SH_DU_PDPXR( i );
          reg.value  = layer->plane.dx;
          SH_DU_CHECK_REG_VALUE( PDPXR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, DY )) {
          reg.offset = SH_DU_PDPYR( i );
          reg.value  = layer->plane.dy;
          SH_DU_CHECK_REG_VALUE( PDPYR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, WIDTH )) {
          reg.offset = SH_DU_PDSXR( i );
          reg.value  = layer->plane.width;
          SH_DU_CHECK_REG_VALUE( PDSXR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, HEIGHT )) {
          reg.offset = SH_DU_PDSYR( i );
          reg.value  = layer->plane.height;
          SH_DU_CHECK_REG_VALUE( PDSYR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, OPACITY )) {
          reg.offset = SH_DU_PALPHAR( i );
          reg.value  = layer->plane.opacity;
          SH_DU_CHECK_REG_VALUE( PALPHAR, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, CKEY8 )) {
          reg.offset = SH_DU_PTC1R( i );
          reg.value  = layer->plane.ckey8;
          SH_DU_CHECK_REG_VALUE( PTC1R, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (SH_DU_PLANE_IS_DIRTY( dirty, CKEY16 )) {
          reg.offset = SH_DU_PTC2R( i );
          reg.value  = layer->plane.ckey16;
          SH_DU_CHECK_REG_VALUE( PTC2R, reg.value );
          if (SH_DU_WRITE( fd, reg ))
               goto err_reset;
     }
     if (palette) {
          reg.offset = SH_DU_CPBASE( layer->id );
          for (i = 0; i < 256; i++) {
               reg.value = palette[i];
               if (SH_DU_WRITE( fd, reg ))
                    goto err_reset;
               reg.offset += 4;
          }
     }

     reg.offset = SH_DU_DSYSR;
     reg.value  = dsys;
     if (SH_DU_WRITE( fd, reg ))
          goto err_out;

     return DFB_OK;

err_reset:
     reg.offset = SH_DU_DSYSR;
     reg.value  = dsys;
     (void)SH_DU_WRITE( fd, reg );
err_out:
     return DFB_IO;
}

static int
sh_du_layer_data_size( void )
{
     D_DEBUG_AT( SH_DU_LAYER, "%s()\n", __FUNCTION__ );
     return sizeof(SHDuLayerData);
}

static int
sh_du_region_data_size( void )
{
     D_DEBUG_AT( SH_DU_LAYER, "%s()\n", __FUNCTION__ );
     return 0;
}

static DFBResult
sh_du_init_layer( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  DFBDisplayLayerDescription *description,
                  DFBDisplayLayerConfig      *config,
                  DFBColorAdjustment         *adjustment )
{
     DFBResult          ret;

     D_DEBUG_AT( SH_DU_LAYER, "%s()\n", __FUNCTION__ );
     D_UNUSED_P( driver_data );
     D_UNUSED_P( layer_data );
     D_UNUSED_P( adjustment );

     ret = dfb_screen_get_screen_size( dfb_layer_screen( layer ),
                                       &config->width, &config->height );
     if (ret)
          goto out;

     config->pixelformat = DSPF_RGB16;
     config->colorspace  = DSCS_RGB;
     config->buffermode  = DLBM_BACKVIDEO;
     config->options     = DLOP_ALPHACHANNEL;
     config->flags       = DLCONF_WIDTH |
                           DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT |
                           DLCONF_COLORSPACE |
                           DLCONF_BUFFERMODE |
                           DLCONF_OPTIONS;

     description->type         = DLTF_GRAPHICS | DLTF_STILL_PICTURE;
     description->caps         = DLCAPS_SURFACE |
                                 DLCAPS_OPACITY |
                                 DLCAPS_ALPHACHANNEL |
                                 DLCAPS_SRC_COLORKEY |
                                 DLCAPS_LEVELS |
                                 DLCAPS_SCREEN_POSITION |
                                 DLCAPS_SCREEN_SIZE;
     description->level        = -1;
     description->regions      = 1;
     description->sources      = 0;
     description->clip_regions = 0;
     description->surface_caps = DSCAPS_VIDEOONLY |
                                 DSCAPS_DOUBLE |
                                 DSCAPS_SHARED;

     snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH,
               "SH DU Layer" );

out:
     return ret;
}

static DFBResult
sh_du_shutdown_layer( CoreLayer *layer,
                      void      *driver_data,
                      void      *layer_data )
{
     D_DEBUG_AT( SH_DU_LAYER, "%s( %u )\n",
                 __FUNCTION__, dfb_layer_id( layer ) );
     D_UNUSED_P( layer );
     D_UNUSED_P( driver_data );
     D_UNUSED_P( layer_data );

     return DFB_OK;
}

static
DFBResult sh_du_get_level( CoreLayer *layer,
                           void      *driver_data,
                           void      *layer_data,
                           int       *level )
{
     SHDuLayerData *lyr;

     D_DEBUG_AT( SH_DU_LAYER, "%s( %u )\n",
                 __FUNCTION__, dfb_layer_id( layer ) );
     D_UNUSED_P( layer );
     D_UNUSED_P( driver_data );

     lyr    = (SHDuLayerData *)layer_data;
     *level = lyr->level;
     return DFB_OK;
}

static DFBResult
sh_du_set_level( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 int        level )
{
     DFBResult      ret;
     SHDuLayerData *lyr;

     D_DEBUG_AT( SH_DU_LAYER, "%s( %u, %d )\n",
                 __FUNCTION__, dfb_layer_id( layer ), level );

     lyr = (SHDuLayerData *)layer_data;
     if (lyr->level == level)
          return DFB_OK;

     ret = sh_du_change_layer_level( dfb_layer_screen( layer ),
                                     driver_data, lyr->id, level);
     if (ret == DFB_OK)
          lyr->level = level;

     return ret;
}

static DFBResult
sh_du_test_region( CoreLayer                  *layer,
                   void                       *driver_data,
                   void                       *layer_data,
                   CoreLayerRegionConfig      *config,
                   CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail;
     DFBResult result;
     int w, h;

     D_DEBUG_AT( SH_DU_LAYER, "%s( %u )\n",
                 __FUNCTION__, dfb_layer_id( layer ) );
     D_UNUSED_P( driver_data );
     D_UNUSED_P( layer_data );

     result = dfb_screen_get_screen_size( dfb_layer_screen( layer ), &w, &h );
     if (result)
          return result;

     fail = CLRCF_NONE;

     if ((config->width < 0) || (config->width > w))
          fail |= CLRCF_WIDTH;
     if ((config->height < 0) || (config->height > h))
          fail |= CLRCF_HEIGHT;

     switch (config->format) {
     case DSPF_RGB16:
     case DSPF_ARGB1555:
     case DSPF_LUT8:
          break;
     default:
          fail |= CLRCF_FORMAT;
          break;
     }

     if (config->buffermode != DLBM_BACKVIDEO)
          fail |= CLRCF_BUFFERMODE;

     if (D_FLAGS_INVALID( config->options, DLOP_ALPHACHANNEL |
                                           DLOP_SRC_COLORKEY |
                                           DLOP_OPACITY ))
          fail |= CLRCF_OPTIONS;

     if (D_FLAGS_ARE_SET( config->options, DLOP_ALPHACHANNEL | DLOP_OPACITY ))
          fail |= CLRCF_OPTIONS;

     if ((config->source.x < 0) ||
         (config->source.y < 0) ||
         (config->source.w < 0) ||
         (config->source.h < 0) ||
         ((config->source.x + config->source.w) > config->width) ||
         ((config->source.y + config->source.h) > config->height))
          fail |= CLRCF_SOURCE;

     if ((config->dest.w != config->source.w) ||
         (config->dest.h != config->source.h))
          fail |= CLRCF_DEST;

     if (config->colorspace != DSCS_RGB)
          fail |= CLRCF_COLORSPACE;

     if (config->num_clips > 0)
          fail |= CLRCF_CLIPS;

     if (failed)
          *failed = fail;

     return (fail == CLRCF_NONE) ? DFB_OK : DFB_UNSUPPORTED;
}

static DFBResult
sh_du_add_region( CoreLayer             *layer,
                  void                  *driver_data,
                  void                  *layer_data,
                  void                  *region_data,
                  CoreLayerRegionConfig *config )
{
     SHGfxDriverData *drv;
     SHDuLayerData   *lyr;
     DFBResult        ret;

     D_DEBUG_AT( SH_DU_LAYER, "%s( %u )\n",
                 __FUNCTION__, dfb_layer_id( layer ) );
     D_UNUSED_P( region_data );

     drv = (SHGfxDriverData *)driver_data;
     lyr = (SHDuLayerData *)layer_data;

     init_layer( drv, lyr, dfb_layer_id( layer ), config );
     ret = update_layer( drv, lyr, PLANE_ALL, NULL );
     if (ret == DFB_OK)
          ret = sh_du_add_layer( dfb_layer_screen( layer ), driver_data,
	                         lyr->id, lyr->plane.id, &lyr->level );

     return ret;
}

static DFBResult
sh_du_set_region( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  void                       *region_data,
                  CoreLayerRegionConfig      *config,
                  CoreLayerRegionConfigFlags  updated,
                  CoreSurface                *surface,
                  CorePalette                *palette,
                  CoreSurfaceBufferLock      *left_lock,
                  CoreSurfaceBufferLock      *right_lock )
{
     DFBResult        ret;
     SHGfxDriverData *drv;
     SHDuLayerData   *lyr;
     u32             *pal, dirty;

     D_DEBUG_AT( SH_DU_LAYER, "%s( %u, 0x%x )\n",
                 __FUNCTION__, dfb_layer_id( layer ), updated );
     D_UNUSED_P( region_data );
     D_UNUSED_P( surface );
     D_UNUSED_P( right_lock );

     drv   = (SHGfxDriverData *)driver_data;
     lyr   = (SHDuLayerData *)layer_data;
     pal   = NULL;
     dirty = 0u;

     /* color palette */
     if (D_FLAGS_IS_SET( updated, CLRCF_PALETTE ) && palette) {
          unsigned int i;

          if (palette->num_entries > 256)
               return DFB_UNSUPPORTED;

          pal = malloc(256 * sizeof(u32));
          if (!pal)
               return DFB_NOSYSTEMMEMORY;

          for (i = 0; i < palette->num_entries; i++)
               pal[i] = SH_DU_COLOR_TO_PALETTE(palette->entries[i]);
          while (i < 256)
               pal[i++] = 0u;
     }

     /* buffer address and stride */
     if (D_FLAGS_IS_SET( updated, CLRCF_SURFACE )) {
          u32 stride;

          stride  = (u32)(left_lock->pitch * 8);
          stride /= DFB_BITS_PER_PIXEL( left_lock->buffer->format );
          if (lyr->plane.stride != stride) {
               lyr->plane.stride = stride;
               SH_DU_PLANE_SET_DIRTY( dirty, STRIDE );
          }
          if (lyr->plane.pa0 != left_lock->phys) {
               lyr->plane.pa0 = left_lock->phys;
               SH_DU_PLANE_SET_DIRTY( dirty, PA0 );
          }
     }

     /* area */
     if (D_FLAGS_IS_SET( updated, CLRCF_WIDTH |
                                  CLRCF_HEIGHT |
                                  CLRCF_SOURCE |
                                  CLRCF_DEST )) {
          u32 sx, sy, dx, dy, w, h;

          if (D_FLAGS_IS_SET( updated, CLRCF_WIDTH )) {
               D_ASSERT( config->width > 0 );
               lyr->width = config->width;
          }
          if (D_FLAGS_IS_SET( updated, CLRCF_HEIGHT )) {
               D_ASSERT( config->height > 0 );
               lyr->height = config->height;
          }
          if (D_FLAGS_IS_SET( updated, CLRCF_SOURCE )) {
               D_ASSERT( config->source.x >= 0 );
               D_ASSERT( config->source.y >= 0 );
               lyr->sx = config->source.x;
               lyr->sy = config->source.y;
          }
          if (D_FLAGS_IS_SET( updated, CLRCF_DEST )) {
               lyr->dx = config->dest.x;
               lyr->dy = config->dest.y;
          }
          if (lyr->dx >= 0) {
               w  = (u32)lyr->width;
               sx = (u32)lyr->sx;
               dx = (u32)lyr->dx;
          }
          else {
               w  = (lyr->width + lyr->dx) <= 0 ?
                    0u : (u32)(lyr->width + lyr->dx);
               sx = (u32)(lyr->sx - lyr->dx);
               dx = 0u;
          }
          if (lyr->dy >= 0) {
               h  = (u32)lyr->height;
               sy = (u32)lyr->sy;
               dy = (u32)lyr->dy;
          }
          else {
               h  = (lyr->height + lyr->dy) <= 0 ?
                    0u : (u32)(lyr->height + lyr->dy);
               sy = (u32)(lyr->sy - lyr->dy);
               dy = 0u;
          }
          if (lyr->plane.width != w) {
               lyr->plane.width = w;
               SH_DU_PLANE_SET_DIRTY( dirty, WIDTH );
          }
          if (lyr->plane.height != h) {
               lyr->plane.height = h;
               SH_DU_PLANE_SET_DIRTY( dirty, HEIGHT );
          }
          if (lyr->plane.sx != sx) {
               lyr->plane.sx = sx;
               SH_DU_PLANE_SET_DIRTY( dirty, SX );
          }
          if (lyr->plane.sy != sy) {
               lyr->plane.sy = sy;
               SH_DU_PLANE_SET_DIRTY( dirty, SY );
          }
          if (lyr->plane.dx != dx) {
               lyr->plane.dx = dx;
               SH_DU_PLANE_SET_DIRTY( dirty, DX );
          }
          if (lyr->plane.dy != dy) {
               lyr->plane.dy = dy;
               SH_DU_PLANE_SET_DIRTY( dirty, DY );
          }
     }

     /* mode and opacity */
     if (D_FLAGS_IS_SET( updated, CLRCF_FORMAT |
                                  CLRCF_OPTIONS |
                                  CLRCF_OPACITY )) {
          u32 mode, opacity;

          mode    = lyr->plane.mode;
          opacity = lyr->plane.opacity;

          if (D_FLAGS_IS_SET( updated, CLRCF_OPACITY ))
	       lyr->opacity = config->opacity;

          if (D_FLAGS_IS_SET( updated, CLRCF_OPTIONS )) {
               if (D_FLAGS_IS_SET( config->options, DLOP_ALPHACHANNEL )) {
                    D_FLAGS_SET( mode, SH_DU_PMR_BLEND );
                    D_FLAGS_CLEAR( opacity, SH_DU_PALPHAR_ABIT_IGNORE );
                    D_FLAGS_SET( opacity, SH_DU_PALPHAR_PALETTE );
               }
               else if (D_FLAGS_IS_SET( config->options, DLOP_OPACITY )) {
                    D_FLAGS_SET( mode, SH_DU_PMR_BLEND );
                    D_FLAGS_SET( opacity, SH_DU_PALPHAR_ABIT_IGNORE );
                    D_FLAGS_CLEAR( opacity, SH_DU_PALPHAR_PALETTE );
               }
               else {
                    D_FLAGS_CLEAR( mode, SH_DU_PMR_BLEND );
               }
               if (D_FLAGS_IS_SET( config->options, DLOP_SRC_COLORKEY ))
                    D_FLAGS_CLEAR( mode, SH_DU_PMR_NOSRCKEY );
               else
                    D_FLAGS_SET( mode, SH_DU_PMR_NOSRCKEY );
          }

          if (D_FLAGS_IS_SET( updated, CLRCF_FORMAT )) {
               mode &= ~SH_DU_PMR_DDF_MASK;
               switch (config->format) {
               case DSPF_RGB16:
                    mode |= SH_DU_PMR_DDF565;
                    break;
               case DSPF_ARGB1555:
                    mode |= SH_DU_PMR_DDF1555;
                    break;
               default:
                    D_ASSERT( config->format == DSPF_LUT8 );
                    mode |= SH_DU_PMR_DDF8;
                    break;
               }
          }

          if (D_FLAGS_IS_SET( mode, SH_DU_PMR_BLEND )) {
	       if (D_FLAGS_IS_SET( opacity, SH_DU_PALPHAR_PALETTE ))
	            opacity |= SH_DU_MASK_PALPHAR; /* pixel alpha blending */
	       else /* const alpha blending */
	            opacity = (opacity & ~SH_DU_MASK_PALPHAR) |
                              (u32)lyr->opacity;
          }
          if (lyr->plane.opacity != opacity) {
               lyr->plane.opacity = opacity;
               SH_DU_PLANE_SET_DIRTY( dirty, OPACITY );
          }
          if (lyr->plane.mode != mode) {
               lyr->plane.mode = mode;
               SH_DU_PLANE_SET_DIRTY( dirty, MODE );
          }
     }

     /* src color key */
     if (D_FLAGS_IS_SET( updated, CLRCF_SRCKEY ))
          lyr->ckey = config->src_key;
     if (D_FLAGS_IS_SET( updated, CLRCF_SRCKEY | CLRCF_FORMAT )) {
          u32 ddf, ckey;

          ddf = lyr->plane.mode & SH_DU_PMR_DDF_MASK;
          if (ddf == SH_DU_PMR_DDF8) {
               ckey = (u32)lyr->ckey.index;
               if (lyr->plane.ckey8 != ckey) {
                    lyr->plane.ckey8 = ckey;
                    SH_DU_PLANE_SET_DIRTY( dirty, CKEY8 );
               }
          }
          else {
               ckey = (ddf == SH_DU_PMR_DDF565) ?
                      SH_DU_COLORKEY_TO_RGB565( lyr->ckey ) :
                      SH_DU_COLORKEY_TO_ARGB1555( lyr->ckey );
               if (lyr->plane.ckey16 != ckey) {
                    lyr->plane.ckey16 = ckey;
                    SH_DU_PLANE_SET_DIRTY( dirty, CKEY16 );
               }
          }
     }

     ret = (dirty || pal) ? update_layer( drv, lyr, dirty, pal ) : DFB_OK;
     if (pal)
          free( pal );

     return ret;
}

static DFBResult
sh_du_remove_region( CoreLayer *layer,
                     void      *driver_data,
                     void      *layer_data,
                     void      *region_data )
{
     SHDuLayerData *lyr;

     D_DEBUG_AT( SH_DU_LAYER, "%s( %u )\n",
                 __FUNCTION__, dfb_layer_id( layer ) );
     D_UNUSED_P( region_data );

     lyr = (SHDuLayerData *)layer_data;

     return sh_du_remove_layer( dfb_layer_screen( layer ),
                                driver_data, lyr->id );
}

static DFBResult
sh_du_flip_region( CoreLayer             *layer,
                   void                  *driver_data,
                   void                  *layer_data,
                   void                  *region_data,
                   CoreSurface           *surface,
                   DFBSurfaceFlipFlags    flags,
                   CoreSurfaceBufferLock *left_lock,
                   CoreSurfaceBufferLock *right_lock )
{
     DFBResult        ret;
     SHGfxDriverData *drv;
     SHDuLayerData   *lyr;
 
     D_DEBUG_AT( SH_DU_LAYER, "%s( %u, 0x%x )\n",
                 __FUNCTION__, dfb_layer_id( layer ), flags );
     D_UNUSED_P( region_data );
     D_UNUSED_P( right_lock );

     ret = DFB_OK;
     drv = (SHGfxDriverData *)driver_data;
     lyr = (SHDuLayerData *)layer_data;

     if (D_FLAGS_IS_SET( flags, DSFLIP_ONSYNC ))
          ret = dfb_layer_wait_vsync( layer );

     if (ret)
          goto out;

     lyr->plane.pa0 = left_lock->phys;
     ret = update_layer( drv, lyr, PLANE_PA0, NULL );

     if (D_FLAGS_IS_SET( flags, DSFLIP_WAIT ))
          ret = dfb_layer_wait_vsync( layer );

     dfb_surface_flip( surface, false );

out:
     return ret;
}

static DFBResult
sh_du_update_region( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data,
                     CoreSurface           *surface,
                     const DFBRegion       *left_update,
                     CoreSurfaceBufferLock *left_lock,
                     const DFBRegion       *right_update,
                     CoreSurfaceBufferLock *right_lock )
{
     D_DEBUG_AT( SH_DU_LAYER, "%s( %u )\n",
                 __FUNCTION__, dfb_layer_id( layer ) );
     D_UNUSED_P( layer );
     D_UNUSED_P( driver_data );
     D_UNUSED_P( layer_data );
     D_UNUSED_P( region_data );
     D_UNUSED_P( surface );
     D_UNUSED_P( left_update );
     D_UNUSED_P( left_lock );
     D_UNUSED_P( right_update );
     D_UNUSED_P( right_lock );

     return DFB_UNSUPPORTED;
}

static const DisplayLayerFuncs sh_du_layer_funcs = {
     .LayerDataSize  = sh_du_layer_data_size,
     .RegionDataSize = sh_du_region_data_size,
     .InitLayer      = sh_du_init_layer,
     .ShutdownLayer  = sh_du_shutdown_layer,
     .GetLevel       = sh_du_get_level,
     .SetLevel       = sh_du_set_level,
     .TestRegion     = sh_du_test_region,
     .AddRegion      = sh_du_add_region,
     .SetRegion      = sh_du_set_region,
     .RemoveRegion   = sh_du_remove_region,
     .FlipRegion     = sh_du_flip_region,
     .UpdateRegion   = sh_du_update_region,
};

CoreLayer *
sh_du_layer_register( CoreScreen *screen, void *driver_data)
{
     return dfb_layers_register( screen, driver_data, &sh_du_layer_funcs );
}


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
#include <core/gfxcard.h>
#include <core/state.h>
#include <core/surface_buffer.h>
#include <gfx/convert.h>
#include "modules/sh_2dg.h"
#include "sh_gfx.h"
#include "sh_2dg_gfx.h"
#include "sh_2dg_reg.h"

#define SH_2DG_SUPPORTED_DRAWINGFLAGS		(DSDRAW_BLEND | \
						 DSDRAW_DST_COLORKEY | \
						 DSDRAW_XOR )

#define SH_2DG_SUPPORTED_DRAWINGFUNCTIONS	(DFXL_FILLRECTANGLE | \
						 DFXL_FILLTRIANGLE | \
						 DFXL_DRAWRECTANGLE | \
						 DFXL_DRAWLINE)

#define SH_2DG_SUPPORTED_BLITTINGFLAGS		(DSBLIT_BLEND_ALPHACHANNEL | \
						 DSBLIT_BLEND_COLORALPHA | \
						 DSBLIT_SRC_COLORKEY | \
						 DSBLIT_DST_COLORKEY | \
						 DSBLIT_XOR | \
						 DSBLIT_FLIP_HORIZONTAL | \
						 DSBLIT_FLIP_VERTICAL )

#define SH_2DG_SUPPORTED_BLITTINGFUNCTIONS	(DFXL_BLIT | \
						 DFXL_STRETCHBLIT)

#define SH_2DG_BLEND_FUNC_PAIR(src, dst)	((src) << 16 | (dst))

#define SH_2DG_CMD_SIZE(n)			((n) * sizeof(u32))

#define SH_2DG_REG(n, offset)			((((n) - 1) << 16) | (offset))

#define SH_2DG_XY(x, y)				((((x) & 0xFFFF) << 16) | \
						 (((y) & 0xFFFF) <<  0))

typedef struct {
     u32                    rcl;
     u32                    sclma;
     u32                    stc;
     u32                    dtc;
     u32                    alph;
     DFBColor               color;
     u32                    dst_key;
     u32                    src_key;
     u32                    rop;
     u32                    mode;
     CoreSurface           *work;
     CoreSurfaceBufferLock  work_lock;
} SH2dgDeviceData;

static DFBResult
create_work_surface( CoreDFB *dfb,
                     const DFBDimension *size,
                     SH2dgDeviceData *dev )
{
     DFBResult            ret;
     CoreSurfaceConfig    config;
     CoreSurfaceTypeFlags type;

     config.flags  = CSCONF_SIZE | CSCONF_FORMAT;
     config.size   = *size;
     config.format = DSPF_A1;

     type = CSTF_EXTERNAL;

     ret = dfb_surface_create( dfb, &config, type, 0, NULL, &dev->work );
     if (ret == DFB_OK) {
          ret = dfb_surface_lock_buffer( dev->work, CSBR_FRONT,
                                         CSAID_GPU, CSAF_READ | CSAF_WRITE,
                                         &dev->work_lock );
          if (ret) {
               dfb_surface_destroy_buffers( dev->work );
               dfb_surface_unlink( &dev->work );
          }
     }
     return ret;
}

static void
destroy_work_surface( SH2dgDeviceData *dev )
{
     dfb_surface_unlock_buffer( dev->work, &dev->work_lock );
     dfb_surface_destroy_buffers( dev->work );
     dfb_surface_unlink( &dev->work );
}


static bool
check_pixelformat( DFBSurfacePixelFormat format )
{
     return (format == DSPF_RGB16) || (format == DSPF_ARGB1555);
}

static bool
check_blend_functions( CardState *state )
{
     bool ret;

     switch (SH_2DG_BLEND_FUNC_PAIR(state->src_blend, state->dst_blend)) {
     //case SH_2DG_BLEND_FUNC_PAIR(DSBF_ONE,      DSBF_ZERO):
     case SH_2DG_BLEND_FUNC_PAIR(DSBF_SRCALPHA, DSBF_INVSRCALPHA):
          ret = true;
     default:
          ret = false;
     }
     return ret;
}

static u32
color_to_pixel( const DFBColor *color, bool argb1555 )
{
     return argb1555 ?
            PIXEL_ARGB1555( (u32)color->a, (u32)color->r,
                            (u32)color->g, (u32)color->b ) :
            PIXEL_RGB16( (u32)color->r, (u32)color->g, (u32)color->b );
}

static int
set_destination( SHGfxDriverData *drv,
                 SH2dgDeviceData *dev,
                 CardState *state )
{
     u32 cmds[9];
     u32 i, sclma;

     D_ASSERT( (state->dst.buffer->format == DSPF_RGB16) ||
               (state->dst.buffer->format == DSPF_ARGB1555) );
     D_ASSERT( state->destination != NULL );

     i = 0;
     cmds[i++] = SH_2DG_WPR;
     cmds[i++] = SH_2DG_REG(1, SH_2DG_RSAR);
     cmds[i++] = state->dst.phys;
     cmds[i++] = SH_2DG_WPR;
     cmds[i++] = SH_2DG_REG(1, SH_2DG_DSTRR);
     cmds[i++] = state->dst.pitch / 2; /* only 16bpp is supported */

     if (state->dst.buffer->format == DSPF_RGB16)
          D_FLAGS_CLEAR(dev->rcl, SH_2DG_RCLR_DPF);
     else
          D_FLAGS_SET(dev->rcl, SH_2DG_RCLR_DPF);

     sclma = SH_2DG_XY( state->destination->config.size.w - 1,
                        state->destination->config.size.h - 1 );
     if (sclma != dev->sclma) {
	     cmds[i++]  = SH_2DG_WPR;
	     cmds[i++]  = SH_2DG_REG(1, SH_2DG_SCLMAR);
	     cmds[i++]  = sclma;
             dev->sclma = sclma;
     }

     return write( drv->gfx_fd, cmds, SH_2DG_CMD_SIZE(i) );
}

static int
set_source( SHGfxDriverData *drv,
            SH2dgDeviceData *dev,
            CardState *state )
{
     u32 cmds[6];

     D_ASSERT( (state->src.buffer->format == DSPF_RGB16) ||
               (state->src.buffer->format == DSPF_ARGB1555) );
     D_ASSERT( state->source != NULL );

     cmds[0] = SH_2DG_WPR;
     cmds[1] = SH_2DG_REG(1, SH_2DG_SSAR);
     cmds[2] = state->src.phys;
     cmds[3] = SH_2DG_WPR;
     cmds[4] = SH_2DG_REG(1, SH_2DG_DSTRR);
     cmds[5] = state->src.pitch / 2; /* only 16bpp is supported */

     if (state->src.buffer->format == DSPF_RGB16)
          D_FLAGS_CLEAR(dev->rcl, SH_2DG_RCLR_SPF);
     else
          D_FLAGS_SET(dev->rcl, SH_2DG_RCLR_SPF);

     return write( drv->gfx_fd, cmds, sizeof(cmds) );
}

static int
set_clip( SHGfxDriverData *drv,
          SH2dgDeviceData *dev,
          CardState *state )
{
     u32 cmds[4];

     D_UNUSED_P( dev );

     cmds[0] = SH_2DG_WPR;
     cmds[1] = SH_2DG_REG(2, SH_2DG_UCLMIR);
     cmds[2] = SH_2DG_XY(state->clip.x1, state->clip.y1);
     cmds[3] = SH_2DG_XY(state->clip.x2, state->clip.y2);

     return write( drv->gfx_fd, cmds, sizeof(cmds) );
}

static int
set_alpha( SHGfxDriverData *drv,
           SH2dgDeviceData *dev )
{
     u32 alph = (u32)dev->color.a & SH_2DG_ALPHR_ALPH;
     if (alph != dev->alph) {
          u32 cmds[3];

          cmds[0] = SH_2DG_WPR;
          cmds[1] = SH_2DG_REG( 1, SH_2DG_ALPHR );
          cmds[2] = alph;
          dev->alph = alph;

          return write( drv->gfx_fd, cmds, sizeof(cmds) );
     }
     return 0;
}

static int
set_dst_key( SHGfxDriverData *drv,
             SH2dgDeviceData *dev )
{
     if ( dev->dtc != dev->dst_key ) {
          u32 cmds[3];

          cmds[0] = SH_2DG_WPR;
          cmds[1] = SH_2DG_REG( 1, SH_2DG_DTCR );
          cmds[2] = dev->dst_key;
          dev->dtc = dev->dst_key;

          return write( drv->gfx_fd, cmds, sizeof(cmds) );
     }
     return 0;
}

static int
set_src_key( SHGfxDriverData *drv,
             SH2dgDeviceData *dev )
{
     if ( dev->stc != dev->src_key ) {
          u32 cmds[3];

          cmds[0] = SH_2DG_WPR;
          cmds[1] = SH_2DG_REG( 1, SH_2DG_STCR );
          cmds[2] = dev->src_key;
          dev->stc = dev->src_key;

          return write( drv->gfx_fd, cmds, sizeof(cmds) );
     }
     return 0;
}

static int
set_drawing( SHGfxDriverData *drv,
             SH2dgDeviceData *dev,
             CardState *state )
{
     int ret = 0;

     dev->rop = D_FLAGS_IS_SET( state->drawingflags, DSDRAW_XOR ) ?
                SH_2DG_ROP_XOR : SH_2DG_ROP_COPY;

     if (D_FLAGS_IS_SET( state->drawingflags, DSDRAW_BLEND )) {
          dev->mode |= SH_2DG_DRAWMODE_AE | SH_2DG_DRAWMODE_BLKE;
          dev->rop   = SH_2DG_ROP_COPY;
          ret = set_alpha( drv, dev );
          if (ret < 0)
               goto out;
     }

     if (D_FLAGS_IS_SET( state->drawingflags, DSDRAW_DST_COLORKEY )) {
          dev->mode |= SH_2DG_DRAWMODE_DTRANS;
          ret = set_dst_key( drv, dev );
     }

out:
     return ret;
}

static int
set_blitting( SHGfxDriverData *drv,
              SH2dgDeviceData *dev,
              CardState *state )
{
     int ret = 0;

     if (D_FLAGS_IS_SET( state->blittingflags, DSBLIT_BLEND_ALPHACHANNEL )) {
          if (D_FLAGS_IS_SET( dev->rcl, SH_2DG_RCLR_SPF ))
               dev->mode |= SH_2DG_DRAWMODE_AE | SH_2DG_DRAWMODE_SAE;
          dev->rop = SH_2DG_ROP_COPY;
     }
     else if (D_FLAGS_IS_SET( state->blittingflags, DSBLIT_BLEND_COLORALPHA )) {
          dev->mode |= SH_2DG_DRAWMODE_AE;
          dev->rop   = SH_2DG_ROP_COPY;
          ret = set_alpha( drv, dev );
          if (ret < 0)
               goto out;
     }
     else {
          dev->rop = D_FLAGS_IS_SET( state->blittingflags, DSBLIT_XOR ) ?
                     SH_2DG_ROP_XOR : SH_2DG_ROP_COPY;
     }

     if (D_FLAGS_IS_SET( state->blittingflags, DSBLIT_SRC_COLORKEY )) {
          dev->mode |= SH_2DG_DRAWMODE_STRANS;
          ret = set_src_key( drv, dev );
          if (ret < 0)
               goto out;
     }

     if (D_FLAGS_IS_SET( state->blittingflags, DSBLIT_DST_COLORKEY )) {
          dev->mode |= SH_2DG_DRAWMODE_DTRANS;
          ret = set_dst_key( drv, dev );
          if (ret < 0)
               goto out;
     }

     if (D_FLAGS_IS_SET( state->blittingflags, DSBLIT_FLIP_HORIZONTAL ))
          dev->mode |= SH_2DG_DRAWMODE_SRCDIRX;
     if (D_FLAGS_IS_SET( state->blittingflags, DSBLIT_FLIP_VERTICAL ))
          dev->mode |= SH_2DG_DRAWMODE_SRCDIRY;
out:
     return ret;
}

static int
clear_stencil( const SHGfxDriverData *drv,
               const SH2dgDeviceData *dev )
{
     u32 cmds[3];

     cmds[0] = SH_2DG_CLRWC( 0 );
     cmds[1] = 0;
     cmds[2] = dev->sclma;

     return write( drv->gfx_fd, cmds, sizeof(cmds) );
}

static int
fill_stencil( const SHGfxDriverData *drv,
              const SH2dgDeviceData *dev,
              const DFBRectangle *bounds,
              const DFBPoint *points,
              int num_points)
{
     int ret, i;
     u32 *cmds, size;

     size = SH_2DG_CMD_SIZE( num_points + 5 );
     cmds = (__u32 *)malloc( size );
     if (!cmds)
          return -ENOMEM;

     cmds[0] = SH_2DG_FTRAPC( dev->mode );
     cmds[1] = num_points + 1;
     cmds[2] = SH_2DG_XY( bounds->x, bounds->y );
     cmds[3] = SH_2DG_XY( bounds->x + bounds->w - 1,
                          bounds->y + bounds->h - 1 );
     for (i = 0; i < num_points; i++)
          cmds[4 + i] = SH_2DG_XY( points[i].x, points[i].y );
     cmds[4 + i] = SH_2DG_XY( points[0].x, points[0].y );

     ret = (write( drv->gfx_fd, cmds, size ));
     free( cmds );

     return ret;
}

static int
fill_rect( const SHGfxDriverData *drv,
           const SH2dgDeviceData *dev,
           const DFBRectangle *rect )
{
     u32 cmds[6];

     D_ASSERT( region->x2 > region->x1 );
     D_ASSERT( region->y2 > region->y1 );

     cmds[0] = SH_2DG_BITBLTC( dev->mode );
     cmds[1] = dev->rop;
     cmds[2] = color_to_pixel( &dev->color,
                               D_FLAGS_IS_SET( dev->rcl, SH_2DG_RCLR_DPF ) );
     cmds[3] = rect->w - 1;
     cmds[4] = rect->h - 1;
     cmds[5] = SH_2DG_XY( rect->x, rect->y );

     return write( drv->gfx_fd, cmds, sizeof(cmds) );
}

static int
fill_quad( const SHGfxDriverData *drv,
           const SH2dgDeviceData *dev,
           const DFBPoint points[4])
{
     u32 cmds[6];
     int i;

     cmds[0] = SH_2DG_POLYGON4C( dev->mode );
     cmds[1] = color_to_pixel( &dev->color,
                               D_FLAGS_IS_SET( dev->rcl, SH_2DG_RCLR_DPF ) );
     for (i = 0; i < 4; i++)
          cmds[2 + i] = SH_2DG_XY( points[i].x, points[i].y );

     return write( drv->gfx_fd, cmds, sizeof(cmds) );
}

static int
copy_rect( const SHGfxDriverData *drv,
           const SH2dgDeviceData *dev,
           const DFBRectangle *rect,
           int dx,
           int dy )
{
     u32 cmds[6];

     cmds[0] = SH_2DG_BITBLTA( dev->mode | SH_2DG_DRAWMODE_SS );
     cmds[1] = dev->rop;
     cmds[2] = SH_2DG_XY( rect->x, rect->y );
     cmds[3] = rect->w - 1;
     cmds[4] = rect->h - 1;
     cmds[5] = SH_2DG_XY( dx, dy );

     return write( drv->gfx_fd, cmds, sizeof(cmds) );
}

static int
copy_quad( const SHGfxDriverData *drv,
           const SH2dgDeviceData *dev,
           const DFBRectangle *rect,
           const DFBPoint points[4] )
{
     u32 cmds[8];
     int i;

     cmds[0] = SH_2DG_POLYGON4A(dev->mode | SH_2DG_DRAWMODE_SS);
     cmds[1] = SH_2DG_XY(rect->x, rect->y);
     cmds[2] = SH_2DG_XY(rect->w, rect->h);
     cmds[3] = 0;
     for (i = 0; i < 4; i++)
          cmds[4 + i] = SH_2DG_XY(points[i].x, points[i].y);

     return write( drv->gfx_fd, cmds, sizeof(cmds) );
}

static void
get_bounds( DFBRectangle *rect,
            const DFBPoint *points,
            int num_points )
{
     int i;

     D_ASSERT(num_points > 1);

     rect->x = rect->w = points[0].x;
     rect->y = rect->h = points[0].y;

     for (i = 1; i < num_points; i++) {
          if (rect->x > points[i].x)
               rect->x = points[i].x;
          else if (rect->w < points[i].x)
               rect->w = points[i].x;
          if (rect->y > points[i].y)
               rect->y = points[i].y;
          else if (rect->h < points[i].y)
               rect->h = points[i].y;
     }
     rect->w = (rect->w - rect->x) + 1;
     rect->h = (rect->h - rect->y) + 1;
}

static DFBResult rc_to_result( int rc )
{
     DFBResult ret;

     if (rc >= 0)
          return DFB_OK;

     if (rc == -1)
          rc = -abs(errno);

     switch (rc) {
     case -EINVAL:
          ret = DFB_INVARG;
          break;
     case -ENOMEM:
          ret = DFB_NOSYSTEMMEMORY;
          break;
     case -EINTR:
          ret = DFB_INTERRUPTED;
          break;
     case -EIO:
          ret = DFB_IO;
          break;
     case -EACCES:
          ret = DFB_ACCESSDENIED;
          break;
     case -EBUSY:
          ret = DFB_BUSY;
          break;
     default:
          ret = DFB_FAILURE;
          break;
     }
     return ret;
}

static void
sh_2dg_engine_reset( void *driver_data, void *device_data )
{
}

static DFBResult
sh_2dg_engine_sync( void *driver_data, void *device_data )
{
     SHGfxDriverData *drv;

     D_UNUSED_P( device_data );

     drv = (SHGfxDriverData *)driver_data;

     return rc_to_result(fsync( drv->gfx_fd ));
}

static void
sh_2dg_flush_texture_cache( void *driver_data, void *device_data )
{
     u32 cmds[1];

     SHGfxDriverData *drv;

     D_UNUSED_P( device_data );

     drv = (SHGfxDriverData *)driver_data;

     cmds[0] = SH_2DG_SYNC( SH_2DG_SYNC_TCLR );

     (void)write( drv->gfx_fd, cmds, sizeof(cmds) );
}

static void
sh_2dg_emit_commands( void *driver_data, void *device_data )
{
     u32 cmds[1];

     SHGfxDriverData *drv;

     D_UNUSED_P( device_data );

     drv = (SHGfxDriverData *)driver_data;

     cmds[0] = SH_2DG_SYNC( SH_2DG_SYNC_WCLR |
                            SH_2DG_SYNC_TCLR |
                            SH_2DG_SYNC_DFLSH );

     if ( write( drv->gfx_fd, cmds, sizeof(cmds) ) >= 0 )
	     (void)fdatasync( drv->gfx_fd );
}

static void
sh_2dg_check_state( void *driver_data,
                    void *device_data,
                    CardState *state,
                    DFBAccelerationMask accel )
{
     D_UNUSED_P( driver_data );
     D_UNUSED_P( device_data );

     if (D_FLAGS_INVALID( accel, SH_2DG_SUPPORTED_DRAWINGFUNCTIONS |
                                 SH_2DG_SUPPORTED_BLITTINGFUNCTIONS ))
          return;

     if (!check_pixelformat( state->destination->config.format ))
          return;

     if (DFB_DRAWING_FUNCTION( accel )) { /* drawing */

          if (D_FLAGS_INVALID( state->drawingflags,
                               SH_2DG_SUPPORTED_DRAWINGFLAGS ))
               return;

          switch (state->drawingflags & (DSDRAW_BLEND | DSDRAW_XOR)) {
          case DSDRAW_BLEND:
               if (!check_blend_functions( state ))
                    return;
               break;
          case DSDRAW_NOFX:
          case DSDRAW_XOR:
               break;
          default:
               return;
          }

          state->accel |= SH_2DG_SUPPORTED_DRAWINGFUNCTIONS;
     }
     else { /* blitting */

          if (D_FLAGS_INVALID( state->blittingflags,
                               SH_2DG_SUPPORTED_BLITTINGFLAGS ))
               return;

          if (!check_pixelformat( state->source->config.format ))
               return;

          switch (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                          DSBLIT_BLEND_COLORALPHA |
                                          DSBLIT_XOR)) {
          case DSBLIT_BLEND_ALPHACHANNEL:
          case DSBLIT_BLEND_COLORALPHA:
               if (!check_blend_functions( state ))
                    return;
               break;
          case DSBLIT_NOFX:
          case DSBLIT_XOR:
               break;
          default:
               return;
          }

          state->accel |= SH_2DG_SUPPORTED_BLITTINGFUNCTIONS;
     }

     return;
}

static void
sh_2dg_set_state( void *driver_data,
                  void *device_data,
                  GraphicsDeviceFuncs *funcs,
                  CardState *state,
                  DFBAccelerationMask accel )
{
     SHGfxDriverData *drv;
     SH2dgDeviceData *dev;
     StateModificationFlags modified;

     D_UNUSED_P( funcs );

     drv      = (SHGfxDriverData *)driver_data;
     dev      = (SH2dgDeviceData *)device_data;
     modified = state->mod_hw;

     dev->mode = SH_2DG_DRAWMODE_CLIP;

     if (D_FLAGS_IS_SET( modified, SMF_DESTINATION )) {
          if (set_destination( drv, dev, state ) < 0)
               goto out;
          D_FLAGS_CLEAR( modified, SMF_DESTINATION );
     }
     if (D_FLAGS_IS_SET( modified, SMF_CLIP )) {
          if (set_clip( drv, dev, state ) < 0)
               goto out;
          D_FLAGS_CLEAR( modified, SMF_CLIP );
     }
     if (D_FLAGS_IS_SET( modified, SMF_COLOR )) {
          dev->color = state->color;
          D_FLAGS_CLEAR( modified, SMF_COLOR );
     }
     if (D_FLAGS_IS_SET( modified, SMF_SRC_COLORKEY )) {
          dev->src_key = state->src_colorkey;
          D_FLAGS_CLEAR( modified, SMF_SRC_COLORKEY );
     }
     if (D_FLAGS_IS_SET( modified, SMF_DST_COLORKEY )) {
          dev->dst_key = state->dst_colorkey;
          D_FLAGS_CLEAR( modified, SMF_DST_COLORKEY );
     }

     if (DFB_DRAWING_FUNCTION( accel )) {
          if (set_drawing( drv, dev, state ) < 0)
               goto out;
     }
     else { /* blitting */
          if (D_FLAGS_IS_SET( modified, SMF_SOURCE )) {
               if (set_source( drv, dev, state ) < 0)
                    goto out;
               D_FLAGS_CLEAR( modified, SMF_SOURCE );
          }
          if (set_blitting( drv, dev, state ) < 0)
               goto out;
     }

out:
     return;
}

static bool
sh_2dg_fill_rectangle( void *driver_data,
                       void *device_data,
                       DFBRectangle *rect )
{
     SHGfxDriverData *drv;
     SH2dgDeviceData *dev;

     drv = (SHGfxDriverData *)driver_data;
     dev = (SH2dgDeviceData *)device_data;

     return fill_rect( drv, dev, rect ) >= 0;
}

static bool
sh_2dg_fill_triangle( void *driver_data,
                      void *device_data,
                      DFBTriangle *tri )
{
     int rc;
     SHGfxDriverData *drv;
     SH2dgDeviceData *dev;
     DFBPoint points[4];

     drv = (SHGfxDriverData *)driver_data;
     dev = (SH2dgDeviceData *)device_data;

     points[0].x = tri->x1;
     points[0].y = tri->y1;
     points[1].x = tri->x2;
     points[1].y = tri->y2;
     points[2].x = tri->x3;
     points[2].y = tri->y3;
     points[3].x = tri->x1;
     points[3].y = tri->y1;

     if ((dev->rop == SH_2DG_ROP_COPY) &&
          !D_FLAGS_IS_SET(dev->mode, SH_2DG_DRAWMODE_DTRANS)) {

          rc = fill_quad( drv, dev, points );
     }
     else {
          __u32 mode;
          DFBRectangle rect;

          get_bounds( &rect, points, 3 );

          mode = dev->mode;
          dev->mode &= SH_2DG_DRAWMODE_CLIP | SH_2DG_DRAWMODE_RCLIP;
          rc = clear_stencil( drv, dev );
          if (rc < 0)
               goto out;

          rc = fill_stencil( drv, dev, &rect, points, 3 );
          if (rc < 0)
               goto out;

          dev->mode = mode | SH_2DG_DRAWMODE_WORK;
          rc = fill_rect( drv, dev, &rect );

          dev->mode = mode;
     }

out:
     return (rc >= 0);
}

static bool
sh_2dg_blit( void *driver_data,
             void *device_data,
             DFBRectangle *rect,
             int dx,
             int dy )
{
     int rc;
     SHGfxDriverData *drv;
     SH2dgDeviceData *dev;
 
     rc  = 0;
     drv = (SHGfxDriverData *)driver_data;
     dev = (SH2dgDeviceData *)device_data;

     if (rect->w >= 8) {
          rc = copy_rect( drv, dev, rect, dx, dy );
     }
     else {
          DFBPoint points[4];
          u32 mode;
          int w;

          points[0].x = dx;
          points[0].y = dy;
          points[1].x = dx + rect->w - 1;
          points[1].y = dy;
          points[2].x = dx + rect->w - 1;
          points[2].y = dy + rect->h - 1;
          points[3].x = dx;
          points[3].y = dy + rect->h - 1;

          mode = dev->mode;

          dev->mode &= SH_2DG_DRAWMODE_CLIP | SH_2DG_DRAWMODE_RCLIP;
          rc = clear_stencil( drv, dev );
          if (rc < 0)
               goto out;
          rc = fill_stencil( drv, dev, rect, points, 4 );
          if (rc < 0)
               goto out;

          dev->mode = mode | SH_2DG_DRAWMODE_WORK;
          w         = rect->w;
          rect->w   = 8;
          rc = copy_rect( drv, dev, rect, dx, dy );
          dev->mode = mode;
          rect->w   = w;
     }

out:
     return (rc >= 0);
}

static bool
sh_2dg_stretch_blit( void *driver_data,
                     void *device_data,
                     DFBRectangle *srect,
                     DFBRectangle *drect )
{
     int rc;

     SHGfxDriverData *drv;
     SH2dgDeviceData *dev;
     DFBPoint points[4];

     drv = (SHGfxDriverData *)driver_data;
     dev = (SH2dgDeviceData *)device_data;

     if (srect->w < 8)
          return false;

     points[0].x = drect->x;
     points[0].y = drect->y;
     points[1].x = drect->x + drect->w - 1;
     points[1].y = drect->y;
     points[2].x = drect->x + drect->w - 1;
     points[2].y = drect->y + drect->h - 1;
     points[3].x = drect->x;
     points[3].y = drect->y + drect->h - 1;

     rc = copy_quad( drv, dev, srect, points );

     return (rc >= 0);
}

unsigned int
sh_2dg_device_data_size( void )
{
     return sizeof(SH2dgDeviceData);
}

void
sh_2dg_set_funcs( GraphicsDeviceFuncs *funcs )
{
     funcs->EngineReset       = sh_2dg_engine_reset;
     funcs->EngineSync        = sh_2dg_engine_sync;
     funcs->FlushTextureCache = sh_2dg_flush_texture_cache;
     funcs->EmitCommands      = sh_2dg_emit_commands;
     funcs->CheckState        = sh_2dg_check_state;
     funcs->SetState          = sh_2dg_set_state;
     funcs->FillRectangle     = sh_2dg_fill_rectangle;
     funcs->FillTriangle      = sh_2dg_fill_triangle;
     funcs->Blit              = sh_2dg_blit;
     funcs->StretchBlit       = sh_2dg_stretch_blit;
}

DFBResult
sh_2dg_init( CoreGraphicsDevice *device,
             GraphicsDeviceInfo *device_info,
             void               *driver_data,
             void               *device_data )
{
     SHGfxDriverData  *drv;
     SH2dgDeviceData  *dev;
     struct sh_2dg_reg reg;
     DFBResult result;

     drv = (SHGfxDriverData *)driver_data;
     dev = (SH2dgDeviceData *)device_data;

     snprintf( device_info->name, DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,
               "SH 2DG Graphics Accelerator" );
     snprintf( device_info->name, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH,
               "Renesas" );

     device_info->caps.flags    = CCF_CLIPPING | CCF_RENDEROPTS;
     device_info->caps.accel    = SH_2DG_SUPPORTED_DRAWINGFUNCTIONS |
                                  SH_2DG_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = SH_2DG_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = SH_2DG_SUPPORTED_BLITTINGFLAGS;

     device_info->limits.surface_byteoffset_alignment = 16;
     device_info->limits.surface_pixelpitch_alignment = 64;
     device_info->limits.dst_min.w                    = 256;
     device_info->limits.dst_min.h                    = 1;
     device_info->limits.dst_max.w                    = 2048;
     device_info->limits.dst_max.h                    = 2048;
     device_info->limits.src_min.w                    = 8;
     device_info->limits.src_min.h                    = 1;
     device_info->limits.src_max.w                    = 2048;
     device_info->limits.src_max.h                    = 2048;

     result = create_work_surface( device->core,
                                   &device_info->limits.dst_max,
                                   dev );
     if (result)
          return result;

     reg.offset = SH_2DG_SCLR;
     reg.value  = 0;
     if (ioctl( drv->gfx_fd, SH_2DG_IOC_SETREG, &reg ))
          goto err_io;

     reg.offset = SH_2DG_WSAR;
     reg.value  = dev->work_lock.phys;
     if (ioctl( drv->gfx_fd, SH_2DG_IOC_SETREG, &reg ))
          goto err_io;

     reg.offset = SH_2DG_IER;
     reg.value  = SH_2DG_IER_CEE | SH_2DG_IER_INE | SH_2DG_IER_TRE;
     if (ioctl( drv->gfx_fd, SH_2DG_IOC_SETREG, &reg ))
          goto err_io;

     return DFB_OK;

err_io:
     return DFB_IO;
}

void
sh_2dg_exit( CoreGraphicsDevice *device,
             void               *driver_data,
             void               *device_data )
{
     SHGfxDriverData  *drv;
     SH2dgDeviceData  *dev;
     struct sh_2dg_reg reg;

     D_UNUSED_P( device );

     drv = (SHGfxDriverData *)driver_data;
     dev = (SH2dgDeviceData *)device_data;

     reg.offset = SH_2DG_IER;
     reg.value  = 0;
     (void)ioctl( drv->gfx_fd, SH_2DG_IOC_SETREG, &reg );

     destroy_work_surface( dev );
}


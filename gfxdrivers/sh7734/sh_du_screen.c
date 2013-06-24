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
#include <fusion/call.h>
#include <core/screens_internal.h>
#include <sh_du.h>
#include "sh_gfx.h"
#include "sh_du_screen.h"
#include "sh_du_reg.h"

#define SH_DU_READ(fd, reg)            ioctl((fd), SH_DU_IOC_GETREG, &(reg))
#define SH_DU_WRITE(fd, reg)           ioctl((fd), SH_DU_IOC_SETREG, &(reg))

#define SH_DU_SIZE(w, h)               (((w) << 16) | (h))

#define SH_DU_COLOR_MASK               (0x00FCFCFC)

#define SH_DU_COLOR_TO_XRGB8888(c)     (((u32)(c).r << 16) | \
                                        ((u32)(c).g <<  8) | \
                                        ((u32)(c).b <<  0))

#define SH_DU_ALL_LAYERS               ((1u << SH_GFX_NUM_LAYERS) - 1u)
#define SH_DU_LAYER_VISIBLE            0x08
#define SH_DU_LAYER_PLANE              0x07
#define SH_DU_LAYER_INVALID_VALUE      0xF7
#define SH_DU_LAYER_INVALID_PRIORITY   0xFF

#define SH_DU_LAYER_VALUE_IS_VALID(v)      ((v) <= 0x0F)
#define SH_DU_LAYER_PLANE_IS_VALID(p)      ((p) <= 0x07)
#define SH_DU_LAYER_PRIORITY_IS_VALID(p)   ((p) <= 0x07)

#define SH_DU_LAYER_LEVEL_MIN              (0)
#define SH_DU_LAYER_LEVEL_MAX              (7)
#define SH_DU_LAYER_LEVEL_TO_PRIORITY(l)   (u8)(7 - (l))
#define SH_DU_LAYER_PRIORITY_TO_LEVEL(p)   (7 - (int)(p))

D_DEBUG_DOMAIN( SH_DU_SCREEN, "SH-DU/Screen", "Renesas DU Screen" );

enum SHDuTimingReg {
     HDS,
     HDE,
     VDS,
     VDE,
     HC,
     HSW,
     VC,
     VSP,
     ESC,
     NUM_TIMING_REGS,
};

typedef struct {
     DFBDimension size;
     u32          bg_color;
     struct {
          u8 values[8]; /* priority order */
          u8 priorities[SH_GFX_NUM_LAYERS]; /* id order */
     } layer;
} SHDuScreenData;

static DFBDisplayLayerIDs
added_layers( const SHDuScreenData *screen )
{
     DFBDisplayLayerIDs ret;
     DFBDisplayLayerID  id;

     DFB_DISPLAYLAYER_IDS_EMPTY( ret );
     for (id = 0; id < SH_GFX_NUM_LAYERS; id++) {
          if (SH_DU_LAYER_PRIORITY_IS_VALID( screen->layer.priorities[id] ))
               DFB_DISPLAYLAYER_IDS_ADD( ret, id );
     }
     return ret;
}

static DFBDisplayLayerIDs
visible_layers( const SHDuScreenData *screen )
{
     DFBDisplayLayerIDs ret;
     DFBDisplayLayerID  id;
     u8                 pri;

     DFB_DISPLAYLAYER_IDS_EMPTY( ret );
     for (id = 0; id < SH_GFX_NUM_LAYERS; id++) {
          pri = screen->layer.priorities[id];
          if (SH_DU_LAYER_PRIORITY_IS_VALID( pri ) &&
              D_FLAGS_IS_SET( screen->layer.values[pri], SH_DU_LAYER_VISIBLE ))
               DFB_DISPLAYLAYER_IDS_ADD( ret, id );
     }
     return ret;
}

static int
update_layers( const SHGfxDriverData *drv,
               const SHDuScreenData *screen )
{
     struct sh_du_reg reg;
     int              i;

     reg.offset = SH_DU_DPPR;
     reg.value  = 0u;
     for (i = 0; i < 8; i++)
          reg.value |= (u32)(screen->layer.values[i] & 0x0F) << (4 * i);

     return SH_DU_WRITE( drv->dpy_fd, reg );
}

static DFBScreenOutputResolution
size_to_resolution(const DFBDimension *size)
{
     DFBScreenOutputResolution ret;

     switch (SH_DU_SIZE(size->w, size->h)) {
     case SH_DU_SIZE(640, 480):
          ret = DSOR_640_480;
          break;
     case SH_DU_SIZE(800, 600):
          ret = DSOR_800_600;
          break;
     case SH_DU_SIZE(1024, 768):
          ret = DSOR_1024_768;
          break;
     case SH_DU_SIZE(1280, 768):
          ret = DSOR_1280_768;
          break;
     default:
          ret = DSOR_UNKNOWN;
          break;
     }

     return ret;
}

static int
sh_du_screen_data_size( void )
{
     D_DEBUG_AT( SH_DU_SCREEN, "%s()\n", __FUNCTION__ );
     return sizeof(SHDuScreenData);
}

static DFBResult
sh_du_init_screen( CoreScreen           *screen,
                   CoreGraphicsDevice   *device,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     D_DEBUG_AT( SH_DU_SCREEN, "%s()\n", __FUNCTION__ );
     D_UNUSED_P( screen );
     D_UNUSED_P( device );
     D_UNUSED_P( driver_data );
     D_UNUSED_P( screen_data );

     description->caps     = DSCCAPS_VSYNC | DSCCAPS_MIXERS | DSCCAPS_OUTPUTS;
     description->mixers   = 1;
     description->encoders = 0;
     description->outputs  = 1;
     snprintf( description->name, DFB_SCREEN_DESC_NAME_LENGTH,
               "SH DU Screen" );

     return DFB_OK;
}

static DFBResult
sh_du_shutdown_screen( CoreScreen *screen,
                        void       *driver_data,
                        void       *screen_data )
{
     D_DEBUG_AT( SH_DU_SCREEN, "%s()\n", __FUNCTION__ );
     D_UNUSED_P( screen );
     D_UNUSED_P( driver_data );
     D_UNUSED_P( screen_data );

     return DFB_OK;
}

static DFBResult
sh_du_init_mixer( CoreScreen                *screen,
                  void                      *driver_data,
                  void                      *screen_data,
                  int                        mixer,
                  DFBScreenMixerDescription *description,
                  DFBScreenMixerConfig      *config )
{
     SHGfxDriverData *drv;
     SHDuScreenData  *scr;
     struct sh_du_reg reg;
     int i;

     D_DEBUG_AT( SH_DU_SCREEN, "%s( %d )\n", __FUNCTION__, mixer );
     D_UNUSED_P( screen );

     if (mixer != 0)
          return DFB_INVARG;

     drv = (SHGfxDriverData *)driver_data;
     scr = (SHDuScreenData *)screen_data;

     description->caps       = DSMCAPS_SUB_LAYERS | DSMCAPS_BACKGROUND;
     description->layers     = SH_DU_ALL_LAYERS;
     description->sub_num    = SH_GFX_NUM_LAYERS;
     description->sub_layers = SH_DU_ALL_LAYERS;
     snprintf( description->name, DFB_SCREEN_MIXER_DESC_NAME_LENGTH,
               "SH DU Mixer" );

     /* enable pixelalpha blending in ARGB1555 format */
     reg.offset = SH_DU_DEFR2;
     reg.value  = SH_DU_DEFR2_CODE | SH_DU_DEFR2_DEFE2G;
     if (SH_DU_WRITE( drv->dpy_fd, reg ))
          goto err;

     /* initialize layers */
     for (i = 0; i < 8; i++)
          scr->layer.values[i] = SH_DU_LAYER_INVALID_VALUE;
     for (i = 0; i < SH_GFX_NUM_LAYERS; i++)
          scr->layer.priorities[i] = SH_DU_LAYER_INVALID_PRIORITY;
     if (update_layers( drv, scr ))
          goto err;

     /* get background color */
     reg.offset = SH_DU_BPOR;
     if (SH_DU_READ( drv->dpy_fd, reg ))
          goto err;
     scr->bg_color = reg.value;

     config->flags        = DSMCONF_TREE | DSMCONF_LAYERS | DSMCONF_BACKGROUND;
     config->tree         = DSMT_SUB_LAYERS;
     config->level        = 0;
     config->layers       = 0;
     config->background.a =  0xFF;
     config->background.r = (scr->bg_color >> 16) & 0xFF;
     config->background.g = (scr->bg_color >>  8) & 0xFF;
     config->background.b = (scr->bg_color >>  0) & 0xFF;

     return DFB_OK;

err:
     return DFB_IO;
}

static DFBResult
sh_du_init_output( CoreScreen *screen,
                   void       *driver_data,
                   void       *screen_data,
                   int         output,
                   DFBScreenOutputDescription *description,
                   DFBScreenOutputConfig      *config )
{
     SHGfxDriverData *drv;
     SHDuScreenData  *scr;
     struct sh_du_reg reg;

     D_DEBUG_AT( SH_DU_SCREEN, "%s( %d )\n", __FUNCTION__, output );
     D_UNUSED_P( screen );

     if (output != 0)
          return DFB_INVARG;

     drv = (SHGfxDriverData *)driver_data;
     scr = (SHDuScreenData *)screen_data;

     description->caps            = DSOCAPS_RESOLUTION;
     description->all_connectors  = DSOC_HDMI;
     description->all_signals     = DSOS_HDMI;
     description->all_resolutions = DSOR_640_480 |
                                    DSOR_800_600 |
                                    DSOR_1024_768 |
                                    DSOR_1280_768;
     snprintf( description->name, DFB_SCREEN_OUTPUT_DESC_NAME_LENGTH,
               "SH DU Output" );

     /* get width and height */
     reg.offset = SH_DU_HDER;
     if (SH_DU_READ( drv->dpy_fd, reg ))
          goto err;
     scr->size.w = (int)reg.value;
     reg.offset = SH_DU_HDSR;
     if (SH_DU_READ( drv->dpy_fd, reg ))
          goto err;
     scr->size.w -= (int)reg.value;
     reg.offset = SH_DU_VDER;
     if (SH_DU_READ( drv->dpy_fd, reg ))
          goto err;
     scr->size.h = (int)reg.value;
     reg.offset = SH_DU_VDSR;
     if (SH_DU_READ( drv->dpy_fd, reg ))
          goto err;
     scr->size.h -= (int)reg.value;

     config->flags          = DSOCONF_RESOLUTION;
     config->encoder        = 0;
     config->out_signals    = DSOS_HDMI;
     config->out_connectors = DSOC_HDMI;
     config->slow_blanking  = DSOSB_OFF;
     config->resolution     = size_to_resolution(&scr->size);

     /* enable VSYNC interrupt */
     reg.offset = SH_DU_DSRCR;
     reg.value  = SH_DU_DSRCR_VBCL;
     if (SH_DU_WRITE( drv->dpy_fd, reg ))
          goto err;
     reg.offset = SH_DU_DIER;
     reg.value  = SH_DU_DIER_VBE;
     if (SH_DU_WRITE( drv->dpy_fd, reg ))
          goto err;

     return DFB_OK;

err:
     return DFB_IO;
}

static DFBResult
sh_du_wait_vsync( CoreScreen *screen,
                  void       *driver_data,
                  void       *screen_data )
{
     SHGfxDriverData *drv;
     struct sh_du_status status;

     D_DEBUG_AT( SH_DU_SCREEN, "%s()\n", __FUNCTION__ );
     D_UNUSED_P( screen );
     D_UNUSED_P( screen_data );

     drv = (SHGfxDriverData *)driver_data;
     status.flags = SH_DU_DSSR_VBK;
     status.mode  = SH_DU_WAIT_OR | SH_DU_CLEAR_BEFORE;
     return ioctl( drv->dpy_fd, SH_DU_IOC_WAIT, &status ) ? DFB_IO : DFB_OK;
}

static DFBResult
sh_du_test_mixer_config( CoreScreen                 *screen,
                         void                       *driver_data,
                         void                       *screen_data,
                         int                         mixer,
                         const DFBScreenMixerConfig *config,
                         DFBScreenMixerConfigFlags  *failed )
{
     SHDuScreenData            *scr;
     DFBScreenMixerConfigFlags  fail;

     D_DEBUG_AT( SH_DU_SCREEN, "%s( %d )\n", __FUNCTION__, mixer );
     D_UNUSED_P( screen );
     D_UNUSED_P( driver_data );
     D_UNUSED_P( screen_data );

     if (mixer != 0)
          return DFB_INVARG;

     scr  = (SHDuScreenData *)screen_data;
     fail = DSMCONF_NONE;

     if (D_FLAGS_IS_SET( config->flags, DSMCONF_TREE ) &&
         (config->tree != DSMT_SUB_LAYERS))
          fail |= DSMCONF_TREE;

     if (D_FLAGS_IS_SET( config->flags, DSMCONF_LEVEL ))
          fail |= DSMCONF_LEVEL;

     if (D_FLAGS_IS_SET( config->flags, DSMCONF_LAYERS ) &&
         D_FLAGS_INVALID( config->layers, added_layers( scr ) ))
          fail |= DSMCONF_LAYERS;

     if (failed)
          *failed = fail;

     return (fail == DSMCONF_NONE) ? DFB_OK : DFB_UNSUPPORTED;
}

static DFBResult
sh_du_set_mixer_config( CoreScreen                 *screen,
                        void                       *driver_data,
                        void                       *screen_data,
                        int                         mixer,
                        const DFBScreenMixerConfig *config )
{
     SHGfxDriverData *drv;
     SHDuScreenData  *scr;

     D_DEBUG_AT( SH_DU_SCREEN, "%s( %d )\n", __FUNCTION__, mixer );
     D_UNUSED_P( screen );

     if (mixer != 0)
          return DFB_INVARG;

     drv = (SHGfxDriverData *)driver_data;
     scr = (SHDuScreenData *)screen_data;

     if (D_FLAGS_IS_SET( config->flags, DSMCONF_LAYERS )) {

          if (D_FLAGS_INVALID( config->layers, added_layers( scr ) ))
               return DFB_INVARG;

          if (config->layers != visible_layers( scr )) {
               DFBDisplayLayerID id;
               u8                pri;

               for (id = 0; id < SH_GFX_NUM_LAYERS; id++) {
                    pri = scr->layer.priorities[id];
                    if (SH_DU_LAYER_PRIORITY_IS_VALID( pri )) {
                         if (DFB_DISPLAYLAYER_IDS_HAVE( config->layers, id ))
                              scr->layer.values[pri] |= SH_DU_LAYER_VISIBLE;
                         else
                              scr->layer.values[pri] &= ~SH_DU_LAYER_VISIBLE;
                    }
               }
               if (update_layers( drv, scr ))
                    goto err;
          }
     }

     if (D_FLAGS_IS_SET( config->flags, DSMCONF_BACKGROUND )) {
          struct sh_du_reg reg;

          reg.value = SH_DU_COLOR_TO_XRGB8888( config->background ) &
                      SH_DU_COLOR_MASK;
          if (reg.value != scr->bg_color) {
               reg.offset = SH_DU_BPOR;
               if (SH_DU_WRITE(drv->dpy_fd, reg))
                    goto err;
               scr->bg_color = reg.value;
          }
     }

     return DFB_OK;

err:
     return DFB_IO;
}

static DFBResult
sh_du_test_output_config( CoreScreen                  *screen,
                          void                        *driver_data,
                          void                        *screen_data,
                          int                          output,
                          const DFBScreenOutputConfig *config,
                          DFBScreenOutputConfigFlags  *failed )
{
     DFBScreenOutputConfigFlags fail;

     D_DEBUG_AT( SH_DU_SCREEN, "%s( %d )\n", __FUNCTION__, output );
     D_UNUSED_P( screen );
     D_UNUSED_P( driver_data );
     D_UNUSED_P( screen_data );

     if (output != 0)
          return DFB_INVARG;

     fail = DSOCONF_NONE;

     if (D_FLAGS_IS_SET( config->flags, DSOCONF_ENCODER ))
          fail |= DSOCONF_ENCODER;

     if (D_FLAGS_IS_SET( config->flags, DSOCONF_SIGNALS ) &&
         (config->out_signals != DSOS_HDMI))
          fail |= DSOCONF_SIGNALS;

     if (D_FLAGS_IS_SET( config->flags, DSOCONF_CONNECTORS ) &&
         (config->out_connectors != DSOC_HDMI))
          fail |= DSOCONF_CONNECTORS;

     if (D_FLAGS_IS_SET( config->flags, DSOCONF_SLOW_BLANKING ))
          fail |= DSOCONF_SLOW_BLANKING;

     if (D_FLAGS_IS_SET( config->flags, DSOCONF_RESOLUTION )) {
          switch (config->resolution) {
          case DSOR_640_480:
          case DSOR_800_600:
          case DSOR_1024_768:
          case DSOR_1280_768:
               break;
          default:
               fail |= DSOCONF_RESOLUTION;
               break;
          }
     }

     if (failed)
          *failed = fail;

     return (fail == DSOCONF_NONE) ? DFB_OK : DFB_UNSUPPORTED;
}

static DFBResult
sh_du_set_output_config( CoreScreen                  *screen,
                         void                        *driver_data,
                         void                        *screen_data,
                         int                          output,
                         const DFBScreenOutputConfig *config )
{
     SHGfxDriverData *drv;
     SHDuScreenData  *scr;

     D_DEBUG_AT( SH_DU_SCREEN, "%s( %d )\n", __FUNCTION__ , output );
     D_UNUSED_P( screen );

     if (output != 0)
          return DFB_INVARG;

     drv = (SHGfxDriverData *)driver_data;
     scr = (SHDuScreenData *)screen_data;

     if (D_FLAGS_IS_SET( config->flags, DSOCONF_RESOLUTION ) &&
         (size_to_resolution( &scr->size ) != config->resolution)) {
          struct sh_du_reg dsys, timings[NUM_TIMING_REGS];
          int i;

          switch (config->resolution) {
          case DSOR_640_480:
               timings[HDS].value = 125;
               timings[HDE].value = 765;
               timings[VDS].value = 31;
               timings[VDE].value = 511;
               timings[HC].value  = 799;
               timings[HSW].value = 95;
               timings[VC].value  = 524;
               timings[VSP].value = 522;
               timings[ESC].value = SH_DU_ESCR_FRQSEL(8);
               break;
          case DSOR_800_600:
               timings[HDS].value = 197;
               timings[HDE].value = 997;
               timings[VDS].value = 21;
               timings[VDE].value = 621;
               timings[HC].value  = 1055;
               timings[HSW].value = 127;
               timings[VC].value  = 627;
               timings[VSP].value = 623;
               timings[ESC].value = SH_DU_ESCR_FRQSEL(5);
               break;
          case DSOR_1024_768:
               timings[HDS].value = 277;
               timings[HDE].value = 1301;
               timings[VDS].value = 27;
               timings[VDE].value = 795;
               timings[HC].value  = 1343;
               timings[HSW].value = 135;
               timings[VC].value  = 805;
               timings[VSP].value = 799;
               timings[ESC].value = SH_DU_ESCR_FRQSEL(3);
               break;
          case DSOR_1280_768:
               timings[HDS].value = 93;
               timings[HDE].value = 1373;
               timings[VDS].value = 10;
               timings[VDE].value = 778;
               timings[HC].value  = 1439;
               timings[HSW].value = 31;
               timings[VC].value  = 789;
               timings[VSP].value = 782;
               timings[ESC].value = SH_DU_ESCR_FRQSEL(3);
               break;
          default:
               return DFB_INVARG;
          }

          timings[HDS].offset = SH_DU_HDSR;
          timings[HDE].offset = SH_DU_HDER;
          timings[VDS].offset = SH_DU_VDSR;
          timings[VDE].offset = SH_DU_VDER;
          timings[HC].offset  = SH_DU_HCR;
          timings[HSW].offset = SH_DU_HSWR;
          timings[VC].offset  = SH_DU_VCR;
          timings[VSP].offset = SH_DU_VSPR;
          timings[ESC].offset = SH_DU_ESCR;
          timings[ESC].value |= SH_DU_ESCR_DCLKSEL;

          dsys.value = 0;
          dsys.offset = SH_DU_DSYSR;
          if (SH_DU_READ( drv->dpy_fd, dsys ))
               goto err;
          dsys.value |= SH_DU_DSYSR_IUPD;
          if (SH_DU_WRITE( drv->dpy_fd, dsys ))
               goto err;
          dsys.value &= ~SH_DU_DSYSR_IUPD;
          for (i = 0; i < NUM_TIMING_REGS; i++) {
               if (SH_DU_WRITE( drv->dpy_fd, timings[i])) {
                    (void)SH_DU_WRITE( drv->dpy_fd, dsys );
                    goto err;
               }
          }
          scr->size.w = (int)timings[HDE].value - (int)timings[HDS].value;
          scr->size.h = (int)timings[VDE].value - (int)timings[VDS].value;

          if (SH_DU_WRITE( drv->dpy_fd, dsys ))
               goto err;
     }
     return DFB_OK;

err:
     return DFB_IO;
}

static DFBResult
sh_du_get_screen_size( CoreScreen *screen,
                       void       *driver_data,
                       void       *screen_data,
                       int        *ret_width,
                       int        *ret_height )
{
     SHDuScreenData *scr;

     D_DEBUG_AT( SH_DU_SCREEN, "%s()\n", __FUNCTION__ );
     D_UNUSED_P( screen );
     D_UNUSED_P( driver_data );

     scr = (SHDuScreenData *)screen_data;

     *ret_width  = scr->size.w;
     *ret_height = scr->size.h;

     return DFB_OK;
}

static const ScreenFuncs sh_du_screen_funcs = {
     .ScreenDataSize   = sh_du_screen_data_size,
     .InitScreen       = sh_du_init_screen,
     .ShutdownScreen   = sh_du_shutdown_screen,
     .InitMixer        = sh_du_init_mixer,
     .InitOutput       = sh_du_init_output,
     .WaitVSync        = sh_du_wait_vsync,
     .TestMixerConfig  = sh_du_test_mixer_config,
     .SetMixerConfig   = sh_du_set_mixer_config,
     .TestOutputConfig = sh_du_test_output_config,
     .SetOutputConfig  = sh_du_set_output_config,
     .GetScreenSize    = sh_du_get_screen_size,
};

CoreScreen *
sh_du_screen_register( CoreGraphicsDevice *device, void *driver_data)
{
     return dfb_screens_register( device, driver_data, &sh_du_screen_funcs );
}

DFBResult
sh_du_add_layer( CoreScreen *screen,
                 void *driver_data,
                 DFBDisplayLayerID  id,
                 u8 plane,
                 int *level )
{
     SHGfxDriverData *drv;
     SHDuScreenData  *scr;
     u8               pri;

     drv = (SHGfxDriverData *)driver_data;
     scr = (SHDuScreenData *)screen->screen_data;

     D_ASSERT( id < SH_GFX_NUM_LAYERS );
     D_ASSERT( SH_DU_LAYER_PLANE_IS_VALID( plane ));

     if (SH_DU_LAYER_PRIORITY_IS_VALID( scr->layer.priorities[id] ))
          return DFB_BUSY; /* already added */

     pri = plane; /* default priority is plane id */

     if (SH_DU_LAYER_VALUE_IS_VALID( scr->layer.values[pri] )) {
         while (++pri < D_ARRAY_SIZE( scr->layer.values ) &&
                SH_DU_LAYER_VALUE_IS_VALID( scr->layer.values[pri] ));
     }
     if (pri >= D_ARRAY_SIZE( scr->layer.values )) {
          pri = plane;
          while ((pri > 0) &&
                 SH_DU_LAYER_PRIORITY_IS_VALID( scr->layer.values[pri] ))
               pri--;
     }
     if (SH_DU_LAYER_VALUE_IS_VALID( scr->layer.values[pri] ))
          return DFB_BUSY; /* all priorities assigned */

     scr->layer.priorities[id] = pri;
     scr->layer.values[pri]    = plane;

     return update_layers( drv, scr ) ? DFB_IO : DFB_OK;
}

DFBResult
sh_du_remove_layer( CoreScreen *screen,
                    void *driver_data,
                    DFBDisplayLayerID id )
{
     SHGfxDriverData *drv;
     SHDuScreenData  *scr;
     u8               pri;

     D_ASSERT( id < SH_GFX_NUM_LAYERS );

     drv = (SHGfxDriverData *)driver_data;
     scr = (SHDuScreenData *)screen->screen_data;
     pri = scr->layer.priorities[id];

     if (!SH_DU_LAYER_PRIORITY_IS_VALID( pri ))
          return DFB_OK; /* already removed */

     scr->layer.priorities[id] = SH_DU_LAYER_INVALID_PRIORITY;
     scr->layer.values[pri]    = SH_DU_LAYER_INVALID_VALUE;

     return update_layers( drv, scr ) ? DFB_IO : DFB_OK;
}

DFBResult
sh_du_show_layer( CoreScreen *screen,
                  void *driver_data,
                  DFBDisplayLayerID id )
{
     SHGfxDriverData *drv;
     SHDuScreenData  *scr;
     u8 pri;

     D_ASSERT( id < SH_GFX_NUM_LAYERS );

     drv = (SHGfxDriverData *)driver_data;
     scr = (SHDuScreenData *)screen->screen_data;
     pri = scr->layer.priorities[id];

     if (!SH_DU_LAYER_PRIORITY_IS_VALID( pri ))
          return DFB_INVARG;

     if (D_FLAGS_IS_SET( scr->layer.values[pri], SH_DU_LAYER_VISIBLE ))
          return DFB_OK;

     D_ASSERT( SH_DU_LAYER_VALUE_IS_VALID( scr->layer.values[pri] ) );
     scr->layer.values[pri] |= SH_DU_LAYER_VISIBLE;

     return update_layers( drv, scr ) ? DFB_IO : DFB_OK;
}

DFBResult
sh_du_change_layer_level( CoreScreen *screen,
                          void *driver_data,
                          DFBDisplayLayerID id,
                          int level )
{
     SHGfxDriverData *drv;
     SHDuScreenData  *scr;
     u8 current, pri;

     D_ASSERT( id < SH_GFX_NUM_LAYERS );
     if ((level < SH_DU_LAYER_LEVEL_MIN) || (level > SH_DU_LAYER_LEVEL_MAX))
          return DFB_INVARG;

     drv     = (SHGfxDriverData *)driver_data;
     scr     = (SHDuScreenData *)screen->screen_data;
     current = scr->layer.priorities[id];
     pri     = SH_DU_LAYER_LEVEL_TO_PRIORITY( level );

     if ( !SH_DU_LAYER_PRIORITY_IS_VALID( current ))
          return DFB_INVARG;

     if ( current == pri )
          return DFB_OK;

     if (SH_DU_LAYER_VALUE_IS_VALID( scr->layer.values[pri] ))
          return DFB_BUSY;

     scr->layer.values[pri]     = scr->layer.values[current];
     scr->layer.values[current] = SH_DU_LAYER_INVALID_VALUE;
     scr->layer.priorities[id]  = pri;

     return update_layers( drv, scr ) ? DFB_IO : DFB_OK;
}


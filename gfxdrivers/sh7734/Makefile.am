## Makefile.am for DirectFB/gfxdrivers/sh7734

SUBDIRS = modules

EXTRA_DIST = \
	README.sh7734		\
	directfbrc.sh7734	\
	modules/Makefile.in	\
	modules/sh_2dg.c	\
	modules/sh_2dg.h	\
	modules/sh_du.c		\
	modules/sh_du.h

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/systems

sh7734_LTLIBRARIES = libdirectfb_sh7734.la

if BUILD_STATIC
sh7734_DATA = $(sh7734_LTLIBRARIES:.la=.o)
endif

sh7734dir = $(MODULEDIR)/gfxdrivers

libdirectfb_sh7734_la_SOURCES = \
	sh7734.c	\
	sh_gfx.h	\
	sh_2dg_gfx.c	\
	sh_2dg_gfx.h	\
	sh_2dg_reg.h	\
	sh_du_screen.c	\
	sh_du_screen.h	\
        sh_du_layer.c	\
        sh_du_layer.h	\
	sh_du_reg.h

libdirectfb_sh7734_la_LDFLAGS = \
	-module		\
	-avoid-version	\
	$(DFB_LDFLAGS)

libdirectfb_sh7734_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la	\
	$(top_builddir)/src/libdirectfb.la

include $(top_srcdir)/rules/libobject.make

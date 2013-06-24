## Makefile.am for DirectFB/gfxdrivers/sh7734

SUBDIRS = kernel-module

EXTRA_DIST = \
	README.sh7734			\
	directfbrc			\
	kernel-module/Makefile.in	\
	kernel-module/sh_2dg.c		\
	kernel-module/sh_2dg.h		\
	kernel-module/sh_du.c		\
	kernel-module/sh_du.h

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/systems		\
	-I$(srcdir)/kernel-module

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

sh7734confdir = $(SYSCONFDIR)
sh7734conf_DATA = directfbrc

include $(top_srcdir)/rules/libobject.make

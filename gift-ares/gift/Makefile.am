# $Id: Makefile.am,v 1.1 2004/11/28 21:10:03 mkern Exp $
AM_CFLAGS = -Wall

plugin_LTLIBRARIES = libAres.la

libAres_la_SOURCES = \
	as_gift_plugin.c

libAres_la_LIBADD = \
	$(top_builddir)/lib/libaresgift.la

INCLUDES = \
	@GIFT_CFLAGS@ \
	-I$(top_srcdir)/lib

libAres_la_LDFLAGS = @GIFT_LIBS@ -lm -module -avoid-version
# -Wl,-z,defs

#EXTRA_DIST = Makefile.msvc

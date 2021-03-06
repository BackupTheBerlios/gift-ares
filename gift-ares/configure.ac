# -*- sh -*-

# $Id: configure.ac,v 1.10 2005/12/02 19:08:18 mkern Exp $

dnl ###########################################################################
dnl Process this file with autoconf to produce a configure script.
dnl ###########################################################################

AC_PREREQ(2.52)

AC_INIT(gift-ares, 0.3.0)
AC_REVISION($Revision: 1.10 $)
AC_CONFIG_SRCDIR([lib/as_ares.h])

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

dnl AM_MAINTAINER_MODE

dnl ###########################################################################
dnl Build information
dnl ###########################################################################

AC_DEFINE_UNQUOTED(BUILD_DATE, "`date`", [Build date])
AC_DEFINE(GIFT_PLUGIN, 1, [This is a giFT plugin])

dnl ###########################################################################
dnl Checks for build tools and configure libtool.
dnl ###########################################################################

AC_PROG_CC
AC_HEADER_STDC
AC_PROG_INSTALL

AM_DISABLE_STATIC
AC_PROG_LIBTOOL

dnl ###########################################################################
dnl Checks for libraries.
dnl ###########################################################################

GIFT_PLUGIN_PREFIX
GIFT_PLUGIN_PKGCONFIG(GIFT, 0.11.8, 0.12.0)

dnl ###########################################################################
dnl Checks for header files.
dnl ###########################################################################

AC_HEADER_STDC

dnl ###########################################################################
dnl Checks for typedefs, structures, and compiler characteristics.
dnl ###########################################################################

AC_C_CONST
AC_HEADER_TIME
AC_STRUCT_TM
AC_HAVE_INADDR_NONE

dnl ###########################################################################
dnl Checks for library functions.
dnl ###########################################################################

AC_FUNC_MEMCMP
AC_CHECK_FUNCS(strdup)

dnl ###########################################################################
dnl All of the lovely arguments
dnl ###########################################################################

AC_ARG_ENABLE(debug,              [  --disable-debug         do not create debug messages],,[enable_debug=yes])
AM_CONDITIONAL(DEBUG,              test x$enable_debug    = xyes)

AC_ARG_ENABLE(heavy_debug,        [  --enable-heavy-debug    create _lots_ debug messages],,[enable_heavy_debug=no])
AM_CONDITIONAL(HEAVY_DEBUG,        test x$enable_heavy_debug    = xyes)

dnl ###########################################################################
dnl normal debug
dnl ###########################################################################

use_debug=no
if test x$enable_debug = xyes; then
	AC_DEFINE(DEBUG, 1, [Enable debugging])
	use_debug=yes
fi

dnl ###########################################################################
dnl heavy debug
dnl ###########################################################################

use_heavy_debug=no
if test x$enable_heavy_debug = xyes; then
	AC_DEFINE(HEAVY_DEBUG, 1, [Enable heavy debugging])
	use_heavy_debug=yes
fi

dnl ###########################################################################
dnl Check for zlib...
dnl ###########################################################################

# set the minimum ZLIB_VER we're willing to link against...
ZLIB_VER=1.1.3

dnl Default to compiler & linker defaults for files & libraries.
OPT_ZLIB=
AC_ARG_WITH(zlib,dnl
[  --with-zlib[=DIR]         where to look for ZLIB [compiler/linker default paths]
                          DIR points to the ZLIB installation prefix [/usr/local]],
 OPT_ZLIB=$withval,
)

#AC_MSG_CHECKING(where to look for ZLIB)
if test X"$OPT_ZLIB" = Xno
then
  zlib_ok=no
else
  ZLIB_DIRS="$OPT_ZLIB /usr /usr/local /sw"
  for ZLIB_DIR in $ZLIB_DIRS;
  do
    LIBS_SAVE="$LIBS"
    CPPFLAGS_SAVE="$CPPFLAGS"
    LIBS="$LIBS -L${ZLIB_DIR}/lib"
    CPPFLAGS="$CPPFLAGS -I${ZLIB_DIR}/include"
    AC_CACHE_CHECK(
      [for zlib version ${ZLIB_VER}+ in ${ZLIB_DIR}],
      zlib_ok,
      AC_TRY_RUN(
        [#include <zlib.h>
         #include <string.h>
         void main() {
           exit(strcmp(ZLIB_VERSION, "${ZLIB_VER}") < 0);
         }
         ],
        [zlib_ok=yes],
        [zlib_ok=no],
        [zlib_ok=yes]))

    if test "$zlib_ok" != "no"; then
      AC_CHECK_FUNC(gzread, , AC_CHECK_LIB(z, gzread))
      break
    fi
    LIBS="$LIBS_SAVE"
    CPPFLAGS="$CPPFLAGS_SAVE"
  done

  if test "$zlib_ok" = "no"; then
    AC_MSG_ERROR([
NOTE: The zlib compression library version ${ZLIB_VER} or greater was not found
on your system.

If zlib ${ZLIB_VER}+ is not installed, install it.
    ])
  fi
fi

dnl ###########################################################################
dnl Configure installation paths
dnl ###########################################################################

GIFT_PLUGIN_INSTALL_PATH

dnl ###########################################################################
dnl Finally do the generation
dnl ###########################################################################

dnl Needed to build with libtoolize 1.4.3 on Debian woody
AC_CONFIG_AUX_DIR(.)

AC_CONFIG_FILES([
  Makefile
  lib/Makefile
  gift/Makefile
  data/Makefile
  m4/Makefile
  win32/Makefile
])
AC_OUTPUT

dnl ###########################################################################
dnl Print summary to the user
dnl ###########################################################################

echo
echo "$PACKAGE $VERSION"
echo
echo "------------------ core ------------------"
echo
echo "normal debugging........: $use_debug"
echo "heavy debugging.........: $use_heavy_debug"
echo
echo "------------------------------------------"
echo
echo "$PACKAGE will be installed in $plugindir/"
echo "configs will be installed in $datadir/"
echo

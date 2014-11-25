
AC_PREREQ([2.63])
AC_INIT([collectps], [0.0.1], [ymettier@free.fr])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_CC_C_O

AM_CONDITIONAL(COMPILER_IS_GCC, test "x$GCC" = "xyes")

# Check for htonll
AC_CACHE_CHECK([if have htonll defined],
               [c_cv_have_htonll],
               AC_LINK_IFELSE([AC_LANG_PROGRAM(
[[[
#include <sys/types.h>
#include <netinet/in.h>
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
]]],
[[[
          return htonll(0);
]]]
    )],
    [c_cv_have_htonll="yes"],
    [c_cv_have_htonll="no"]
    )
)
if test "x$c_cv_have_htonll" = "xyes"
then
    AC_DEFINE(HAVE_HTONLL, 1, [Define if the function htonll exists.])
fi



GLIB_G_THREAD_INIT_NEEDED=0

AM_PATH_GLIB_2_0([2.10.0], , ,[gthread])

AC_MSG_CHECKING([if deprecated g_thread_init() is needed])
if test $glib_config_major_version != 2 ; then
    AC_MSG_ERROR([unknown glib major version number (we know only 2.x versions)])
fi
if test $glib_config_minor_version -lt 32 ; then
    AC_MSG_RESULT([yes])
    GLIB_G_THREAD_INIT_NEEDED=1
    AC_DEFINE([GLIB_G_THREAD_INIT_NEEDED], [1], [g_thread_init() is needed])
else
    AC_MSG_RESULT([no])
fi
# Checks for libraries.
# --with-libmicrohttpd {{{
AC_ARG_WITH(libmicrohttpd, [AS_HELP_STRING([--with-libmicrohttpd@<:@=PREFIX@:>@], [Path to libmicrohttpd.])],
[
 if test "x$withval" = "xyes"
 then
     with_libmicrohttpd="yes"
     LIBMICROHTTPD_CPPFLAGS="$LIBMICROHTTPD_CPPFLAGS -I$withval/include"
     LIBMICROHTTPD_LDFLAGS="$LIBMICROHTTPD_LDFLAGS -L$withval/lib"
 else if test "x$withval" = "xno"
 then
     with_libmicrohttpd="no"
 else
     with_libmicrohttpd="yes"
     LIBMICROHTTPD_CPPFLAGS="$LIBMICROHTTPD_CPPFLAGS -I$withval/include"
     LIBMICROHTTPD_LDFLAGS="$LIBMICROHTTPD_LDFLAGS -L$withval/lib"
 fi; fi
],
[with_libmicrohttpd="yes"])

SAVE_CPPFLAGS="$CPPFLAGS"
SAVE_LDFLAGS="$LDFLAGS"

CPPFLAGS="$CPPFLAGS $LIBMICROHTTPD_CPPFLAGS"
LDFLAGS="$LDFLAGS $LIBMICROHTTPD_LDFLAGS"

if test "x$with_libmicrohttpd" = "xyes"
then
    if test "x$LIBMICROHTTPD_CPPFLAGS" != "x"
    then
        AC_MSG_NOTICE([libmicrohttpd CPPFLAGS: $LIBMICROHTTPD_CPPFLAGS])
    fi
    AC_CHECK_HEADERS(microhttpd.h,
    [with_libmicrohttpd="yes"],
    [with_libmicrohttpd="no (microhttpd.h not found)"])
fi
if test "x$with_libmicrohttpd" = "xyes"
then
    if test "x$LIBMICROHTTPD_LDFLAGS" != "x"
    then
        AC_MSG_NOTICE([libmicrohttpd LDFLAGS: $LIBMICROHTTPD_LDFLAGS])
    fi
    AC_CHECK_LIB(microhttpd, MHD_get_version,
    [with_libmicrohttpd="yes"],
    [with_libmicrohttpd="no (symbol 'microhttpd_info' not found)"])
fi

CPPFLAGS="$SAVE_CPPFLAGS"
LDFLAGS="$SAVE_LDFLAGS"

if test "x$with_libmicrohttpd" = "xyes"
then
    BUILD_WITH_LIBMICROHTTPD_CPPFLAGS="$LIBMICROHTTPD_CPPFLAGS"
    BUILD_WITH_LIBMICROHTTPD_LDFLAGS="$LIBMICROHTTPD_LDFLAGS"
    BUILD_WITH_LIBMICROHTTPD_LIBS="-lmicrohttpd"
    AC_SUBST(BUILD_WITH_LIBMICROHTTPD_CPPFLAGS)
    AC_SUBST(BUILD_WITH_LIBMICROHTTPD_LDFLAGS)
    AC_SUBST(BUILD_WITH_LIBMICROHTTPD_LIBS)
fi
AM_CONDITIONAL(BUILD_WITH_LIBMICROHTTPD, test "x$with_libmicrohttpd" = "xyes")
# }}}

# Checks for crypto library {{{
AC_ARG_ENABLE([gcrypt],
    AS_HELP_STRING([--disable-gcrypt],[Disable optional GCRYPT support]),
        [case "${enableval}" in
            yes) have_gcrypt=yes ;;
            no) have_gcrypt=no ;;
            *) AC_MSG_ERROR(bad value ${enableval} for --disable-gcrypt) ;;
        esac],
        [have_gcrypt=auto])

if test "x${have_gcrypt}" != xno ; then
    AM_PATH_LIBGCRYPT(
        [1.4.5],
        [have_gcrypt=yes],
        [if test "x$have_gcrypt" = xyes ; then
            AC_MSG_ERROR([*** GCRYPT headers not found.])
        fi])

    if test "x$have_gcrypt" = xyes ; then
        GCRYPT_LIBS="$LIBGCRYPT_LIBS"
        GCRYPT_CFLAGS="$LIBGCRYPT_CFLAGS"
        AC_DEFINE(HAVE_GCRYPT, 1, [GCRYPT available])
    else
        have_gcrypt=no
    fi
else
    GCRYPT_LIBS=
    GCRYPT_CFLAGS=
fi
AC_SUBST(GCRYPT_LIBS)
AC_SUBST(GCRYPT_CFLAGS)
AM_CONDITIONAL([HAVE_GCRYPT], [test "x$have_gcrypt" != xno])
# }}}

# Checks for libev library {{{
AC_MSG_CHECKING([for libev support])
AC_ARG_WITH([libev],
    [AS_HELP_STRING([--with-libev@<:@=PATH@:>@],[Search for libev in PATH/include and PATH/lib])],
    [WITH_LIBEV=$withval],[WITH_LIBEV=yes])

LIBEV_CFLAGS=""
LIBEV_LIBS=""

PKG_CHECK_MODULES([LIBEV], [libev], [], [
  # no pkg-config for libev, searching manually:

  if test "$WITH_LIBEV" != "yes"; then
    LIBEV_CFLAGS="-I$WITH_LIBEV/include"
    LIBEV_LIBS="-L$WITH_LIBEV/lib -lev"
  else
    AC_CHECK_HEADERS([ev.h],[
      AC_CHECK_LIB([ev], [ev_time], [
          LIBEV_LIBS="-lev"
        ],[
          AC_MSG_ERROR([libev not found])
        ]
      )],[
        AC_MSG_ERROR([libev not found])
      ]
    )
  fi
])

AC_SUBST([LIBEV_CFLAGS])
AC_SUBST([LIBEV_LIBS])
# }}}

LIBCURL_CHECK_CONFIG([yes],[],[],[AC_MSG_ERROR([libcurl development files required])])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 conf/Makefile
                 src/Makefile])
AC_OUTPUT

# vim: set fdm=marker :

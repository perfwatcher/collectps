
AC_PREREQ([2.63])
AC_INIT([collectps-agent], [0.0.1], [ymettier@free.fr])
AC_CANONICAL_HOST
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


AC_MSG_CHECKING([for kernel type ($host_os)])
case $host_os in
	*linux*)
	AC_DEFINE([KERNEL_LINUX], 1, [True if program is to be compiled for a Linux kernel])
	ac_system="Linux"
	;;
	*solaris*)
	AC_DEFINE([KERNEL_SOLARIS], 1, [True if program is to be compiled for a Solaris kernel])
	ac_system="Solaris"
	;;
	*darwin*)
	ac_system="Darwin"
	;;
	*openbsd*)
	ac_system="OpenBSD"
	;;
	*aix*)
	AC_DEFINE([KERNEL_AIX], 1, [True if program is to be compiled for a AIX kernel])
	ac_system="AIX"
	;;
	*freebsd*)
	AC_DEFINE([KERNEL_FREEBSD], 1, [True if program is to be compiled for a FreeBSD kernel])
	ac_system="FreeBSD"
	;;
	*)
	ac_system="unknown"
esac
AC_MSG_RESULT([$ac_system])

AC_SEARCH_LIBS(socket, socket)
AC_SEARCH_LIBS(gethostbyname, nsl)

AC_C_BIGENDIAN

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


clock_gettime_needs_rt="no"
clock_gettime_needs_posix4="no"
have_clock_gettime="no"
AC_CHECK_FUNCS(clock_gettime, [have_clock_gettime="yes"])
if test "x$have_clock_gettime" = "xno"
then
    AC_CHECK_LIB(rt, clock_gettime, [clock_gettime_needs_rt="yes"
                                     have_clock_gettime="yes"])
fi
if test "x$have_clock_gettime" = "xno"
then
    AC_CHECK_LIB(posix4, clock_gettime, [clock_gettime_needs_posix4="yes"
                                         have_clock_gettime="yes"])
fi
if test "x$have_clock_gettime" = "xyes"
then
    AC_DEFINE(HAVE_CLOCK_GETTIME, 1, [Define if the clock_gettime(2) function is available.])
else
    AC_MSG_WARN(cannot find clock_gettime)
fi
nanosleep_needs_rt="no"
nanosleep_needs_posix4="no"
AC_CHECK_FUNCS(nanosleep,
    [],
    AC_CHECK_LIB(rt, nanosleep,
        [nanosleep_needs_rt="yes"],
        AC_CHECK_LIB(posix4, nanosleep,
            [nanosleep_needs_posix4="yes"],
            AC_MSG_ERROR(cannot find nanosleep)
        )
    )
)
AM_CONDITIONAL(BUILD_WITH_LIBRT, test "x$clock_gettime_needs_rt" = "xyes" || test "x$nanosleep_needs_rt" = "xyes")
AM_CONDITIONAL(BUILD_WITH_LIBPOSIX4, test "x$clock_gettime_needs_posix4" = "xyes" || test "x$nanosleep_needs_posix4" = "xyes")



AH_TEMPLATE([HAVE_LIBCONFIG_1_3], [Defined if using old libconfig version 1.3])
AC_MSG_NOTICE([Checking for libconfig >= 1.4])
PKG_CHECK_MODULES([LIBCONFIG], [libconfig >= 1.4],,
	[ 
	AC_MSG_NOTICE([libconfig >= 1.4 not found. Checking for libconfig >= 1.3])
		PKG_CHECK_MODULES([LIBCONFIG], [libconfig >= 1.3],
				[
					AC_DEFINE([HAVE_LIBCONFIG_1_3])
					AC_MSG_NOTICE([OK, we are compiling with libconfig 1.3. Some features will be disabled (like @include)])
				],
				AC_MSG_ERROR([libconfig 1.3 or newer not found.])
				)
	]
)


AM_PATH_GLIB_2_0([2.10.0])

# Checks for libraries.
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

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 conf/Makefile
                 src/Makefile])
AC_OUTPUT

# vim: set fdm=marker :

if COMPILER_IS_GCC
AM_CFLAGS = -Wall -Werror
endif

AM_CPPFLAGS = -DPREFIX='"${prefix}"'
AM_CPPFLAGS += -DCONFIGFILE='"${sysconfdir}/${PACKAGE_NAME}.conf"'
AM_CPPFLAGS += -DLOCALSTATEDIR='"${localstatedir}"'
AM_CPPFLAGS += -DPIDFILE='"${localstatedir}/run/${PACKAGE_NAME}.pid"'
AM_CPPFLAGS += -DBINDIR='"${bindir}"'
AM_CPPFLAGS += -DPKGDATADIR='"${pkgdatadir}"'



bin_PROGRAMS=collectps

collectps_SOURCES=\
	main.c main.h \
	util_log.c util_log.h \
	util_config.c util_config.h \
	util_buffer.c util_buffer.h \
	util_thread_pool.c util_thread_pool.h \
	util_ev.c util_ev.h \
	http_daemon.c http_daemon.h \
	workers.c workers.h \
	collectps.h

collectps_CFLAGS = $(AM_CFLAGS) $(BUILD_WITH_LIBMICROHTTPD_CPPFLAGS) ${GLIB_CFLAGS} ${LIBEV_CFLAGS} ${LIBCURL_CPPFLAGS} ${LIBCONFIG_CFLAGS}
collectps_LDADD = $(BUILD_WITH_LIBMICROHTTPD_LIBS) ${GLIB_LIBS} ${LIBEV_LIBS} ${LIBCURL} ${LIBCONFIG_LIBS}

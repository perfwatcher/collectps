if COMPILER_IS_GCC
AM_CFLAGS = -Wall -Werror
endif

AM_CPPFLAGS = -DPREFIX='"${prefix}"'
AM_CPPFLAGS += -DCONFIGFILE='"${sysconfdir}/${PACKAGE_NAME}.conf"'
AM_CPPFLAGS += -DLOCALSTATEDIR='"${localstatedir}"'
AM_CPPFLAGS += -DPIDFILE='"${localstatedir}/run/${PACKAGE_NAME}.pid"'
AM_CPPFLAGS += -DBINDIR='"${bindir}"'
AM_CPPFLAGS += -DPKGDATADIR='"${pkgdatadir}"'



bin_PROGRAMS=collectps_agent

collectps_agent_SOURCES=\
	main.c main.h \
	get_process.c get_process.h \
	util_log.c util_log.h \
	util_config.c util_config.h \
	util_time.c util_time.h \
	util_prev_process_info.c util_prev_process_info.h \
	util_process_info.c util_process_info.h \
	util_network.c util_network.h \
	collectps_agent.h

collectps_agent_CFLAGS = $(AM_CFLAGS) ${GLIB_CFLAGS} ${LIBCONFIG_CFLAGS}
collectps_agent_LDADD = ${GLIB_LIBS} ${LIBCONFIG_LIBS}

if BUILD_WITH_LIBRT
collectps_agent_LDADD += -lrt
endif
if BUILD_WITH_LIBPOSIX4
collectps_agent_LDADD += -lposix4
endif


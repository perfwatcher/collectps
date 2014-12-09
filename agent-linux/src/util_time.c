/**
 * collectps - util_time.c
 * Copyright (C) 2014 Yves Mettier and other authors listed below
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors:
 * Yves Mettier <ymettier at free.fr>
 **/


/* Code borrowed from
 * - Project Collectd (http://collectd.org)
 *
 * Read the disclaimer below
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>

#include <string.h>

#include "config.h"

#include "collectps_agent.h"



/*
 * Start of code from Collectd src/utils_time.h, src/utils_time.c, src/collectd.h 
 ********************************************************************************/

/**
 * collectd - src/utils_time.h, src/utils_time.c, src/collectd.h
 * Copyright (C) 2010  Florian octo Forster
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Florian octo Forster <ff at octo.it>
 **/


#define NS_TO_CDTIME_T(ns) ((cdtime_t) (((double) (ns)) * 1.073741824))
#define US_TO_CDTIME_T(us) ((cdtime_t) (((double) (us)) * 1073.741824))
#define TIMESPEC_TO_CDTIME_T(ts) (TIME_T_TO_CDTIME_T ((ts)->tv_sec) + NS_TO_CDTIME_T ((ts)->tv_nsec))
#define TIMEVAL_TO_CDTIME_T(tv) (TIME_T_TO_CDTIME_T ((tv)->tv_sec) + US_TO_CDTIME_T ((tv)->tv_usec))

#ifdef HAVE_CLOCK_GETTIME
cdtime_t cdtime (void) { /* {{{ */
    int status;
    struct timespec ts = { 0, 0 };

    status = clock_gettime (CLOCK_REALTIME, &ts);
    if (status != 0) {
        char errbuf[1024];
        L(LOGLEVEL_ERROR, "cdtime: clock_gettime failed: %s", strerror_r (errno, errbuf, sizeof (errbuf)));
        return (0);
    }

    return (TIMESPEC_TO_CDTIME_T (&ts));
} /* }}} cdtime_t cdtime */
#else
/* Work around for Mac OS X which doesn't have clock_gettime(2). *sigh* */
cdtime_t cdtime (void) { /* {{{ */
    int status;
    struct timeval tv = { 0, 0 };

    status = gettimeofday (&tv, /* struct timezone = */ NULL);
    if (status != 0) {
        char errbuf[1024];
        L(LOGLEVEL_ERROR, "cdtime: gettimeofday failed: %s", strerror_r (errno, errbuf, sizeof (errbuf)));
        return (0);
    }

    return (TIMEVAL_TO_CDTIME_T (&tv));
} /* }}} cdtime_t cdtime */
#endif

/******************************************************************************
 * End of code from Collectd src/utils_time.h, src/utils_time.c, src/collectd.h 
 */


/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

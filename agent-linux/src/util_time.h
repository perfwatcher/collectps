#ifndef UTIL_TIME_H
#define UTIL_TIME_H

#include <stdint.h>

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

typedef uint64_t cdtime_t;

#define TIME_T_TO_CDTIME_T(t) (((cdtime_t) (t)) * 1073741824)
#define CDTIME_T_TO_TIME_T(t) ((time_t) ((t) / 1073741824))

cdtime_t cdtime (void);

/******************************************************************************
 * End of code from Collectd src/utils_time.h, src/utils_time.c, src/collectd.h 
 */

#endif
/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

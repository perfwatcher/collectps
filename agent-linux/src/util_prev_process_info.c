/**
 * collectps - util_prev_process_info.c
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>

#include <unistd.h>

#include <glib.h>

#include "config.h"

#if KERNEL_LINUX
static int hz;
#define TICKS_TO_SECONDS(x) ((time_t)(((x) / (hz))))
#endif

#if KERNEL_SOLARIS
#define TICKS_TO_SECONDS(x) ((time_t)(x))
#endif

#include "collectps_agent.h"

typedef struct {
    unsigned long utime;
    unsigned long stime;
    cdtime_t tm;
} prev_process_info_t;

extern cps_config_t global_config;

static GTree *prev_process_info = NULL;

typedef struct prev_process_info__userdata_t {
    GSList *gl;
    cdtime_t tm;
} prev_process_info__userdata_t;


static int intcmp(gconstpointer a, gconstpointer b, gpointer user_data) { /* {{{ */
  return (a-b);
} /* }}} */

int prev_process_info__compute_load(cdtime_t tm, pid_t pid, unsigned long stime, unsigned long  utime, unsigned long *spercent, unsigned long *upercent) { /* {{{ */
/* Notes :
 * stime and utime are provided in machine unit (time or ticks) and will be converted in seconds here
 * tm is provided in cdtime_t (and will be converted into seconds here)
 * result values spercent and upercent are percent values
 * this function returns 0 if spercent and upercernt are meaningful.
 */
    prev_process_info_t *info;
    if(NULL == (info = g_tree_lookup(prev_process_info, GINT_TO_POINTER(pid)))) {
        info = g_slice_alloc(sizeof(*info));
        info->tm = tm;
        info->stime = stime;
        info->utime = utime;
        g_tree_insert(prev_process_info, GINT_TO_POINTER(pid), info);
        *spercent = 0;
        *upercent = 0;
        return(-1);
    }

    *spercent = TICKS_TO_SECONDS((100 * (stime - info->stime)) / CDTIME_T_TO_TIME_T(tm - info->tm));
    *upercent = TICKS_TO_SECONDS((100 * (utime - info->utime)) / CDTIME_T_TO_TIME_T(tm - info->tm));
    info->stime = stime;
    info->utime = utime;
    info->tm = tm;
    return(0);
} /* }}} */

static void prev_process_info__free(gpointer data) { /* {{{ */
    prev_process_info_t *info = data;
    g_slice_free1(sizeof(*info), info);
} /* }}} */

static gboolean prev_process_info__check_unused (gpointer key, gpointer value, gpointer data) { /* {{{ */
    prev_process_info__userdata_t *d = data;
    prev_process_info_t *info = value;

    if(info->tm < d->tm) {
        d->gl = g_slist_prepend(d->gl, key);
    }
    return(FALSE); /* return TRUE to stop the traversal */
} /* }}} */

void prev_process_info__garbage_collector(cdtime_t tm) { /* {{{ */
    prev_process_info__userdata_t userdata;
    GSList *cur;

    userdata.gl = NULL;
    userdata.tm = tm;

    g_tree_foreach(prev_process_info, prev_process_info__check_unused, &userdata);
    for(cur=userdata.gl; cur; cur = cur->next) {
        g_tree_remove(prev_process_info, cur->data);
    }
    g_slist_free(userdata.gl);
} /* }}} */

void prev_process_info__init() { /* {{{ */
#if KERNEL_LINUX
    hz = sysconf(_SC_CLK_TCK);
#endif
    prev_process_info = g_tree_new_full(intcmp, NULL, NULL, prev_process_info__free);
} /* }}} */


/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <errno.h>

#include <glib.h>

#include "config.h"
#include <collectps.h>

extern cps_config_t global_config;

GThreadPool *workers_thread_pool = NULL;

void collectps_thread_pool_init() {
    GError *error = NULL;

    if(NULL == (workers_thread_pool = g_thread_pool_new(
                    /* GFunc func = */ worker_parser,
                    /* gpointer user_data = */ NULL,
                    /* gint max_threads = */ global_config.workers__threads_nb_max,
                    /* gboolean exclusive = */ TRUE,
                    /* GError **error = */ &error
                    ))) {
        L (LOGLEVEL_CRITICAL, "Could not create a thread pool");
        close_all_and_exit(EXIT_FAILURE);
    }

}


/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

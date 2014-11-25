#ifndef PICT_H
#define PICT_H

#include <glib.h>

#include "config.h"

typedef enum {
    LOGLEVEL_CRITICAL,
    LOGLEVEL_ERROR,
    LOGLEVEL_WARNING,
    LOGLEVEL_INFO,
    LOGLEVEL_DEBUG,
    LOGLEVEL_DEBUG_VERBOSE,
} loglevel_e;

typedef struct ip_list_t {
    char *addr;
    int family;
    struct ip_list_t *next;
} ip_list_t;

typedef struct {
    /* From command line */
    char *config_file;
    gboolean show_version;
    gboolean daemonize;

    /* [listener] */
    char *listener__port;
    ip_list_t *listener__ip_list;
    size_t listener__buffer_max_size;

    /* [daemon] */
    char *daemon__working_dir;
    mode_t daemon__umask;

    /* [workers] */
    gint workers__threads_nb_max;

    /* [log] */
    char *log__file;
    gint log__level;

    /* [influxdb] */
    char *influxdb__database;
    char *influxdb__url;
    char *influxdb__username;
    char *influxdb__password;
    char *influxdb__proxy_url;
    long influxdb__proxy_port;

    /* [httpd] */
    gint httpd__port;

} config_t;

#include "main.h"
#include "util_log.h"
#include "util_config.h"
#include "workers.h"
#include "util_thread_pool.h"
#include "util_buffer.h"
#include "util_ev.h"
#include "http_daemon.h"

#endif

/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

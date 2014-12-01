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
    const char *addr;
    int family;
    struct ip_list_t *next;
} ip_list_t;

typedef struct {
    /* From command line */
    char *config_file;
    gboolean show_version;
    gboolean daemonize;

    /* [listener] */
    unsigned int listener__port;
    ip_list_t *listener__ip_list;
    size_t listener__buffer_max_size;

    /* [daemon] */
    const char *daemon__working_dir;
    mode_t daemon__umask;

    /* [workers] */
    gint workers__threads_nb_max;

    /* [log] */
    const char *log__file;
    gint log__level;

    /* [influxdb] */
    const char *influxdb__database;
    const char *influxdb__url;
    const char *influxdb__username;
    const char *influxdb__password;
    const char *influxdb__proxy_url;
    unsigned long influxdb__proxy_port;

    /* [httpd] */
    gint httpd__port;

} cps_config_t;

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

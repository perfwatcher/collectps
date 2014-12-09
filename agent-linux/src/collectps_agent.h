#ifndef PICT_H
#define PICT_H

#include <glib.h>

#include "util_time.h"

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
    char *hostname;
    unsigned int port;
    int sockfd;
} server_info_t;

typedef struct {
    /* From command line */
    char *config_file;
    gboolean show_version;
    gboolean daemonize;

    /* [daemon] */
    const char *daemon__working_dir;
    mode_t daemon__umask;

    /* [log] */
    const char *log__file;
    gint log__level;

    /* [server] */
    GList *server__server;

    /* [agent] */
    time_t agent__interval;
    time_t agent__max_cache_time;
    cdtime_t agent__max_cache_cdtime;
    char *agent__hostname;
    size_t agent__hostname_length;

} cps_config_t;

#include "main.h"
#include "get_process.h"
#include "util_log.h"
#include "util_config.h"
#include "util_network.h"
#include "util_process_info.h"
#include "util_prev_process_info.h"

#endif

/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

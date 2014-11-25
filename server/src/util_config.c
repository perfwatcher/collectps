#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#include <glib.h>

#include <collectps.h>

config_t global_config;

static GOptionEntry optionentries[] = 
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &(global_config.show_version), "show version, then exit", NULL },
    { "config_file", 'c', G_OPTION_FLAG_FILENAME, G_OPTION_ARG_FILENAME, &(global_config.config_file), "Configuration file", NULL },
    { "no_daemon", 'd', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &(global_config.daemonize), "Do not daemonize", NULL },
    { NULL }
};

static short is_ipv6_available() {
    int fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (-1 == fd) return(0);
    close(fd);
    return(1);
}

void parse_config_file(void) { /* {{{ */
    GError *error = NULL;
    GKeyFile *config_file;
    config_file = g_key_file_new();
    char *umask;
    char **string_list;

    error = NULL;
    if(!g_key_file_load_from_file(config_file, global_config.config_file, 0, &error)) {
        L (LOGLEVEL_CRITICAL, "Failed to load config file %s : %s", global_config.config_file, error->message);
        close_all_and_exit(EXIT_FAILURE);
    }
    /* [listener] */
    error = NULL;
    global_config.listener__ip_list = NULL;
    if(NULL == (string_list = g_key_file_get_string_list(config_file, "listener", "ip_list", NULL, &error))) {
        L (LOGLEVEL_WARNING, "Failed to parse 'ip_list' in [listener] group : %s", error->message);
        L (LOGLEVEL_WARNING, "Setting default value for 'ip_list' as 'all'");
    } else {
        char *cur;
        ip_list_t *ip_last = NULL;
        for(cur = *string_list; cur; cur++) {
            ip_list_t *ip;
            if(!strcmp(cur, "all")) continue;
            if(NULL == (ip = malloc(sizeof(*ip)))) {
                L (LOGLEVEL_CRITICAL, "Failed to allocate memory");
                close_all_and_exit(EXIT_FAILURE);
            }
            if(NULL == (ip->addr = strdup(cur))) {
                L (LOGLEVEL_CRITICAL, "Failed to allocate memory");
                close_all_and_exit(EXIT_FAILURE);
            }
            ip->family = AF_UNSPEC;
            ip->next = NULL;
            if(NULL == ip_last) {
                global_config.listener__ip_list = ip;
            } else {
                ip_last->next = ip;
            }
            ip_last = ip;
        }
        g_strfreev(string_list);
    }

    error = NULL;
    if(NULL == (global_config.listener__port = g_key_file_get_string(config_file, "listener", "port", &error))) {
        global_config.listener__port = strdup("25927");
        L (LOGLEVEL_WARNING, "Failed to parse 'port' in [listener] group : %s", error->message);
        L (LOGLEVEL_WARNING, "Setting default value for 'port' as %s", global_config.listener__port);
    }
    error = NULL;
    if(0 == (global_config.listener__buffer_max_size = g_key_file_get_integer(config_file, "listener", "buffer_max_size", &error))) {
        global_config.listener__buffer_max_size = 8192;
        L (LOGLEVEL_WARNING, "Failed to parse 'buffer_max_size' in [listener] group : %s", error->message);
        L (LOGLEVEL_WARNING, "Setting default value for 'buffer_max_size' as %d", global_config.listener__buffer_max_size);
    }

    /* [daemon] */
    error = NULL;
    if(NULL == (global_config.daemon__working_dir = g_key_file_get_string(config_file, "daemon", "working_dir", &error))) {
        L (LOGLEVEL_CRITICAL, "Failed to find 'working_dir' in [daemon] group : %s", error->message);
        close_all_and_exit(EXIT_FAILURE);
    }
    error = NULL;
    if(NULL == (umask = g_key_file_get_string(config_file, "daemon", "umask", &error))) {
        umask = g_strdup("022");
        L (LOGLEVEL_CRITICAL, "Failed to find 'umask' in [daemon] group : %s", error->message);
        L (LOGLEVEL_CRITICAL, "Setting default value for 'umask' as %s", umask);
    }
    errno=0;
    global_config.daemon__umask = strtol(umask, NULL, 8) & 0777;
    if(errno) {
        L (LOGLEVEL_CRITICAL, "Failed to convert umask '%s' to an octal value (example : '022')", umask, error->message);
        close_all_and_exit(EXIT_FAILURE);
    }
    g_free(umask);

    /* [influxdb] */
    error = NULL;
    if(NULL == (global_config.influxdb__database = g_key_file_get_string(config_file, "influxdb", "database", &error))) {
        L (LOGLEVEL_CRITICAL, "Failed to find 'database' in [influxdb] group : %s", error->message);
        close_all_and_exit(EXIT_FAILURE);
    }
    error = NULL;
    if(NULL == (global_config.influxdb__url = g_key_file_get_string(config_file, "influxdb", "url", &error))) {
        L (LOGLEVEL_CRITICAL, "Failed to find 'url' in [influxdb] group : %s", error->message);
        close_all_and_exit(EXIT_FAILURE);
    }
    error = NULL;
    if(NULL == (global_config.influxdb__username = g_key_file_get_string(config_file, "influxdb", "username", &error))) {
        L (LOGLEVEL_CRITICAL, "Failed to find 'username' in [influxdb] group : %s", error->message);
        close_all_and_exit(EXIT_FAILURE);
    }
    error = NULL;
    if(NULL == (global_config.influxdb__password = g_key_file_get_string(config_file, "influxdb", "password", &error))) {
        L (LOGLEVEL_CRITICAL, "Failed to find 'password' in [influxdb] group : %s", error->message);
        close_all_and_exit(EXIT_FAILURE);
    }
    error = NULL;
    if(NULL == (global_config.influxdb__proxy_url = g_key_file_get_string(config_file, "influxdb", "proxy_url", &error))) {
        global_config.influxdb__proxy_port = -1;
        L (LOGLEVEL_WARNING, "Failed to find 'proxy_url' in [influxdb] group : %s", error->message);
        L (LOGLEVEL_WARNING, "I will use no proxy to connect to InfluxDB");
    }
    error = NULL;
    if(0 == (global_config.influxdb__proxy_port = g_key_file_get_integer(config_file, "influxdb", "proxy_port", &error))) {
        global_config.influxdb__proxy_port = -1;
        L (LOGLEVEL_WARNING, "Failed to parse 'proxy_port' in [influxdb] group : %s", error->message);
        L (LOGLEVEL_WARNING, "I will use no proxy to connect to InfluxDB");
    }

    /* [workers] */
    error = NULL;
    if(0 == (global_config.workers__threads_nb_max = g_key_file_get_integer(config_file, "workers", "threads_nb_max", &error))) {
        global_config.workers__threads_nb_max = 10;
        L (LOGLEVEL_WARNING, "Failed to parse 'threads_nb_max' in [workers] group : %s", error->message);
        L (LOGLEVEL_WARNING, "Setting default value for 'threads_nb_max' as %d", global_config.workers__threads_nb_max);
    }

    /* [log] */
    error = NULL;
    if(0 == (global_config.log__level = g_key_file_get_integer(config_file, "log", "level", &error))) {
        global_config.log__level = 2;
        L (LOGLEVEL_WARNING, "Failed to parse 'level' in [log] group : %s", error->message);
        L (LOGLEVEL_WARNING, "Setting default value for 'level' as %d", global_config.log__level);
    }
    error = NULL;
    if(NULL == (global_config.log__file = g_key_file_get_string(config_file, "log", "file", &error))) {
        L (LOGLEVEL_WARNING, "Failed to parse 'file' in [log] group : %s", error->message);
        L (LOGLEVEL_WARNING, "Setting default value for 'file' as stderr", global_config.log__file);
    }

    /* [httpd] */
    error = NULL;
    if(0 == (global_config.httpd__port = g_key_file_get_integer(config_file, "httpd", "port", &error))) {
        global_config.httpd__port = 25926;
        L (LOGLEVEL_WARNING, "Failed to parse 'port' in [httpd] group : %s", error->message);
        L (LOGLEVEL_WARNING, "Setting default value for 'port' as %d", global_config.httpd__port);
    }
    error = NULL;

    g_key_file_free(config_file);
} /* }}} parse_config_file */

void show_version() { /* {{{ */
    printf("This is %s\n", PACKAGE);
    printf("Copyright 2014 Yves Mettier\n");
    printf("Version %s (Compiled at %s %s)\n", PACKAGE_VERSION, __DATE__, __TIME__);
    printf("Contact : %s\n", PACKAGE_BUGREPORT);
    printf("\n");
} /* }}} show_version */

void config_init(int argc, char **argv) { /* {{{ */
    GError *error = NULL;
    GOptionContext *optioncontext;

    memset(&global_config, '\0', sizeof(global_config));

    optioncontext = g_option_context_new("- Daemon that collects Process Information from servers and store them where you want");
    g_option_context_add_main_entries(optioncontext, optionentries, NULL);
    global_config.daemonize = TRUE;
    if(!g_option_context_parse(optioncontext, &argc, &argv, &error)) {
        L (LOGLEVEL_CRITICAL, "Failed to parse the command line : %s", error->message);
        close_all_and_exit(EXIT_FAILURE);
    }

    if(global_config.show_version) {
        show_version();
        close_all_and_exit(EXIT_SUCCESS);
    }

    if(NULL == global_config.config_file) {
        L (LOGLEVEL_CRITICAL, "No config file was provided on the command line.");
        close_all_and_exit(EXIT_FAILURE);

    }
    parse_config_file();
    util_log_set_level(global_config.log__level);

    if(global_config.daemonize) {
        /* Configuration stops here if we want to daemonize. Only the not-daemonized child will continue. */
        return;
    }

    if(NULL == global_config.listener__ip_list) {
        ip_list_t *ip;
        if(is_ipv6_available()) {
            /* Listen on all interfaces / IPv6 */
            if(NULL == (ip = malloc(sizeof(*ip)))) {
                L (LOGLEVEL_CRITICAL, "Failed to allocate memory");
                close_all_and_exit(EXIT_FAILURE);
            }
            ip->addr = NULL;
            ip->family = AF_INET6;
            ip->next = NULL;
            global_config.listener__ip_list = ip;
        }

        /* Listen on all interfaces / IPv4 */
        if(NULL == (ip = malloc(sizeof(*ip)))) {
            L (LOGLEVEL_CRITICAL, "Failed to allocate memory");
            close_all_and_exit(EXIT_FAILURE);
        }
        ip->addr = NULL;
        ip->family = AF_INET;
        ip->next = NULL;

        if(global_config.listener__ip_list) {
            global_config.listener__ip_list->next = ip;
        } else {
            global_config.listener__ip_list = ip;
        }
    }

    return;
} /* }}} config_init */

/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

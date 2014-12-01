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
#include <libconfig.h>

#include <collectps.h>

cps_config_t global_config;

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
    config_t cfg;
    config_setting_t *setting;
#ifdef HAVE_LIBCONFIG_1_3
#define LIBCONFIG_INT_TYPE long
#else
#define LIBCONFIG_INT_TYPE int
#endif
    const char *umask_string;
    mode_t umask;
    LIBCONFIG_INT_TYPE log__level;
    unsigned int i;
    unsigned int count;
    LIBCONFIG_INT_TYPE n;
    ip_list_t *ip_last = NULL;

    config_init(&cfg);

    /* Read the file */
    if(! config_read_file(&cfg, global_config.config_file)) {
        L (LOGLEVEL_CRITICAL, "Error with config file %s:%d : %s", 
#ifdef HAVE_LIBCONFIG_1_3
                global_config.config_file,
#else
                config_error_file(&cfg),
#endif
                config_error_line(&cfg), 
                config_error_text(&cfg) 
          );
        config_destroy(&cfg);
        close_all_and_exit(EXIT_FAILURE);
    }
    /* [log] */
    util_log_set_level(INT_MAX);
    L (LOGLEVEL_INFO, "Setting Log level to max while reading the configuration file");
    if(! config_lookup_string(&cfg, "log.file", &(global_config.log__file))) {
        global_config.log__file = NULL;
        L (LOGLEVEL_WARNING, "Failed to find log.file");
        L (LOGLEVEL_WARNING, "Setting default value for log.file as stderr");
    }
    L (LOGLEVEL_INFO, "Config : log.file %s", global_config.log__file);
    if(! config_lookup_int(&cfg, "log.level", &(log__level))) {
        log__level = 2;
        L (LOGLEVEL_WARNING, "Failed to find log.level");
        L (LOGLEVEL_WARNING, "Setting default value for log.level as %d", global_config.log__level);
    }
    global_config.log__level = log__level;
    L (LOGLEVEL_INFO, "Config : log.level %d", global_config.log__level);


    /* [daemon] */
    if(! config_lookup_string(&cfg, "daemon.working_dir", &(global_config.daemon__working_dir))) {
        L (LOGLEVEL_CRITICAL, "Failed to find daemon.working_dir");
        close_all_and_exit(EXIT_FAILURE);
    }
    L (LOGLEVEL_INFO, "Config : daemon.working_dir %s", global_config.daemon__working_dir);

    if(! config_lookup_string(&cfg, "daemon.umask", &(umask_string))) {
        umask = 0022;
        L (LOGLEVEL_WARNING, "Failed to find daemon.umask");
        L (LOGLEVEL_WARNING, "Setting default value for daemon.umask as %o", umask);
    } else {
        errno = 0;
        umask = strtol(umask_string, NULL, 8);
        if(errno) {
            umask = 0022;
            L (LOGLEVEL_CRITICAL, "Wrong value for daemon.umask");
            L (LOGLEVEL_CRITICAL, "Setting default value for daemon.umask as %o", umask);
        }
    }
    global_config.daemon__umask = umask & 0777;
    L (LOGLEVEL_INFO, "Config : daemon.umask %o (from configuration %o)", global_config.daemon__umask, umask);


    /* [listener] */
    if(NULL == (setting = config_lookup(&cfg, "listener.interface"))) {
        L (LOGLEVEL_CRITICAL, "Failed to find listener.interface");
        close_all_and_exit(EXIT_FAILURE);
    }
    global_config.listener__ip_list = NULL;
    count = config_setting_length(setting);
    for (i=0; i<count; i++) {
        const char *str;
        ip_list_t *ip;

        if(NULL == (str = config_setting_get_string_elem(setting, i))) {
            L (LOGLEVEL_WARNING, "Failed to find a listener.interface definition");
            continue;
        }

        if(!strcmp(str, "all")) continue;
        if(NULL == (ip = g_malloc(sizeof(*ip)))) {
            L (LOGLEVEL_CRITICAL, "Failed to allocate memory");
            close_all_and_exit(EXIT_FAILURE);
        }
        ip->addr = str;
        ip->family = AF_UNSPEC;
        ip->next = NULL;
        if(NULL == ip_last) {
            global_config.listener__ip_list = ip;
        } else {
            ip_last->next = ip;
        }
        ip_last = ip;
    }
    if(global_config.listener__ip_list) {
        ip_list_t *cur;
        for(cur=global_config.listener__ip_list; cur; cur = cur->next) {
            L (LOGLEVEL_INFO, "Config : listener.interface : %s", cur->addr);
        }
    } else {
        L (LOGLEVEL_INFO, "Config : listener.interface : all");
    }

    if(! config_lookup_int(&cfg, "listener.port", &n)) {
        n = 25927;
        L (LOGLEVEL_WARNING, "Failed to find a listener.port");
        L (LOGLEVEL_WARNING, "Setting default value for 'port' as %s", n);
    }
    global_config.listener__port = n;
    L (LOGLEVEL_INFO, "Config : listener.port : %d", global_config.listener__port);

    if(! config_lookup_int(&cfg, "listener.buffer_max_size", &n)) {
        n = 8192;
        L (LOGLEVEL_WARNING, "Failed to find a listener.buffer_max_size");
        L (LOGLEVEL_WARNING, "Setting default value for 'buffer_max_size' as %d", n);
    }
    global_config.listener__buffer_max_size = n;

    /* [influxdb] */
    if(! config_lookup_string(&cfg, "influxdb.database", &(global_config.influxdb__database))) {
        L (LOGLEVEL_CRITICAL, "Failed to find influxdb.database");
        close_all_and_exit(EXIT_FAILURE);
    }
    L (LOGLEVEL_INFO, "Config : influxdb.database : %s", global_config.influxdb__database);

    if(! config_lookup_string(&cfg, "influxdb.url", &(global_config.influxdb__url))) {
        L (LOGLEVEL_CRITICAL, "Failed to find influxdb.url");
        close_all_and_exit(EXIT_FAILURE);
    }
    L (LOGLEVEL_INFO, "Config : influxdb.url : %s", global_config.influxdb__url);

    if(! config_lookup_string(&cfg, "influxdb.username", &(global_config.influxdb__username))) {
        L (LOGLEVEL_CRITICAL, "Failed to find influxdb.username");
        close_all_and_exit(EXIT_FAILURE);
    }
    L (LOGLEVEL_INFO, "Config : influxdb.username : %s", global_config.influxdb__username);

    if(! config_lookup_string(&cfg, "influxdb.password", &(global_config.influxdb__password))) {
        L (LOGLEVEL_CRITICAL, "Failed to find influxdb.password");
        close_all_and_exit(EXIT_FAILURE);
    }
    L (LOGLEVEL_INFO, "Config : influxdb.password : %s", global_config.influxdb__password);

    if(! config_lookup_string(&cfg, "influxdb.proxy_url", &(global_config.influxdb__proxy_url))) {
        L (LOGLEVEL_WARNING, "Failed to find influxdb.proxy_url");
        L (LOGLEVEL_WARNING, "I will use no proxy to connect to InfluxDB");
        global_config.influxdb__proxy_url = NULL;
    } else {
        L (LOGLEVEL_INFO, "Config : influxdb.proxy_url : %s", global_config.influxdb__proxy_url);
    }

    if(! config_lookup_int(&cfg, "influxdb.proxy_port", &(n))) {
        n = -1;
        L (LOGLEVEL_WARNING, "Failed to find influxdb.proxy_port");
        L (LOGLEVEL_WARNING, "I will use no proxy to connect to InfluxDB");
    } else {
        L (LOGLEVEL_INFO, "Config : influxdb.proxy_port : %s", n);
    }
    global_config.influxdb__proxy_port = n;


    /* [workers] */
    if(! config_lookup_int(&cfg, "workers.threads_nb_max", &(n))) {
        n = 10;
        L (LOGLEVEL_WARNING, "Failed to find workers.threads_nb_max");
        L (LOGLEVEL_WARNING, "Setting default value for 'threads_nb_max' as %d", n);
    }
    global_config.workers__threads_nb_max = n;
    L (LOGLEVEL_INFO, "Config : workers.threads_nb_max : %d", global_config.workers__threads_nb_max);

    /* [httpd] */
    if(! config_lookup_int(&cfg, "httpd.port", &(n))) {
        n = -1;
        L (LOGLEVEL_WARNING, "Failed to find httpd.port");
        L (LOGLEVEL_WARNING, "I will use no proxy to connect to InfluxDB");
    } else {
        L (LOGLEVEL_INFO, "Config : httpd.port : %d", n);
    }
    global_config.httpd__port = n;

} /* }}} parse_config_file */

void show_version() { /* {{{ */
    printf("This is %s\n", PACKAGE);
    printf("Copyright 2014 Yves Mettier\n");
    printf("Version %s (Compiled at %s %s)\n", PACKAGE_VERSION, __DATE__, __TIME__);
    printf("Contact : %s\n", PACKAGE_BUGREPORT);
    printf("\n");
} /* }}} show_version */

void cps_config_init(int argc, char **argv) { /* {{{ */
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
} /* }}} cps_config_init */

/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

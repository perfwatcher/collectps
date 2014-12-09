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

#include "collectps_agent.h"

cps_config_t global_config;

static GOptionEntry optionentries[] = 
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &(global_config.show_version), "show version, then exit", NULL },
    { "config_file", 'c', G_OPTION_FLAG_FILENAME, G_OPTION_ARG_FILENAME, &(global_config.config_file), "Configuration file", NULL },
    { "no_daemon", 'd', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &(global_config.daemonize), "Do not daemonize", NULL },
    { NULL }
};


void parse_config_file(void) { /* {{{ */
    config_t cfg;
    config_setting_t *setting;
#ifdef HAVE_LIBCONFIG_1_3
#define LIBCONFIG_INT_TYPE long
#else
#define LIBCONFIG_INT_TYPE int
#endif
    const char *umask_string;
    char hostname[HOST_NAME_MAX];
    const char *parsed_hostname;
    mode_t umask;
    LIBCONFIG_INT_TYPE log__level;
    unsigned int i;
    unsigned int count;
    LIBCONFIG_INT_TYPE n;

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


    /* [server] */
    if(NULL == (setting = config_lookup(&cfg, "server"))) {
        L (LOGLEVEL_CRITICAL, "Failed to find server section");
        close_all_and_exit(EXIT_FAILURE);
    }
    global_config.server__server = NULL;
    count = config_setting_length(setting);
    for (i=0; i<count; i++) {
        config_setting_t *server = config_setting_get_elem(setting, i);
        char *h;
        LIBCONFIG_INT_TYPE p;
        server_info_t *el;
        if(!config_setting_lookup_string(server, "hostname", (const char **)&h)) {
            L (LOGLEVEL_CRITICAL, "Failed to find a server.hostname definition");
            close_all_and_exit(EXIT_FAILURE);
        }
        if(!config_setting_lookup_int(server, "port", &p)) {
            L (LOGLEVEL_CRITICAL, "Failed to find a server.port definition");
            close_all_and_exit(EXIT_FAILURE);
        }
        el = g_malloc(sizeof(*el));
        el->hostname = h;
        el->port = p;
        el->sockfd = -1;
        global_config.server__server = g_list_append(global_config.server__server, el);
        L (LOGLEVEL_INFO, "Config : Adding server %s:%d", h, p);
    }

    /* [agent] */
    if(! config_lookup_int(&cfg, "agent.interval", &(n))) {
        n = 60;
        L (LOGLEVEL_WARNING, "Failed to find agent.interval");
        L (LOGLEVEL_WARNING, "Setting default value for agent.interval as %d", n);
    }
    global_config.agent__interval = n;
    L (LOGLEVEL_INFO, "Config : agent.interval : %ld", global_config.agent__interval);

    if(! config_lookup_int(&cfg, "agent.max_cache_time", &(n))) {
        n = 5 * global_config.agent__interval;
        L (LOGLEVEL_WARNING, "Failed to find agent.max_cache_time");
        L (LOGLEVEL_WARNING, "Setting default value for agent.max_cache_time as %d", n);
    }
    global_config.agent__max_cache_time = n;
    global_config.agent__max_cache_cdtime = TIME_T_TO_CDTIME_T(n);
    L (LOGLEVEL_INFO, "Config : agent.max_cache_time : %ld", global_config.agent__max_cache_time);

    if(! config_lookup_string(&cfg, "agent.hostname", &(parsed_hostname))) {
        if(0 != gethostname(hostname, sizeof(hostname))) {
            L (LOGLEVEL_CRITICAL, "Failed to find agent.hostname and gethostname() also failed.");
            close_all_and_exit(EXIT_FAILURE);
        }
        global_config.agent__hostname = g_strdup(hostname);
        L (LOGLEVEL_WARNING, "Failed to find agent.hostname");
        L (LOGLEVEL_WARNING, "Setting default value for agent.hostname as %s", hostname);
    } else {
        global_config.agent__hostname = g_strdup(parsed_hostname);
    }
    global_config.agent__hostname_length = strlen(global_config.agent__hostname);
    L (LOGLEVEL_INFO, "Config : agent.hostname : %s", global_config.agent__hostname);

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

    optioncontext = g_option_context_new("- Daemon that collects Process Information from servers and send them to a server");
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

    return;
} /* }}} cps_config_init */

/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

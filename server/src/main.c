#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <signal.h>

#include <ev.h>
#include <curl/curl.h>

#include "collectps.h"

extern cps_config_t global_config;

pid_t exec_ourselves_as_child(char**argv_array) { /* {{{ */
    char *str;
    pid_t pid;

    if(0 != (pid = fork())) return(pid);

    execv(argv_array[0], argv_array);
    /* execv should not return. If it does, there is a problem. */
    str = g_strjoinv("' '", argv_array);
    L (LOGLEVEL_CRITICAL, "Failed to launch '%s'", str);
    g_free(str);
    exit(EXIT_FAILURE);
} /* }}} exec_ourselves_as_child */

void run_watchdog(char**argv_array) { /* {{{ */
    pid_t pid;
    time_t tm;
    time_t last_tm;
    int count;

    last_tm = time(NULL);
    pid = exec_ourselves_as_child(argv_array);
    count = 0;
    while(1) {
        int status;
        waitpid(pid, &status, 0);
        L (LOGLEVEL_WARNING, "%s (pid %d) ended. Will try to restart it.", argv_array[0], pid);

        tm = time(NULL);
        count++;
        if((tm - last_tm) > 60) count = 0; /* enough elapsed time since last restart */

        if(count > 5) {
            L (LOGLEVEL_ERROR, "%s launched too many times. Sleeping 20 minutes", argv_array[0]);
            sleep(20 * 60);
            count = 0;
            L (LOGLEVEL_WARNING, "OK, trying again to launch %s", argv_array[0]);
        }

        last_tm = time(NULL);
        pid = exec_ourselves_as_child(argv_array);
    }

} /* }}} run_watchdog */

void signal_handler_for_watchdog_stop(int signal_id, siginfo_t *siginfo, void *context) { /* {{{ */
    printf("Signal %d received\n", signal_id);
    kill (0, SIGTERM);
    L (LOGLEVEL_WARNING, PACKAGE_NAME "-" PACKAGE_VERSION " ending");
    exit(EXIT_SUCCESS);
} /* }}} signal_handler_for_watchdog_stop */

void signal_handler_for_stop(int signal_id, siginfo_t *siginfo, void *context) { /* {{{ */
    printf("Signal %d received\n", signal_id);
    L (LOGLEVEL_WARNING, PACKAGE_NAME "-" PACKAGE_VERSION " ending");
    exit(EXIT_SUCCESS);
} /* }}} signal_handler_for_stop */

void daemonize() { /* {{{ */
    int pid;
    int i;
    int lfp;
    char pidstr[15];
    struct sigaction sa;

    if(1 == getppid()) return; /* already a daemon */

    /* Fork */
    pid = fork();
    switch(pid) {
        case -1 : exit(EXIT_FAILURE);
        case 0: break; /* child */
        default: _exit(EXIT_SUCCESS);
    }
    /* Obtain a new process group */
    setsid();

    /* Fork again (ignore sigup) */
    signal(SIGHUP,SIG_IGN);
    pid = fork();
    switch(pid) {
        case -1 : exit(EXIT_FAILURE);
        case 0: break; /* child */
        default: _exit(EXIT_SUCCESS);
    }

    /* Descriptors : close all and reopen standard ones on /dev/null */
    for (i=getdtablesize();i>=0;--i) close(i);
    if (open("/dev/null",O_RDONLY) == -1) {
        L (LOGLEVEL_CRITICAL, "failed to reopen stdin while daemonizing (errno=%d)",errno);
        exit(EXIT_FAILURE);
    }
    if (open("/dev/null",O_WRONLY) == -1) {
        L (LOGLEVEL_CRITICAL, "failed to reopen stdout while daemonizing (errno=%d)",errno);
        exit(EXIT_FAILURE);
    }
    if (open("/dev/null",O_RDWR) == -1) {
        L (LOGLEVEL_CRITICAL, "failed to reopen stderr while daemonizing (errno=%d)",errno);
        exit(EXIT_FAILURE);
    }

    /* Set working directory and umask */
    chdir(global_config.daemon__working_dir);
    umask(global_config.daemon__umask);

    lfp=open(PACKAGE_NAME ".pid",O_RDWR|O_CREAT,0640);
    if (lfp < 0) {
        L (LOGLEVEL_CRITICAL, "Could not open '%s/" PACKAGE_NAME ".pid'. Either this file is locked or " PACKAGE_NAME " is already running ? (%d)",global_config.daemon__working_dir,errno);
        exit(EXIT_FAILURE);
    }
    if (lockf(lfp,F_TLOCK,0) < 0) {
        L (LOGLEVEL_CRITICAL, "Could not lock '%s/" PACKAGE_NAME ".pid'. (%d)",global_config.daemon__working_dir,errno);
        exit(EXIT_FAILURE);

    }
    /* first instance continues */
    sprintf(pidstr,"%d\n",getpid());
    write(lfp,pidstr,strlen(pidstr)); /* record pid to lockfile */

#define SET_SIGNAL_IGN(s) do {\
   sa.sa_handler=SIG_IGN; \
   sigemptyset(&sa.sa_mask); \
   sa.sa_flags = 0; \
   sigaction(s,&sa, NULL); \
} while(0)

#define SET_SIGNAL(s,sh) do {\
   memset (&sa, 0, sizeof (sa)); \
   sa.sa_sigaction=sh; \
   sigemptyset(&sa.sa_mask); \
   sa.sa_flags = SA_SIGINFO; \
   sigaction(s,&sa, NULL); \
} while(0)

    SET_SIGNAL_IGN(SIGCHLD); /* ignore child */
    SET_SIGNAL_IGN(SIGTSTP); /* ignore tty signals */
    SET_SIGNAL_IGN(SIGTTOU);
    SET_SIGNAL_IGN(SIGTTIN);
    SET_SIGNAL(SIGQUIT,signal_handler_for_watchdog_stop); /* catch hangup signal */
    SET_SIGNAL(SIGINT,signal_handler_for_watchdog_stop); /* catch hangup signal */
    SET_SIGNAL(SIGHUP,signal_handler_for_watchdog_stop); /* catch hangup signal */
    SET_SIGNAL(SIGTERM,signal_handler_for_watchdog_stop); /* catch kill signal */
    } /* }}} daemonize */

char **argv_to_array(int argc, char *argv[]) { /* {{{ */
    char **a;
    int i;
    a = malloc(sizeof(*a) * (argc + 3)); /* argc + 1 + (2 possible extra args) */
    assert(NULL != a); /* if we start with an allocation problem, stop before doing any work and fix the pb ... */
    for(i=0; i<argc; i++) {
        a[i] = argv[i];
    }
    a[i] = NULL;
    return(a);
} /* }}} argv_to_array */

char *get_command_line(char **a) { /* {{{ */
    char *cmdline_args;
    char *cmdline;

    assert(NULL != a); /* if we start with an allocation problem, stop before doing any work and fix the pb ... */
    cmdline_args = g_strjoinv("' '", a);
    cmdline = g_strdup_printf("'%s'", cmdline_args);
    g_free(cmdline_args);
    return(cmdline);
} /* }}} get_command_line */


char *get_full_path_name_of_this_running_process(char **argv) { /* {{{ */
    char *f;
    char *cwd;

    if(argv[0][0] == '/') {
        f = g_strdup(argv[0]);
    } else {
        cwd = g_get_current_dir();
        f = g_strdup_printf("%s/%s", cwd, argv[0]);
        g_free(cwd);
    }

    return(f);
} /* }}} get_full_path_name_of_this_running_process */

void set_argv_for_child_process(char **a) { /* {{{ */
    char *self_filename;
    int i;
    self_filename = get_full_path_name_of_this_running_process(a);
    a[0] = self_filename;
    for(i=0; a[i]; i++);
    a[i++] = strdup("-d");
    a[i++] = NULL;

    return;
} /* }}} set_argv_for_child_process */

void close_all_and_exit(int exit_code) { /* {{{ */
    L (LOGLEVEL_WARNING, PACKAGE_NAME "-" PACKAGE_VERSION " ending");
    exit(exit_code);
} /* }}} close_all_and_exit */

int main(int argc, char **argv) {
    struct sigaction sa;
    char *cmdline_str;
    char **argv_array;
    struct ev_loop *loop = ev_default_loop (0);

    argv_array = argv_to_array(argc, argv);
    cmdline_str = get_command_line(argv_array);
    set_argv_for_child_process(argv_array);

    cps_config_init(argc, argv);

    if(global_config.daemonize) {
        /* When daemonizing, a child process will be launched with daemonization disabled */
        daemonize();
        run_watchdog(argv_array);
        /* Watchdog should never stop ! */
        exit(EXIT_FAILURE);
    }

    /* The child (or not daemonized process) will continue here. */
    L (LOGLEVEL_WARNING, PACKAGE_NAME "-" PACKAGE_VERSION " starting");
    L (LOGLEVEL_DEBUG, "Command line : %s", cmdline_str);
    g_free(cmdline_str);

#if GLIB_G_THREAD_INIT_NEEDED == 1
    g_thread_init(NULL);
#endif

    curl_global_init(CURL_GLOBAL_ALL);

    collectps_httpd_daemon_init();
    collectps_thread_pool_init();

    /* Signals initialization {{{ */
    memset (&sa, 0, sizeof (sa));
    sa.sa_sigaction = signal_handler_for_stop;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&(sa.sa_mask));
    if(0 != sigaction(SIGTERM, &sa, NULL)) {
        L (LOGLEVEL_CRITICAL, "Could not set signal handler for TERM");
        exit(EXIT_FAILURE);
    }
    if(0 != sigaction(SIGINT, &sa, NULL)) {
        L (LOGLEVEL_CRITICAL, "Could not set signal handler for INT");
        close_all_and_exit(EXIT_FAILURE);
    }

    if(0 != sigaction(SIGQUIT, &sa, NULL)) {
        L (LOGLEVEL_CRITICAL, "Could not set signal handler for QUIT");
        close_all_and_exit(EXIT_FAILURE);
    }
    /* }}} */

    collectps_httpd_daemon_start();

    uev_start_listeners(loop);

    ev_loop (loop, 0);

    L (LOGLEVEL_WARNING, PACKAGE_NAME "-" PACKAGE_VERSION " ending");

    exit(EXIT_SUCCESS);
}

/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

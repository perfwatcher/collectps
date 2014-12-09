#ifndef UTIL_PROCESS_INFO_H
#define UTIL_PROCESS_INFO_H

#include "util_time.h"


void process_info__add_process(
        cdtime_t tm, /* timestamp */
        int pid, /* PID */
        int ppid, /* Parent PID */
        unsigned long uid, /* UID */
        char *pw_name, /* User name */
        unsigned long gid, /* GID */
        char *gr_name, /* group name */
        long rss, /* RSS (memory) */
        unsigned long sload, /* load in kernelland */
        unsigned long uload, /* load in userland */
        char *name, /* name */
        char *cmd /* command */
        );
void process_info__send_all();
void process_info__garbage_collector();
void process_info__init();


#endif
/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

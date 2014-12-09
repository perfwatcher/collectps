#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>

#include "collectps_agent.h"

extern cps_config_t global_config;
static loglevel_e log_level = 0;

const char *log_level_string(loglevel_e level) { /* {{{ */
    const char *rs;

    rs = "UNDEF";
    switch(level) {
        case LOGLEVEL_CRITICAL : rs = "CRITICAL"; break;
        case LOGLEVEL_ERROR : rs = "ERROR"; break;
        case LOGLEVEL_WARNING : rs = "WARNING"; break;
        case LOGLEVEL_INFO : rs = "INFO"; break;
        case LOGLEVEL_DEBUG : rs = "DEBUG"; break;
        case LOGLEVEL_DEBUG_VERBOSE : rs = "DEBUG_VERBOSE"; break;
    }

    return(rs);
} /* }}} log_level_string */

void util_log_set_level(loglevel_e level) { /* {{{ */
    log_level = level;
} /* }}} util_log_set_level */

void util_log(loglevel_e level, char *file, int line, gboolean allow_recurion, char *format, ...) { /* {{{ */
    va_list ap;
    time_t tm;
    struct tm stm;
    char buffer[2048];
    char timebuffer[] = "YYYY/MM/DD HH:MM:SSgarbage";
    FILE *fd;
    gboolean can_write_to_file;
    pid_t pid;

    /* Check the level and leave if too low */
    if(level > log_level) return;

    /* Prepare the args for fprintf */
    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    /* Prepare the date */
    tm = time(NULL);
    if(NULL == (localtime_r(&tm, &stm))) {
        strcpy(timebuffer, "YYYY/MM/DD HH:MM:SS");
    } else {
        strftime(timebuffer, sizeof(timebuffer), "%Y/%m/%d %H:%M:%S", &stm);
    }

    /* Prepare the pid */
    pid = getpid();

    /* Prepare the output */
    fd = stderr;
    can_write_to_file = FALSE;

    /* Open the log file (if not stderr) */
    if(NULL != global_config.log__file) {
        if(!strcmp(global_config.log__file, "stderr")) {
            fd = stderr;
            can_write_to_file = FALSE;
        } else if(!strcmp(global_config.log__file, "stdout")) {
            fd = stdout;
            can_write_to_file = FALSE;
        } else {
            if(NULL == (fd = fopen(global_config.log__file, "a"))) {
                fprintf(stderr, "%s : could not open %s (%s:%d). Below is the message :\n", timebuffer, global_config.log__file, __FILE__, __LINE__);
                fd = stderr;
                can_write_to_file = FALSE;
            } else {
                can_write_to_file = TRUE;
            }
        }
    }
    /* Write the log message */
    fprintf(fd, "%s [%d] (%s:%d) %s : %s\n", timebuffer, pid, file, line, log_level_string(level), buffer);

    /* Flush and close (close if not stdout/stderr) */
    fflush(fd);
    if(can_write_to_file) {
        fclose(fd);
    }

} /* }}} util_log */

/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

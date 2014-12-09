#ifndef UTIL_PREV_PROCESS_INFO_H
#define UTIL_PREV_PROCESS_INFO_H

#include "util_time.h"

int prev_process_info__compute_load(cdtime_t tm, pid_t pid, unsigned long stime, unsigned long  utime, unsigned long *spercent, unsigned long *upercent);
void prev_process_info__garbage_collector(cdtime_t tm);
void prev_process_info__init();


#endif
/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

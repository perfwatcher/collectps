#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <glib.h>

#define L(level, format, ...) do { util_log((level), __FILE__, __LINE__, TRUE, (format), ## __VA_ARGS__); } while(0)
#define LDEBUG() do { util_log((LOGLEVEL_CRITICAL), __FILE__, __LINE__, TRUE, "THIS LINE SHOULD NOT APPEAR IN PRODUCTION RELEASE"); } while(0)

void util_log_set_level(loglevel_e);
void util_log(loglevel_e level, char *file, int line, gboolean allow_recurion, char *format, ...);

#endif
/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */


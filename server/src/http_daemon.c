#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <microhttpd.h>

#include <collectps.h>

extern config_t global_config;

struct MHD_Daemon *collectps_daemon;

int answer_to_connection ( /* {{{ */
        void *cls, struct MHD_Connection *connection, 
        const char *url, 
        const char *method, const char *version, 
        const char *upload_data, 
        size_t *upload_data_size, void **con_cls)
{
    const char *page  = "<html><body>Hello, browser!</body></html>";

    struct MHD_Response *response;
    int ret;

    response = MHD_create_response_from_buffer (strlen (page),
            (void*) page, MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return ret;
} /* }}} */

int collectps_httpd_daemon_init() { /* {{{ */

    return(0);
} /* }}} */

int collectps_httpd_daemon_start() { /* {{{ */

    collectps_daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, global_config.httpd__port, NULL, NULL, 
            &answer_to_connection, NULL, MHD_OPTION_END);
    if (NULL == collectps_daemon) return 1;

    return(0);
} /* }}} */

int collectps_httpd_daemon_stop() { /* {{{ */
    MHD_stop_daemon (collectps_daemon);
    return(0);
} /* }}} */

/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <glib.h>
#include <curl/curl.h>

#include "config.h"
#include <collectps.h>

extern cps_config_t global_config;

static char *influxdb_full_url__series = NULL;

static CURL *worker_influxdb_curl_init() { /* {{{ */
    CURL *curl = curl_easy_init();
    static gsize influxdb_full_url__series_is_initialized = 0;

    if(g_once_init_enter(&influxdb_full_url__series_is_initialized)) {
        char *database;
        char *username;
        char *password;

        database = curl_easy_escape(curl, global_config.influxdb__database, 0);
        username = curl_easy_escape(curl, global_config.influxdb__username, 0);
        password = curl_easy_escape(curl, global_config.influxdb__password, 0);

        influxdb_full_url__series = g_strdup_printf("%s/%s/series?u=%s&p=%s",
                global_config.influxdb__url,
                database,
                username,
                password
                );
        curl_free(database);
        curl_free(username);
        curl_free(password);
        g_once_init_leave(&influxdb_full_url__series_is_initialized, 1);
    }

    return(curl);
} /* }}} */

static int worker_influxdb_curl_send(CURL *curl, object_buffer_t *obj) { /* {{{ */
    char *json_string = NULL;
    CURLcode c_rc;

    if(NULL == curl) {
        L (LOGLEVEL_ERROR, "Curl is not initialized. Something wrong may have happened");
        return(-1);
    }
    if(NULL == influxdb_full_url__series) {
        L (LOGLEVEL_ERROR, "Curl URL is not initialized. Something wrong may have happened");
        return(-1);
    }

    json_string = object_buffer_to_json(obj);

    curl_easy_setopt(curl, CURLOPT_URL, influxdb_full_url__series);
    curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, json_string);
    if((-1 != global_config.influxdb__proxy_port) && (NULL != (global_config.influxdb__proxy_url))) {
        /* Set the proxy */
        curl_easy_setopt(curl, CURLOPT_PROXY, global_config.influxdb__proxy_url);
        curl_easy_setopt(curl, CURLOPT_PROXYPORT, global_config.influxdb__proxy_port);
    } else {
        /* Disable proxy for this URL */
        char *h;
        int i;
        for(i=0; global_config.influxdb__url[i] && global_config.influxdb__url[i] != '/'; i++);
        for(;global_config.influxdb__url[i] && global_config.influxdb__url[i] == '/'; i++);
        if(global_config.influxdb__url[i]) {
            h = g_strdup(global_config.influxdb__url+i);
            for(i=0; h[i] && h[i] != '/' && h[i] != ':'; i++);
            h[i] = '\0';
        }
        curl_easy_setopt(curl, CURLOPT_NOPROXY, h);
        g_free(h);
    }

    c_rc = curl_easy_perform(curl);

    if(c_rc != CURLE_OK) {
        L (LOGLEVEL_WARNING, "curl_easy_perform() failed: %s", curl_easy_strerror(c_rc));
    }

    g_free(json_string);
    return(0);
} /* }}} */

static void worker_influxdb_curl_close(CURL *curl) { /* {{{ */
    if(NULL != curl) {
        curl_easy_cleanup(curl);
        curl=NULL;
    }
} /* }}} */

void worker_parser(gpointer data, gpointer user_data) { /* {{{ */
    object_buffer_t *obj = data;
    CURL *curl;
    char *rs;

    curl = worker_influxdb_curl_init();
    worker_influxdb_curl_send(curl, obj);
    worker_influxdb_curl_close(curl);
    rs = object_buffer_to_json(obj);

    L (LOGLEVEL_DEBUG, "Worker parsing an object");
    L (LOGLEVEL_DEBUG, "Content : %s", rs?rs:"(null)");
    g_free(rs);

    object_buffer_free(obj);

} /* }}} */

/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

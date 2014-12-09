#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <glib.h>

#include "collectps_agent.h"

extern cps_config_t global_config;

static int disconnect_one(server_info_t *si) { /* {{{ */
    L(LOGLEVEL_WARNING, "Disconnecting from %s:%d", si->hostname, si->port);
    if(si->sockfd >= 0) {
        shutdown(si->sockfd, SHUT_RDWR);
        close(si->sockfd);
    }
    si->sockfd = -1;
    return(0);
} /* }}} */

int cps_send_data(server_info_t *si, char *buffer, size_t buffer_len) { /* {{{ */
    size_t n;
    size_t write_remaining;
    char *next_write;

    errno=0;
    write_remaining = buffer_len;
    next_write = buffer;
    while(write_remaining > 0) {
        n = write(si->sockfd, next_write, write_remaining);
        if(-1 == n) {
            char errbuf[1024];
            strerror_r (errno, errbuf, sizeof (errbuf));
            L(LOGLEVEL_WARNING, "Failed to write to %s:%d : %s", si->hostname, si->port, errbuf);
            disconnect_one(si);
            return(-1);
        }
        next_write+=n;
        write_remaining-=n;
    }
    return(0);
} /* }}} */

static int connect_one(char *hostname, int port) { /* {{{ */
    int sockfd;
    struct hostent *server;
    struct sockaddr_in server_addr;

    if(-1 == (sockfd = socket(AF_INET, SOCK_STREAM, 0))) {
        L(LOGLEVEL_ERROR, "Could not create a socket");
        return(-1);
    }
    if(NULL == (server = gethostbyname(hostname))) {
        L(LOGLEVEL_WARNING, "hostname '%s' does not exist", hostname);
        close(sockfd);
        return(-1);
    }

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);

    if (0 != connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
        L(LOGLEVEL_WARNING, "Failed to connect to %s:%d", hostname, port);
        close(sockfd);
        return(-1);
    } 
    return(sockfd);
} /* }}} */

void cps_connect_to_all_servers() { /* {{{ */
    GList *cur;
    for(cur=global_config.server__server; cur; cur = cur->next) {
        server_info_t *el = cur->data;
        if(-1 == el->sockfd) {
            el->sockfd = connect_one(el->hostname, el->port);
        }
    }
} /* }}} */



/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

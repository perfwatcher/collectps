/**
 * collectps - util_process_info.c
 * Copyright (C) 2014 Yves Mettier and other authors listed below
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors:
 * Yves Mettier <ymettier at free.fr>
 **/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

#include <glib.h>

#include "config.h"

#include "collectps_agent.h"

#define SIZEOF_BUFFER_FOR_NUMERIC_VALUES 21 /* 2^64 contains 20 digits */

extern cps_config_t global_config;

static GList *process_info__list = NULL;

typedef struct {
    cdtime_t tm; /* timestamp */
    int pid; /* PID */
    int ppid; /* Parent PID */
    unsigned long uid; /* UID */
    char *pw_name; /* User name */
    unsigned long gid; /* GID */
    char *gr_name; /* group name */
    long rss; /* RSS (memory) */
    unsigned long sload; /* load in kernelland */
    unsigned long uload; /* load in userland */
    char *name; /* name */
    char *cmd; /* command */
} process_info__metrics_t;

typedef struct {
    process_info__metrics_t info;
    char *packet_data;
    size_t packet_data_allocated_size;
    size_t packet_length;
    GList *server;
} process_info_t;

/* From collectPS util_buffer.h */

/* Packet format
 * =============
 * |  2   |   4    | n    |
 * | type | length | data |
 *
 * type (2 bytes) is one of the types described in packet_type_e
 * length (4 bytes) is the length of the packet, including the 6 header bytes
 *
 * Object format
 * =============
 * An object is either a simple packet or composite.
 * PACKET_TYPE__LIST and PACKET_TYPE__HASH are composite.
 * Composite packets start with a packet of PACKET_TYPE__LIST or PACKET_TYPE__HASH and
 * its data is the number of elements.
 *
 * Note : with PACKET_TYPE__HASH, an element is also composite and is made of
 * one PACKET_TYPE__STRING followed with another simple packet.
 *
 * Note : composite of composite is not allowed (not supported yet ?).
 */

typedef enum {
    PACKET_TYPE__STRING  = 0x0000, /* length : variable ; note : should be NULL terminated */
    PACKET_TYPE__NUMERIC = 0x0001, /* length : 8 bytes  ; note : packet length is 14 */
    PACKET_TYPE__LIST    = 0x0002, /* length : 8 bytes. data is the nb of elements */
    PACKET_TYPE__HASH    = 0x0003, /* length : 8 bytes. data is the nb of elements */
    PACKET_TYPE__UNDEFINED = 0xFFFE, /* should not happen */
} packet_type_e;

#ifndef HAVE_HTONLL
unsigned long long htonll (unsigned long long n) { /* {{{ */
#ifdef WORDS_BIGENDIAN
    return (n);
#else
    return (((unsigned long long) htonl (n)) << 32) + htonl (n >> 32);
#endif
} /* unsigned long long htonll */ /* }}} */
#endif /* HAVE_HTONLL */

static int buffer_append_string(char *buffer, char *str, size_t len) { /* {{{ */
    /* Note : len is the length of the string, not the size of it. Add the ending \0 char */
    uint16_t packet_type;
    uint32_t packet_length;

    packet_type = htons(PACKET_TYPE__STRING);
    packet_length = htonl(sizeof(packet_type) + sizeof(packet_length) + len + 1);

    memcpy(buffer, &packet_type, sizeof(packet_type));
    memcpy(buffer + sizeof(packet_type), &packet_length, sizeof(packet_length));
    memcpy(buffer + sizeof(packet_type) + sizeof(packet_length), str, len+1);

    return(sizeof(packet_type) + sizeof(packet_length) + len + 1);
} /* }}} */

static int buffer_append_hash_header(char *buffer, int nb_elem) { /* {{{ */
    uint16_t packet_type;
    uint32_t packet_length;
    uint64_t packet_value;
    assert(14 == (sizeof(packet_type) + sizeof(packet_length) + sizeof(packet_value)));

    packet_type = htons(PACKET_TYPE__HASH);
    packet_length = htonl(sizeof(packet_type) + sizeof(packet_length) + sizeof(packet_value));
    packet_value = htonll(nb_elem);
    memcpy(buffer, &packet_type, sizeof(packet_type));
    memcpy(buffer + sizeof(packet_type), &packet_length, sizeof(packet_length));
    memcpy(buffer + sizeof(packet_type) + sizeof(packet_length), &packet_value, sizeof(packet_value));

    return(sizeof(packet_type) + sizeof(packet_length) + sizeof(packet_value));
} /* }}} */

static int buffer_append_int_as_string(char *buffer, int value) { /* {{{ */
    char value_buffer[SIZEOF_BUFFER_FOR_NUMERIC_VALUES];
    int n;

    n = snprintf(value_buffer, sizeof(value_buffer), "%d", value);
    assert(n < sizeof(value_buffer));

    return(buffer_append_string(buffer, value_buffer, n));
} /* }}} */

#define SIZEOF_PACKET_STRING_WITH_LEN(len) (sizeof(uint16_t) + sizeof(uint32_t) + len + 1)
#define SIZEOF_PACKET_STRING_AND_CALC_SIZE(s) (sizeof(uint16_t) + sizeof(uint32_t) + (size_##s = (strlen(pi->info.s))) + 1)
#define SIZEOF_PACKET_CHARS(s) (sizeof(uint16_t) + sizeof(uint32_t) + sizeof(s) )
#define SIZEOF_PACKET_NUMERIC (sizeof(uint16_t) + sizeof(uint32_t) + SIZEOF_BUFFER_FOR_NUMERIC_VALUES )
#define SIZEOF_PACKET_HASH_HEADER (sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint64_t) )
static void process_info__packet_new(process_info_t *pi) { /* {{{ */
    size_t s = 0;
    int nb_elem = 0;
    char *wr;

    size_t size_pw_name = -1;
    size_t size_gr_name = -1;
    size_t size_name = -1;
    size_t size_cmd = -1;

    assert(NULL == pi->packet_data);

    s += SIZEOF_PACKET_HASH_HEADER;
    s += SIZEOF_PACKET_CHARS("cdtime") + SIZEOF_PACKET_NUMERIC; nb_elem++;
    s += SIZEOF_PACKET_CHARS("hostname") + SIZEOF_PACKET_STRING_WITH_LEN(global_config.agent__hostname_length); nb_elem++;
    s += SIZEOF_PACKET_CHARS("pid") + SIZEOF_PACKET_NUMERIC; nb_elem++;
    s += SIZEOF_PACKET_CHARS("ppid") + SIZEOF_PACKET_NUMERIC; nb_elem++;
    s += SIZEOF_PACKET_CHARS("uid") + SIZEOF_PACKET_NUMERIC; nb_elem++;
    if(pi->info.pw_name) {
        s += SIZEOF_PACKET_CHARS("pw_name") + SIZEOF_PACKET_STRING_AND_CALC_SIZE(pw_name); nb_elem++;
    }
    s += SIZEOF_PACKET_CHARS("gid") + SIZEOF_PACKET_NUMERIC; nb_elem++;
    if(pi->info.gr_name) {
        s += SIZEOF_PACKET_CHARS("gr_name") + SIZEOF_PACKET_STRING_AND_CALC_SIZE(gr_name); nb_elem++;
    }
    s += SIZEOF_PACKET_CHARS("rss") + SIZEOF_PACKET_NUMERIC; nb_elem++;
    s += SIZEOF_PACKET_CHARS("sload") + SIZEOF_PACKET_NUMERIC; nb_elem++;
    s += SIZEOF_PACKET_CHARS("uload") + SIZEOF_PACKET_NUMERIC; nb_elem++;
    if(pi->info.name) {
        s += SIZEOF_PACKET_CHARS("name") + SIZEOF_PACKET_STRING_AND_CALC_SIZE(name); nb_elem++;
    }
    if(pi->info.cmd) {
        s += SIZEOF_PACKET_CHARS("cmd") + SIZEOF_PACKET_STRING_AND_CALC_SIZE(cmd); nb_elem++;
    }

    pi->packet_data_allocated_size = s;
    pi->packet_data = g_slice_alloc(s);
    wr = pi->packet_data;

    wr += buffer_append_hash_header(wr, nb_elem);

    wr += buffer_append_string(wr, "cdtime", sizeof("cdtime")-1);
    wr += buffer_append_int_as_string(wr, CDTIME_T_TO_TIME_T(pi->info.tm));

    wr += buffer_append_string(wr, "hostname", sizeof("hostname")-1);
    wr += buffer_append_string(wr, global_config.agent__hostname, global_config.agent__hostname_length);

    wr += buffer_append_string(wr, "pid", sizeof("pid")-1);
    wr += buffer_append_int_as_string(wr, pi->info.pid);

    wr += buffer_append_string(wr, "ppid", sizeof("ppid")-1);
    wr += buffer_append_int_as_string(wr, pi->info.ppid);

    wr += buffer_append_string(wr, "uid", sizeof("uid")-1);
    wr += buffer_append_int_as_string(wr, pi->info.uid);

    if(pi->info.pw_name) {
        assert(-1 != size_pw_name);
        wr += buffer_append_string(wr, "pw_name", sizeof("pw_name")-1);
        wr += buffer_append_string(wr, pi->info.pw_name, size_pw_name);
    }

    wr += buffer_append_string(wr, "gid", sizeof("gid")-1);
    wr += buffer_append_int_as_string(wr, pi->info.gid);

    if(pi->info.gr_name) {
        assert(-1 != size_gr_name);
        wr += buffer_append_string(wr, "gr_name", sizeof("gr_name")-1);
        wr += buffer_append_string(wr, pi->info.gr_name, size_gr_name);
    }

    wr += buffer_append_string(wr, "rss", sizeof("rss")-1);
    wr += buffer_append_int_as_string(wr, pi->info.rss);

    wr += buffer_append_string(wr, "sload", sizeof("sload")-1);
    wr += buffer_append_int_as_string(wr, pi->info.sload);

    wr += buffer_append_string(wr, "uload", sizeof("uload")-1);
    wr += buffer_append_int_as_string(wr, pi->info.uload);

    if(pi->info.name) {
        assert(-1 != size_name);
        wr += buffer_append_string(wr, "name", sizeof("name")-1);
        wr += buffer_append_string(wr, pi->info.name, size_name);
    }

    if(pi->info.cmd) {
        assert(-1 != size_cmd);
        wr += buffer_append_string(wr, "cmd", sizeof("cmd")-1);
        wr += buffer_append_string(wr, pi->info.cmd, size_cmd);
    }

    pi->packet_length = wr - pi->packet_data;

} /* }}} */



void process_info__add_process( /* {{{ */
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
        ) {

    process_info_t *pi;
    pi = g_slice_alloc(sizeof(*pi));

    pi->server = NULL;

    pi->info.tm = tm;
    pi->info.pid = pid;
    pi->info.ppid = ppid;
    pi->info.uid = uid;
    pi->info.pw_name = pw_name;
    pi->info.gid = gid;
    pi->info.gr_name = gr_name;
    pi->info.rss = rss;
    pi->info.sload = sload;
    pi->info.uload = uload;
    pi->info.name = name;
    pi->info.cmd = cmd;

    pi->server = g_list_copy(global_config.server__server);
    pi->packet_data = NULL;
    pi->packet_data_allocated_size = 0;
    pi->packet_length = 0;

    process_info__packet_new(pi);

    process_info__list = g_list_append(process_info__list, pi);
} /* }}} */

static void process_info__free_item(process_info_t *pi) { /* {{{ */
    assert(NULL == pi->server);
    if(pi->info.pw_name) g_free(pi->info.pw_name);
    if(pi->info.gr_name) g_free(pi->info.gr_name);
    if(pi->info.name) g_free(pi->info.name);
    if(pi->info.cmd) g_free(pi->info.cmd);
    if(pi->packet_data) g_slice_free1(pi->packet_data_allocated_size, pi->packet_data);
    g_slice_free1(sizeof(*pi), pi);
} /* }}} */

#ifdef NEED__PROCESS_INFO__PRINT_LOG_ITEM
static void process_info__print_log_item(process_info_t *pi) { /* {{{ */
    char buf[4096];
    char buftime[40];
    const struct tm *stm;
    time_t tm;

    tm = CDTIME_T_TO_TIME_T(pi->info.tm);
    stm = localtime(&tm);

    strftime(buftime, sizeof(buftime), "%Y-%m-%d %H:%M:%S", stm); 
    snprintf(buf, sizeof(buf), "\n%s %5d %5d %5lu %10s %5lu %10s %10ld %3ld %3ld %s ][ %s",
            buftime,
            pi->info.pid,
            pi->info.ppid,
            pi->info.uid,
            pi->info.pw_name?pi->info.pw_name:"NA",
            pi->info.gid,
            pi->info.gr_name?pi->info.gr_name:"NA",
            pi->info.rss,
            pi->info.sload,
            pi->info.uload,
            pi->info.name?pi->info.name:"NA",
            pi->info.cmd?pi->info.cmd:"NA");
    L (LOGLEVEL_INFO, buf);

} /* }}} */
#endif

static void process_info__send_one(process_info_t *pi) { /* {{{ */
    GList *cur = pi->server;

    while (cur != NULL) {
        GList *next = cur->next;
        int s_rc = 0;
        server_info_t *si = cur->data;

        if(si && si->sockfd >= 0) {
            #ifdef DEBUG_PRINT_PROCESS_INFO_BEFORE_SENDING_IT
            process_info__print_log_item(pi);
            #endif
            s_rc = cps_send_data(si,pi->packet_data, pi->packet_length);
            if(0 == s_rc) {
                pi->server = g_list_delete_link(pi->server, cur);
            }
        }
        cur = next;
    }


} /* }}} */

void process_info__send_all() { /* {{{ */
    GList *cur;
    cps_connect_to_all_servers();

    for(cur=process_info__list; cur; cur = cur->next) {
        process_info_t *pi = cur->data;
        process_info__send_one(pi);
    }

} /* }}} */

static void process_info__print_log_nb_item(char *msg) { /* {{{ */
    int n = 0;
    GList *cur;

    for(cur=process_info__list; cur; cur = cur->next) n++;

    L(LOGLEVEL_DEBUG, "process_info__list length is %d (%s)", n, msg);
} /* }}} */

void process_info__garbage_collector() { /* {{{ */
    GList *cur = process_info__list;
    cdtime_t now = cdtime();
    process_info__print_log_nb_item("Before garbage collector");
    while (cur != NULL) {
        GList *next = cur->next;
        process_info_t *pi = cur->data;
        if ((now - pi->info.tm) > global_config.agent__max_cache_cdtime) {
            /* Remove all pi->server because this ps is too old. */
            GList *cur_server = pi->server;

            while (cur_server != NULL) {
                GList *next_server = cur_server->next;

                pi->server = g_list_delete_link(pi->server, cur_server);
                cur_server = next_server;
            }
        }
        if (NULL == pi->server) {
            process_info__free_item(pi);
            process_info__list = g_list_delete_link (process_info__list, cur);
        }
        cur = next;
    }
    process_info__print_log_nb_item("After garbage collector");
} /* }}} */

void process_info__init() { /* {{{ */
    process_info__list = NULL;
    L(LOGLEVEL_DEBUG_VERBOSE, "agent__max_cache_cdtime = %ud", global_config.agent__max_cache_cdtime);
    L(LOGLEVEL_DEBUG_VERBOSE, "agent__max_cache_time = %ud", global_config.agent__max_cache_time);
} /* }}} */


/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

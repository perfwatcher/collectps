#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <glib.h>

#include "config.h"
#include <collectps.h>

extern cps_config_t global_config;

#ifndef HAVE_HTONLL
unsigned long long ntohll (unsigned long long n)
{
#if BYTE_ORDER == BIG_ENDIAN
    return (n);
#else
    return (((unsigned long long) ntohl (n)) << 32) + ntohl (n >> 32);
#endif
} /* unsigned long long ntohll */
#endif

uint64_t parse_part_number(uint8_t *buf) { /* {{{ */
    uint64_t tmp64;
    memcpy ((void *) &tmp64, buf, sizeof (tmp64));
    return(ntohll(tmp64));
} /* }}} */

packet_buffer_t *packet_buffer_new() { /* {{{ */
    packet_buffer_t *buf;
    if(NULL == (buf = g_slice_alloc(sizeof(*buf)))) return(NULL);
    buf->write_offset = 0;
    buf->buf = NULL;
    buf->len = 0;
    buf->ready = FALSE;
    buf->garbage = FALSE;
    buf->state = PACKET_STATE__HEADER;
    return(buf);
} /* }}} */

int packet_buffer_alloc_buffer(packet_buffer_t *buf) { /* {{{ */
    int rc = 0;
    uint16_t type;
    uint32_t len;
    assert(NULL != buf);

    memcpy(&type, &(buf->header.b[0]), sizeof(type));
    memcpy(&len, &(buf->header.b[2]), sizeof(len));

    len = ntohl (len) - PACKET_HEADER_SIZE;
    buf->header.field.length = len;

    type = ntohs (type);
    buf->header.field.type = type;

    if(len > global_config.listener__buffer_max_size) {
        L (LOGLEVEL_DEBUG, "Receiving a packet with length = %zd, which is bigger than buffer_max_size", len);
        rc = -3;
        goto packet_buffer_alloc_buffer__error;
    }

    buf->write_offset = 0;
    buf->len = len;
    buf->ready = FALSE;

    switch(type) {
        case PACKET_TYPE__STRING: buf->state = PACKET_STATE__STRING; break;
        case PACKET_TYPE__NUMERIC: buf->state = PACKET_STATE__NUMERIC; break;
        case PACKET_TYPE__LIST: buf->state = PACKET_STATE__LIST; break;
        case PACKET_TYPE__HASH: buf->state = PACKET_STATE__HASH; break;
        default: 
                                L (LOGLEVEL_DEBUG, "Receiving a packet with unknown type = %hd", type);
                                rc = -4; 
                                goto packet_buffer_alloc_buffer__error;
    }

    if(NULL == (buf->buf = g_slice_alloc(buf->len * sizeof(*(buf->buf))))) {
        rc = -5;
        goto packet_buffer_alloc_buffer__error;
    }
    return(0);
packet_buffer_alloc_buffer__error:
    buf->garbage = TRUE;
    return(rc);
} /* }}} */

void packet_buffer_free(packet_buffer_t *buf) { /* {{{ */
    if(! buf) return;
    if(buf->buf) g_slice_free1(buf->len * sizeof(*(buf->buf)), buf->buf);
    g_slice_free1(sizeof(*buf), buf);
} /* }}} */

ssize_t packet_buffer_read(int fd, packet_buffer_t *buf) { /* {{{ */
    ssize_t n;
    ssize_t count;

    if(buf->state == PACKET_STATE__HEADER) {
        int rc = 0;
        if( 0 >= (n = read(fd, buf->header.b + buf->write_offset, PACKET_HEADER_SIZE - buf->write_offset))) return(n);
        buf->write_offset += n;
        if(buf->write_offset < PACKET_HEADER_SIZE) return(n);
        if(0 > (rc = packet_buffer_alloc_buffer(buf))) return(rc);
    }
    assert( ! buf->garbage );
    assert(buf->state != PACKET_STATE__HEADER);
    
    count = buf->len - buf->write_offset;
    
    if( 0 >= (n = read(fd, buf->buf + buf->write_offset, count))) return(n);
    buf->write_offset += n;
    if(buf->write_offset >= buf->len) {
        switch(buf->header.field.type) {
            case PACKET_TYPE__NUMERIC: 
            case PACKET_TYPE__LIST: 
            case PACKET_TYPE__HASH: 
                buf->numeric = ntohll(*((uint64_t*) buf->buf));
                break;
        }
        buf->ready = TRUE;
    }

    return n;
} /* }}} */

ssize_t packet_buffer_write(int fd, packet_buffer_t *buf) { /* {{{ */
    size_t n;
    size_t count;

    if(buf->state == PACKET_STATE__HEADER) {
        if( 0 >= (n = write(fd, buf->header.b + buf->write_offset, PACKET_HEADER_SIZE - buf->write_offset))) return(n);
        buf->write_offset += n;
        if(buf->write_offset < PACKET_HEADER_SIZE) return(n);
        buf->state = PACKET_STATE__ANY;
        buf->write_offset = 0;
    }
    assert(buf->state != PACKET_STATE__HEADER);

    count = buf->len - buf->write_offset;

    if( 0 >= (n = write(fd, buf->buf + buf->write_offset, count))) return(n);
    buf->write_offset += n;
    if(buf->write_offset >= buf->len) {
        buf->ready = TRUE;
    }

    return n;
} /* }}} */

char *packet_buffer_to_string(packet_buffer_t *buf) { /* {{{ */
    char *rs = NULL;
    assert( ! buf->garbage );
    assert( buf->ready );
    switch(buf->header.field.type) {
        case PACKET_TYPE__STRING: 
            if(buf->len > 0) {
                buf->buf[buf->len -1] = '\0';
                rs = g_strdup_printf("String [[%s]]", (char *)buf->buf);
            } else {
                rs = g_strdup("NULL string");
            }
            break;
        case PACKET_TYPE__NUMERIC: 
            rs = g_strdup_printf("%zd (0x%zX)", buf->numeric, buf->numeric);
            break;
        case PACKET_TYPE__LIST: 
            rs = g_strdup_printf("Beginning list with %zd items", buf->numeric);
            break;
        case PACKET_TYPE__HASH: 
            rs = g_strdup_printf("Beginning hash with %zd items", buf->numeric);
            break;
        default: 
            rs = g_strdup_printf("Unknown type %d", buf->header.field.type);
    }
    return(rs);
} /* }}} */

static size_t strlen_escape_json(char *str) { /* {{{ */
    size_t i,escaped;
    escaped = 0;
    for(i=0; str[i]; i++) {
        if((str[i] == '"') || (str[i] == '\\')) escaped++;
    }
    return(escaped+i+sizeof("\"\"")-1);
} /* }}} */

static size_t strcpy_escape_json(char *dest, char *src, size_t len, gboolean add_quotes, gboolean append_comma) { /* {{{ */
/* Returns :
 * -1 : error
 * value > len : len chars including start and end quotes were copied, but not the ending \0.
 *               The string may have been truncated.
 *               This is a case of error.
 * value <= len : nb of copied chars including the quotes and ending \0 : SUCCESS
 */
    size_t in,out;
    size_t extra_chars_size = (add_quotes?sizeof("\"\"")-1:0) + (append_comma?sizeof(",")-1:0) + 1;
    if(len < extra_chars_size) return(-1);

    out=0;
    if(add_quotes) dest[out++] = '"';
    in = 0;

    while(src[in] && out < (len+2-extra_chars_size)) {
        if((src[in] == '"') || (src[in] == '\\')) {
            if(out+extra_chars_size-2 > len) {
                break;
            }
            dest[out++] = '\\';
        }
        dest[out++] = src[in++];
    }
    if(add_quotes) dest[out++] = '"';
    if(append_comma) dest[out++] = ',';
    if(out < len) {
        dest[out++] = '\0';
    } else {
        out++;
    }
    return(out);
} /* }}} */

static size_t packet_buffer_to_json(packet_buffer_t *buf, char *dest, size_t len, gboolean add_quotes, gboolean append_comma) { /* {{{ */
#define JSON_EMPTY_STRING "\"\""
    assert( ! buf->garbage );
    assert( buf->ready );
    size_t rl = 0;
    switch(buf->header.field.type) {
        case PACKET_TYPE__STRING: 
            if(buf->len > 0) {
                buf->buf[buf->len -1] = '\0';
                rl = strcpy_escape_json(dest, (char *)buf->buf, len, add_quotes, append_comma);
                if((rl == -1) || (rl > len)) {
                    L (LOGLEVEL_CRITICAL, "Not enough space to copy '%s'");
                    rl = -1;
                }
            } else if(append_comma && (len >= sizeof(JSON_EMPTY_STRING ","))) {
                memcpy(dest, JSON_EMPTY_STRING ",", sizeof(JSON_EMPTY_STRING ","));
                rl = sizeof(JSON_EMPTY_STRING ",") - 1;
            } else if(len >= sizeof(JSON_EMPTY_STRING)) {
                memcpy(dest, JSON_EMPTY_STRING, sizeof(JSON_EMPTY_STRING));
                rl = sizeof(JSON_EMPTY_STRING) - 1;
            } else {
                L (LOGLEVEL_CRITICAL, "Not enough space to copy an empty string !");
                rl = -1;
            }
            break;
        case PACKET_TYPE__NUMERIC: 
            rl = snprintf(dest, len, "%zd", buf->numeric);
            if(rl < 0) {
                L (LOGLEVEL_CRITICAL, "An error occured while converting numeric to string (%zd)", buf->numeric);
                rl = -1;
            } else if(rl >= len) {
                L (LOGLEVEL_CRITICAL, "Not enough space to convert numeric (%zd) to string", buf->numeric);
                rl = -1;
            }
            break;
        case PACKET_TYPE__LIST:  /* Nothing to do */
        case PACKET_TYPE__HASH:  /* Nothing to do */
            break;
        default: 
            L (LOGLEVEL_WARNING, "packet_buffer_to_json, unknown type %d", buf->header.field.type);
    }
    return(rl);
} /* }}} */


object_buffer_t *object_buffer_new() { /* {{{ */
    object_buffer_t *obj;
    if(NULL == (obj = g_slice_alloc(sizeof(*obj)))) return(NULL);
    if(NULL == (obj->buf = packet_buffer_new())) {
        L (LOGLEVEL_ERROR, "Could not allocate memory");
        g_slice_free1(sizeof(*obj), obj);
        return(NULL);
    }

    obj->list = NULL;
    obj->type = PACKET_TYPE__UNDEFINED;
    obj->nb_elements = 0;
    obj->nb_elements_remaining = 0;
    obj->ready = FALSE;

    return(obj);                                               
} /* }}} */

void object_buffer_free(object_buffer_t *obj) { /* {{{ */
    GList *el;

    if(! obj) return;

    packet_buffer_free(obj->buf);

    for(el = obj->list; el; el = el->next) packet_buffer_free(el->data);
    g_list_free(obj->list);

    g_slice_free1(sizeof(*obj), obj);
} /* }}} */

ssize_t object_buffer_read(int fd, object_buffer_t *obj) { /* {{{ */
/* Object location :
 * if obj->list is set, read obj->list
 * else read obj->buf
 */
/* Return value :
 * n >= 0 : nb values read
 * n == -1 : read failed; errno is set
 * n == -2 : other failure. errno is not set
 */
    ssize_t n;
    if(0 > (n = packet_buffer_read(fd, obj->buf))) {
        int errno_sav = errno;
        if(obj->buf->garbage) {
            packet_buffer_free(obj->buf);
            obj->buf = NULL;
        }
        errno = errno_sav;
        return(n);
    }
    if(! obj->buf->ready) return(n);

    if(obj->type == PACKET_TYPE__UNDEFINED) {
        /* First packet */
        assert(obj->buf->state != PACKET_STATE__HEADER);

        obj->type = obj->buf->header.field.type;
        switch(obj->type) {
            case PACKET_TYPE__HASH : 
            case PACKET_TYPE__LIST :
                obj->nb_elements = parse_part_number(obj->buf->buf);
                obj->nb_elements_remaining = obj->nb_elements;
                if(0 == obj->nb_elements) {
                    obj->ready = TRUE;
                    return(n); 
                }

                if(PACKET_TYPE__HASH == obj->type) obj->nb_elements_remaining *= 2;

                obj->list = g_list_append(obj->list, obj->buf);
                if(NULL == (obj->buf = packet_buffer_new())) {
                    L (LOGLEVEL_ERROR, "Could not allocate memory");
                    object_buffer_free(obj);
                    return(-2);
                }
                break;
            default: obj->nb_elements = 0;
                     obj->nb_elements_remaining = 0;
                     obj->ready = TRUE;
        }
    } else {
        /* array or list */
        assert((obj->type == PACKET_TYPE__HASH) || (obj->type == PACKET_TYPE__LIST));
        obj->nb_elements_remaining -= 1;
        obj->list = g_list_append(obj->list, obj->buf);
        if(0 < obj->nb_elements_remaining) {
            if(NULL == (obj->buf = packet_buffer_new())) {
                L (LOGLEVEL_ERROR, "Could not allocate memory");
                object_buffer_free(obj);
                return(-2);
            }
        } else {
            obj->buf = NULL;
            obj->ready = TRUE;
        }
    }
    return(n);
} /* }}} */

char *object_buffer_to_string(object_buffer_t *obj) { /* {{{ */
    char *rs = NULL;

    if(obj->list) {
        gboolean is_hash = FALSE;
        gboolean is_hash_key = TRUE;
        GList *gl = obj->list;

        if(((packet_buffer_t*)gl->data)->header.field.type == PACKET_TYPE__HASH) {
            is_hash = TRUE;
        }

        for(gl = obj->list; gl; gl = gl->next) {
            char *prs = packet_buffer_to_string(gl->data);
            if(NULL == rs) {
                rs = g_strdup_printf("%s", prs);
            } else {
                char *old_rs = rs;
                if(is_hash) {
                    if(is_hash_key) {
                        rs = g_strdup_printf("%s\n  %s", rs, prs);
                        is_hash_key = FALSE;
                    } else {
                        rs = g_strdup_printf("%s => %s", rs, prs);
                        is_hash_key = TRUE;
                    }
                } else {
                    rs = g_strdup_printf("%s\n  %s", rs, prs);
                }
                g_free(old_rs);
            }
            g_free(prs);
        }

    } else {
        rs = packet_buffer_to_string(obj->buf);
    }

    return(rs);
} /* }}} */

#define PACKET_BUFFER_COMPUTE_SIZE(_maxlen, l, type) /* {{{ */do { \
    (_maxlen) = 0; \
        switch(type) { \
                                      /* We assert that all chars may be escaped */ \
            case PACKET_TYPE__STRING: (_maxlen) = 2*(l) + sizeof("''") -1; break; \
                                      /* 2^64-1 needs 20 digits; add the \0 to have 21 */ \
            case PACKET_TYPE__NUMERIC: (_maxlen) = 21; break; \
            case PACKET_TYPE__LIST: break; \
            case PACKET_TYPE__HASH: break; \
            default: L (LOGLEVEL_WARNING, "Unknown type %d", (type)); \
        } \
} while(0); /* }}} */

#ifdef JSON_AS_A_HASH
char *object_buffer_to_json(object_buffer_t *obj) { /* {{{ */
    char *rs = NULL;

    if(obj->list) {
        gboolean is_hash = FALSE;
        gboolean is_hash_key = TRUE;
        GList *gl = obj->list;
        size_t maxlen = 0;
        size_t pos = 0;


        /* Compute the size */
        maxlen = sizeof("[]");
        for(gl = obj->list; gl; gl = gl->next) {
            int l;
            packet_buffer_t *buf = gl->data;
            PACKET_BUFFER_COMPUTE_SIZE(l, buf->len, buf->header.field.type);
            if(l > 0) maxlen += (l+sizeof(",")-2);
        }

        rs = g_malloc(maxlen);

        /* Start the result string */
        if(((packet_buffer_t*)obj->list->data)->header.field.type == PACKET_TYPE__HASH) {
            is_hash = TRUE;
            rs[0] = '{';
        } else {
            rs[0] = '[';
        }
        pos++;
        maxlen--;

        /* Fill the result string */
        for(gl = obj->list; gl; gl = gl->next) {
            size_t l;
            l = packet_buffer_to_json(gl->data, rs+pos, maxlen, TRUE, FALSE);
            if(-1 == l) {
                g_free(rs);
                rs = NULL;
                break;
            } else if(l > 0) {
                pos += l;
                maxlen -= l;
                if(is_hash && is_hash_key) {
                    rs[pos] = ':';
                    is_hash_key = FALSE;
                } else {
                    rs[pos] = ',';
                    is_hash_key = TRUE;
                }
                pos++;
                maxlen--;
            }
            assert(maxlen >= 0);
        }
        /* End the result string */
        pos--; /* last char was a ',' so we *replace* it with the closing bracket */
        if(rs[pos] != ',') {
            /* ... except for empty structures */
            pos++; 
            maxlen--;
        }
        if(is_hash) {
            rs[pos] = '}';
        } else {
            rs[pos] = ']';
        }
        pos++;
        assert(maxlen >= 0);
        rs[pos] = '\0';
    } else {
        int maxlen = 0;
        PACKET_BUFFER_COMPUTE_SIZE(maxlen, obj->buf->len, obj->buf->header.field.type);
        if(maxlen > 0) {
            size_t l;
            rs = g_malloc(maxlen);
            l = packet_buffer_to_json(obj->buf, rs, maxlen, TRUE, FALSE);
            if(0 == l) {
                g_free(rs);
                rs = NULL;
            }
        }
    }

    return(rs);
} /* }}} */
#endif

#define JSON_FOR_INFLUXDB
#ifdef JSON_FOR_INFLUXDB
char *object_buffer_to_json(object_buffer_t *obj) { /* {{{ */
    char *rs = NULL;
    gboolean is_hash_key = TRUE;
    GList *gl = obj->list;
    GList *gl_hostname = NULL;
    GList *gl_cdtime = NULL;
    GList *gl_pid = NULL;
    GList *gl_name = NULL;
    size_t maxlen = 0;
    size_t pos = 0;
    size_t pos_name = 0;
    size_t l;
    size_t i;

    if(! obj->list) {
        L (LOGLEVEL_INFO, "Received something else than a hash. Will be ignored.");
        return(NULL);
    }
    if(((packet_buffer_t*)obj->list->data)->header.field.type != PACKET_TYPE__HASH) {
        L (LOGLEVEL_INFO, "Received something else than a hash. Will be ignored.");
        return(NULL);
    }
/* CollectPS input format :
 * {
 *   key1:value1,
 *   key2:value2,
 *   key3:value3,
 *   ... :...
 *   hostname:<hostname>,
 *   cdtime:<cdtime>
 * }
*/
/* Influxdb output format :
 *    [
 *      {
 *        "name": "<hostname>/pid/cmd", (without special chars)
 *        "columns": ["time", "key1", "key2", "key3", ...],
 *        "points": [
 *             [cdtime, "value1", "value2", "value3", ...]
 *        ]
 *      }
 *    ]
 *
 */

    /* Compute the size */
#define INFLUXDB_STRING__START "[{\"name\":"
#define INFLUXDB_STRING__COLUMNS ",\"columns\":[\"time\","
#define INFLUXDB_STRING__POINTS "],\"points\":[["
#define INFLUXDB_STRING__END "]]}]"
    maxlen = sizeof(INFLUXDB_STRING__START INFLUXDB_STRING__COLUMNS INFLUXDB_STRING__POINTS INFLUXDB_STRING__END);
    is_hash_key = TRUE;
    for(gl = obj->list->next; gl; gl = gl->next) {
        int l;
        packet_buffer_t *buf = gl->data;
        if(buf->len > 0) {
            buf->buf[buf->len -1] = '\0';
            if(buf->header.field.type == PACKET_TYPE__STRING) {
                if(is_hash_key && (0 == strcmp("hostname", (char *)buf->buf))) {
                    gl_hostname = gl->next;
                } else if(is_hash_key && (0 == strcmp("cdtime", (char *)buf->buf))) {
                    gl_cdtime = gl->next;
                } else if(is_hash_key && (0 == strcmp("PID", (char *)buf->buf))) {
                    gl_pid = gl->next;
                } else if(is_hash_key && (0 == strcmp("pid", (char *)buf->buf))) {
                    gl_pid = gl->next;
                } else if(is_hash_key && (0 == strcmp("Name", (char *)buf->buf))) {
                    gl_name = gl->next;
                }
            }
        }
        /* Note : this will add extra size with cdtime */
        PACKET_BUFFER_COMPUTE_SIZE(l, buf->len, buf->header.field.type);
        if(l > 0) maxlen += (l+sizeof(",")-2);
        is_hash_key = is_hash_key?FALSE:TRUE;
    }
    if(NULL == gl_hostname) goto object_buffer_to_json__failed;
    if(NULL == gl_cdtime) goto object_buffer_to_json__failed;
    if(NULL == gl_pid) goto object_buffer_to_json__failed;
    if(NULL == gl_name) goto object_buffer_to_json__failed;

    maxlen += strlen_escape_json((char *)((packet_buffer_t *)(gl_pid->data))->buf);
    maxlen += strlen_escape_json((char *)((packet_buffer_t *)(gl_name->data))->buf);
    maxlen += strlen_escape_json((char *)((packet_buffer_t *)(gl_hostname->data))->buf);
    maxlen += sizeof("\\\\");

    rs = g_malloc(maxlen);
    pos = 0;

    /* Start the result string */
    memcpy(rs+pos,INFLUXDB_STRING__START, sizeof(INFLUXDB_STRING__START));
    is_hash_key = TRUE;
    pos += sizeof(INFLUXDB_STRING__START) - 1;
    maxlen -= sizeof(INFLUXDB_STRING__START) - 1;

    /* Copy the hostname/pid/cmd */
    l = packet_buffer_to_json(gl_hostname->data, rs+pos, maxlen, TRUE, FALSE);
    if((-1 == l) || (l > maxlen)) goto object_buffer_to_json__failed;
    pos_name = pos+1;
    pos += l-1;
    maxlen -= l-1;
    rs[pos-1] = '/'; /* replace the last quote with a slash */

    l = packet_buffer_to_json(gl_pid->data, rs+pos, maxlen, FALSE, TRUE);
    if((-1 == l) || (l > maxlen)) goto object_buffer_to_json__failed;
    pos += l-1;
    maxlen -= l-1;
    rs[pos-1] = '/'; /* replace the last coma with a slash */

    l = packet_buffer_to_json(gl_name->data, rs+pos, maxlen, FALSE, TRUE);
    if((-1 == l) || (l > maxlen)) goto object_buffer_to_json__failed;
    pos += l-1;
    maxlen -= l-1;
    rs[pos-1] = '"'; /* replace the last coma with a quote to end the string */
    for(i=pos_name; i<pos-1; i++) {
        if((rs[i] >= 'a')&& (rs[i] <= 'z')) continue;
        if((rs[i] >= 'A')&& (rs[i] <= 'Z')) continue;
        if((rs[i] >= '0')&& (rs[i] <= '9')) continue;
        if(rs[i] == '_') continue;
        if(rs[i] == '-') continue;
        if(rs[i] == '.') continue;
        rs[i] = '_';
    }

    /* Copy the "columns" */
    memcpy(rs+pos,INFLUXDB_STRING__COLUMNS, sizeof(INFLUXDB_STRING__COLUMNS));
    pos += sizeof(INFLUXDB_STRING__COLUMNS) - 1;
    maxlen -= sizeof(INFLUXDB_STRING__COLUMNS) - 1;

    /* Copy the keys */
    is_hash_key = FALSE;
    for(gl = obj->list->next; gl; gl = gl->next) {
        is_hash_key = is_hash_key?FALSE:TRUE;
        if(! is_hash_key) continue;
        if(gl->data == gl_cdtime->prev->data) continue;

        l = packet_buffer_to_json(gl->data, rs+pos, maxlen, TRUE, TRUE);
        if((-1 == l) || (l > maxlen)) goto object_buffer_to_json__failed;

        pos += l-1;
        maxlen -= l-1;
        assert(maxlen >= 0);
    }
    /* being here, we have copied an extra coma. Let's get back one char to remove it */
    pos--;
    maxlen++;

    /* Copy the "points" */
    memcpy(rs+pos,INFLUXDB_STRING__POINTS, sizeof(INFLUXDB_STRING__POINTS));
    pos += sizeof(INFLUXDB_STRING__POINTS) - 1;
    maxlen -= sizeof(INFLUXDB_STRING__POINTS) - 1;

    /* Copy the values : 1st is the cdtime */
    l = packet_buffer_to_json(gl_cdtime->data, rs+pos, maxlen, FALSE, TRUE);
    if((-1 == l) || (l > maxlen)) goto object_buffer_to_json__failed;
    pos += l-1;
    maxlen -= l-1;

    /* Copy the values : go on with the other values */
    is_hash_key = FALSE;
    for(gl = obj->list->next; gl; gl = gl->next) {
        gboolean copy_with_quotes = TRUE;
        packet_buffer_t *bufkey = gl->prev->data;

        is_hash_key = is_hash_key?FALSE:TRUE;

        if(is_hash_key) continue;
        if(gl->data == gl_cdtime->data) continue;

        if((0 == strcmp("VirtualSize", (char *)bufkey->buf))) {
            copy_with_quotes = FALSE;
        } else if((0 == strcmp("ThreadCount", (char *)bufkey->buf))) {
            copy_with_quotes = FALSE;
        } else if((0 == strcmp("stime", (char *)bufkey->buf))) {
            copy_with_quotes = FALSE;
        } else if((0 == strcmp("utime", (char *)bufkey->buf))) {
            copy_with_quotes = FALSE;
        }
        l = packet_buffer_to_json(gl->data, rs+pos, maxlen, copy_with_quotes, TRUE);
        if((-1 == l) || (l > maxlen)) goto object_buffer_to_json__failed;

        pos += l-1;
        maxlen -= l-1;
        assert(maxlen >= 0);
    }
    /* being here, we have copied an extra coma. Let's get back one char to remove it */
    pos--;
    maxlen++;

    /* Copy the end of the string */
    memcpy(rs+pos,INFLUXDB_STRING__END, sizeof(INFLUXDB_STRING__END));
    pos += sizeof(INFLUXDB_STRING__END) - 1;
    maxlen -= sizeof(INFLUXDB_STRING__END) - 1;

    assert(maxlen >= 0);
    rs[pos] = '\0';

    return(rs);
object_buffer_to_json__failed:
    if(rs) {
        g_free(rs);
        rs = NULL;
    }
    return(NULL);
} /* }}} */
#endif

client_sink_t *client_sink_new(size_t capacity) { /* {{{ */
    client_sink_t *msgs;
    if(NULL == (msgs = g_slice_alloc(sizeof(*msgs)))) return(NULL);
    msgs->buf = NULL;
    msgs->queue = g_async_queue_new_full((GDestroyNotify)packet_buffer_free);
    msgs->capacity = capacity;
    return(msgs);
} /* }}} */

void client_sink_free(client_sink_t *msgs) { /* {{{ */
    packet_buffer_free(msgs->buf);
    g_async_queue_unref(msgs->queue);
    g_slice_free1(sizeof(*msgs), msgs);
} /* }}} */

gboolean client_sink_dequeue(client_sink_t *msgs) { /* {{{ */
    /* Dequeue */
    if(msgs->buf) {
        if(!msgs->buf->ready) return(TRUE);
        packet_buffer_free(msgs->buf);
    }
    msgs->buf = g_async_queue_try_pop(msgs->queue);
    /* Return true if some data is available */
    if(msgs->buf) return(TRUE);
    if(g_async_queue_length(msgs->queue) > 0) return(TRUE);
    return(FALSE);
} /* }}} */

/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

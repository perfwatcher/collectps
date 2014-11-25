#ifndef UTIL_BUFFER_H
#define UTIL_BUFFER_H

#include <stdint.h>
#include <glib.h>

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

typedef enum {
    PACKET_STATE__HEADER,
    PACKET_STATE__ANY,
    PACKET_STATE__STRING,
    PACKET_STATE__NUMERIC,
    PACKET_STATE__LIST,
    PACKET_STATE__HASH,
} packet_state_e;

#define PACKET_HEADER_SIZE 6
typedef struct packet_buffer_t {
    union header {
        /* Warning : the alignment prevents the members of the struct to be aligned with the data.
         * So we need to do some rewriting at runtime.
         */
        struct field { 
            uint16_t type;
            uint32_t length;
        } field;
        uint8_t b[PACKET_HEADER_SIZE+2];
    } header;
    uint8_t *buf;             /* Buffer */
    uint64_t numeric;         /* Value if it is a numeric, array or hash */
    size_t write_offset;      /* source : fill at that offset; sink : remove at that offset */
    size_t len;               /* size of the buffer */
    gboolean ready;           /* source : true when filled; sink : true when empty */
    gboolean garbage;         /* true if garbage detected */
    packet_state_e state;
} packet_buffer_t;

typedef struct {
    GList *list;
    packet_buffer_t *buf;
    packet_type_e type;
    size_t nb_elements;
    size_t nb_elements_remaining;
    gboolean ready;
} object_buffer_t;

typedef struct client_sink_t {
    GAsyncQueue *queue;
    packet_buffer_t *buf;
    size_t capacity;
} client_sink_t;


packet_buffer_t *packet_buffer_new();
void packet_buffer_free(packet_buffer_t *buf);
ssize_t packet_buffer_read(int fd, packet_buffer_t *buf);
ssize_t packet_buffer_write(int fd, packet_buffer_t *buf);
char *packet_buffer_to_string(packet_buffer_t *buf);

object_buffer_t *object_buffer_new();
void object_buffer_free(object_buffer_t *buf);
ssize_t object_buffer_read(int fd, object_buffer_t *obj);
char *object_buffer_to_string(object_buffer_t *obj);
char *object_buffer_to_json(object_buffer_t *obj);

client_sink_t *client_sink_new(size_t capacity);
void client_sink_free(client_sink_t *msgs);
gboolean client_sink_dequeue(client_sink_t *msgs);

#endif
/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#include <glib.h>
#include <ev.h>

#include <collectps.h>

extern cps_config_t global_config;
extern GThreadPool *workers_thread_pool;

/* NOTE : a lot of code here is borrowed from https://github.com/dhess/echoev/
 *
 * Unless otherwise specified, the software contained herein is
 * copyright (c) 2011 Drew Hess <dhess-src@bothan.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */ 

void uev_log_with_addr(loglevel_e priority, char *fmt, const struct sockaddr *addr, socklen_t size) {
    char host[NI_MAXHOST];
    int err = getnameinfo((const struct sockaddr *) addr, size, host, sizeof(host), 0, 0, NI_NUMERICHOST);
    if (err) {
        L(LOGLEVEL_WARNING, "uev_log_with_addr getnameinfo failed: %s", gai_strerror(err)); 
    } else {
        L(priority, fmt, host);
    }
}

/*
 * The echo protocol message/line delimiter.
 */
static int set_nonblocking(int fd) {
    int flags;
    if(-1 == (flags = fcntl(fd, F_GETFL, 0))) return -1;
    flags |= O_NONBLOCK;
    if (-1 == (fcntl(fd, F_SETFL, flags))) return -1;
    return 0;
}

/* Network utilities */

typedef struct client_timer {
    ev_timer timer;
    ev_tstamp last_activity;
    struct client_io *eio;
} client_timer;

typedef struct client_io {
    ev_io io;
    object_buffer_t *source;
    client_sink_t *sink;
    client_timer timeout;
    short half_closed;
} client_io;

static void free_client_watcher(client_io *w) {
    object_buffer_free(w->source);
    client_sink_free(w->sink);
    free(w);
}

static void stop_client_watcher(EV_P_ client_io *w) {
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(w->io.fd, (struct sockaddr *) &addr, &addr_len) == -1)
        L (LOGLEVEL_ERROR, "stop_client_watcher getpeername: %m");
    else
        uev_log_with_addr(LOGLEVEL_INFO,
                "closed connection from %s",
                (const struct sockaddr *) &addr,
                addr_len);
    ev_io_stop(EV_A_ &w->io);
    ev_timer_stop(EV_A_ &w->timeout.timer);
    close(w->io.fd);
    free_client_watcher(w);
}

static void reset_client_watcher(EV_P_ ev_io *w, int revents);

static void client_cb(EV_P_ ev_io *w_, int revents) {
    L (LOGLEVEL_DEBUG, "client_cb called");

    client_io *w = (client_io *) w_;
    client_sink_t *sink = w->sink;
    if(NULL == w->source) {
        if(NULL == (w->source = object_buffer_new())) {
            L (LOGLEVEL_CRITICAL, "Could not allocate memory for read buffer");
            w->half_closed = 1;
            reset_client_watcher(EV_A_ &w->io, EV_WRITE);
        }
    }

    if (revents & EV_WRITE) {
        packet_buffer_t *buf = sink->buf;

        L (LOGLEVEL_DEBUG, "client_cb write event");

        if(! buf ) {
            client_sink_dequeue(sink);
            buf = sink->buf;
        }
        while (buf && (!buf->ready)) {
            ssize_t n;
            if(-1 == (n = packet_buffer_write(w->io.fd, buf))) {
                switch(errno) {
                    case EAGAIN: break;
#if EAGAIN != EWOULDBLOCK
                    case EWOULDBLOCK: break;
#endif
                    case EINTR: break;
                    default:
                                L (LOGLEVEL_CRITICAL, "Write on descriptor %d failed: %m", w->io.fd);
                                stop_client_watcher(EV_A_ w);
                                return;
                }
                break;
            }
            w->timeout.last_activity = ev_now(EV_A);
            L (LOGLEVEL_DEBUG, "client_cb %zd bytes written", n);
        }
        if (buf && buf->ready) {
            packet_buffer_free(buf);
            sink->buf = NULL;
            buf = NULL;
        }

        if(!buf) {
            ssize_t msgs_are_available;
            msgs_are_available = client_sink_dequeue(sink);
            if(w->half_closed && (!msgs_are_available)) {
                stop_client_watcher(EV_A_ w);
            }
        }
    }
    if (revents & EV_READ) {
        ssize_t n = 0;
        L (LOGLEVEL_DEBUG, "client_cb read event");
        while((0 == w->half_closed) && (0 < (n = object_buffer_read(w->io.fd, w->source)))) {
            w->timeout.last_activity = ev_now(EV_A);
            L (LOGLEVEL_DEBUG, "client_cb %zd bytes read", n);

            if(w->source->ready) {
                g_thread_pool_push(workers_thread_pool, w->source, NULL);

                if(NULL == (w->source = object_buffer_new())) {
                    L (LOGLEVEL_CRITICAL, "Could not allocate memory for read buffer");
                    w->half_closed = 1;
                    reset_client_watcher(EV_A_ &w->io, EV_WRITE);
                }
            }
        }

        if((w->half_closed) || (0 == n)) {
            /* EOF or failure: drain remaining writes or close connection */
            if(0 == n) {
                L (LOGLEVEL_DEBUG, "client_cb EOF received");
            }
            w->timeout.last_activity = ev_now(EV_A);
            if(w->source) {
                if(w->source && w->source->buf && (((w->source->buf->write_offset > 0) || w->source->buf->state != PACKET_STATE__HEADER))) {
                    L (LOGLEVEL_WARNING, "An object is not fully received while we are closing the connection");
                }
                object_buffer_free(w->source);
                w->source = NULL;
            }
            if (sink->buf || client_sink_dequeue(sink) ) {
                w->half_closed = 1;
                reset_client_watcher(EV_A_ &w->io, EV_WRITE);
            } else {
                stop_client_watcher(EV_A_ w);
            }
            return;
        } else if (-1 == n) {
            switch(errno) {
                case EAGAIN: break;
#if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK: break;
#endif
                case EINTR: break; /* Nothing more to read for now. */
                default:
                            L (LOGLEVEL_CRITICAL, "Read on descriptor %d failed: %m", w->io.fd);
                            if(w->source) {
                                /* should not happen, but in case... */
                                L (LOGLEVEL_WARNING, "An object was being read and will be dropped");

                                object_buffer_free(w->source);
                                w->source = NULL;
                            }
                            stop_client_watcher(EV_A_ w);
            }
            return;
        } else if (-2 >= n) {
            /* errno is not set */
            if(w->source) {
                L (LOGLEVEL_WARNING, "An object was being read and will be dropped (n=%zd)", n);

                object_buffer_free(w->source);
                w->source = NULL;
            }
            stop_client_watcher(EV_A_ w);
            return;
        }
    }
}

/* Default connection timeout, in seconds. */
static const ev_tstamp ECHO_CONNECTION_TIMEOUT = 120.0;

static void timeout_cb(EV_P_ ev_timer *t_, int revents) {
    client_timer *t = (client_timer *) t_;

    ev_tstamp now = ev_now(EV_A);
    ev_tstamp timeout = t->last_activity + ECHO_CONNECTION_TIMEOUT;
    if (timeout < now) {
        /* A real timeout. */
        L (LOGLEVEL_INFO, "Timeout, closing connection");
        stop_client_watcher(EV_A_ t->eio);
    } else {
        /* False alarm, re-arm timeout. */
        t_->repeat = timeout - now;
        ev_timer_again(EV_A_ t_);
    }
}

void reset_client_watcher(EV_P_ ev_io *w, int revents) {
    ev_io_stop(EV_A_ w);
    ev_io_init(w, client_cb, w->fd, revents);
    ev_io_start(EV_A_ w);
}

static client_io * make_client_watcher(EV_P_ int wfd) {
    client_io *watcher;
    ev_io *io;
    ev_timer *timer;

    if (-1 == set_nonblocking(wfd)) return NULL;
    if(NULL == (watcher = malloc(sizeof(*watcher)))) return NULL;

    watcher->sink = client_sink_new(global_config.listener__buffer_max_size);
    watcher->source = NULL;
    watcher->half_closed = 0;

    io = &watcher->io;
    ev_io_init(io, client_cb, wfd, EV_READ);

    timer = &watcher->timeout.timer;
    ev_init(timer, timeout_cb);

    watcher->timeout.last_activity = ev_now(EV_A);
    watcher->timeout.eio = watcher;
    timeout_cb(EV_A_ timer, EV_TIMER);

    return watcher;
}

/*
 * Each "listener" watcher comes with a cooldown timer. When accept()
 * in listen_cb fails due to insufficient resources, it stops the
 * listener watcher and starts a cool-down timer, so that accept()
 * doesn't repeatedly fail. When the timer expires, it re-activates
 * the listener.
 *
 * The cooldown timer is created when the listener is created; it
 * could be created on-demand when accept() fails, but that would
 * exacerbate the resource problems, and would likely fail, anyway.
 */
typedef struct cooldown_timer {
    ev_timer timer;
    ev_io *listener;
} cooldown_timer;

typedef struct listener_io {
    ev_io listener;
    cooldown_timer cooldown;
} listener_io;

static void listen_cb(EV_P_ ev_io *w_, int revents) {
    L(LOGLEVEL_DEBUG, "listen_cb called");
    listener_io *w = (listener_io *) w_;
    /*
     * libev recommends calling accept() in a loop for best
     * performance when using the select or poll back ends. The ev_io
     * watcher's file descriptor had better be non-blocking!
     */
    while (1) {
        struct sockaddr_storage addr;
        int fd;
        client_io *watcher;

        socklen_t addr_len = sizeof(addr);
        if(-1 == (fd = accept(w->listener.fd, (struct sockaddr *) &addr, &addr_len))) {
            /*
             * EWOULDBLOCK and EAGAIN mean no more connections to
             * accept. ECONNABORTED and EPROTO mean the client has
             * aborted the connection, so just ignore it. EINTR means
             * we were interrupted by a signal. (We could re-try the
             * accept in case of EINTR, but we choose not to, in the
             * interest of making forward progress.)
             */
            switch(errno) {
                case EWOULDBLOCK: break;
                case ECONNABORTED: break;
                case EINTR: break;
                case EMFILE:
                case ENFILE:
                case ENOMEM:
                            /*
                             * Running out of resources; log error and stop
                             * accepting connections for a bit.
                             */
                            L(LOGLEVEL_CRITICAL, "accept failed due to insufficient resources: %s", strerror(errno));
                            L(LOGLEVEL_WARNING, "listen_cb: insufficient resources, backing off for a bit");
                            ev_io_stop(EV_A_ &w->listener);
                            ev_timer_start(EV_A_ &w->cooldown.timer);
                            break;
                default:
                            L(LOGLEVEL_CRITICAL, "Can't accept connection: %s", strerror(errno));
            }
            break;
        }
        uev_log_with_addr(LOGLEVEL_INFO, "accepted connection from %s", (const struct sockaddr *) &addr, addr_len);

        if(NULL == (watcher = make_client_watcher(EV_A_ fd))) {
            L (LOGLEVEL_WARNING, "Can't create session with client: %s", strerror(errno));
            close(fd);
        } else {
            ev_io_start(EV_A_ &watcher->io);
        }
    }
}

/*
 * Create, bind, and listen on a non-blocking socket using the given
 * socket address.
 *
 * Return the socket's file descriptor, or -1 if an error occured, in
 * which case the error code is left in errno, and -1 is returned.
 */
static int listen_on(const struct sockaddr *addr, socklen_t addr_len) {
    int errnum;
    int fd;
    const int on = 1;
    if (-1 == (fd = socket(addr->sa_family, SOCK_STREAM, 0))) return -1;
    if (-1 == set_nonblocking(fd)) goto err__listen_on;
    if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) goto err__listen_on;
    if (-1 == bind(fd, addr, addr_len)) goto err__listen_on;
    if (-1 == listen(fd, 8)) goto err__listen_on;
    return fd;
err__listen_on:
    errnum = errno;
    close(fd);
    errno = errnum;
    return -1;
}

/*
 * Default "cool-down" duration (in seconds).
 */
const ev_tstamp COOLDOWN_DURATION = 10.0;

static void cooldown_cb(EV_P_ ev_timer *t_, int revents) {
    cooldown_timer *t = (cooldown_timer *) t_;
    ev_timer_stop(EV_A_ &t->timer);
    ev_io_start(EV_A_ t->listener);
}

static listener_io *make_listener(const struct sockaddr *addr, socklen_t addr_len) {
    listener_io *lio;
    int listen_fd;
    if(-1 == (listen_fd = listen_on(addr, addr_len))) return(NULL);
    if(NULL == (lio = malloc(sizeof(*lio)))) return(NULL);

    ev_io *evio_listener = &lio->listener;
    ev_timer *evio_cooldowntimer = &lio->cooldown.timer;
    ev_io_init(evio_listener, listen_cb, listen_fd, EV_READ);
    ev_timer_init(evio_cooldowntimer, cooldown_cb, COOLDOWN_DURATION, 0);
    lio->cooldown.listener = &lio->listener;
    return lio;
}

int uev_start_listeners(struct ev_loop *loop) {
    ip_list_t *ip;
    char port[20];
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
    for (ip = global_config.listener__ip_list; ip; ip = ip->next) {
        struct addrinfo *res;
        struct listener_io *lio = NULL;
        int err = 0;

        hints.ai_family = ip->family;
        snprintf(port, sizeof(port), "%d", global_config.listener__port);
        if(0 != (err = getaddrinfo(ip->addr, port, &hints, &res))) {
            L (LOGLEVEL_CRITICAL, "getaddrinfo failed : %s", gai_strerror(err));
            close_all_and_exit(EXIT_FAILURE);
        }
        assert(!res->ai_next);

        if(NULL == (lio = make_listener(res->ai_addr, res->ai_addrlen))) {
            L (LOGLEVEL_CRITICAL, "Can't create listening socket : %s", strerror(errno));
            close_all_and_exit(EXIT_FAILURE);
        }

        uev_log_with_addr(LOGLEVEL_INFO, "listening on %s", res->ai_addr, res->ai_addrlen);
        ev_io_start(loop, &lio->listener);
        freeaddrinfo(res);
    }
    return(0);
}


/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

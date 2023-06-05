/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "../../mod_http2/config.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif /* HAVE_FCNTL_H */
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif /* HAVE_NETDB_H */
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <nghttp2/nghttp2.h>

#define MAKE_NV(NAME, VALUE)                                                   \
  {                                                                            \
    (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1,    \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }

#define MAKE_NV_CS(NAME, VALUE)                                                \
  {                                                                            \
    (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, strlen(VALUE),        \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }


static int verbose;
static const char *cmd;

static void log_out(const char *level, const char *where, const char *msg)
{
    fprintf(stderr, "[%s][%s] %s\n", level, where, msg);
}

static void log_err(const char *where, const char *msg)
{
    log_out("ERROR", where, msg);
}

static void log_info(const char *where, const char *msg)
{
    if (verbose)
        log_out("INFO", where, msg);
}

static void log_debug(const char *where, const char *msg)
{
    if (verbose > 1)
        log_out("DEBUG", where, msg);
}

#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
static void log_errf(const char *where, const char *msg, ...)
{
    char buffer[8*1024];
    va_list ap;

    va_start(ap, msg);
    vsnprintf(buffer, sizeof(buffer), msg, ap);
    va_end(ap);
    log_err(where, buffer);
}

#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
static void log_infof(const char *where, const char *msg, ...)
{
    if (verbose) {
        char buffer[8*1024];
        va_list ap;

        va_start(ap, msg);
        vsnprintf(buffer, sizeof(buffer), msg, ap);
        va_end(ap);
        log_info(where, buffer);
    }
}

#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
static void log_debugf(const char *where, const char *msg, ...)
{
    if (verbose > 1) {
        char buffer[8*1024];
        va_list ap;

        va_start(ap, msg);
        vsnprintf(buffer, sizeof(buffer), msg, ap);
        va_end(ap);
        log_debug(where, buffer);
    }
}

static void usage(const char *msg)
{
    if(msg)
        fprintf(stderr, "%s\n", msg);
    fprintf(stderr,
        "usage: [options] ws-uri\n"
        "  make a websocket connection to host, options:\n"
        "  -c host:port connect to host:port\n"
        "  -v         increase verbosity\n"
    );
}

static int parse_host_port(const char **phost, uint16_t *pport,
                           int *pipv6, size_t *pconsumed,
                           const char *s, size_t len, uint16_t def_port)
{
    size_t i, offset;
    char *host = NULL;
    int port = 0;
    int rv = 1, ipv6 = 0;

    if (!len)
        goto leave;
    offset = 0;
    if (s[offset] == '[') {
        ipv6 = 1;
        for (i = offset++; i < len; ++i) {
            if (s[i] == ']')
              break;
        }
        if (i >= len || i == offset)
            goto leave;
        host = strndup(s + offset, i - offset);
        offset = i + 1;
    }
    else {
        for (i = offset; i < len; ++i) {
            if (strchr(":/?#", s[i]))
              break;
        }
        if (i == offset) {
            log_debugf("parse_uri", "empty host name in '%.*s", (int)len, s);
            goto leave;
        }
        host = strndup(s + offset, i - offset);
        offset = i;
    }
    if (offset < len && s[offset] == ':') {
        port = 0;
        ++offset;
        for (i = offset; i < len; ++i) {
            if (strchr("/?#", s[i]))
                break;
            if (s[i] < '0' || s[i] > '9') {
                log_debugf("parse_uri", "invalid port char '%c'", s[i]);
                goto leave;
            }
            port *= 10;
            port += s[i] - '0';
            if (port > 65535) {
                log_debugf("parse_uri", "invalid port number '%d'", port);
                goto leave;
            }
        }
        offset = i;
    }
    rv = 0;

leave:
    *phost = rv? NULL : host;
    *pport = rv? 0 : (port? (uint16_t)port : def_port);
    if (pipv6)
      *pipv6 = ipv6;
    if (pconsumed)
      *pconsumed = offset;
    return rv;
}

struct uri {
  const char *scheme;
  const char *host;
  const char *authority;
  const char *path;
  uint16_t port;
  int ipv6;
};

static int parse_uri(struct uri *uri, const char *s, size_t len)
{
    char tmp[8192];
    size_t n, offset = 0;
    uint16_t def_port = 0;
    int rv = 1;

    /* NOT A REAL URI PARSER */
    memset(uri, 0, sizeof(*uri));
    if (len > 5 && !memcmp("ws://", s, 5)) {
        uri->scheme = "ws";
        def_port = 80;
        offset = 5;
    }
    else if (len > 6 && !memcmp("wss://", s, 6)) {
        uri->scheme = "wss";
        def_port = 443;
        offset = 6;
    }
    else {
        /* not a scheme we process */
        goto leave;
    }

    if (parse_host_port(&uri->host, &uri->port, &uri->ipv6, &n, s + offset,
                        len - offset, def_port))
        goto leave;
    offset += n;

    if (uri->port == def_port)
      uri->authority = uri->host;
    else if (uri->ipv6) {
      snprintf(tmp, sizeof(tmp), "[%s]:%u", uri->host, uri->port);
      uri->authority = strdup(tmp);
    }
    else {
      snprintf(tmp, sizeof(tmp), "%s:%u", uri->host, uri->port);
      uri->authority = strdup(tmp);
    }

    if (offset < len) {
        uri->path = strndup(s + offset, len - offset);
    }
    rv = 0;

leave:
    return rv;
}

static int sock_nonblock_nodelay(int fd) {
  int flags, rv;
  int val = 1;

  while ((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR)
      ;
  if (flags == -1) {
      log_errf("sock_nonblock_nodelay", "fcntl get error %d (%s)",
               errno, strerror(errno));
      return -1;
  }
  while ((rv = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1 && errno == EINTR)
    ;
  if (rv == -1) {
      log_errf("sock_nonblock_nodelay", "fcntl set error %d (%s)",
               errno, strerror(errno));
      return -1;
  }
  rv = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, (socklen_t)sizeof(val));
  if (rv == -1) {
      log_errf("sock_nonblock_nodelay", "set nodelay error %d (%s)",
               errno, strerror(errno));
      return -1;
  }
  return 0;
}

static int open_connection(const char *host, uint16_t port)
{
    char service[NI_MAXSERV];
    struct addrinfo hints;
    struct addrinfo *res = NULL, *rp;
    int rv, fd = -1;

    memset(&hints, 0, sizeof(hints));
    snprintf(service, sizeof(service), "%u", port);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    rv = getaddrinfo(host, service, &hints, &res);
    if (rv) {
      log_err("getaddrinfo", gai_strerror(rv));
      goto leave;
    }

    for (rp = res; rp; rp = rp->ai_next) {
      fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (fd == -1) {
        continue;
      }
      while ((rv = connect(fd, rp->ai_addr, rp->ai_addrlen)) == -1 &&
             errno == EINTR)
        ;
      if (!rv) /* connected */
          break;
      close(fd);
      fd = -1;
    }

leave:
    if (res)
      freeaddrinfo(res);
    return fd;
}

struct h2_stream;

#define IO_WANT_NONE   0
#define IO_WANT_READ   1
#define IO_WANT_WRITE  2

struct h2_session {
    const char *server_name;
    const char *connect_host;
    uint16_t connect_port;
    int fd;
    nghttp2_session *ngh2;
    struct h2_stream *streams;
    int aborted;
    int want_io;
    int is_ssl;
};

typedef void h2_stream_closed_cb(struct h2_stream *stream);
typedef void h2_stream_recv_data(struct h2_stream *stream,
                                 const uint8_t *data, size_t len);

struct h2_stream {
    struct h2_stream *next;
    struct uri *uri;
    int32_t id;
    uint32_t error_code;
    int closed;
    h2_stream_closed_cb *on_close;
    h2_stream_recv_data *on_recv_data;
};

static void h2_session_stream_add(struct h2_session *session,
                                  struct h2_stream *stream)
{
    struct h2_stream *s;
    for (s = session->streams; s; s = s->next) {
        if (s == stream)  /* already there? */
            return;
    }
    stream->next = session->streams;
    session->streams = stream;
}

static void h2_session_stream_remove(struct h2_session *session,
                                     struct h2_stream *stream)
{
    struct h2_stream *s, **pnext;
    pnext = &session->streams;
    s = session->streams;
    while (s) {
        if (s == stream) {
            *pnext = s->next;
            s->next = NULL;
            break;
        }
        pnext = &s->next;
        s = s->next;
    }
}

static struct h2_stream *h2_session_stream_get(struct h2_session *session,
                                               int32_t id)
{
    struct h2_stream *s;
    for (s = session->streams; s; s = s->next) {
        if (s->id == id)
            return s;
    }
    return NULL;
}

static ssize_t h2_session_send(nghttp2_session *ngh2, const uint8_t *data,
                               size_t length, int flags, void *user_data)
{
    struct h2_session *session = user_data;
    ssize_t nwritten;
    (void)ngh2;
    (void)flags;

    session->want_io = IO_WANT_NONE;
    nwritten = send(session->fd, data, length, 0);
    if (nwritten < 0) {
      int err = errno;
      if ((EWOULDBLOCK == err) || (EAGAIN == err) ||
          (EINTR == err) || (EINPROGRESS == err)) {
          return NGHTTP2_ERR_WOULDBLOCK;
      }
      log_errf("h2_session_send", "error sending %ld bytes: %d (%s)",
               (long)length, err, strerror(err));
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    return nwritten;
}

static ssize_t h2_session_recv(nghttp2_session *ngh2, uint8_t *buf,
                               size_t length, int flags, void *user_data)
{
    struct h2_session *session = user_data;
    ssize_t nread;
    (void)ngh2;
    (void)flags;

    session->want_io = IO_WANT_NONE;
    nread = recv(session->fd, buf, length, 0);
    if (nread < 0) {
      int err = errno;
      if ((EWOULDBLOCK == err) || (EAGAIN == err) || (EINTR == err)) {
          return NGHTTP2_ERR_WOULDBLOCK;
      }
      log_errf("h2_session_recv", "error reading %ld bytes: %d (%s)",
               (long)length, err, strerror(err));
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    return nread;
}

static int h2_session_on_frame_send(nghttp2_session *session,
                                    const nghttp2_frame *frame,
                                    void *user_data)
{
    size_t i;
    (void)user_data;

    switch (frame->hd.type) {
    case NGHTTP2_HEADERS:
      if (nghttp2_session_get_stream_user_data(session, frame->hd.stream_id)) {
        const nghttp2_nv *nva = frame->headers.nva;
        log_infof("frame send", "FRAME[HEADERS, stream=%d",
                  frame->hd.stream_id);
        for (i = 0; i < frame->headers.nvlen; ++i) {
            log_infof("frame send", "  %.*s: %.*s",
                      (int)nva[i].namelen, nva[i].name,
                      (int)nva[i].valuelen, nva[i].value);
        }
        log_infof("frame send", "]");
      }
      break;
    case NGHTTP2_RST_STREAM:
        log_infof("frame send", "FRAME[RST, stream=%d]",
                  frame->hd.stream_id);
        break;
    case NGHTTP2_GOAWAY:
        log_infof("frame send", "FRAME[GOAWAY]");
        break;
    }
    return 0;
}

static int h2_session_on_frame_recv(nghttp2_session *ngh2,
                                    const nghttp2_frame *frame,
                                    void *user_data)
{
    (void)user_data;

    switch (frame->hd.type) {
    case NGHTTP2_HEADERS:
        if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
          log_infof("frame recv", "FRAME[HEADERS, stream=%d]",
                    frame->hd.stream_id);
        }
        break;
    case NGHTTP2_DATA:
        log_infof("frame recv", "FRAME[DATA, stream=%d, len=%lu, eof=%d]",
                  frame->hd.stream_id, frame->hd.length,
                  (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) != 0);
        break;
    case NGHTTP2_RST_STREAM:
        log_infof("frame recv", "FRAME[RST, stream=%d]",
                  frame->hd.stream_id);
        break;
    case NGHTTP2_GOAWAY:
        log_infof("frame recv", "FRAME[GOAWAY]");
        break;
    }
    return 0;
}

static int h2_session_on_header(nghttp2_session *ngh2,
                                const nghttp2_frame *frame,
                                const uint8_t *name, size_t namelen,
                                const uint8_t *value, size_t valuelen,
                                uint8_t flags, void *user_data)
{
    (void)flags;
    (void)user_data;
    log_infof("frame recv", "stream=%d, HEADER   %.*s: %.*s",
              frame->hd.stream_id, (int)namelen, name,
              (int)valuelen, value);
    return 0;
}

static int h2_session_on_stream_close(nghttp2_session *ngh2, int32_t stream_id,
                                      uint32_t error_code, void *user_data)
{
    struct h2_session *session = user_data;
    struct h2_stream *stream;

    stream = h2_session_stream_get(session, stream_id);
    if (stream) {
        /* closed known stream */
        stream->error_code = error_code;
        stream->closed = 1;
        if (error_code) {
            log_errf("stream close", "stream %d closed with error %d",
                     stream_id, error_code);
        }

        h2_session_stream_remove(session, stream);
        if (stream->on_close)
            stream->on_close(stream);
        /* last one? */
        if (!session->streams) {
            int rv;
            rv = nghttp2_session_terminate_session(ngh2, NGHTTP2_NO_ERROR);
            if (rv) {
                log_errf("terminate session", "error %d (%s)",
                         rv, nghttp2_strerror(rv));
                session->aborted = 1;
            }
        }
    }
    return 0;
}

static int h2_session_on_data_chunk_recv(nghttp2_session *ngh2, uint8_t flags,
                                         int32_t stream_id, const uint8_t *data,
                                         size_t len, void *user_data) {
    struct h2_session *session = user_data;
    struct h2_stream *stream;

    stream = h2_session_stream_get(session, stream_id);
    if (stream && stream->on_recv_data) {
        stream->on_recv_data(stream, data, len);
    }
    return 0;
}

static int h2_session_open(struct h2_session *session, const char *server_name,
                           const char *host, uint16_t port)
{
    nghttp2_session_callbacks *cbs = NULL;
    int rv = -1;

    memset(session, 0, sizeof(*session));
    session->server_name = server_name;
    session->connect_host = host;
    session->connect_port = port;
    /* establish socket */
    session->fd = open_connection(session->connect_host, session->connect_port);
    if (session->fd < 0) {
      log_errf(cmd, "could not connect to %s:%u",
               session->connect_host, session->connect_port);
      goto leave;
    }
    if (sock_nonblock_nodelay(session->fd))
        goto leave;
    session->want_io = IO_WANT_NONE;

    log_infof(cmd, "connected to %s via %s:%u", session->server_name,
              session->connect_host, session->connect_port);

    rv = nghttp2_session_callbacks_new(&cbs);
    if (rv) {
        log_errf("setup callbacks", "error_code=%d, msg=%s\n", rv,
                 nghttp2_strerror(rv));
        rv = -1;
        goto leave;
    }
    /* setup session callbacks */
    nghttp2_session_callbacks_set_send_callback(cbs, h2_session_send);
    nghttp2_session_callbacks_set_recv_callback(cbs, h2_session_recv);
    nghttp2_session_callbacks_set_on_frame_send_callback(
        cbs, h2_session_on_frame_send);
    nghttp2_session_callbacks_set_on_frame_recv_callback(
        cbs, h2_session_on_frame_recv);
    nghttp2_session_callbacks_set_on_header_callback(
        cbs, h2_session_on_header);
    nghttp2_session_callbacks_set_on_stream_close_callback(
        cbs, h2_session_on_stream_close);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
        cbs, h2_session_on_data_chunk_recv);
    /* create the ngh2 session */
    rv = nghttp2_session_client_new(&session->ngh2, cbs, session);
    if (rv) {
        log_errf("client new", "error_code=%d, msg=%s\n", rv,
                 nghttp2_strerror(rv));
        rv = -1;
        goto leave;
    }
    /* submit initial settings */
    rv = nghttp2_submit_settings(session->ngh2, NGHTTP2_FLAG_NONE, NULL, 0);
    if (rv) {
        log_errf("submit settings", "error_code=%d, msg=%s\n", rv,
                 nghttp2_strerror(rv));
        rv = -1;
        goto leave;
    }
    rv = 0;

leave:
    if (cbs)
        nghttp2_session_callbacks_del(cbs);
    return rv;
}

static int h2_session_io(struct h2_session *session) {
    int rv;
    rv = nghttp2_session_recv(session->ngh2);
    if (rv) {
        log_errf("session recv", "error_code=%d, msg=%s\n", rv,
                 nghttp2_strerror(rv));
        return 1;
    }
    rv = nghttp2_session_send(session->ngh2);
    if (rv) {
        log_errf("session send", "error_code=%d, msg=%s\n", rv,
                 nghttp2_strerror(rv));
    }
    return 0;
}

static void h2_session_set_poll(struct h2_session *session,
                                struct pollfd *pollfd)
{
    pollfd->events = 0;
    if (nghttp2_session_want_read(session->ngh2) ||
        session->want_io == IO_WANT_READ) {
        pollfd->events |= POLLIN;
    }
    if (nghttp2_session_want_write(session->ngh2) ||
        session->want_io == IO_WANT_WRITE) {
        pollfd->events |= POLLOUT;
    }
}

static void h2_session_run(struct h2_session *session)
{
  struct pollfd pollfds[1];
  nfds_t npollfds = 1;

  pollfds[0].fd = session->fd;
  h2_session_set_poll(session, pollfds);
  while (nghttp2_session_want_read(session->ngh2) ||
         nghttp2_session_want_write(session->ngh2)) {
    int nfds = poll(pollfds, npollfds, -1);
    if (nfds == -1) {
        log_errf("session run", "poll error %d (%s)", errno, strerror(errno));
        break;
    }
    if (pollfds[0].revents & (POLLIN | POLLOUT)) {
        h2_session_io(session);
    }
    if ((pollfds[0].revents & POLLHUP) || (pollfds[0].revents & POLLERR)) {
        log_errf("session run", "connection error");
        break;
    }
    h2_session_set_poll(session, pollfds);
  }
}

static void h2_session_close(struct h2_session *session)
{
    log_infof(cmd, "closed session to %s:%u",
              session->connect_host, session->connect_port);
}

/* websocket stream */

struct ws_stream {
  struct h2_stream s;
  const char *protocol;
};

static void ws_stream_on_close(struct h2_stream *stream)
{
    log_infof("ws stream", "stream %d closed", stream->id);
}

static void ws_stream_on_recv_data(struct h2_stream *stream,
                            const uint8_t *data, size_t len)
{
    log_infof("ws stream", "stream %d recv %lu data bytes",
              stream->id, (unsigned long)len);
    fwrite(data, len, 1, stderr);
}

static int ws_stream_create(struct ws_stream **pstream, struct uri *uri,
                            const char *protocol)
{
    struct ws_stream *stream;

    stream = calloc(1, sizeof(*stream));
    if (!stream) {
        log_errf("ws stream create", "out of memory");
        *pstream = NULL;
        return -1;
    }
    stream->s.uri = uri;
    stream->s.id = -1;
    stream->s.on_close = ws_stream_on_close;
    stream->s.on_recv_data = ws_stream_on_recv_data;
    stream->protocol = protocol;
    *pstream = stream;
    return 0;
}

static int ws_stream_submit(struct ws_stream *stream,
                            struct h2_session *session)
{
    const char *scheme = session->is_ssl? "https" : "http";
    const nghttp2_nv nva[] = {
        MAKE_NV(":method", "CONNECT"),
        MAKE_NV_CS(":path", stream->s.uri->path),
        MAKE_NV_CS(":scheme", scheme),
        MAKE_NV_CS(":authority", stream->s.uri->authority),
        MAKE_NV_CS(":protocol", stream->protocol),
        MAKE_NV("accept", "*/*"),
        MAKE_NV("user-agent", "mod_h2/h2ws-test")
    };

    stream->s.id = nghttp2_submit_request(session->ngh2, NULL, nva,
                                          sizeof(nva) / sizeof(nva[0]),
                                          NULL, NULL);
    if (stream->s.id < 0) {
        log_errf("ws stream submit", "nghttp2_submit_request: error %d",
                 stream->s.id);
        return -1;
    }

    h2_session_stream_add(session, &stream->s);
    log_infof("ws stream submit", "stream %d opened for %s%s",
              stream->s.id, stream->s.uri->authority, stream->s.uri->path);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *host = NULL;
    uint16_t port = 80;
    struct uri uri;
    struct h2_session session;
    struct ws_stream *stream;
    char ch;

    cmd = argv[0];
    while((ch = getopt(argc, argv, "c:vh")) != -1) {
        switch(ch) {
        case 'c':
            if (parse_host_port(&host, &port, NULL, NULL,
                                optarg, strlen(optarg), 80)) {
                log_errf(cmd, "could not parse connect '%s'", optarg);
                return 1;
            }
            break;
        case 'h':
            usage(NULL);
            return 2;
            break;
        case 'v':
            ++verbose;
            break;
        default:
           usage("invalid option");
           return 1;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc < 1) {
        usage("need URL");
        return 1;
    }
    if (parse_uri(&uri, argv[0], strlen(argv[0]))) {
        log_errf(cmd, "could not parse uri '%s'", argv[0]);
        return 1;
    }
    log_debugf(cmd, "normalized uri: %s://%s:%u%s", uri.scheme, uri.host,
               uri.port, uri.path? uri.path : "");

    if (!host) {
        host = uri.host;
        port = uri.port;
    }

    if (h2_session_open(&session, uri.host, host, port))
        return 1;

    if (ws_stream_create(&stream, &uri, "websocket"))
        return 1;
    if (ws_stream_submit(stream, &session))
        return 1;

    h2_session_run(&session);
    h2_session_close(&session);
    return 0;
}
/*
 * Copyright (c) 2020 Dave Voutila <voutilad@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DWS_H
#define	DWS_H

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <netinet/in.h>
#include <netdb.h>
#endif

#include <sys/types.h>

/*
 * We only do Binary frames. Why? You might ask...
 * Well Text frames require utf-8 support, which is hella gross.
 */
enum ws_opcode {
	TEXT	= 0x1,
	BINARY	= 0x2,
	CLOSE	= 0x8,
	PING	= 0x9,
	PONG	= 0xa,
};

/*
 * A websocket contains all the state needed for both establishing the
 * connection as well as re-connecting if required. It's possibly a
 * server might close the connection (or it may drop) and require the
 * client to reconnect. This should be easy, for some definition of easy.
 */
struct websocket {
	int s;
	struct addrinfo      addr; // for reconnects
	struct tls          *ctx;
	struct tls_config   *cfg;

	/* Retain connection details so we can avoid addrinfo nonsense. */
	uint16_t             port;
	char                *host;

	// TODO: add basic auth details?
};

/*
 * Possible non-error responses from dumb_recv() based on the state of the
 * socket or the next websocket control message (e.g. PING).
 */
#define DWS_WANT_POLL	-2
#define DWS_WANT_PONG	-3
#define DWS_SHUTDOWN	-4

/*
 * Simplistic error code approach using define's.
 */
#define DWS_OK			0
#define DWS_ERR_CONN_CREATE	-1
#define DWS_ERR_CONN_RESOLVE	-2
#define DWS_ERR_CONN_CONNECT	-3
#define DWS_ERR_MALLOC		-4
#define DWS_ERR_READ		-5
#define DWS_ERR_WRITE		-6
#define DWS_ERR_INVALID		-7
#define DWS_ERR_HANDSHAKE_BUF	-8
#define DWS_ERR_HANDSHAKE_RES	-9
#define DWS_ERR_TOO_LARGE	-10

int dumb_connect(struct websocket *ws, const char*, uint16_t);
int dumb_connect_tls(struct websocket *ws, const char*, uint16_t, int);
int dumb_handshake(struct websocket *s, const char*, const char*);

ssize_t dumb_send(struct websocket *ws, const void*, size_t);
ssize_t dumb_recv(struct websocket *ws, void*, size_t);
int dumb_ping(struct websocket *ws);
int dumb_close(struct websocket *ws);

#endif /* DWS_H */

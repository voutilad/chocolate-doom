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

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _CRT_RAND_S
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <limits.h>
#include <errno.h>

#include <tls.h>

#include "dws.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

// It's ludicrous to think we'd have a server handshake response larger
#define HANDSHAKE_BUF_SIZE 1024

// The largest frame header in bytes, assuming the largest payload
#define FRAME_MAX_HEADER_SIZE 14

static const char server_handshake[] = "HTTP/1.1 101 Switching Protocols";

static const char HANDSHAKE_TEMPLATE[] =
    "GET %s HTTP/1.1\r\n"
    "Host: %s:%d\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: %s\r\n"
    "Sec-WebSocket-Protocol: %s\r\n"
    "Sec-WebSocket-Version: 13\r\n\r\n";

static int rng_initialized = 0;

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static ssize_t ws_read(struct websocket *, void *, size_t);
static ssize_t ws_read_all(struct websocket *, void *, size_t);
static ssize_t ws_read_txt(struct websocket *, void *, size_t);
static void ws_shutdown(struct websocket *);


static void __attribute__((noreturn))
crap(int code, const char *fmt, ...)
{
	char buf[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	fprintf(stderr, "oh crap! %s\n", buf);
	exit(code);
}

static uint32_t
portable_random(void)
{
#if _WIN32 || _WIN64
	errno_t err;
	uint32_t r = 0;
	err = rand_s(&r);
	if (err != 0)
		crap(err, "%s: rand_s failed", __func__);
	return r;
#elif (__OpenBSD__ || __FreeBSD__ || __NetBSD__ || __APPLE__) \
	|| (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 36)
	return arc4random();
#else
	static long r = 0;
	uint32_t ret = 0;

	if (r == 0) {
		r = random();
		ret = (uint32_t)(r & 0xFFFFFFFF);
	} else {
		r = r >> 32;
		ret = (uint32_t)r;
		r = 0;
	}
	return ret;
#endif
}
static void
init_rng(void)
{
#ifndef _WIN32
	// XXX: why doesn't every platform just have arc4random(3)?!
	int fd;
	ssize_t len;
	char state[256];

	fd = open("/dev/urandom", O_RDONLY);
	len = read(fd, state, sizeof(state));
	if (len < (ssize_t) sizeof(state))
		crap(1, "%s: failed to fill state buffer", __func__);
	close(fd);

	initstate(time(NULL), state, sizeof(state));
#endif
	rng_initialized = 1;
}

static int
choose(unsigned int upper_bound)
{
	if (!rng_initialized)
		init_rng();

	return (int) portable_random() % upper_bound;
}

/*
 * This is the dumbest 16-byte base64 data generator.
 *
 * Since RFC6455 says we don't care about the random 16-byte value used
 * for the key (the server never decodes it), why bother actually writing
 * a proper base64 encoding when we can just pick 22 valid base64 characters
 * to make our key?
 */
static void
dumb_key(char *out)
{
	int i, r;

	/* 25 because 22 for the fake b64 + == + NULL */
	memset(out, 0, 25);
	for (i = 0; i < 22; i++) {
		r = choose(sizeof(B64) - 1);
		out[i] = B64[r];
	}
	out[22] = '=';
	out[23] = '=';
	out[24] = '\0';
}

/*
 * As the name implies, it just makes a random mask for use in our frames.
 */
static void
dumb_mask(uint8_t mask[4])
{
	uint32_t r;

	if (!rng_initialized)
		init_rng();
	r = portable_random();

	mask[0] = r >> 24;
	mask[1] = (r & 0x00FF0000) >> 16;
	mask[2] = (r & 0x0000FF00) >> 8;
	mask[3] = (r & 0x000000FF);
}

/*
 * Safely read at most `n` bytes into the given buffer.
 */
static ssize_t
ws_read(struct websocket *ws, void *buf, size_t buflen)
{
	ssize_t _buflen, sz, len = 0;
	char *_buf;

	if (buflen > INT_MAX)
		crap(1, "ws_read: buflen too large");

	_buf = (char*) buf;
	_buflen = (ssize_t) buflen;

	while (_buflen > 0) {
		if (ws->ctx) {
			sz = tls_read(ws->ctx, _buf, _buflen);
			if (sz == TLS_WANT_POLLIN || sz == TLS_WANT_POLLOUT) {
				if (len == 0)
					return DWS_WANT_POLL;
				break;
			} else if (sz == -1)
				return -1;
		} else {
			sz = recv(ws->s, _buf, _buflen, 0);
			if (sz == -1 && errno == EAGAIN) {
				if (len == 0)
					return DWS_WANT_POLL;
				break;
			}
			else if (sz == -1) {
				// TODO: check some common errno's and return
				// something better.
				return -1;
			} else if (sz == 0) {
				// Disconnect/EOF?
				// TODO!
				return -1;
			}
		}

		_buf += sz;
		_buflen -= sz;
		len += sz;
	}

	// TODO: figure out how we want to handle errors...
	// win32 spits out a different error than posix systems, btw.

	return len;
}

/*
 * Read at `buflen` bytes into the given buffer, busy polling as needed.
 */
static ssize_t
ws_read_all(struct websocket *ws, void *buf, size_t buflen)
{
	ssize_t _buflen, sz, len = 0;
	char *_buf;

	if (buflen > INT_MAX)
		crap(1, "%s: buflen too large", __func__);
	if (buflen == 0)
		crap(1, "%s: buflen == 0?!", __func__);

	_buf = (char*) buf;
	_buflen = (ssize_t) buflen;

	while (_buflen > 0) {
		if (ws->ctx) {
			sz = tls_read(ws->ctx, _buf, _buflen);
			if (sz == TLS_WANT_POLLIN || sz == TLS_WANT_POLLOUT)
				continue;
			else if (sz == -1)
				return -1;
		} else {
			sz = recv(ws->s, _buf, _buflen, 0);
			if (sz == -1 && errno == EAGAIN)
				continue;
			else if (sz == -1)
				return -1;
			else if (sz == 0) // TODO: disconnect!
				return -1;
		}

		_buf += sz;
		_buflen -= sz;
		len += sz;
	}

	// TODO: figure out how we want to handle errors...
	// win32 spits out a different error than posix systems, btw.

	return len;
}

/*
 * Read up to buflen bytes into buf, looking for `\r\n` terminators.
 */
static ssize_t
ws_read_txt(struct websocket *ws, void *buf, size_t buflen)
{
	ssize_t _buflen, sz, len = 0;
	char *_buf, *end = NULL;

	if (buflen > INT_MAX)
		crap(1, "ws_read: buflen too large");

	_buf = (char*) buf;
	_buflen = (ssize_t) buflen;

	while (_buflen > 0) {
		if (ws->ctx) {
			sz = tls_read(ws->ctx, _buf, _buflen);
			if (sz == TLS_WANT_POLLIN || sz == TLS_WANT_POLLOUT)
				continue;
			else if (sz == -1)
				return -1;
		} else {
			sz = recv(ws->s, _buf, _buflen, 0);
			if (sz == -1 && errno == EAGAIN)
				continue;
			else if (sz == -1)
				return -1;
			else if (sz == 0)
				return -1; // TODO: Disconnect!
		}

		_buf += sz;
		_buflen -= sz;
		len += sz;

		if (len >= 4) {
			// Look for terminator pattern.
			end = _buf - 4;
			if (memcmp(end, "\r\n\r\n", 4) == 0)
				break;
		}
	}

	// TODO: figure out how we want to handle errors...
	// win32 spits out a different error than posix systems, btw.

	return len;
}

/*
 * Safely write the given buf up to buflen via the socket.
 *
 * Will write the entirety of the given buffer. Does not currently use any
 * poll like functionality, so will busy poll the socket!
 */
static ssize_t
ws_write(struct websocket *ws, const void *buf, size_t buflen)
{
	ssize_t _buflen, sz, len = 0;
	char *_buf;

	if (buflen > INT_MAX)
		return -1;
	if (buflen == 0)
		return 0;

	_buf = (char *)buf;
	_buflen = (ssize_t) buflen;

	while (_buflen > 0) {
		if (ws->ctx) {
			sz = tls_write(ws->ctx, _buf, (size_t) _buflen);
			if (sz == TLS_WANT_POLLOUT || sz == TLS_WANT_POLLIN)
				continue;
			else if (sz == -1)
				return -1;
		} else {
			sz = send(ws->s, _buf, (size_t) _buflen, 0);
			if (sz == -1 && errno == EAGAIN)
				continue;
			else if (sz == -1)
				return -1;
		}

		_buf += sz;
		_buflen -= sz;
		len += sz;
	}

	return len;
}

/*
 * Initialize a frame buffer, returning the current size of the frame in bytes.
 *
 * For reference, this is what frames look like per RFC6455 sec. 5.2:
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-------+-+-------------+-------------------------------+
 *    |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 *    |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 *    |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 *    | |1|2|3|       |K|             |                               |
 *    +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 *    |     Extended payload length continued, if payload len == 127  |
 *    + - - - - - - - - - - - - - - - +-------------------------------+
 *    |                               |Masking-key, if MASK set to 1  |
 *    +-------------------------------+-------------------------------+
 *    | Masking-key (continued)       |          Payload Data         |
 *    +-------------------------------- - - - - - - - - - - - - - - - +
 *    :                     Payload Data continued ...                :
 *    + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 *    |                     Payload Data continued ...                |
 *    +---------------------------------------------------------------+
 */
static ssize_t
init_frame(uint8_t *frame, enum ws_opcode opcode, uint8_t mask[4], size_t len)
{
	int idx = 0;
	uint16_t payload;

	// Just a quick safety check: we don't do large payloads
	if (len > (1 << 24))
		return -1;

	frame[0] = (uint8_t) (0x80 + opcode);
	if (len < 126) {
		// The trivial "7 bit" payload case
		frame[1] = 0x80 + (uint8_t) len;
		idx = 1;
	} else {
		// The "7+16 bits" payload len case
		frame[1] = 0x80 + 126;

		// Payload length in network byte order
		payload = htons(len);
		frame[2] = payload & 0xFF;
		frame[3] = payload >> 8;
		idx = 3;
	}
	// And that's it, because 2^24 bytes should be enough for anyone!

	// Gotta send a copy of the mask
	frame[++idx] = mask[0];
	frame[++idx] = mask[1];
	frame[++idx] = mask[2];
	frame[++idx] = mask[3];

	// XXX: remember, we return the true size...not the offset
	return idx + 1;
}

#ifdef DEBUG
static void
dump_frame(uint8_t *frame, size_t len)
{
	size_t i;
	int first = 1;

	for (i = 0; i < len; i++) {
		printf("0x%02x ", frame[i]);
		if (!first && (i+1) % 4 == 0)
			printf("\n");
		first = 0;
	}
	printf("\n");
}
#endif

/*
 * dumb_frame
 *
 * Construct a Binary frame containing a given payload
 *
 * Parameters:
 *  (out) frame: pointer to a buffer to write the frame data to
 *  data: pointer to the binary data payload to frame
 *  len: length of the binary payload to frame
 *
 * Assumptions:
 *  - you've properly sized the destination buffer (*out)
 *
 * Returns:
 *  size of the frame in bytes,
 * -1 when len is too large
 *
 */
static ssize_t
dumb_frame(uint8_t *frame, const uint8_t *data, size_t len)
{
	int i;
	ssize_t header_len;
	uint8_t mask[4] = { 0, 0, 0, 0 };

	// Just a quick safety check: we don't do large payloads
	if (len > (1 << 24))
		return DWS_ERR_TOO_LARGE;

	// Pretend we're in Eyes Wide Shut
	dumb_mask(mask);

	header_len = init_frame(frame, BINARY, mask, len);
	if (header_len < 0)
		crap(1, "init_frame: bad frame length");

	for (i = 0; i < (int) len; i++) {
		// We just transmit in host byte order, someone else's problem
		frame[header_len + i] = data[i] ^ mask[i % 4];
	}

	return header_len + i;
}

/*
 * dumb_handshake
 *
 * Take an existing, connected socket and do the secret websocket fraternity
 * handshake to prove we are a dumb websocket client.
 *
 * Parameters:
 *  ws: a pointer to a connected websocket
 *  host: string representing the hostname
 *  path: the uri path, like "/" or "/dumb"
 *
 * Returns:
 *  0 on success,
 *  DWS_ERR_HANDSHAKE_BUF if it failed to generate the handshake buffer,
 *  DWS_ERR_HANDSHAKE_ERR if it received an invalid handshake response,
 *  fatal error otherwise.
 */
int
dumb_handshake(struct websocket *ws, const char *path, const char *proto)
{
	int len, ret = 0;
	char key[25], buf[HANDSHAKE_BUF_SIZE];
	ssize_t sz = 0;

	memset(key, 0, sizeof(key));
	dumb_key(key);

	len = snprintf(buf, sizeof(buf), HANDSHAKE_TEMPLATE,
				   path, ws->host, ws->port, key, proto);
	if (len < 1)
		return DWS_ERR_HANDSHAKE_BUF;

	// Send our upgrade request.
	sz = ws_write(ws, buf, len);
	if (sz != len)
		crap(1, "dumb_handshake: ws_write");

	memset(buf, 0, sizeof(buf));
	len = ws_read_txt(ws, buf, sizeof(buf));
	if (len == -1)
		return DWS_ERR_HANDSHAKE_BUF;

	/* XXX: If we gave a crap, we'd validate the returned key per the
	 * requirements of RFC6455 sec. 4.1, but we don't.
	 */
	if (memcmp(server_handshake, buf, sizeof(server_handshake) - 1)) {
        ret = DWS_ERR_HANDSHAKE_RES;
    }

	return ret;
}

/*
 * dumb_connect
 *
 * Ugh, just connect to a host/port, ok? This just simplifies some of the
 * setup of a socket connection, so is totally optional.
 *
 * Parameters:
 *    ws: (out) pointer to a websocket struct to initialize
 *  host: hostname or ip address a string
 *  port: tcp port number
 *
 * Returns:
 *  0 on success,
 *  DWS_ERR_CONN_CREATE if it failed to create a socket,
 *  DWS_ERR_CONN_RESOLVE if it failed to resolve host (check h_errno),
 *  DWS_ERR_CONN_CONNECT if it failed to connect(2).
 */
int
dumb_connect(struct websocket *ws, const char *host, uint16_t port)
{
	int s;
	char port_buf[8];
	struct addrinfo hints, *res;

#ifdef _WIN32
	int ret;
	WSADATA wsaData = {0};
	ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret)
		crap(1, "WSAStartup failed: %d", ret);
#endif

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return DWS_ERR_CONN_CREATE;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_CANONNAME;

	memset(port_buf, 0, sizeof(port_buf));
	snprintf(port_buf, sizeof(port_buf), "%d", port);
	if (getaddrinfo(host, port_buf, &hints, &res))
		return DWS_ERR_CONN_RESOLVE;

	// XXX: for now we're lazy and only try the first addrinfo
	if (connect(s, res->ai_addr, res->ai_addrlen))
		return DWS_ERR_CONN_CONNECT;

	// Set to non blocking
	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		return DWS_ERR_CONN_CONNECT;

	// Store some state
	ws->port = port;
	ws->host = strdup(host);
	ws->s = s;
	ws->ctx = NULL;
	memset(&ws->addr, 0, sizeof(ws->addr));
	memcpy(&ws->addr, res, sizeof(ws->addr));

	freeaddrinfo(res);

	return 0;
}

/*
 * dumb_connect_tls
 *
 * Like dumb_connect, but establishes a TLS connection.
 *
 * Parameters:
 *  ws: pointer to a websocket structure,
 *  host: hostname or ip address to connect to,
 *  port: tcp port to connect to,
 *  insecure: set to non-zero to disable cert verification
 */
int
dumb_connect_tls(struct websocket *ws, const char *host, uint16_t port,
				 int insecure)
{
	int ret;
	ret = dumb_connect(ws, host, port);
	// TODO: better error handling...for now we hard fail for debugging
	if (ret)
		crap(ret, "dumb_connect failed");

	ws->ctx = tls_client();
	if (ws->ctx == NULL)
		crap(1, "%s: tls_client failure", __func__);

	ws->cfg = tls_config_new();

	if (insecure) {
		// XXX: I sure hope you know what you're doing :-)
		tls_config_insecure_noverifycert(ws->cfg);
		tls_config_insecure_noverifyname(ws->cfg);
	}

	ret = tls_configure(ws->ctx, ws->cfg);
	if (ret)
		crap(1, "%s: invalid tls config", __func__);

	return tls_connect_socket(ws->ctx, ws->s, host);
}

/*
 * dumb_send
 *
 * Send some data to a dumb websocket server in a binary frame. Handles the
 * dumb framing so you don't have toooooo!
 *
 * Parameters:
 *  ws: a pointer to a connected dumb websocket
 *  payload: the binary payload to send
 *  len: the length of the payload in bytes
 *
 * Returns:
 *  the amount of bytes sent,
 *  DWS_ERR_MALLOC on failure to calloc(3) a buffer for the dumb websocket
 *  frame, or whatever ws_write might return on error (zero or a negative value)
 */
ssize_t
dumb_send(struct websocket *ws, const void *payload, size_t len)
{
	uint8_t *frame;
	ssize_t frame_len, n;

	// We need payload size + 14 bytes minimum, but pad a little extra
	frame = calloc(1, len + 16);
	if (frame == NULL)
		return DWS_ERR_MALLOC;

	frame_len = dumb_frame(frame, payload, len);
	if (frame_len < 0)
		crap(1, "%s: invalid frame payload length", __func__);

	n = ws_write(ws, frame, (size_t) frame_len);

	free(frame);
	return n;
}

/*
 * dumb_recv
 *
 * Try to receive some data from a dumb websocket server. Strips away all the
 * dumb framing so you get just the data ;-)
 *
 * If the data is too large to fit in the destination buffer, it is truncated
 * due to using memcpy(3).
 *
 * Parameters:
 *  ws: a pointer to a connected websocket
 * (out) out: pointer to a buffer to copy to resulting payload to
 * len: max size of the out-buffer
 *
 * Returns:
 *  the number of bytes received in the payload (not including frame headers),
 *  DWS_ERR_READ on failure to recv(2) data, DWS_WANT_POLL or DWS_SHUTDOWN.
 */
ssize_t
dumb_recv(struct websocket *ws, void *buf, size_t buflen)
{
	uint8_t frame[4] = { 0 };
	ssize_t payload_len;
	ssize_t n = 0;

	// Read first 2 bytes to figure out the framing details.
	n = ws_read(ws, frame, 2);
	if (n < 0) {
		if (n == -1)
			return DWS_ERR_READ;
		return n;
	}

	// Now to validate the frame...
	if (!(frame[0] & 0x80)) {
		// XXX: We don't currently support fragmentation
		crap(1, "%s: fragmentation unsupported", __func__);
	}

	switch (frame[0] & 0x0F) {
	case TEXT:
		crap(1, "%s: unsupported TEXT frame!", __func__);
		// unreached
	case CLOSE:
		// Unexpected, but possible if the server hates us apparently!
		ws_shutdown(ws);
		return DWS_SHUTDOWN;
	case PING:
		// Also unexpected! WTF.
		return DWS_WANT_PONG;
	case PONG:
		// This...should not happen, but process the message.
		// Fallthrough
	case BINARY:
		// Fallthrough
	default:
		// Ok. We have something we *think* we can work with!
		payload_len = frame[1] & 0x7F;
	}

	if (payload_len == 126) {
		// Need the next two bytes to get the actual payload size, which
		// arrives in network byte order.
		n = ws_read_all(ws, frame + 2, 2);
		if (n < 2)
			return DWS_ERR_READ;
		payload_len = frame[2] << 8;
		payload_len += frame[3];
	} else if (payload_len > 126)
		crap(1, "%s: unsupported payload size", __func__);

	// We can now read the the payload, if there is one.
	payload_len = MIN((size_t)payload_len, buflen);
	if (payload_len == 0)
		return 0;

	n = ws_read_all(ws, buf, (size_t)payload_len);
	if (n < payload_len)
		return DWS_ERR_READ;

	return payload_len;
}

/*
 * dumb_ping
 *
 * Send a websocket ping to the server. It's dumb to have payloads here, so
 * it doesn't support them ;P
 *
 * Parameters:
 *  ws: pointer to a connected websocket for sending the ping
 *
 * Returns:
 *  0 on success,
 *  DWS_ERR_WRITE on failure during send(2),
 *  DWS_ERR_READ on failure to recv(2) the response,
 *  DWS_ERR_INVALID on the response being invalid (i.e. not a PONG)
 */
int
dumb_ping(struct websocket *ws)
{
	ssize_t len, payload_len;
	uint8_t mask[4];
	uint8_t frame[128];

	memset(frame, 0, sizeof(frame));
	dumb_mask(mask);

	len = init_frame(frame, PING, mask, 0);

	len = ws_write(ws, frame, (size_t) len);
	if (len < 1)
		return DWS_ERR_WRITE;

	memset(frame, 0, sizeof(frame));

	// Read first 2 bytes.
	len = ws_read_all(ws, frame, 2);
	if (len < 0)
		return DWS_ERR_READ;

	// We should have a PONG reply.
	if (frame[0] != (0x80 + PONG))
		return DWS_ERR_INVALID;

	payload_len = frame[1] & 0x7F;
	if (payload_len >= 126)
		crap(1, "dumb_ping: unsupported pong payload size > 125");

	// Dump the rest of the data on the floor.
	if (payload_len > 0) {
		len = ws_read_all(ws, frame + 2,
		    MIN((size_t)payload_len, sizeof(frame) - 2));
		if (len < 1)
			return DWS_ERR_INVALID;
	}

	return 0;
}

#ifdef _WIN32
#define HOW SD_BOTH
#else
#define HOW SHUT_RDWR
#endif

static void
ws_shutdown(struct websocket *ws)
{
	// Now close/shutdown our socket.
	if (ws->ctx)
		tls_close(ws->ctx);

	// Don't care if shutdown fails. Other side may have closed some things first.
	shutdown(ws->s, HOW);

	ws->ctx = NULL; // XXX does this leak anything?
	ws->s = -1;

	// Not sure if it make sense to "free" things here or not.
	free(ws->host);
	ws->host = NULL;
	ws->port = 0;
}

/*
 * dumb_close
 *
 * Sadly, websockets have some "close" frame that some servers expect. If a
 * client disconnects without sending one, they sometimes get snippy. It's
 * sorta dumb.
 *
 * Note: doesn't free the data structures as it's reopenable, but the socket
 * does get closed per the spec.
 *
 * Parameters:
 *  ws: a pointer to a connected websocket to close
 *
 * Returns:
 *  0 on success,
 *  DWS_ERR_WRITE on failure to send(2) the close frame,
 *  DWS_ERR_READ on failure to recv(2) a response,
 *  DWS_ERR_INVALID on a response being invalid (i.e. not a CLOSE),
 */
int
dumb_close(struct websocket *ws)
{
	ssize_t len, payload_len;
	uint8_t mask[4];
	uint8_t frame[128];

	memset(frame, 0, sizeof(frame));
	dumb_mask(mask);

	len = init_frame(frame, CLOSE, mask, 0);

	len = ws_write(ws, frame, (size_t) len);
	if (len < 1)
		return DWS_ERR_WRITE;

	memset(frame, 0, sizeof(frame));

	// A valid RFC6455 websocket server MUST send a Close frame in response
	// Read first 2 bytes.
	len = ws_read_all(ws, frame, 2);
	if (len != 2)
		return DWS_ERR_READ;

	// If we don't have a CLOSE frame...someone screwed up before calling
	// dumb_close and there's still unread data!
	if (frame[0] != (0x80 + CLOSE))
		return DWS_ERR_INVALID;

	payload_len = frame[1] & 0x7F;
	if (payload_len > 126)
		crap(1, "dumb_close: unsupported close payload size > 125");

	// Dump the rest of the data on the floor.
	if (payload_len > 0) {
		len = ws_read_all(ws, frame + 2,
		    MIN((size_t)payload_len, sizeof(frame) - 2));
		if (len < 1)
			return DWS_ERR_READ;
	}

	ws_shutdown(ws);

	return 0;
}

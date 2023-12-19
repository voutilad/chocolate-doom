/*
MIT License

Copyright(c) 2018 Liam Bindle

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <dws.h>
#include <mqtt.h>
#include <mqtt_dws.h>
#include <stdio.h>

ssize_t mqtt_pal_sendall(mqtt_pal_socket_handle fd, const void *buf, size_t bufsz,
                         int flags) {
    ssize_t sz = 0;
    struct websocket *ws = (struct websocket *)fd;

    sz = dumb_send(ws, buf, bufsz);
    if (sz == -1)
        return MQTT_ERROR_SOCKET_ERROR;
    return sz;
}

ssize_t mqtt_pal_recvall(mqtt_pal_socket_handle fd, void *buf, size_t bufsz, int flags) {
    ssize_t sz = 0;
    struct websocket *ws = (struct websocket *)fd;

    sz = dumb_recv(ws, buf, bufsz);
    if (sz == DWS_WANT_POLL)
        return 0;
    else if (sz == DWS_SHUTDOWN)
        return MQTT_ERROR_CONNECTION_CLOSED;
    else if (sz == DWS_WANT_PONG) {
        // Ugh. This is silly.
        return MQTT_ERROR_SOCKET_ERROR;
    } else if (sz == -1)
        return MQTT_ERROR_SOCKET_ERROR;

    return sz;
}

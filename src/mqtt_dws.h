#if !defined(__MQTT_DWS_H__)
#define __MQTT_DWS_H__

/*
MIT License

Copyright(c) 2023 Dave Voutila
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

/* UNIX-like platform support */
#if defined(__unix__) || defined(__APPLE__) || defined(__NuttX__)
    #include <limits.h>
    #include <string.h>
    #include <stdarg.h>
    #include <time.h>
    #include <arpa/inet.h>
    #include <pthread.h>

    #define MQTT_PAL_HTONS(s) htons(s)
    #define MQTT_PAL_NTOHS(s) ntohs(s)

    #define MQTT_PAL_TIME() time(NULL)

    typedef time_t mqtt_pal_time_t;
    typedef pthread_mutex_t mqtt_pal_mutex_t;

    #define MQTT_PAL_MUTEX_INIT(mtx_ptr) pthread_mutex_init(mtx_ptr, NULL)
    #define MQTT_PAL_MUTEX_LOCK(mtx_ptr) pthread_mutex_lock(mtx_ptr)
    #define MQTT_PAL_MUTEX_UNLOCK(mtx_ptr) pthread_mutex_unlock(mtx_ptr)

#elif defined(_MSC_VER) || defined(WIN32)
    #include <limits.h>
    #include <winsock2.h>
    #include <windows.h>
    #include <time.h>
    #include <stdint.h>

    typedef SSIZE_T ssize_t;
    #define MQTT_PAL_HTONS(s) htons(s)
    #define MQTT_PAL_NTOHS(s) ntohs(s)

    #define MQTT_PAL_TIME() time(NULL)

    typedef time_t mqtt_pal_time_t;
    typedef CRITICAL_SECTION mqtt_pal_mutex_t;

    #define MQTT_PAL_MUTEX_INIT(mtx_ptr) InitializeCriticalSection(mtx_ptr)
    #define MQTT_PAL_MUTEX_LOCK(mtx_ptr) EnterCriticalSection(mtx_ptr)
    #define MQTT_PAL_MUTEX_UNLOCK(mtx_ptr) LeaveCriticalSection(mtx_ptr)
#endif

/* Establish our PAL interface for dumb-ws... */
#include "dws.h"
typedef struct websocket *mqtt_pal_socket_handle;

/**
 * @brief Sends all the bytes in a buffer.
 * @ingroup pal
 *
 * @param[in] fd The file-descriptor (or handle) of the socket.
 * @param[in] buf A pointer to the first byte in the buffer to send.
 * @param[in] len The number of bytes to send (starting at \p buf).
 * @param[in] flags Flags which are passed to the underlying socket.
 *
 * @returns The number of bytes sent if successful, an \ref MQTTErrors otherwise.
 *
 * Note about the error handling:
 * - On an error, if some bytes have been processed already,
 *   this function should return the number of bytes successfully
 *   processed. (partial success)
 * - Otherwise, if the error is an equivalent of EAGAIN, return 0.
 * - Otherwise, return MQTT_ERROR_SOCKET_ERROR.
 */
ssize_t mqtt_pal_sendall(mqtt_pal_socket_handle fd, const void* buf, size_t len, int flags);

/**
 * @brief Non-blocking receive all the byte available.
 * @ingroup pal
 *
 * @param[in] fd The file-descriptor (or handle) of the socket.
 * @param[in] buf A pointer to the receive buffer.
 * @param[in] bufsz The max number of bytes that can be put into \p buf.
 * @param[in] flags Flags which are passed to the underlying socket.
 *
 * @returns The number of bytes received if successful, an \ref MQTTErrors otherwise.
 *
 * Note about the error handling:
 * - On an error, if some bytes have been processed already,
 *   this function should return the number of bytes successfully
 *   processed. (partial success)
 * - Otherwise, if the error is an equivalent of EAGAIN, return 0.
 * - Otherwise, return MQTT_ERROR_SOCKET_ERROR.
 */
ssize_t mqtt_pal_recvall(mqtt_pal_socket_handle fd, void* buf, size_t bufsz, int flags);

#endif /* __MQTT_DWS_H__ */

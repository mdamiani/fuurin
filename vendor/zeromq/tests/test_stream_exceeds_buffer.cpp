/*
    Copyright (c) 2007-2016 Contributors as noted in the AUTHORS file

    This file is part of libzmq, the ZeroMQ core engine in C++.

    libzmq is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License (LGPL) as published
    by the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    As a special exception, the Contributors give you permission to link
    this library with independent modules to produce an executable,
    regardless of the license terms of these independent modules, and to
    copy and distribute the resulting executable under terms of your choice,
    provided that you also meet, for each linked independent module, the
    terms and conditions of the license of that module. An independent
    module is a module which is not derived from or based on this library.
    If you modify this library, you must extend this exception to your
    version of the library.

    libzmq is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "testutil.hpp"
#include "testutil_unity.hpp"

void setUp ()
{
    setup_test_context ();
}

void tearDown ()
{
    teardown_test_context ();
}

#if defined(ZMQ_HAVE_WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdexcept>
#define close closesocket
#endif

void test_stream_exceeds_buffer ()
{
    const int msgsize = 8193;
    char sndbuf[msgsize] = "\xde\xad\xbe\xef";
    unsigned char rcvbuf[msgsize];
    char my_endpoint[MAX_SOCKET_STRING];

    int server_sock =
      TEST_ASSERT_SUCCESS_RAW_ERRNO (socket (AF_INET, SOCK_STREAM, 0));
    int enable = 1;
    TEST_ASSERT_SUCCESS_RAW_ERRNO (setsockopt (server_sock, SOL_SOCKET,
                                               SO_REUSEADDR, (char *) &enable,
                                               sizeof (enable)));

    struct sockaddr_in saddr;
    memset (&saddr, 0, sizeof (saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT >= 0x0600)
    saddr.sin_port = 0;
#else
    saddr.sin_port = htons (12345);
#endif

    TEST_ASSERT_SUCCESS_RAW_ERRNO (
      bind (server_sock, (struct sockaddr *) &saddr, sizeof (saddr)));
    TEST_ASSERT_SUCCESS_RAW_ERRNO (listen (server_sock, 1));

#if !defined(_WIN32_WINNT) || (_WIN32_WINNT >= 0x0600)
    socklen_t saddr_len = sizeof (saddr);
    TEST_ASSERT_SUCCESS_RAW_ERRNO (
      getsockname (server_sock, (struct sockaddr *) &saddr, &saddr_len));
#endif
    sprintf (my_endpoint, "tcp://127.0.0.1:%d", ntohs (saddr.sin_port));

    void *zsock = test_context_socket (ZMQ_STREAM);
    TEST_ASSERT_SUCCESS_ERRNO (zmq_connect (zsock, my_endpoint));

    int client_sock =
      TEST_ASSERT_SUCCESS_RAW_ERRNO (accept (server_sock, NULL, NULL));

    TEST_ASSERT_SUCCESS_RAW_ERRNO (close (server_sock));

    TEST_ASSERT_EQUAL_INT (msgsize, send (client_sock, sndbuf, msgsize, 0));

    zmq_msg_t msg;
    zmq_msg_init (&msg);

    int rcvbytes = 0;
    while (rcvbytes == 0) // skip connection notification, if any
    {
        TEST_ASSERT_SUCCESS_ERRNO (zmq_msg_recv (&msg, zsock, 0)); // peerid
        TEST_ASSERT_TRUE (zmq_msg_more (&msg));
        rcvbytes = TEST_ASSERT_SUCCESS_ERRNO (zmq_msg_recv (&msg, zsock, 0));
        TEST_ASSERT_FALSE (zmq_msg_more (&msg));
    }

    // for this test, we only collect the first chunk
    // since the corruption already occurs in the first chunk
    memcpy (rcvbuf, zmq_msg_data (&msg), zmq_msg_size (&msg));

    zmq_msg_close (&msg);
    test_context_socket_close (zsock);
    close (client_sock);

    TEST_ASSERT_GREATER_OR_EQUAL (4, rcvbytes);

    // notice that only the 1st byte gets corrupted
    TEST_ASSERT_EQUAL_UINT (0xef, rcvbuf[3]);
    TEST_ASSERT_EQUAL_UINT (0xbe, rcvbuf[2]);
    TEST_ASSERT_EQUAL_UINT (0xad, rcvbuf[1]);
    TEST_ASSERT_EQUAL_UINT (0xde, rcvbuf[0]);
}

int main ()
{
    UNITY_BEGIN ();
    RUN_TEST (test_stream_exceeds_buffer);
    return UNITY_END ();
}

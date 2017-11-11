/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ZMQLOW_H
#define ZMQLOW_H


#include "types.h"


#include <zmq.h>
#include <boost/config.hpp> // for BOOST_LIKELY


#if defined(FUURIN_ENDIANESS_BIG) && defined(FUURIN_ENDIANESS_LITTLE)
#error "Please #define either FUURIN_ENDIANESS_BIG or FUURIN_ENDIANESS_LITTLE, not both"
#endif

#if !defined(FUURIN_ENDIANESS_BIG) && !defined(FUURIN_ENDIANESS_LITTLE)
#define FUURIN_ENDIANESS_BIG
#endif


namespace fuurin {
namespace zmq {


/**
 * \brief Copies data, according to the target network endianess.
 *
 * Given data is copied from \c source to \c dest. Both of these
 * pointers must no overlap, otherwise the result is undefined.
 *
 * The network endianess can be choosen at compile time,
 * by defining one of these macros:
 *
 *  - \c FUURIN_ENDIANESS_BIG: network endianess is Big Endian.
 *  - \c FUURIN_ENDIANESS_LITTLE: network endianess is Little Endian.
 *
 * If none of the previous macros is defined,
 * then the default network endianess is Big Endian.
 *
 * \param[out] dest Destination buffer.
 * \param[in] source Source buffer.
 * \param[in] size Bytes to copy.
 *
 * \see memcpyToMessage
 * \see memcpyFromMessage
 */
inline void memcpyWithEndian(void *dest, const void *source, size_t size);


/**
 * \brief Applies the correct endianess to \c data, in order to send it over network.
 *
 * \param[in] data Data to send over the network.
 * \param[out] dest Destination buffer.
 *
 * \see memcpyWithEndian
 */
template <typename T>
void memcpyToMessage(const T &data, void *dest);


/**
 * \brief Applies the correct endianess to a data, that was received from network.
 *
 * \param[in] source Data received from the network, as an array of bytes.
 * \param[in] size Size of the \c source buffer.
 * \param[out] part Returned part, with correct host endianess.
 *
 * \see memcpyWithEndian
 */
template <typename T>
void memcpyFromMessage(const void *source, size_t size, T *part);


/**
 * \brief Sends a ZMQ multipart message.
 *
 * The behaviour of this function if the same of \c zmq_msg_send.
 * The endianess of passed \c part is reversed when
 * host endianess doesn't match the network's.
 *
 * \param[in] socket A valid ZMQ socket.
 * \param[in] flags Flags accepted by function \c zmq_msg_send.
 * \param[in] part A single message part to send.
 *
 * \return Number of bytes in the message if successful.
 *         Otherwise -1 and set \c errno variable with the error code.
 *         Error codes are the ones returned by functions
 *         \c zmq_msg_init_size and \c zmq_msg_send.
 *
 * \see memcpyToMessage
 * \see sendMultipartMessage(void*, int, const T &, Args...)
 */
template <typename T>
int sendMultipartMessage(void *socket, int flags, const T &part);


/**
 * \brief Sends a ZMQ multipart message.
 *
 * \param[in] socket A valid ZMQ socket.
 * \param[in] flags Flags accepted by function \c zmq_msg_send.
 * \param[in] part Current message part to send.
 * \param[in] args Additional message parts to send.
 *
 * \return Number of bytes in the last message if successful.
 *         Otherwise -1 in case the multipart message could not be sent.
 *
 * \see sendMultipartMessage(void*, int, const T &)
 */
template <typename T, typename... Args>
int sendMultipartMessage(void *socket, int flags, const T &part, Args... args)
{
    const int rc = sendMultipartMessage(socket, flags | ZMQ_SNDMORE, part);

    if (BOOST_LIKELY(rc != -1))
        return sendMultipartMessage(socket, flags, args...);
    else
        return rc;
}


/**
 * \brief Receives a ZMQ multipart message.
 *
 * The behaviour of this function if the same of \c zmq_recv_msg.
 * The endianess of received data is reversed when
 * host endianess doesn't match the network's.
 *
 * \param[in] socket A valid ZMQ socket.
 * \param[in] flags Flags accepted by function \c zmq_msg_recv.
 * \param[out] part A single message part to fill with the received data.
 *
 * \return Number of bytes in the message if successful.
 *         Otherwise -1 and set \c errno variable with the error code.
 *         Error codes are the ones returned by function \c zmq_msg_recv.
 *
 * \see memcpyFromMessage
 * \see recvMultipartMessage(void*, int, const T &, Args...)
 */
template <typename T>
int recvMultipartMessage(void *socket, int flags, T *part);


/**
 * \brief Receives a ZMQ multipart message.
 *
 * \param[in] socket A valid ZMQ socket.
 * \param[in] flags Flags accepted by function \c zmq_msg_recv.
 * \param[out] part A single message part to fill with the received data.
 * \param[out] args Additional message parts to receive.
 *
 * \return Number of bytes in the last message if successful.
 *         Otherwise -1 in case the multipart message could not be received.
 *
 * \see recvMultipartMessage(void*, int, T *)
 */
template <typename T, typename... Args>
int recvMultipartMessage(void *socket, int flags, T *part, Args... args)
{
    const int rc = recvMultipartMessage(socket, flags, part);

    if (BOOST_LIKELY(rc != -1))
        return recvMultipartMessage(socket, flags, args...);
    else
        return rc;
}


/**
 * \brief Creates a ZMQ socket.
 *
 * This function calls \c zmq_socket.
 *
 * \param[in] context A valid ZMQ context.
 * \param[in] type Type of ZMQ socket.
 *
 * \return A handle to a ZMQ socket or \c null in case of errors.
 */
void *createSocket(void *context, int type);


/**
 * \brief Closes a ZMQ socket.
 *
 * This function calls \c zmq_close.
 * Before closing the \c socket, its \c ZMQ_LINGER property is set to 0.
 *
 * \param[in] socket A valid ZMQ socket.
 *
 * \return \c false in case \c socket is invalid or it could not be closed.
 *
 * \see setSocketOption
 */
bool closeSocket(void *socket);


/**
 * \brief Connects a ZMQ socket.
 *
 * This functions calls \c zmq_connect.
 *
 * \param[in] socket A valid ZMQ socket.
 * \param[in] endpoint A ZMQ endpoint to use.
 *
 * \return \c false in case \c socket could not be connected.
 */
bool connectSocket(void *socket, const char *endpoint);


/**
 * \brief Binds a ZMQ socket.
 *
 * This functions calls \c zmq_bind.
 *
 * Possibly the bind action is retried until the specified \c timeout,
 * in case of EADDRINUSE error.
 * This is done to avoid the 'address already in use' error,
 * when fast respawing the same application.
 *
 * \param[in] socket A valid ZMQ socket.
 * \param[in] endpoint A ZMQ endpoint to use.
 * \param[in] timeout Timeout in milliseconds until retry is performed,
 *                    before returning an error. When passing -1,
 *                    retry is not performed at all. When passing 0,
 *                    retring is done forever.
 *
 * \return \c false in case \c socket could not be bound to \c endpoint.
 */
bool bindSocket(void *socket, const char *endpoint, int timeout = -1);


/**
 * \brief Sets a socket \c option of a ZMQ \c socket.
 *
 * Valid options are listed in the manual of ZMQ function \c zmq_setsockopt.
 *
 * \param[in] socket A valid ZMQ socket.
 * \param[in] option Option to set.
 * \param[in] value Value to set.
 *
 * \return \c true in case the specified \c option and \c value could be set.
 */
bool setSocketOption(void *socket, int option, int value);


/**
 * \brief Queries a socket \c option of a ZMQ \c socket.
 *
 * Valid options are listed in the manual of ZMQ function \c zmq_getsockopt.
 *
 * \param[in] socket A valid ZMQ socket.
 * \param[in] option Option to query.
 * \param[in] defaultValue Value to return in case \c option could not be queried.
 * \param[out] Optional argument to store whether the \c option could queried or not.
 *
 * \return The value of the specified \c option, otherwise \c defaultValue.
 */
int socketOption(void *socket, int option, int defaultValue = 0, bool *ok = 0);


}
}

#endif // ZMQLOW_H

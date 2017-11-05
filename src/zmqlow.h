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
 *
 * \return Data with correct host endianess.
 *
 * \see memcpyWithEndian
 */
template <typename T>
T memcpyFromMessage(const void *source, size_t size);


/**
 * \brief Sends a ZMQ multipart message.
 *
 * The behaviour of this function if the same of \c zmq_send.
 *
 * \param[in] socket A valid ZMQ socket.
 * \param[in] flags Flags accepted by function \c zmq_send.
 * \param[in] part A single message part to send.
 *
 * \return Number of bytes in the message if successful.
 *         Otherwise -1 and set \c errno variable with the error code.
 *         Error codes are the ones returned by functions
 *         \c zmq_msg_init_size and \c zmq_msg_send.
 *
 * \see sendMultipartMessage(void*, int, const T &, Args...)
 */
template <typename T>
int sendMultipartMessage(void *socket, int flags, const T &part);


/**
 * \brief Sends a ZMQ multipart message.
 *
 * \param[in] socket A valid ZMQ socket.
 * \param[in] flags Flags accepted by function \c zmq_send.
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


}
}

#endif // ZMQLOW_H

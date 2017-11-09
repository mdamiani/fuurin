/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "zmqlow.h"
#include "failure.h"
#include "log.h"

#include <boost/preprocessor/seq/for_each.hpp>

#include <cerrno>
#include <cstring>
#include <algorithm>


#if defined(FUURIN_ENDIANESS_BIG)
#define ENDIANESS_STRAIGHT __BIG_ENDIAN
#define ENDIANESS_OPPOSITE __LITTLE_ENDIAN
#endif

#if defined (FUURIN_ENDIANESS_LITTLE)
#define ENDIANESS_STRAIGHT __LITTLE_ENDIAN
#define ENDIANESS_OPPOSITE __BIG_ENDIAN
#endif


namespace fuurin {
namespace zmq {


inline void memcpyWithEndian(void *dest, const void *source, size_t size)
{
#if __BYTE_ORDER == ENDIANESS_STRAIGHT
    std::memcpy(dest, source, size);
#elif __BYTE_ORDER == ENDIANESS_OPPOSITE
    std::reverse_copy((uint8_t *) source, ((uint8_t *) source) + size, (uint8_t *) dest);
#else
    #error "Unable to detect endianness"
#endif
}


template <typename T>
void memcpyToMessage(const T &data, void *dest)
{
    memcpyWithEndian(dest, &data, sizeof(data));
}


template <>
void memcpyToMessage(const ByteArray &data, void *dest)
{
    std::memcpy(dest, data.data(), data.size());
}


template <typename T>
void memcpyFromMessage(const void *source, size_t size, T *part)
{
    ASSERT(sizeof(T) == size, "memcpyFromMessage size mismatch!");
    memcpyWithEndian(part, source, size);
}


template <>
void memcpyFromMessage(const void *source, size_t size, ByteArray *part)
{
    part->resize(size);
    std::memcpy(part->data(), source, size);
}


#define TEMPLATE_TO_NETWORK(r, data, T) \
    template void memcpyToMessage(const T &, void *);

#define TEMPLATE_FROM_NETWORK(r, data, T) \
    template void memcpyFromMessage(const void *, size_t, T *);

#define MSG_PART_INT_TYPES \
    (uint8_t)              \
    (uint16_t)             \
    (uint32_t)             \
    (uint64_t)

BOOST_PP_SEQ_FOR_EACH(TEMPLATE_TO_NETWORK, _, MSG_PART_INT_TYPES)
BOOST_PP_SEQ_FOR_EACH(TEMPLATE_FROM_NETWORK, _, MSG_PART_INT_TYPES)


namespace {
template <typename T>
inline size_t getPartSize(const T &part)
{
    return sizeof(part);
}


template <>
inline size_t getPartSize(const ByteArray &part)
{
    return part.size();
}

inline void closeZmqMsg(zmq_msg_t *msg) {
    const int rc = zmq_msg_close(msg);
    ASSERT(rc == 0, "zmq_msg_close failed");
}
}


template <typename T>
int sendMultipartMessage(void *socket, int flags, const T &part)
{
    int rc;

    zmq_msg_t msg;
    rc = zmq_msg_init_size(&msg, getPartSize<T>(part));
    if (BOOST_UNLIKELY(rc != 0)) {
        LOG_ERROR(format("zmq_msg_init_size: %s",
            zmq_strerror(errno)));
        return -1;
    }

    memcpyToMessage<T>(part, zmq_msg_data(&msg));

    do {
        rc = zmq_msg_send(&msg, socket, flags);
    } while (rc == -1 && errno == EINTR);

    if (BOOST_UNLIKELY(rc == -1)) {
        if (errno != EAGAIN) {
            LOG_ERROR(format("zmq_send: %s",
                zmq_strerror(errno)));
        }

        closeZmqMsg(&msg);
    }

    return rc;
}


#define TEMPLATE_SEND_MULTIPART_MESSAGE(r, data, T) \
    template int sendMultipartMessage(void *, int, const T &);

BOOST_PP_SEQ_FOR_EACH(TEMPLATE_SEND_MULTIPART_MESSAGE, _, MSG_PART_INT_TYPES(ByteArray))


template <typename T>
int recvMultipartMessage(void *socket, int flags, T *part)
{
    int rc;

    zmq_msg_t msg;
    rc = zmq_msg_init(&msg);
    ASSERT(rc == 0, "zmq_msg_init failed");

    do {
        rc = zmq_msg_recv(&msg, socket, flags);
    } while (rc == -1 && errno == EINTR);

    if (BOOST_UNLIKELY(rc == -1)) {
        if (errno != EAGAIN) {
            LOG_ERROR(format("zmq_msg_recv: %s",
                zmq_strerror(errno)));
        }
    }
    else {
        memcpyFromMessage<T>(zmq_msg_data(&msg), zmq_msg_size(&msg), part);
    }

    closeZmqMsg(&msg);
    return rc;
}


#define TEMPLATE_RECV_MULTIPART_MESSAGE(r, data, T) \
    template int recvMultipartMessage(void *, int, T *);

BOOST_PP_SEQ_FOR_EACH(TEMPLATE_RECV_MULTIPART_MESSAGE, _, MSG_PART_INT_TYPES(ByteArray))


}
}

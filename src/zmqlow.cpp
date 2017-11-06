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
T memcpyFromMessage(const void *source, size_t size)
{
    T ret;
    memcpyWithEndian(&ret, source, size);
    return ret;
}


template <>
ByteArray memcpyFromMessage(const void *source, size_t size)
{
    return ByteArray((uint8_t *) source, ((uint8_t *) source) + size);
}


#define TEMPLATE_TO_NETWORK(r, data, T) \
    template void memcpyToMessage(const T &, void *);

#define TEMPLATE_FROM_NETWORK(r, data, T) \
    template T memcpyFromMessage(const void *, size_t);

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
}


template <typename T>
int sendMultipartMessage(void *socket, int flags, const T &part)
{
    zmq_msg_t msg;
    if (zmq_msg_init_size(&msg, getPartSize<T>(part)) != 0) {
        LOG_ERROR(std::string("zmq_msg_init_size: ") + zmq_strerror(errno));
        return -1;
    }

    memcpyToMessage<T>(part, zmq_msg_data(&msg));

    int rc;
    do {
        rc = zmq_msg_send(&msg, socket, flags);
    } while (rc == -1 && errno == EINTR);

    if (BOOST_UNLIKELY(rc == -1)) {
        if (errno != EAGAIN) {
            LOG_ERROR(std::string("zmq_send: ") + zmq_strerror(errno));
        }

        const int err = errno;
        const int rc2 = zmq_msg_close(&msg);
        if (rc2 != 0) {
            LOG_ERROR(std::string("zmq_msg_close: ") + zmq_strerror(errno));
        }
        errno = err;
    }

    return rc;
}


#define TEMPLATE_SEND_MULTIPART_MESSAGE(r, data, T) \
    template int sendMultipartMessage(void *, int, const T &);

BOOST_PP_SEQ_FOR_EACH(TEMPLATE_SEND_MULTIPART_MESSAGE, _, MSG_PART_INT_TYPES(ByteArray))


}
}

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

void memcpyWithEndian(void *dest, const void *source, size_t size)
{
#if __BYTE_ORDER == ENDIANESS_STRAIGHT
    std::memcpy(dest, source, size);
#elif __BYTE_ORDER == ENDIANESS_OPPOSITE
    std::reverse_copy((char *) source, ((char *) source) + size, (char *) dest);
#else
    #error "Unable to detect endianness"
#endif
}


template <>
void dataToNetworkEndian(const ByteArray &data, uint8_t *dest)
{
    std::memcpy(dest, data.data(), data.size());
}


template <>
ByteArray dataFromNetworkEndian(const uint8_t *source, size_t size)
{
    return ByteArray(source, source+size);
}


}
}

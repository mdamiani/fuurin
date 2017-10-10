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

#include <zmq.h>

#include <cerrno>
#include <cstring>
#include <algorithm>



#if defined(FUURIN_ENDIANESS_BIG) && defined(FUURIN_ENDIANESS_LITTLE)
#error "Please #define either FUURIN_ENDIANESS_BIG or FUURIN_ENDIANESS_LITTLE, not both"
#endif

#if !defined(FUURIN_ENDIANESS_BIG) && !defined(FUURIN_ENDIANESS_LITTLE)
#define FUURIN_ENDIANESS_BIG
#endif

#if defined(FUURIN_ENDIANESS_BIG)
#define ENDIANESS_STRAIGHT __BIG_ENDIAN
#define ENDIANESS_OPPOSITE __LITTLE_ENDIAN
#endif

#if defined (FUURIN_ENDIANESS_LITTLE)
#define ENDIANESS_STRAIGHT __LITTLE_ENDIAN
#define ENDIANESS_OPPOSITE __BIG_ENDIAN
#endif


namespace {

/**
 * \brief Copies data, taking care of the network send endianess.
 *
 * Given data is copied from \c source to \c dest. Both of these
 * pointers must no overlap, otherwise the result is undefined.
 *
 * The actual endianess can be choosen through compiter macros:
 *
 *  - \c FUURIN_ENDIANESS_BIG: network data has Big Endian format.
 *  - \c FUURIN_ENDIANESS_LITTLE: network data has Little Endian format.
 *
 * \param[out] dest Destination data.
 * \param[in] source Source data.
 * \param[in] size Size to copy.
 *
 * \see dataToNetworkEndian
 * \see dataFromNetworkEndian
 */
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

/**
 * \brief Applies the correct endianess to a \c data, in order to send it over network.
 *
 * \param[in] data Data to send over the network.
 * \param[out] dest Destination buffer.
 *
 * \see memcpyWithEndian
 */
template <typename T>
void dataToNetworkEndian(const T &data, uint8_t *dest)
{
    memcpyWithEndian(dest, &data, sizeof(data));
}

/**
 * \brief Applies the correct endianess to a data, that was received from network.
 *
 * \param[in] source Data received from the network, as an array of bytes.
 *
 * \return Data with correct host endianess.
 *
 * \see memcpyWithEndian
 */
template <typename T>
T dataFromNetworkEndian(const uint8_t *source)
{
    T ret;
    memcpyWithEndian(&ret, source, sizeof(source));
    return ret;
}

}

#endif // ZMQLOW_H

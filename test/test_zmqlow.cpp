/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE zmqlow
#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include "../src/zmqlow.h"

#include <cstring>


using namespace fuurin;

typedef boost::mpl::list<
    uint8_t,
    uint16_t,
    uint32_t,
    uint64_t
> networkIntTypes;

typedef boost::mpl::list<
    ByteArray
> networkArrayTypes;

static const uint8_t networkDataBuf[] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA,
};


BOOST_AUTO_TEST_CASE_TEMPLATE(endianessSendInt, T, networkIntTypes)
{
    T val = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        val |= uint64_t(networkDataBuf[i]) << (8*i);
    }

    uint8_t dest[sizeof(T)];
    zmq::memcpyToMessage(val, &dest[0]);

    for (size_t i = 0; i < sizeof(T); ++i) {
        #if defined(FUURIN_ENDIANESS_BIG)
        BOOST_TEST(dest[i] == networkDataBuf[sizeof(T)-i-1]);
        #elif defined(FUURIN_ENDIANESS_LITTLE)
        BOOST_TEST(dest[i] == networkDataBuf[i]);
        #else
        BOOST_TEST(false, "network endianess not defined!");
        #endif
    }
}


BOOST_AUTO_TEST_CASE_TEMPLATE(endianessSendArray, T, networkArrayTypes)
{
    T val(networkDataBuf, networkDataBuf+sizeof(networkDataBuf));

    uint8_t dest[val.size()];
    zmq::memcpyToMessage(val, &dest[0]);

    BOOST_TEST(std::memcmp(&dest[0], &val[0], sizeof(networkDataBuf)) == 0);
}


BOOST_AUTO_TEST_CASE_TEMPLATE(endianessRecvInt, T, networkIntTypes)
{
    T val;
    std::memset(&val, 'a', sizeof(T));
    zmq::memcpyFromMessage<T>(&networkDataBuf[0], sizeof(T), &val);

    for (size_t i = 0; i < sizeof(T); ++i) {
        const uint8_t byte = (val >> (8*i)) & 0xFF;
        #if defined(FUURIN_ENDIANESS_BIG)
        BOOST_TEST(byte == networkDataBuf[sizeof(T)-i-1]);
        #elif defined(FUURIN_ENDIANESS_LITTLE)
        BOOST_TEST(byte == networkDataBuf[i]);
        #else
        BOOST_TEST(false, "network endianess not defined!");
        #endif
    }
}


BOOST_AUTO_TEST_CASE_TEMPLATE(endianessRecvArray, T, networkArrayTypes)
{
    const int sz = sizeof(networkDataBuf);

    T val = ByteArray(sz, 'a');
    zmq::memcpyFromMessage<T>(&networkDataBuf[0], sz, &val);

    BOOST_TEST(std::memcmp(&val[0], &networkDataBuf[0], sz) == 0);
}

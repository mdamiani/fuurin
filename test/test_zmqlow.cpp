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
#include <boost/test/data/test_case.hpp>
#include <boost/mpl/list.hpp>

#include "../src/zmqlow.h"

#include <string>


using namespace fuurin;
namespace bdata = boost::unit_test::data;

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


struct TVal {
    std::string type;
    uint64_t val;
    ByteArray arr;

    TVal(const std::string &t, uint64_t v) : type(t), val(v) {}
    TVal(const std::string &t, const ByteArray &v) : type(t), val(0), arr(v) {}
};

std::ostream& operator<<(std::ostream& os, const TVal& ts)
{
    os << ts.type << " " << ts.val;
    if (!ts.arr.empty()) {
        os << " ";
        for (auto i = ts.arr.begin(); i != ts.arr.end(); ++i)
            os << *i;
    }
    return os;
}

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

template <typename T>
void testTransferSingle(const T &part) {
    void *ctx = zmq::initContext();
    BOOST_TEST_REQUIRE(ctx, "failed to create ZMQ context");

    void *s1 = zmq::createSocket(ctx, ZMQ_PAIR);
    BOOST_TEST_REQUIRE(s1, "failed to create ZMQ send socket");

    void *s2 = zmq::createSocket(ctx, ZMQ_PAIR);
    BOOST_TEST_REQUIRE(s2, "failed to create ZMQ recv socket");

    const bool ok1 = zmq::connectSocket(s1, "inproc://transfer");
    BOOST_TEST_REQUIRE(ok1, "failed to connect ZMQ socket");

    const bool ok2 = zmq::bindSocket(s2, "inproc://transfer", 10);
    BOOST_TEST_REQUIRE(ok2, "failed to bind ZMQ socket");

    const int sz = getPartSize<T>(part);

    const int rc1 = zmq::sendMultipartMessage<T>(s1, 0, part);
    BOOST_TEST(rc1 == sz);

    T value;
    const int rc2 = zmq::recvMultipartMessage<T>(s2, 0, &value);
    BOOST_TEST(rc2 == sz);
    BOOST_TEST(value == part);

    BOOST_TEST(zmq::closeSocket(s1));
    BOOST_TEST(zmq::closeSocket(s2));
    BOOST_TEST(zmq::deleteContext(ctx));
}

BOOST_DATA_TEST_CASE(transferMultipart, bdata::make({
    TVal("uint8_t",     0u),
    TVal("uint8_t",     255u),
    TVal("uint16_t",    0u),
    TVal("uint16_t",    61689u),
    TVal("uint16_t",    65535u),
    TVal("uint32_t",    0ul),
    TVal("uint32_t",    4278583165ul),
    TVal("uint32_t",    4294967295ul),
    TVal("uint64_t",    0ull),
    TVal("uint64_t",    11460521682733600767ull),
    TVal("uint64_t",    18446744073709551615ull),
    TVal("ByteArray",   ByteArray()),
    TVal("ByteArray",   ByteArray{'a','b','c'}),
    TVal("ByteArray",   ByteArray(2048, 'z')),
}))
{
#define TRANSF_SINGLE(T, field)             \
    if (sample.type == #T) {                \
        const T value = sample.field;       \
        BOOST_TEST(value == sample.field);  \
        testTransferSingle<T>(value);       \
        return;                             \
    }

    TRANSF_SINGLE(uint8_t,   val);
    TRANSF_SINGLE(uint16_t,  val);
    TRANSF_SINGLE(uint32_t,  val);
    TRANSF_SINGLE(uint64_t,  val);
    TRANSF_SINGLE(ByteArray, arr);

    BOOST_FAIL("unsupported type");
}

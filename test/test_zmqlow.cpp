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
#include <benchmark/benchmark.h>

#include "../src/zmqcontext.h"
#include "../src/zmqsocket.h"
#include "../src/zmqmessage.h"
#include "../src/zmqlow.h"

#include <string>
#include <tuple>
#include <memory>
#include <type_traits>


using namespace std::string_literals;

using namespace fuurin::zmq;
namespace utf = boost::unit_test;
namespace bdata = utf::data;


void testMessageIntValue(const Message* m, uint8_t v1, uint16_t v2, uint32_t v4, uint64_t v8)
{
    BOOST_TEST(m->toUint8() == v1);
    BOOST_TEST(m->toUint16() == v2);
    BOOST_TEST(m->toUint32() == v4);
    BOOST_TEST(m->toUint64() == v8);
}


static const uint8_t networkDataBuf[] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA};

typedef boost::mpl::list<uint8_t, uint16_t, uint32_t, uint64_t> messageIntTypes;


BOOST_AUTO_TEST_CASE_TEMPLATE(messageEndianessInt, T, messageIntTypes)
{
    T val = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        val |= uint64_t(networkDataBuf[i]) << (8 * i);
    }

    const Message m(val);

    BOOST_TEST(m.size() == sizeof(T));

    for (size_t i = 0; i < sizeof(T); ++i) {
#if defined(FUURIN_ENDIANESS_BIG)
        BOOST_TEST(m.data()[i] == char(networkDataBuf[sizeof(T) - i - 1]));
#elif defined(FUURIN_ENDIANESS_LITTLE)
        BOOST_TEST(m.data()[i] == char(networkDataBuf[i]));
#else
        BOOST_FAIL("network endianess not defined!");
#endif
    }

    switch (sizeof(T)) {
    case 1:
        testMessageIntValue(&m, val, 0, 0, 0);
        break;
    case 2:
        testMessageIntValue(&m, 0, val, 0, 0);
        break;
    case 4:
        testMessageIntValue(&m, 0, 0, val, 0);
        break;
    case 8:
        testMessageIntValue(&m, 0, 0, 0, val);
        break;
    default:
        BOOST_FAIL("Message data type not supported!");
    }
}


BOOST_AUTO_TEST_CASE(messageEndianessArray)
{
    std::string val(networkDataBuf, networkDataBuf + sizeof(networkDataBuf));

    const Message m(val.data(), val.size());

    BOOST_TEST(m.size() == val.size());
    BOOST_TEST(std::memcmp(m.data(), &val[0], sizeof(networkDataBuf)) == 0);

    testMessageIntValue(&m, 0, 0, 0, 0);
}


BOOST_AUTO_TEST_CASE(transferZMQSimple)
{
    int rc;

    auto ctx = zmq_ctx_new();
    BOOST_TEST(ctx != nullptr);

    auto s1 = zmq_socket(ctx, ZMQ_PAIR);
    BOOST_TEST(s1 != nullptr);

    auto s2 = zmq_socket(ctx, ZMQ_PAIR);
    BOOST_TEST(s2 != nullptr);

    rc = zmq_connect(s1, "inproc://transfer");
    BOOST_TEST(rc == 0);

    rc = zmq_bind(s2, "inproc://transfer");
    BOOST_TEST(rc == 0);

    const uint64_t sdata = 11460521682733600767ull;
    const size_t size = sizeof(sdata);

    zmq_msg_t msg1;
    rc = zmq_msg_init_size(&msg1, size);
    std::memcpy(zmq_msg_data(&msg1), &sdata, size);
    BOOST_TEST(rc == 0);

    rc = zmq_msg_send(&msg1, s1, 0);
    BOOST_TEST(rc == int(size));

    rc = zmq_msg_close(&msg1);
    BOOST_TEST(rc == 0);

    zmq_msg_t msg2;
    rc = zmq_msg_init(&msg2);
    BOOST_TEST(rc == 0);

    rc = zmq_msg_recv(&msg2, s2, 0);
    BOOST_TEST(rc == int(size));
    BOOST_TEST(zmq_msg_size(&msg2) == size);

    uint64_t rdata;
    std::memcpy(&rdata, zmq_msg_data(&msg2), size);
    BOOST_TEST(sdata == rdata);

    rc = zmq_msg_close(&msg2);
    BOOST_TEST(rc == 0);

    rc = zmq_close(s1);
    BOOST_TEST(rc == 0);

    rc = zmq_close(s2);
    BOOST_TEST(rc == 0);

    rc = zmq_ctx_term(ctx);
    BOOST_TEST(rc == 0);
}


struct TVal
{
    std::string type;
    uint64_t val;
    std::string str;

    TVal(const std::string& t, uint64_t v, const std::string& s)
        : type(t)
        , val(v)
        , str(s)
    {
    }

    TVal(const std::string& t, uint64_t v)
        : TVal(t, v, {})
    {
    }

    TVal(const std::string& t, const std::string& v)
        : TVal(t, 0, v)
    {
    }
};

std::ostream& operator<<(std::ostream& os, const TVal& ts)
{
    os << ts.type << " " << ts.val << ts.str;
    return os;
}


std::tuple<std::shared_ptr<Context>, std::shared_ptr<Socket>, std::shared_ptr<Socket>>
transferSetup(int type1, int type2)
{
    auto ctx = std::make_shared<Context>();
    auto s1 = std::make_shared<Socket>(ctx.get(), type1);
    auto s2 = std::make_shared<Socket>(ctx.get(), type2);

    s1->setEndpoints({"inproc://transfer"});
    s2->setEndpoints({"inproc://transfer"});

    s1->connect();
    s2->bind(); // TODO: pass timeout 10000

    return std::make_tuple(ctx, s1, s2);
}

void transferTeardown(std::shared_ptr<Context> ctx, std::shared_ptr<Socket> s1, std::shared_ptr<Socket> s2)
{
    s1->close();
    s2->close();
    UNUSED(ctx);
}

template<typename T>
void testTransferSingle(const T& part)
{
    const auto [ctx, s1, s2] = transferSetup(ZMQ_PAIR, ZMQ_PAIR);

    int sz;
    const char* data;
    Message m1;

    if constexpr (std::is_integral<T>::value) {
        sz = sizeof(part);
        data = (char*)&part;
        m1.move(Message(part));
    } else {
        sz = part.size();
        data = part.data();
        m1.move(Message(part.data(), part.size()));
    }

    const int rc1 = s1->sendMessage(m1);
    BOOST_TEST(rc1 == sz);

    Message m2;
    const int rc2 = s2->recvMessage(&m2);
    BOOST_TEST(rc2 == sz);

    BOOST_TEST(std::memcmp(m2.data(), data, sz) == 0);

    transferTeardown(ctx, s1, s2);
}

BOOST_DATA_TEST_CASE(transferSinglePart,
    bdata::make({
        TVal{"uint8_t", 0u},
        TVal{"uint8_t", 255u},
        TVal{"uint16_t", 0u},
        TVal{"uint16_t", 61689u},
        TVal{"uint16_t", 65535u},
        TVal{"uint32_t", 0ul},
        TVal{"uint32_t", 4278583165ul},
        TVal{"uint32_t", 4294967295ul},
        TVal{"uint64_t", 0ull},
        TVal{"uint64_t", 11460521682733600767ull},
        TVal{"uint64_t", 18446744073709551615ull},
        TVal{"std::string", std::string()},
        TVal{"std::string", std::string{"汉字漢字唐字"}},
        TVal{"std::string", std::string(2048, 'y')},
    }))
{
#define TRANSF_SINGLE(T, field) \
    if (sample.type == #T) { \
        const T value = sample.field; \
        BOOST_TEST(value == sample.field); \
        testTransferSingle<T>(value); \
        return; \
    }
    TRANSF_SINGLE(uint8_t, val);
    TRANSF_SINGLE(uint16_t, val);
    TRANSF_SINGLE(uint32_t, val);
    TRANSF_SINGLE(uint64_t, val);
    TRANSF_SINGLE(std::string, str);

    BOOST_FAIL("unsupported type");
}


//BOOST_AUTO_TEST_CASE(transferMultiPart)
//{
//    const auto [ctx, s1, s2] = transferSetup(ZMQ_PAIR, ZMQ_PAIR);
//
//    uint8_t send_p1{255u};
//    uint16_t send_p2{65535u};
//    uint32_t send_p3{4294967295ul};
//    uint64_t send_p4{18446744073709551615ull};
//    ByteArray send_p5{'a', 'b', 'c'};
//    ByteArray send_p6;
//
//    const int sz = sizeof(send_p1) + sizeof(send_p2) + sizeof(send_p3) + sizeof(send_p4) +
//        send_p5.size() + send_p6.size();
//
//    const int rc1 =
//        zmq::sendMultipartMessage(s1, 0, send_p1, send_p2, send_p3, send_p4, send_p5, send_p6);
//
//    BOOST_TEST(rc1 == sz);
//
//    uint8_t recv_p1;
//    uint16_t recv_p2;
//    uint32_t recv_p3;
//    uint64_t recv_p4;
//    ByteArray recv_p5;
//    ByteArray recv_p6;
//
//    const int rc2 = zmq::recvMultipartMessage(
//        s2, 0, &recv_p1, &recv_p2, &recv_p3, &recv_p4, &recv_p5, &recv_p6);
//
//    BOOST_TEST(rc2 == sz);
//
//    BOOST_TEST(send_p1 == recv_p1);
//    BOOST_TEST(send_p2 == recv_p2);
//    BOOST_TEST(send_p3 == recv_p3);
//    BOOST_TEST(send_p4 == recv_p4);
//    BOOST_TEST(send_p5 == recv_p5);
//    BOOST_TEST(send_p6 == recv_p6);
//
//    transferTeardown(ctx, s1, s2);
//}


//void testWaitForEvents(void* socket, short event, int timeout, bool expected)
//{
//    zmq_pollitem_t items[] = {
//        {socket, 0, event, 0},
//    };
//
//    BOOST_TEST(zmq::pollSocket(items, 1, timeout) == true);
//    BOOST_TEST(!!(items[0].revents & event) == !!expected);
//}
//
//
//BOOST_DATA_TEST_CASE(waitForEvents,
//    bdata::make({
//        std::make_tuple('w', ZMQ_POLLOUT, 250, true),
//        std::make_tuple('w', ZMQ_POLLIN, 250, false),
//        std::make_tuple('r', ZMQ_POLLOUT, 250, true),
//        std::make_tuple('r', ZMQ_POLLIN, 2500, true),
//    }),
//    type, event, timeout, expected)
//{
//    const auto [ctx, s1, s2] = transferSetup(ZMQ_PAIR, ZMQ_PAIR);
//    uint32_t data = 0;
//
//    if (type == 'w')
//        testWaitForEvents(s1, event, timeout, expected);
//
//    const int rc1 = zmq::sendMultipartMessage(s1, 0, data);
//    BOOST_TEST(rc1 != -1);
//
//    if (type == 'r')
//        testWaitForEvents(s2, event, timeout, expected);
//
//    const int rc2 = zmq::recvMultipartMessage(s2, 0, &data);
//    BOOST_TEST(rc2 != -1);
//
//    transferTeardown(ctx, s1, s2);
//}
//
//
//BOOST_DATA_TEST_CASE(publishMessage,
//    bdata::make({
//        std::make_tuple("filt1"s, "filt1"s, 100, 50, true),
//        std::make_tuple("filt1"s, ""s, 100, 50, true),
//        std::make_tuple(""s, ""s, 100, 50, true),
//        std::make_tuple(""s, "filt2"s, 100, 10, false),
//        std::make_tuple("filt1"s, "filt2"s, 100, 10, false),
//    }),
//    pubFilt, subFilt, timeout, count, expected)
//{
//    const auto [ctx, s1, s2] = transferSetup(ZMQ_PUB, ZMQ_SUB);
//
//    BOOST_TEST(zmq::setSocketSubscription(s2, subFilt) == true);
//
//    int retry = 0, rc1;
//    bool ready, p2;
//
//    do {
//        rc1 = zmq::sendMultipartMessage(s1, 0, pubFilt);
//        BOOST_TEST(rc1 != -1);
//
//        zmq_pollitem_t items[] = {
//            {s2, 0, ZMQ_POLLIN, 0},
//        };
//
//        p2 = zmq::pollSocket(items, 1, timeout);
//        BOOST_TEST(p2 == true);
//
//        ready = items[0].revents & ZMQ_POLLIN;
//        ++retry;
//    } while (!ready && retry < count && rc1 != -1 && p2 == true);
//
//    BOOST_TEST(ready == expected);
//
//    if (ready) {
//        std::string recvFilt;
//
//        const int rc2 = zmq::recvMultipartMessage(s2, 0, &recvFilt);
//        BOOST_TEST(rc2 != -1);
//
//        if (!subFilt.empty())
//            BOOST_TEST(recvFilt == subFilt);
//    }
//
//    transferTeardown(ctx, s1, s2);
//}


static void BM_TransferSinglePartSmall(benchmark::State& state)
{
    const auto [ctx, s1, s2] = transferSetup(ZMQ_PAIR, ZMQ_PAIR);
    const auto part = uint64_t(11460521682733600767ull);

    for (auto _ : state) {
        Message m1(part);
        s1->sendMessage(m1);

        Message m2;
        s2->recvMessage(&m2);
    }
    transferTeardown(ctx, s1, s2);
}
BENCHMARK(BM_TransferSinglePartSmall);


static void BM_TransferSinglePartBig(benchmark::State& state)
{
    const auto [ctx, s1, s2] = transferSetup(ZMQ_PAIR, ZMQ_PAIR);
    const auto part = std::string(2048, 'y');

    for (auto _ : state) {
        Message m1(part.data(), part.size());
        s1->sendMessage(m1);

        Message m2;
        s2->recvMessage(&m2);
    }
    transferTeardown(ctx, s1, s2);
}
BENCHMARK(BM_TransferSinglePartBig);


static void BM_TransferMultiPart(benchmark::State& state)
{
    const auto [ctx, s1, s2] = transferSetup(ZMQ_PAIR, ZMQ_PAIR);
    const auto part01 = uint8_t{0u};
    const auto part02 = uint8_t{255u};
    const auto part03 = uint16_t{0u};
    const auto part04 = uint16_t{61689u};
    const auto part05 = uint16_t{65535u};
    const auto part06 = uint32_t{0ul};
    const auto part07 = uint32_t{4278583165ul};
    const auto part08 = uint32_t{4294967295ul};
    const auto part09 = uint64_t{0ull};
    const auto part10 = uint64_t{11460521682733600767ull};
    const auto part11 = uint64_t{18446744073709551615ull};
    const auto part12 = std::string();
    const auto part13 = std::string{"汉字漢字唐字"};
    const auto part14 = std::string(2048, 'y');

    for (auto _ : state) {
        Message s01(part01);
        Message s02(part02);
        Message s03(part03);
        Message s04(part04);
        Message s05(part05);
        Message s06(part06);
        Message s07(part07);
        Message s08(part08);
        Message s09(part09);
        Message s10(part10);
        Message s11(part11);
        Message s12(part12.data(), part12.size());
        Message s13(part13.data(), part13.size());
        Message s14(part14.data(), part14.size());
        s1->sendMessage(s01, s02, s03, s04, s05, s06, s07,
            s08, s09, s10, s11, s12, s13, s14);

        Message r01;
        Message r02;
        Message r03;
        Message r04;
        Message r05;
        Message r06;
        Message r07;
        Message r08;
        Message r09;
        Message r10;
        Message r11;
        Message r12;
        Message r13;
        Message r14;
        s2->recvMessage(&r01, &r02, &r03, &r04, &r05, &r06, &r07,
            &r08, &r09, &r10, &r11, &r12, &r13, &r14);
    }

    transferTeardown(ctx, s1, s2);
}
BENCHMARK(BM_TransferMultiPart);


BOOST_AUTO_TEST_CASE(bench, *utf::disabled())
{
    ::benchmark::RunSpecifiedBenchmarks();
}

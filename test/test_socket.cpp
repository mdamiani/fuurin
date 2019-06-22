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
#include "../src/zmqpart.h"
#include "../src/zmqsocket.h"
#include "../src/zmqpoller.h"

#include <zmq.h>

#include <string>
#include <tuple>
#include <memory>
#include <type_traits>
#include <sstream>


using namespace std::literals;

using namespace fuurin::zmq;
namespace utf = boost::unit_test;
namespace bdata = utf::data;


void testMessageIntValue(const Part* m, uint8_t v1, uint16_t v2, uint32_t v4, uint64_t v8)
{
    BOOST_TEST(m->toUint8() == v1);
    BOOST_TEST(m->toUint16() == v2);
    BOOST_TEST(m->toUint32() == v4);
    BOOST_TEST(m->toUint64() == v8);
}


static const uint8_t networkDataBuf[] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA};

typedef boost::mpl::list<uint8_t, uint16_t, uint32_t, uint64_t> messageIntTypes;


BOOST_AUTO_TEST_CASE_TEMPLATE(partEndianessInt, T, messageIntTypes)
{
    T val = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        val |= uint64_t(networkDataBuf[i]) << (8 * i);
    }

    const Part m(val);

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
        BOOST_FAIL("Part data type not supported!");
    }
}


BOOST_AUTO_TEST_CASE(partEndianessArray)
{
    std::string val(networkDataBuf, networkDataBuf + sizeof(networkDataBuf));

    const Part m(val.data(), val.size());

    BOOST_TEST(m.size() == val.size());
    BOOST_TEST(std::memcmp(m.data(), &val[0], sizeof(networkDataBuf)) == 0);
    BOOST_TEST(m.toString() == val);

    testMessageIntValue(&m, 0, 0, 0, 0);
}


BOOST_AUTO_TEST_CASE(partOstream)
{
    Part m{"ijk"sv};
    std::ostringstream os;
    os << m;

    BOOST_TEST(os.str() == "696A6B");
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


struct sock_setup_t
{
    std::shared_ptr<Context> ctx;
    std::shared_ptr<Socket> s1;
    std::shared_ptr<Socket> s2;
};


sock_setup_t transferSetup(Socket::Type type1, Socket::Type type2)
{
    auto ctx = std::make_shared<Context>();
    auto s1 = std::make_shared<Socket>(ctx.get(), type1);
    auto s2 = std::make_shared<Socket>(ctx.get(), type2);

    s1->setEndpoints({"inproc://transfer"});
    s2->setEndpoints({"inproc://transfer"});

    s1->bind();
    s2->connect(); // TODO: pass timeout 10000

    BOOST_TEST(s1->isOpen());
    BOOST_TEST(s2->isOpen());

    return {ctx, s1, s2};
}

void transferTeardown(sock_setup_t ss)
{
    ss.s1->close();
    ss.s2->close();

    BOOST_TEST(!ss.s1->isOpen());
    BOOST_TEST(!ss.s2->isOpen());
}

template<typename T>
void testTransferSingle(const T& part)
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PAIR, Socket::Type::PAIR);

    int sz;
    const char* data;
    Part m1;

    if constexpr (std::is_integral<T>::value) {
        sz = sizeof(part);
        data = (char*)&part;
        m1.move(Part(part));
    } else {
        sz = part.size();
        data = part.data();
        m1.move(Part(part.data(), part.size()));
    }

    const int rc1 = s1->sendMessage(m1);
    BOOST_TEST(rc1 == sz);

    Part m2;
    const int rc2 = s2->recvMessage(&m2);
    BOOST_TEST(rc2 == sz);

    BOOST_TEST(std::memcmp(m2.data(), data, sz) == 0);

    transferTeardown({ctx, s1, s2});
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


BOOST_AUTO_TEST_CASE(transferMultiPart)
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PAIR, Socket::Type::PAIR);

    const auto v1 = uint8_t(255u);
    const auto v2 = uint16_t(65535u);
    const auto v3 = uint32_t(4294967295ul);
    const auto v4 = uint64_t(18446744073709551615ull);
    const auto v5 = "ijk"sv;
    const auto v7 = "last"sv;

    Part p1{v1}, p2{v2}, p3{v3}, p4{v4}, p5{v5}, p6{}, p7{v7};
    const int sz = p1.size() + p2.size() + p3.size() + p4.size() + p5.size() + p6.size() + p7.size();
    const int np = s1->sendMessage(p1, p2, p3, p4, p5, p6, Part().copy(p7));

    BOOST_TEST(np == sz);

    BOOST_TEST(p1.empty());
    BOOST_TEST(p2.empty());
    BOOST_TEST(p3.empty());
    BOOST_TEST(p4.empty());
    BOOST_TEST(p5.empty());
    BOOST_TEST(p6.empty());
    BOOST_TEST(!p7.empty());

    Part r1, r2, r3, r4, r5, r6, r7;
    const int nr = s2->recvMessage(&r1, &r2, &r3, &r4, &r5, &r6, &r7);

    BOOST_TEST(nr == sz);

    Part t1{v1}, t2{v2}, t3{v3}, t4{v4}, t5{v5}, t6{}, t7{v7};

    BOOST_TEST(t1 == r1);
    BOOST_TEST(t2 == r2);
    BOOST_TEST(t3 == r3);
    BOOST_TEST(t4 == r4);
    BOOST_TEST(t5 == r5);
    BOOST_TEST(t6 == r6);
    BOOST_TEST(t7 == r7);

    transferTeardown({ctx, s1, s2});
}


void testWaitForEvents(Socket* socket, PollerEvents::Type type, int timeout, bool expected)
{
    Poller poll(type, socket);
    poll.setTimeout(timeout);

    const auto events = poll.wait();

    BOOST_TEST(poll.type() == type);
    BOOST_TEST(events.type() == type);
    BOOST_TEST(events.size() == size_t(!!expected));
    BOOST_TEST(events.empty() == !expected);

    if (!events.empty())
        BOOST_TEST(events[0] == socket);

    size_t i = 0;
    for (auto it : events) {
        ++i;
        BOOST_TEST(it == socket);
    }
    BOOST_TEST(i == events.size());
}


BOOST_DATA_TEST_CASE(waitForEventOne,
    bdata::make({
        std::make_tuple('w', PollerEvents::Type::Write, 250, true),
        std::make_tuple('w', PollerEvents::Type::Read, 250, false),
        std::make_tuple('r', PollerEvents::Type::Write, 250, true),
        std::make_tuple('r', PollerEvents::Type::Read, 2500, true),
    }),
    type, event, timeout, expected)
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PAIR, Socket::Type::PAIR);
    Part data(uint32_t(0));

    if (type == 'w')
        testWaitForEvents(s1.get(), event, timeout, expected);

    const int ns = s1->sendMessage(data);
    BOOST_TEST(ns == int(sizeof(uint32_t)));

    if (type == 'r')
        testWaitForEvents(s2.get(), event, timeout, expected);

    const int nr = s2->recvMessage(&data);
    BOOST_TEST(nr == int(sizeof(uint32_t)));

    transferTeardown({ctx, s1, s2});
}


BOOST_AUTO_TEST_CASE(waitForEventMore)
{
    auto ctx = std::make_shared<Context>();
    auto s1 = std::make_shared<Socket>(ctx.get(), Socket::Type::PAIR);
    auto s2 = std::make_shared<Socket>(ctx.get(), Socket::Type::PAIR);
    auto s3 = std::make_shared<Socket>(ctx.get(), Socket::Type::PAIR);
    auto s4 = std::make_shared<Socket>(ctx.get(), Socket::Type::PAIR);

    s1->setEndpoints({"inproc://transfer1"});
    s2->setEndpoints({"inproc://transfer1"});
    s3->setEndpoints({"inproc://transfer2"});
    s4->setEndpoints({"inproc://transfer2"});

    s1->connect();
    s3->connect();
    s2->bind();
    s4->bind();

    BOOST_TEST(s1->isOpen());
    BOOST_TEST(s2->isOpen());
    BOOST_TEST(s3->isOpen());
    BOOST_TEST(s4->isOpen());

    s1->sendMessage(Part(uint8_t(1)));
    s3->sendMessage(Part(uint8_t(2)));

    Poller poll(PollerEvents::Type::Read, s2.get(), s4.get());
    poll.setTimeout(2500);

    const auto events = poll.wait();

    BOOST_TEST(poll.type() == PollerEvents::Type::Read);
    BOOST_TEST(events.type() == PollerEvents::Type::Read);
    BOOST_TEST(events.size() == size_t(2));

    BOOST_TEST(((events[0] == s2.get() && events[1] == s4.get()) ||
        (events[0] == s4.get() && events[1] == s2.get())));

    size_t i = 0;
    for (auto it : events) {
        BOOST_TEST(it == events[i]);
        ++i;
    }
    BOOST_TEST(i == events.size());

    s1->close();
    s2->close();
    s3->close();
    s4->close();

    BOOST_TEST(!s1->isOpen());
    BOOST_TEST(!s2->isOpen());
    BOOST_TEST(!s3->isOpen());
    BOOST_TEST(!s4->isOpen());
}


BOOST_DATA_TEST_CASE(publishMessage,
    bdata::make({
        std::make_tuple("filt1"s, "filt1"s, 100, 50, true),
        std::make_tuple("filt1"s, ""s, 100, 50, true),
        std::make_tuple(""s, ""s, 100, 50, true),
        std::make_tuple(""s, "filt2"s, 100, 10, false),
        std::make_tuple("filt1"s, "filt2"s, 100, 10, false),
    }),
    pubFilt, subFilt, timeout, count, expected)
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PUB, Socket::Type::SUB);

    s2->setSubscriptions({subFilt});
    s2->close();
    s2->connect();

    Poller poll(PollerEvents::Type::Read, s2.get());
    poll.setTimeout(timeout);

    int retry = 0;
    bool ready = false;

    do {
        const int ns = s1->sendMessage(Part(pubFilt));
        BOOST_TEST(ns == int(pubFilt.size()));

        ready = !poll.wait().empty();
        ++retry;
    } while (!ready && retry < count);

    BOOST_TEST(ready == expected);

    if (ready) {
        Part recvFilt;
        const int nr = s2->recvMessage(&recvFilt);
        BOOST_TEST(nr == int(pubFilt.size()));

        if (!subFilt.empty())
            BOOST_TEST(recvFilt.toString() == subFilt);
    }

    transferTeardown({ctx, s1, s2});
}


static void BM_TransferSinglePartSmall(benchmark::State& state)
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PAIR, Socket::Type::PAIR);
    const auto part = uint64_t(11460521682733600767ull);

    for (auto _ : state) {
        Part m1(part);
        s1->sendMessage(m1);

        Part m2;
        s2->recvMessage(&m2);
    }
    transferTeardown({ctx, s1, s2});
}
BENCHMARK(BM_TransferSinglePartSmall);


static void BM_TransferSinglePartBig(benchmark::State& state)
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PAIR, Socket::Type::PAIR);
    const auto part = std::string(2048, 'y');

    for (auto _ : state) {
        Part m1(part.data(), part.size());
        s1->sendMessage(m1);

        Part m2;
        s2->recvMessage(&m2);
    }
    transferTeardown({ctx, s1, s2});
}
BENCHMARK(BM_TransferSinglePartBig);


static void BM_TransferMultiPart(benchmark::State& state)
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PAIR, Socket::Type::PAIR);

    const auto v1 = uint8_t{0u};
    const auto v2 = uint8_t{255u};
    const auto v3 = uint16_t{0u};
    const auto v4 = uint16_t{61689u};
    const auto v5 = uint16_t{65535u};
    const auto v6 = uint32_t{0ul};
    const auto v7 = uint32_t{4278583165ul};
    const auto v8 = uint32_t{4294967295ul};
    const auto v9 = uint64_t{0ull};
    const auto v10 = uint64_t{11460521682733600767ull};
    const auto v11 = uint64_t{18446744073709551615ull};
    const auto v12 = std::string();
    const auto v13 = std::string{"汉字漢字唐字"};
    const auto v14 = std::string(2048, 'y');

    for (auto _ : state) {
        Part p1(v1), p2(v2), p3(v3), p4(v4), p5(v5), p6(v6), p7(v7), p8(v8), p9(v9),
            p10(v10), p11(v11), p12(v12), p13(v13), p14(v14);
        s1->sendMessage(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);

        Part r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14;
        s2->recvMessage(&r1, &r2, &r3, &r4, &r5, &r6, &r7,
            &r8, &r9, &r10, &r11, &r12, &r13, &r14);
    }

    transferTeardown({ctx, s1, s2});
}
BENCHMARK(BM_TransferMultiPart);


BOOST_AUTO_TEST_CASE(bench, *utf::disabled())
{
    ::benchmark::RunSpecifiedBenchmarks();
}

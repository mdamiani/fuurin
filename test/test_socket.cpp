/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE socket
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/mpl/list.hpp>
#include <benchmark/benchmark.h>

#include "fuurin/zmqcontext.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"

#include <zmq.h>

#include <string>
#include <tuple>
#include <memory>
#include <type_traits>
#include <sstream>
#include <limits>
#include <thread>


namespace std {
namespace chrono {
inline std::ostream& operator<<(std::ostream& os, const std::chrono::milliseconds& millis)
{
    os << millis.count();
    return os;
}
} // namespace chrono

inline std::ostream& operator<<(std::ostream& os, const std::tuple<int, int>& t)
{
    os << "<" << std::get<0>(t) << ", " << std::get<1>(t) << ">";
    return os;
}
} // namespace std


using namespace std::literals;
using namespace std::chrono_literals;

using namespace fuurin::zmq;
namespace utf = boost::unit_test;
namespace bdata = utf::data;


BOOST_AUTO_TEST_CASE(partCreateCopy)
{
    Part p1{uint64_t(12345)};
    Part p2{p1};

    BOOST_TEST(!p1.empty());
    BOOST_TEST(!p2.empty());
    BOOST_TEST(p1 == p2);
    BOOST_TEST(!(p1 != p2));
}


BOOST_AUTO_TEST_CASE(partCreateMove)
{
    Part p1{Part(uint64_t(12345))};
    BOOST_TEST(!p1.empty());
    BOOST_TEST(p1.toUint64() == 12345ull);

    Part p2;
    p2.move(p1);
    BOOST_TEST(p1.empty());
    BOOST_TEST(!p2.empty());
    BOOST_TEST(p2.toUint64() == 12345ull);
}


BOOST_AUTO_TEST_CASE(partCopyShare)
{
    Part p1{Part(std::string(4096, 'a'))};
    Part p2;
    p2.share(p1);
    Part p3{p2};

    BOOST_TEST(!p1.empty());
    BOOST_TEST(!p2.empty());
    BOOST_TEST(!p3.empty());
    BOOST_TEST(p1 == p2);
    BOOST_TEST(!(p1 != p2));
    BOOST_TEST(p1 == p3);
    BOOST_TEST(!(p1 != p3));
}


BOOST_AUTO_TEST_CASE(partAssignment)
{
    Part p1{Part(std::string(4096, 'a'))};
    Part p2{Part(uint64_t(12345))};

    BOOST_TEST(!p1.empty());
    BOOST_TEST(!p2.empty());
    BOOST_TEST(!(p1 == p2));
    BOOST_TEST(p1 != p2);

    p2 = p1;

    BOOST_TEST(!p1.empty());
    BOOST_TEST(!p2.empty());
    BOOST_TEST(p1 == p2);
    BOOST_TEST(!(p1 != p2));
}


void testPartIntValue(const Part* m, uint8_t v1, uint16_t v2, uint32_t v4, uint64_t v8)
{
    BOOST_TEST(m->toUint8() == v1);
    BOOST_TEST(m->toUint16() == v2);
    BOOST_TEST(m->toUint32() == v4);
    BOOST_TEST(m->toUint64() == v8);
}


static const uint8_t networkDataBuf[] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA};

typedef boost::mpl::list<uint8_t, uint16_t, uint32_t, uint64_t> partIntTypes;


BOOST_AUTO_TEST_CASE_TEMPLATE(partEndianessInt, T, partIntTypes)
{
    T val = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        val |= uint64_t(networkDataBuf[i]) << (8 * i);
    }

    const Part m{val};

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
        testPartIntValue(&m, val, 0, 0, 0);
        break;
    case 2:
        testPartIntValue(&m, 0, val, 0, 0);
        break;
    case 4:
        testPartIntValue(&m, 0, 0, val, 0);
        break;
    case 8:
        testPartIntValue(&m, 0, 0, 0, val);
        break;
    default:
        BOOST_FAIL("Part data type not supported!");
    }
}


BOOST_AUTO_TEST_CASE(partEndianessArray)
{
    std::string val{networkDataBuf, networkDataBuf + sizeof(networkDataBuf)};

    const Part m{val.data(), val.size()};

    BOOST_TEST(m.size() == val.size());
    BOOST_TEST(std::memcmp(m.data(), &val[0], sizeof(networkDataBuf)) == 0);
    BOOST_TEST(m.toString() == val);

    testPartIntValue(&m, 0, 0, 0, 0);
}


BOOST_AUTO_TEST_CASE(partOstream)
{
    Part m{"ijk"sv};
    std::ostringstream os;
    os << m;

    BOOST_TEST(os.str() == "696A6B");
}


BOOST_AUTO_TEST_CASE(partMultiEmpty)
{
    Part a = PartMulti::pack<>();
    std::tuple<> t = PartMulti::unpack<>(a);

    BOOST_TEST(a.size() == size_t(0));
    BOOST_TEST(std::tuple_size_v<decltype(t)> == size_t(0));
    BOOST_TEST(a == Part());
}


BOOST_AUTO_TEST_CASE(partMultiEmptyString)
{
    Part a = PartMulti::pack<std::string>("");
    std::tuple<std::string> t = PartMulti::unpack<std::string>(a);

    BOOST_TEST(a.size() == sizeof(PartMulti::string_length_t));
    BOOST_TEST(std::tuple_size_v<decltype(t)> == size_t(1));
    BOOST_TEST(std::get<0>(t) == std::string());
}


BOOST_AUTO_TEST_CASE_TEMPLATE(partMultiIntType, T, partIntTypes)
{
    T val = T(1) << (sizeof(T) * 8 - 1);
    Part a = PartMulti::pack(val);
    std::tuple<T> t = PartMulti::unpack<T>(a);

    BOOST_TEST(a.size() == sizeof(T));
    BOOST_TEST(std::tuple_size_v<decltype(t)> == size_t(1));
    BOOST_TEST(std::get<0>(t) == val);
}


typedef boost::mpl::list<std::string_view, std::string, Part> partMultiStringTypes;

BOOST_AUTO_TEST_CASE_TEMPLATE(partMultiStringType, T, partMultiStringTypes)
{
    const auto val = "testdata"sv;
    Part a = PartMulti::pack(T{val});
    std::tuple<T> t = PartMulti::unpack<T>(a);

    BOOST_TEST(a.size() == val.size() + sizeof(PartMulti::string_length_t));
    BOOST_TEST(std::tuple_size_v<decltype(t)> == size_t(1));
    BOOST_TEST(std::get<0>(t).size() == val.size());
    BOOST_TEST(std::memcmp(std::get<0>(t).data(), val.data(), val.size()) == 0);
}


typedef boost::mpl::list<char, unsigned char> partMultiArrayItemTypes;

BOOST_AUTO_TEST_CASE_TEMPLATE(partMultiCharArray, T, partMultiArrayItemTypes)
{
    using A = std::array<T, 5>;

    auto mkArr = []() {
        return A{'a', 'b', 'c', 'd', 'e'};
    };

    auto val = mkArr();
    Part a = PartMulti::pack(val);
    std::tuple<A> t = PartMulti::unpack<A>(a);

    BOOST_TEST(a.size() == val.size());
    BOOST_TEST(std::tuple_size_v<decltype(t)> == size_t(1));
    BOOST_TEST(std::get<0>(t).size() == val.size());
    BOOST_TEST(std::memcmp(std::get<0>(t).data(), val.data(), val.size()) == 0);

    // pack template variations
    auto val1 = mkArr();
    auto const val2 = mkArr();
    auto& val3 = val1;
    auto const& val4 = mkArr();
    Part a1 = PartMulti::pack(val1);
    Part a2 = PartMulti::pack(val2);
    Part a3 = PartMulti::pack(val3);
    Part a4 = PartMulti::pack(val4);
    Part a5 = PartMulti::pack(mkArr());

    BOOST_TEST(std::memcmp(a1.data(), val.data(), val.size()) == 0);
    BOOST_TEST(std::memcmp(a2.data(), val.data(), val.size()) == 0);
    BOOST_TEST(std::memcmp(a3.data(), val.data(), val.size()) == 0);
    BOOST_TEST(std::memcmp(a4.data(), val.data(), val.size()) == 0);
    BOOST_TEST(std::memcmp(a5.data(), val.data(), val.size()) == 0);
}


BOOST_AUTO_TEST_CASE(partMultiMore)
{
    Part a = PartMulti::pack(uint32_t(3), uint32_t(14),
        "123123"sv, "string"s, Part{uint64_t(12345)});

    std::tuple<uint32_t, uint32_t, std::string_view, std::string, Part> t =
        PartMulti::unpack<uint32_t, uint32_t, std::string_view, std::string, Part>(a);

    BOOST_TEST(a.size() == size_t(40));
    BOOST_TEST(std::tuple_size_v<decltype(t)> == size_t(5));
    BOOST_TEST(std::get<0>(t) == uint32_t(3));
    BOOST_TEST(std::get<1>(t) == uint32_t(14));
    BOOST_TEST(std::get<2>(t) == "123123"sv);
    BOOST_TEST(std::get<3>(t) == "string"s);
    BOOST_TEST(std::get<4>(t).toUint64() == uint64_t(12345));
}


BOOST_AUTO_TEST_CASE(partMultiRecursive)
{
    Part a = PartMulti::pack(10u, 20u);
    Part b = PartMulti::pack(30u, a);

    BOOST_TEST(a.size() == sizeof(unsigned) * 2);
    BOOST_TEST(b.size() == sizeof(unsigned) + sizeof(PartMulti::string_length_t) + a.size());

    std::tuple<unsigned, Part> tb = PartMulti::unpack<unsigned, Part>(b);

    BOOST_TEST(std::tuple_size_v<decltype(tb)> == size_t(2));
    BOOST_TEST(std::get<0>(tb) == 30u);

    std::tuple<unsigned, unsigned> ta = PartMulti::unpack<unsigned, unsigned>(std::get<1>(tb));

    BOOST_TEST(std::tuple_size_v<decltype(ta)> == size_t(2));
    BOOST_TEST(std::get<0>(ta) == 10u);
    BOOST_TEST(std::get<1>(ta) == 20u);
}


BOOST_AUTO_TEST_CASE(partMultiUnpackIntErr)
{
    Part a = PartMulti::pack<uint16_t>(1);
    BOOST_REQUIRE_THROW(PartMulti::unpack<uint32_t>(a),
        fuurin::err::ZMQPartAccessFailed);
}


BOOST_AUTO_TEST_CASE(partMultiUnpackStringErr)
{
    // size access error.
    Part a = PartMulti::pack<uint16_t>(1);
    BOOST_REQUIRE_THROW(PartMulti::unpack<std::string>(a),
        fuurin::err::ZMQPartAccessFailed);

    // contensts access error.
    Part b = PartMulti::pack<PartMulti::string_length_t>(1);
    BOOST_REQUIRE_THROW(PartMulti::unpack<std::string>(b),
        fuurin::err::ZMQPartAccessFailed);
}


BOOST_AUTO_TEST_CASE(partMultiPackIter)
{
    // Pack same data from a container.
    std::list<std::string> src = {"rosemary", "basil", "pepper"};
    Part a = PartMulti::pack(src.begin(), src.end());
    BOOST_TEST(a.size() == 20u + 19u);

    std::list<std::string> dst1{3};
    PartMulti::unpack(a, dst1.begin());

    std::string dst2[3];
    PartMulti::unpack(a, &dst2[0]);

    std::list<std::string> dst3;
    PartMulti::unpack(a, std::back_inserter(dst3));

    std::list<std::string> dst4;
    PartMulti::unpack(a, std::front_inserter(dst4));
    std::reverse(dst4.begin(), dst4.end());

    std::list<std::string> dst5;
    PartMulti::unpack(a, std::inserter(dst5, dst5.begin()));

    std::list<std::string> dst6;
    PartMulti::unpack<std::string>(a, [&dst6](const std::string& s) {
        dst6.push_back(s);
    });

    BOOST_TEST(src == dst1);
    BOOST_TEST(src == dst2);
    BOOST_TEST(src == dst3);
    BOOST_TEST(src == dst4);
    BOOST_TEST(src == dst5);
    BOOST_TEST(src == dst6);


    // This one should stress the pack template type inference,
    // it must not pick the iterator version.
    Part b = PartMulti::pack<uint8_t, uint8_t>(10u, 20u);
    auto [n1, n2] = PartMulti::unpack<uint8_t, uint8_t>(b);
    BOOST_TEST(n1 == 10u);
    BOOST_TEST(n2 == 20u);


    // Pack an empty container.
    std::list<std::string> empty1;
    Part c = PartMulti::pack(empty1.begin(), empty1.end());
    BOOST_TEST(c.size() == 8u);

    std::list<std::string> empty2;
    PartMulti::unpack(c, std::back_inserter(empty2));
    BOOST_TEST(empty2.empty());

    // Pack with iterator variant and type conversion
    std::list<uint32_t> srcint{1, 2, 0, 4, 0};
    auto d = PartMulti::pack<bool>(srcint.begin(), srcint.end());
    BOOST_TEST(d.size() == size_t(8 + 5));

    std::list<uint32_t> dstint;
    PartMulti::unpack<bool>(d, std::back_inserter(dstint));
    BOOST_TEST((dstint == std::list<uint32_t>{1, 1, 0, 1, 0}));
}


BOOST_AUTO_TEST_CASE(partMultiUnpackFromViewArgs)
{
    const auto val1 = uint16_t{200};
    const auto val2 = "testdata"sv;

    const Part a = PartMulti::pack(val1, val2);
    const std::string s1{a.toString().data(), a.toString().size()};
    const std::string s2{a.toString()};

    const auto testUnpackView = [&](auto s) {
        const auto [unp1, unp2] = PartMulti::unpack<uint16_t, std::string_view>(s);
        BOOST_TEST(val1 == unp1);
        BOOST_TEST(val2 == unp2);
    };

    testUnpackView(s1);
    testUnpackView(s2);
}


BOOST_AUTO_TEST_CASE(partMultiUnpackFromViewIter)
{
    std::list<std::string> lst = {"rosemary", "basil", "pepper"};

    const Part a = PartMulti::pack(lst.begin(), lst.end());
    const std::string s1{a.toString().data(), a.toString().size()};
    const std::string s2{a.toString()};

    const auto testUnpackView = [&](auto s) {
        std::list<std::string> unp;
        PartMulti::unpack(s, std::back_inserter(unp));
        BOOST_TEST(lst == unp);
    };

    testUnpackView(s1);
    testUnpackView(s2);
}


namespace fuurin {
namespace zmq {
class TestPartMulti
{
public:
    template<typename... Args>
    static void pack(Part* pm, Args&&... args)
    {
        PartMulti::pack2(*pm, 0, args...);
    }
};
} // namespace zmq
} // namespace fuurin


BOOST_AUTO_TEST_CASE(partMultiPackIntErr)
{
    Part a = PartMulti::pack<uint16_t>(0);
    BOOST_REQUIRE_THROW(TestPartMulti::pack<uint32_t>(&a, uint32_t(1)),
        fuurin::err::ZMQPartCreateFailed);
}


BOOST_AUTO_TEST_CASE(partMultiPackStringErr)
{
    // size access error.
    Part a = PartMulti::pack<uint16_t>(0);
    BOOST_REQUIRE_THROW(TestPartMulti::pack<std::string>(&a, ""),
        fuurin::err::ZMQPartCreateFailed);
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
        m1.move(Part{part});
    } else {
        sz = part.size();
        data = part.data();
        m1.move(Part{part.data(), part.size()});
    }

    const int rc1 = s1->send(m1);
    BOOST_TEST(rc1 == sz);

    Part m2;
    const int rc2 = s2->recv(&m2);
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
    const int np = s1->send(p1, p2, p3, p4, p5, p6, Part().share(p7));

    BOOST_TEST(np == sz);

    BOOST_TEST(p1.empty());
    BOOST_TEST(p2.empty());
    BOOST_TEST(p3.empty());
    BOOST_TEST(p4.empty());
    BOOST_TEST(p5.empty());
    BOOST_TEST(p6.empty());
    BOOST_TEST(!p7.empty());

    Part r1, r2, r3, r4, r5, r6, r7;
    const int nr = s2->recv(&r1, &r2, &r3, &r4, &r5, &r6, &r7);

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


BOOST_AUTO_TEST_CASE(trySendSinglePart, *utf::timeout(2))
{
    Context ctx;
    Socket s(&ctx, Socket::Type::PUSH);

    s.setEndpoints({"inproc://transfer"});
    s.bind();

    const auto rc = s.trySend(Part{uint32_t(101)});
    BOOST_TEST(rc == -1);
}


BOOST_AUTO_TEST_CASE(trySendMultiPart, *utf::timeout(2))
{
    Context ctx;
    Socket s(&ctx, Socket::Type::PUSH);

    s.setEndpoints({"inproc://transfer"});
    s.bind();

    const auto rc = s.trySend(Part{uint32_t(101)}, Part{uint32_t(102)});
    BOOST_TEST(rc == -1);
}


BOOST_AUTO_TEST_CASE(tryRecvSinglePart, *utf::timeout(2))
{
    Context ctx;
    Socket s(&ctx, Socket::Type::PULL);

    s.setEndpoints({"inproc://transfer"});
    s.bind();

    Part m1;
    const auto rc = s.tryRecv(&m1);
    BOOST_TEST(rc == -1);
}


BOOST_AUTO_TEST_CASE(tryRecvMultiPart, *utf::timeout(2))
{
    Context ctx;
    Socket s(&ctx, Socket::Type::PULL);

    s.setEndpoints({"inproc://transfer"});
    s.bind();

    Part m1, m2;
    const auto rc = s.tryRecv(&m1, &m2);
    BOOST_TEST(rc == -1);
}


BOOST_AUTO_TEST_CASE(tryTransferMultiPart, *utf::timeout(2))
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PUSH, Socket::Type::PULL);

    uint32_t a(101), b(102);

    const size_t nw = s1->trySend(Part{a}, Part{b});
    BOOST_TEST(nw == sizeof(a) + sizeof(b));

    Part m1, m2;
    const size_t nr = s2->tryRecv(&m1, &m2);
    BOOST_TEST(nr == sizeof(a) + sizeof(b));
    BOOST_TEST(m1.toUint32() == a);
    BOOST_TEST(m2.toUint32() == b);

    transferTeardown({ctx, s1, s2});
}


BOOST_AUTO_TEST_CASE(hasMoreParts, *utf::timeout(2))
{
    Context ctx;
    Socket s1{&ctx, Socket::Type::PUSH};
    Socket s2{&ctx, Socket::Type::PULL};

    s1.setEndpoints({"inproc://transfer"});
    s2.setEndpoints({"inproc://transfer"});

    s1.connect();
    s2.bind();

    int n = s1.send(Part{uint8_t(1)}, Part{uint16_t(2)});
    BOOST_REQUIRE(n == 3);

    Part r;
    n = s2.tryRecv(&r);
    BOOST_REQUIRE(n == 1);
    BOOST_REQUIRE(r.toUint8() == 1u);
    BOOST_REQUIRE(r.hasMore());

    n = s2.tryRecv(&r);
    BOOST_REQUIRE(n == 2);
    BOOST_REQUIRE(r.toUint16() == 2u);
    BOOST_REQUIRE(!r.hasMore());
}


void testWaitForEvents(Socket* socket, PollerEvents::Type type, std::chrono::milliseconds timeout, bool expected)
{
    Poller poll{type, timeout, socket};

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
        std::make_tuple('w', PollerEvents::Type::Write, 250ms, true),
        std::make_tuple('w', PollerEvents::Type::Read, 250ms, false),
        std::make_tuple('r', PollerEvents::Type::Write, 250ms, true),
        std::make_tuple('r', PollerEvents::Type::Read, 2500ms, true),
    }),
    type, event, timeout, expected)
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PAIR, Socket::Type::PAIR);
    Part data(uint32_t(0));

    if (type == 'w')
        testWaitForEvents(s1.get(), event, timeout, expected);

    const int ns = s1->send(data);
    BOOST_TEST(ns == int(sizeof(uint32_t)));

    if (type == 'r')
        testWaitForEvents(s2.get(), event, timeout, expected);

    const int nr = s2->recv(&data);
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

    s1->send(Part{uint8_t(1)});
    s3->send(Part{uint8_t(2)});

    Poller poll{PollerEvents::Type::Read, s2.get(), s4.get()};
    poll.setTimeout(2500ms);

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


BOOST_AUTO_TEST_CASE(waitForEventOpen)
{
    auto ctx = std::make_shared<Context>();
    auto s1 = std::make_shared<Socket>(ctx.get(), Socket::Type::PAIR);
    auto s2 = std::make_shared<Socket>(ctx.get(), Socket::Type::PAIR);

    s1->setEndpoints({"inproc://transfer1"});
    s2->setEndpoints({"inproc://transfer1"});

    // Create poller on closed sockets
    auto poll1 = std::shared_ptr<PollerWaiter>(new PollerAuto{PollerEvents::Type::Read, 500ms, s2.get()});
    auto poll2 = std::shared_ptr<PollerWaiter>(new PollerAuto{PollerEvents::Type::Read, 500ms, s2.get()});

    BOOST_TEST(int(s2->pollersCount()) == 2);
    BOOST_TEST(poll1->wait().empty());
    BOOST_TEST(poll2->wait().empty());

    // Connect sockets
    s1->connect();
    s2->bind();

    s1->send(Part{uint8_t(10)});
    BOOST_TEST(!poll1->wait().empty());
    BOOST_TEST(!poll2->wait().empty());

    // Remove one poller
    poll1.reset();
    BOOST_TEST(int(s2->pollersCount()) == 1);
    BOOST_TEST(!poll2->wait().empty());

    // Close sockets
    s1->close();
    s2->close();
    BOOST_TEST(int(s2->pollersCount()) == 1);
    BOOST_TEST(poll2->wait().empty());

    // Reconnect sockets
    s1->connect();
    s2->bind();
    s1->send(Part{uint8_t(10)});
    BOOST_TEST(!poll2->wait().empty());

    s1->close();
    s2->close();

    poll2.reset();
    BOOST_TEST(int(s2->pollersCount()) == 0);
}


BOOST_AUTO_TEST_CASE(pollerOpenSockets)
{
    Context ctx;
    Socket s{&ctx, Socket::Type::PAIR};

    BOOST_REQUIRE_THROW(Poller p(PollerEvents::Type::Read, &s),
        fuurin::err::ZMQPollerAddSocketFailed);
}


BOOST_AUTO_TEST_CASE(pollerAutoOpenWithException)
{
    Context ctx;
    Socket s{&ctx, Socket::Type::PAIR};
    PollerAuto poll{PollerEvents::Type::Read, &s};

    BOOST_REQUIRE_THROW(s.bind(), fuurin::err::ZMQSocketBindFailed);
    BOOST_TEST(!s.isOpen());
    BOOST_TEST(s.pollersCount() == 1u);
}


BOOST_AUTO_TEST_CASE(pollerAutoClosedSocket)
{
    Context ctx;
    Socket s{&ctx, Socket::Type::PAIR};
    PollerAuto poll{PollerEvents::Type::Read, &s};

    s.setEndpoints({"inproc://transfer1"});
    s.bind();
    s.close();
    BOOST_TEST(!s.isOpen());
    BOOST_TEST(s.pollersCount() == 1u);
}


BOOST_AUTO_TEST_CASE(pollerAutoAddUnknownSocket)
{
    Context ctx;
    Socket s{&ctx, Socket::Type::PAIR};
    PollerAuto poll{PollerEvents::Type::Read, &s};

    Socket s2{&ctx, Socket::Type::PAIR};
    BOOST_REQUIRE_THROW(poll.updateOnOpen(&s2), fuurin::err::ZMQPollerAddSocketFailed);
}


BOOST_AUTO_TEST_CASE(pollerAutoDelUnknownSocket)
{
    Context ctx;
    Socket s{&ctx, Socket::Type::PAIR};
    PollerAuto poll{PollerEvents::Type::Read, &s};

    Socket s2{&ctx, Socket::Type::PAIR};
    BOOST_REQUIRE_NO_THROW(poll.updateOnClose(&s2));
}


BOOST_DATA_TEST_CASE(publishMessage,
    bdata::make({
        std::make_tuple("filt1"s, "filt1"s, 100ms, 50, true),
        std::make_tuple("filt1"s, ""s, 100ms, 50, true),
        std::make_tuple(""s, ""s, 100ms, 50, true),
        std::make_tuple(""s, "filt2"s, 100ms, 10, false),
        std::make_tuple("filt1"s, "filt2"s, 100ms, 10, false),
    }),
    pubFilt, subFilt, timeout, count, expected)
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PUB, Socket::Type::SUB);

    s2->setSubscriptions({subFilt});
    s2->close();
    s2->connect();

    Poller poll{PollerEvents::Type::Read, timeout, s2.get()};

    int retry = 0;
    bool ready = false;

    do {
        const int ns = s1->send(Part{pubFilt});
        BOOST_TEST(ns == int(pubFilt.size()));

        ready = !poll.wait().empty();
        ++retry;
    } while (!ready && retry < count);

    BOOST_TEST(ready == expected);

    if (ready) {
        Part recvFilt;
        const int nr = s2->recv(&recvFilt);
        BOOST_TEST(nr == int(pubFilt.size()));

        if (!subFilt.empty())
            BOOST_TEST(recvFilt.toString() == subFilt);
    }

    transferTeardown({ctx, s1, s2});
}


BOOST_AUTO_TEST_CASE(socketProps)
{
    Context ctx;
    Socket s{&ctx, Socket::Type::PAIR};

    BOOST_TEST(s.linger() == 0s);
    BOOST_TEST(s.highWaterMark() == std::make_tuple(0, 0));

    s.setLinger(10s);
    BOOST_TEST(s.linger() == 10s);

    s.setHighWaterMark(1000, 2000);
    BOOST_TEST(s.highWaterMark() == std::make_tuple(1000, 2000));
}


BOOST_AUTO_TEST_CASE(partRoutingID)
{
    Part a;
    BOOST_TEST(a.routingID() == 0u);

    a.setRoutingID(10);
    BOOST_TEST(a.routingID() == 10u);

    BOOST_REQUIRE_THROW(a.setRoutingID(0),
        fuurin::err::ZMQPartRoutingIDFailed);

    BOOST_TEST(a.routingID() == 10u);

    Part b = a;
    Part c;
    c = a;

    BOOST_TEST(b.routingID() == 10u);
    BOOST_TEST(c.routingID() == 10u);
}


BOOST_AUTO_TEST_CASE(socketClientServer)
{
    Context ctx;
    Socket s1{&ctx, Socket::Type::CLIENT};
    Socket s2{&ctx, Socket::Type::SERVER};

    s1.setEndpoints({"inproc://transfer1"});
    s2.setEndpoints({"inproc://transfer1"});

    s1.connect();
    s2.bind();

    s1.send(Part{uint8_t(8)});
    Part x;
    s2.recv(&x);
    BOOST_TEST(x.toUint8() == 8);

    s2.send(Part{"hello"sv}.withRoutingID(x.routingID()));
    Part y;
    s1.recv(&y);
    BOOST_TEST(y.routingID() == 0u);
    BOOST_TEST(y.toString() == "hello"sv);
}


BOOST_AUTO_TEST_CASE(partGroup)
{
    char long_group[ZMQ_GROUP_MAX_LENGTH + 2];
    ::memset(long_group, 'A', sizeof(long_group));
    long_group[sizeof(long_group) - 1] = '\0';

    char empty_group[1];
    empty_group[0] = '\0';

    Part a;
    BOOST_TEST(a.group() != nullptr);
    BOOST_TEST(::strnlen(a.group(), ZMQ_GROUP_MAX_LENGTH) == 0u);

    BOOST_REQUIRE_THROW(a.setGroup(long_group),
        fuurin::err::ZMQPartGroupFailed);

    BOOST_REQUIRE_THROW(a.setGroup(nullptr),
        fuurin::err::ZMQPartGroupFailed);

    BOOST_TEST(::strnlen(a.group(), ZMQ_GROUP_MAX_LENGTH) == 0u);

    a.setGroup("test group");
    BOOST_TEST(::strncmp(a.group(), "test group", ZMQ_GROUP_MAX_LENGTH) == 0);

    Part b = a;
    Part c;
    c = b;
    BOOST_TEST(::strncmp(b.group(), "test group", ZMQ_GROUP_MAX_LENGTH) == 0);
    BOOST_TEST(::strncmp(c.group(), "test group", ZMQ_GROUP_MAX_LENGTH) == 0);

    a.setGroup(empty_group);
    BOOST_TEST(::strnlen(a.group(), ZMQ_GROUP_MAX_LENGTH) == 0u);
}


BOOST_AUTO_TEST_CASE(socketRadioDish)
{
    Context ctx;
    Socket s1{&ctx, Socket::Type::RADIO};
    Socket s2{&ctx, Socket::Type::DISH};

    s1.setEndpoints({"inproc://transfer1"});
    s2.setEndpoints({"inproc://transfer1"});

    s2.setGroups({"Movies", "TV"});

    s1.connect();
    s2.bind();

    std::this_thread::sleep_for(500ms);

    BOOST_TEST(s1.send(Part{"Friends"sv}.withGroup("TV")) == 7);
    BOOST_TEST(s1.send(Part{"Godfather"sv}.withGroup("Movies")) == 9);

    Part r;

    BOOST_TEST(s2.recv(&r) == 7);
    BOOST_TEST(r.toString() == "Friends"s);
    BOOST_TEST("TV"s == r.group());

    BOOST_TEST(s2.recv(&r) == 9);
    BOOST_TEST(r.toString() == "Godfather"s);
    BOOST_TEST("Movies"s == r.group());
}


BOOST_AUTO_TEST_CASE(testHighWaterMarkOption)
{
    Context ctx;
    Socket s1{&ctx, Socket::Type::PUSH};
    Socket s2{&ctx, Socket::Type::PULL};

    s1.setEndpoints({"inproc://transfer1"});
    s2.setEndpoints({"inproc://transfer1"});

    const int hwm = 255;

    s1.setHighWaterMark(hwm, hwm);
    s2.setHighWaterMark(hwm, hwm);

    s1.connect();
    s2.bind();

    // send until high water mark
    for (auto v = 0; v <= hwm; ++v) {
        const int w = s1.trySend(Part{uint8_t(v)});
        BOOST_REQUIRE(w == 1);
    }

    // exceeding message
    bool notSent = false;
    for (auto v = 0; v <= 10 * hwm; ++v) {
        const int w = s1.trySend(Part{uint8_t(v)});
        if (w == -1) {
            notSent = true;
            break;
        }
    }
    BOOST_REQUIRE(notSent);

    Poller poll{PollerEvents::Type::Read, 0s, &s2};

    for (auto v = 0; v <= hwm; ++v) {
        BOOST_REQUIRE(!poll.wait().empty());

        Part r;
        s2.recv(&r);
        BOOST_REQUIRE(r.toUint8() == v);
    }
}


BOOST_AUTO_TEST_CASE(testConflateOption)
{
    Context ctx;
    Socket s1{&ctx, Socket::Type::PUSH};
    Socket s2{&ctx, Socket::Type::PULL};

    s1.setEndpoints({"inproc://transfer1"});
    s2.setEndpoints({"inproc://transfer1"});

    s1.setHighWaterMark(1, 1);
    s2.setHighWaterMark(1, 1);

    s1.setConflate(true);
    s2.setConflate(true);

    s1.connect();
    s2.bind();

    for (auto v = 0; v <= 255; ++v) {
        const int w = s1.trySend(Part{uint8_t(v)});
        BOOST_REQUIRE(w == 1);
    }

    Poller poll{PollerEvents::Type::Read, 0s, &s2};

    BOOST_REQUIRE(!poll.wait().empty());

    Part r;
    s2.recv(&r);
    BOOST_TEST(r.toUint8() == 255);

    BOOST_REQUIRE(poll.wait().empty());
}


BOOST_AUTO_TEST_CASE(testFileDescriptorOption)
{
    Context ctx;
    Socket s{&ctx, Socket::Type::PUSH};

    s.setEndpoints({"inproc://transfer1"});

    BOOST_CHECK_THROW(s.fileDescriptor(), fuurin::err::ZMQSocketOptionGetFailed);

    s.bind();

    BOOST_TEST(s.fileDescriptor() > 0);

    s.close();

    BOOST_CHECK_THROW(s.fileDescriptor(), fuurin::err::ZMQSocketOptionGetFailed);
}


BOOST_AUTO_TEST_CASE(testInprocNoIOThread)
{
    Context ctx;
    Socket s1{&ctx, Socket::Type::PUSH};
    Socket s2{&ctx, Socket::Type::PULL};

    s1.setEndpoints({"inproc://transfer1"});
    s2.setEndpoints({"inproc://transfer1"});

    s1.connect();
    s2.bind();

    Part r;
    BOOST_REQUIRE(s2.tryRecv(&r) == -1);
    BOOST_REQUIRE(s1.trySend(Part{uint64_t(1)}) == sizeof(uint64_t));
    BOOST_REQUIRE(s2.tryRecv(&r) == sizeof(uint64_t));
}


static void BM_TransferSinglePartSmall(benchmark::State& state)
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PAIR, Socket::Type::PAIR);
    const auto part = uint64_t(11460521682733600767ull);

    for (auto _ : state) {
        Part m1{part};
        s1->send(m1);

        Part m2;
        s2->recv(&m2);
    }
    transferTeardown({ctx, s1, s2});
}
BENCHMARK(BM_TransferSinglePartSmall);


static void BM_TransferSinglePartBig(benchmark::State& state)
{
    const auto [ctx, s1, s2] = transferSetup(Socket::Type::PAIR, Socket::Type::PAIR);
    const auto part = std::string(2048, 'y');

    for (auto _ : state) {
        Part m1{part.data(), part.size()};
        s1->send(m1);

        Part m2;
        s2->recv(&m2);
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
        Part p1{v1}, p2{v2}, p3{v3}, p4{v4}, p5{v5}, p6{v6}, p7{v7}, p8{v8}, p9{v9},
            p10{v10}, p11{v11}, p12{v12}, p13{v13}, p14{v14};
        s1->send(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);

        Part r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14;
        s2->recv(&r1, &r2, &r3, &r4, &r5, &r6, &r7,
            &r8, &r9, &r10, &r11, &r12, &r13, &r14);
    }

    transferTeardown({ctx, s1, s2});
}
BENCHMARK(BM_TransferMultiPart);


BOOST_AUTO_TEST_CASE(bench, *utf::disabled())
{
    ::benchmark::RunSpecifiedBenchmarks();
}

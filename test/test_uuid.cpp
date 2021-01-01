/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE uuid
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include "fuurin/uuid.h"
#include "fuurin/zmqpart.h"

#include <algorithm>
#include <stdexcept>


using namespace fuurin;
namespace utf = boost::unit_test;
namespace bdata = utf::data;

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;


BOOST_AUTO_TEST_CASE(testInit)
{
    Uuid u;
    BOOST_TEST(u.size() == size_t(16));
    BOOST_TEST(u.size() == u.bytes().size());
    BOOST_TEST(u.isNull());
    BOOST_TEST(u.toString() == Uuid::NullFmt);
    BOOST_TEST(u.toShortString() == std::string_view(Uuid::NullFmt.data(), 8));
    BOOST_TEST(std::equal(u.bytes().begin(), u.bytes().end(),
        Uuid::Bytes{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}.begin()));
}


BOOST_AUTO_TEST_CASE(testFromStringValid)
{
    const auto& str = "01234567-89ab-cdef-0123-456789abcdef"sv;
    Uuid u = Uuid::fromString(str);
    Uuid::Bytes wantBytes = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, //
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};

    BOOST_TEST(!u.isNull());
    BOOST_TEST(u.bytes() == wantBytes);
    BOOST_TEST(u.toString() == str);
    BOOST_TEST(u.toShortString() == std::string_view(str.data(), 8));
}


BOOST_AUTO_TEST_CASE(testFromStringException)
{
    BOOST_REQUIRE_THROW(Uuid::fromString("{ZZZZ}"sv), std::runtime_error);
}


BOOST_AUTO_TEST_CASE(testRandom)
{
    Uuid u = Uuid::createRandomUuid();
    BOOST_TEST(u.size() == size_t(16));
    BOOST_TEST(!u.isNull());
    BOOST_TEST(u.toString() != Uuid{}.toString());
    BOOST_TEST(u.toShortString() != Uuid{}.toShortString());
    BOOST_TEST(u.bytes() != Uuid{}.bytes());
}


BOOST_AUTO_TEST_CASE(testNamespace)
{
    Uuid u = Uuid::createNamespaceUuid(Uuid::Ns::Dns, "test.com"sv);
    BOOST_TEST(u.size() == size_t(16));
    BOOST_TEST(!u.isNull());
    BOOST_TEST(u.toString() == "1c39b279-6010-55d9-b677-859bffab8081"sv);
    BOOST_TEST(u.toShortString() == "1c39b279"sv);
    Uuid::Bytes wantBytes = {0x1c, 0x39, 0xb2, 0x79, 0x60, 0x10, 0x55, 0xd9, //
        0xb6, 0x77, 0x85, 0x9b, 0xff, 0xab, 0x80, 0x81};
    BOOST_TEST(u.bytes() == wantBytes);
}


BOOST_AUTO_TEST_CASE(testCopy)
{
    Uuid u1 = Uuid::createRandomUuid();
    Uuid u2 = Uuid::createRandomUuid();
    Uuid u3 = Uuid::createRandomUuid();
    Uuid u4 = u3;
    BOOST_TEST(u1 != u2);
    BOOST_TEST(u1.toString() != u2.toString());

    u1 = u2;
    BOOST_TEST(u1 == u2);
    BOOST_TEST(u1.toString() == u2.toString());

    u1 = u3;
    BOOST_TEST(u1 == u3);
    BOOST_TEST(u1.toString() == u3.toString());

    // u4 has no cache
    BOOST_TEST(u4 == u3);
}


BOOST_AUTO_TEST_CASE(testPart)
{
    Uuid u1 = Uuid::createRandomUuid();
    auto p1 = u1.toPart();
    BOOST_TEST(p1.size() == Uuid::Bytes{}.size());

    Uuid u2 = Uuid::fromPart(p1);
    BOOST_TEST(u2 == u1);
}

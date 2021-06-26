/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE common
#include <boost/test/unit_test.hpp>

#include "fuurin/c/cuuid.h"
#include "fuurin/uuid.h"

#include <string_view>
#include <algorithm>


using namespace std::literals::string_view_literals;

namespace utf = boost::unit_test;


/**
 * CUuid
 */

BOOST_AUTO_TEST_CASE(CUuid_null)
{
    CUuid id = CUuid_createNullUuid();
    for (auto b : id.bytes) {
        BOOST_TEST(b == 0);
    }
}


BOOST_AUTO_TEST_CASE(CUuid_random)
{
    CUuid id = CUuid_createRandomUuid();
    bool found = false;
    for (auto b : id.bytes) {
        if (b != 0) {
            found = true;
            break;
        }
    }
    BOOST_TEST(found);
}


BOOST_AUTO_TEST_CASE(CUuid_dns)
{
    CUuid got = CUuid_createDnsUuid("test.name");
    auto want = fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::Dns, "test.name"sv);
    BOOST_TEST(std::equal(want.bytes().begin(), want.bytes().end(), got.bytes));
}


BOOST_AUTO_TEST_CASE(CUuid_url)
{
    CUuid got = CUuid_createUrlUuid("test.name");
    auto want = fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::Url, "test.name"sv);
    BOOST_TEST(std::equal(want.bytes().begin(), want.bytes().end(), got.bytes));
}


BOOST_AUTO_TEST_CASE(CUuid_oid)
{
    CUuid got = CUuid_createOidUuid("test.name");
    auto want = fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::Oid, "test.name"sv);
    BOOST_TEST(std::equal(want.bytes().begin(), want.bytes().end(), got.bytes));
}


BOOST_AUTO_TEST_CASE(CUuid_x500dn)
{
    CUuid got = CUuid_createX500dnUuid("test.name");
    auto want = fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::X500dn, "test.name"sv);
    BOOST_TEST(std::equal(want.bytes().begin(), want.bytes().end(), got.bytes));
}


BOOST_AUTO_TEST_CASE(CUuid_copy)
{
    CUuid id1 = CUuid_createDnsUuid("test1.name");
    CUuid id2 = CUuid_createDnsUuid("test2.name");
    BOOST_TEST(!std::equal(id1.bytes, id1.bytes + sizeof(CUuid::bytes), id2.bytes));

    id2 = id1;
    BOOST_TEST(std::equal(id1.bytes, id1.bytes + sizeof(CUuid::bytes), id2.bytes));

    id2.bytes[0]++;
    BOOST_TEST(!std::equal(id1.bytes, id1.bytes + sizeof(CUuid::bytes), id2.bytes));
}

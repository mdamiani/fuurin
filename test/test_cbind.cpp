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
#include "fuurin/c/ctopic.h"
#include "fuurin/c/cworker.h"
#include "fuurin/uuid.h"
#include "fuurin/topic.h"

#include <string_view>
#include <algorithm>
#include <future>


using namespace std::literals::string_view_literals;
using namespace std::literals::string_literals;

namespace utf = boost::unit_test;


namespace {
bool uuidEqual(fuurin::Uuid a, CUuid b)
{
    return std::equal(a.bytes().begin(), a.bytes().end(), b.bytes);
}

bool uuidEqual(CUuid a, CUuid b)
{
    return std::equal(a.bytes, a.bytes + sizeof(a.bytes), b.bytes);
}

CTopic* topicConvert(fuurin::Topic* t)
{
    return reinterpret_cast<CTopic*>(t);
}
} // namespace


/**
 * CUuid
 */

BOOST_AUTO_TEST_CASE(testCUuid_null)
{
    CUuid id = CUuid_createNullUuid();
    for (auto b : id.bytes) {
        BOOST_TEST(b == 0);
    }
}


BOOST_AUTO_TEST_CASE(testCUuid_random)
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


BOOST_AUTO_TEST_CASE(testCUuid_dns)
{
    CUuid got = CUuid_createDnsUuid("test.name");
    auto want = fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::Dns, "test.name"sv);
    BOOST_TEST(uuidEqual(want, got));
}


BOOST_AUTO_TEST_CASE(testCUuid_url)
{
    CUuid got = CUuid_createUrlUuid("test.name");
    auto want = fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::Url, "test.name"sv);
    BOOST_TEST(uuidEqual(want, got));
}


BOOST_AUTO_TEST_CASE(testCUuid_oid)
{
    CUuid got = CUuid_createOidUuid("test.name");
    auto want = fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::Oid, "test.name"sv);
    BOOST_TEST(uuidEqual(want, got));
}


BOOST_AUTO_TEST_CASE(testCUuid_x500dn)
{
    CUuid got = CUuid_createX500dnUuid("test.name");
    auto want = fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::X500dn, "test.name"sv);
    BOOST_TEST(uuidEqual(want, got));
}


BOOST_AUTO_TEST_CASE(testCUuid_copy)
{
    CUuid id1 = CUuid_createDnsUuid("test1.name");
    CUuid id2 = CUuid_createDnsUuid("test2.name");
    BOOST_TEST(!uuidEqual(id1, id2));

    id2 = id1;
    BOOST_TEST(uuidEqual(id1, id2));

    id2.bytes[0]++;
    BOOST_TEST(!uuidEqual(id1, id2));
}


/**
 * CTopic
 */

BOOST_AUTO_TEST_CASE(testCTopic_uuid)
{
    fuurin::Topic t;
    CTopic* ct = topicConvert(&t);

    auto bid = fuurin::Uuid::createRandomUuid();
    auto wid = fuurin::Uuid::createRandomUuid();

    t.withBroker(bid);
    t.withWorker(wid);

    BOOST_TEST(uuidEqual(bid, CTopic_brokerUuid(ct)));
    BOOST_TEST(uuidEqual(wid, CTopic_workerUuid(ct)));
}


BOOST_AUTO_TEST_CASE(testCTopic_seqnum)
{
    fuurin::Topic t;
    CTopic* ct = topicConvert(&t);

    t.withSeqNum(1000);
    BOOST_TEST(CTopic_seqNum(ct) == 1000u);
}


BOOST_AUTO_TEST_CASE(testCTopic_type)
{
    fuurin::Topic t;
    CTopic* ct = topicConvert(&t);

    t.withType(fuurin::Topic::State);
    BOOST_TEST(CTopic_type(ct) == TopicState);

    t.withType(fuurin::Topic::Event);
    BOOST_TEST(CTopic_type(ct) == TopicEvent);
}


BOOST_AUTO_TEST_CASE(testCTopic_name)
{
    fuurin::Topic t;
    CTopic* ct = topicConvert(&t);

    t.withName("topic1"sv);
    BOOST_TEST(std::string(CTopic_name(ct)) == "topic1"s);
}


BOOST_AUTO_TEST_CASE(testCTopic_data)
{
    fuurin::Topic t;
    CTopic* ct = topicConvert(&t);

    t.withData(fuurin::zmq::Part{"payload"sv});
    BOOST_TEST(std::string(CTopic_data(ct)) == "payload"s);
    BOOST_TEST(CTopic_size(ct) == 7u);
}


/**
 * Runner
 */

BOOST_AUTO_TEST_CASE(testCWorker_create)
{
    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w != nullptr);

    CWorker_delete(w);
}


BOOST_AUTO_TEST_CASE(testCWorker_name)
{
    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w != nullptr);

    BOOST_TEST(std::string(CWorker_name(w)) == "test"s);

    CWorker_delete(w);
}


BOOST_AUTO_TEST_CASE(testCWorker_uuid)
{
    CUuid wantId = CUuid_createRandomUuid();
    CWorker* w = CWorker_new(wantId, 0, "test");
    BOOST_REQUIRE(w != nullptr);

    auto gotId = CWorker_uuid(w);

    BOOST_TEST(uuidEqual(wantId, gotId));

    CWorker_delete(w);
}


BOOST_AUTO_TEST_CASE(testCWorker_endpoints)
{
    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w != nullptr);

    BOOST_TEST(std::string(CWorker_endpointDelivery(w)) == "ipc:///tmp/worker_delivery"s);
    BOOST_TEST(std::string(CWorker_endpointDispatch(w)) == "ipc:///tmp/worker_dispatch"s);
    BOOST_TEST(std::string(CWorker_endpointSnapshot(w)) == "ipc:///tmp/broker_snapshot"s);

    CWorker_clearEndpoints(w);

    CWorker_addEndpoints(w, "endp1", "endp2", "endp3");
    CWorker_addEndpoints(w, "endp4", "endp5", "endp6");

    BOOST_TEST(std::string(CWorker_endpointDelivery(w)) == "endp1"s);
    BOOST_TEST(std::string(CWorker_endpointDispatch(w)) == "endp2"s);
    BOOST_TEST(std::string(CWorker_endpointSnapshot(w)) == "endp3"s);

    CWorker_clearEndpoints(w);

    BOOST_TEST(CWorker_endpointDelivery(w) == nullptr);
    BOOST_TEST(CWorker_endpointDispatch(w) == nullptr);
    BOOST_TEST(CWorker_endpointSnapshot(w) == nullptr);

    CWorker_delete(w);
}


BOOST_AUTO_TEST_CASE(testCWorker_run, *utf::timeout(10))
{
    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w != nullptr);

    for (int i = 1; i < 2; ++i) {
        CWorker_start(w);
        BOOST_REQUIRE(CWorker_isRunning(w));
    }

    auto wait = std::async(std::launch::async, [w]() {
        for (int i = 0; i < 2; ++i) {
            CWorker_stop(w);
        }
    });

    CWorker_wait(w);

    BOOST_REQUIRE(!CWorker_isRunning(w));

    CWorker_wait(w);

    wait.get();

    CWorker_delete(w);
}


BOOST_AUTO_TEST_CASE(testCWorker_subscribe)
{
    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w != nullptr);

    CWorker_setTopicsAll(w);

    BOOST_TEST(CWorker_topicsAll(w) == true);
    BOOST_TEST(CWorker_topicsNames(w) == nullptr);

    CWorker_addTopicsNames(w, "topic1");
    CWorker_addTopicsNames(w, "topic2");

    BOOST_TEST(CWorker_topicsAll(w) == false);
    BOOST_TEST(std::string(CWorker_topicsNames(w)) == "topic1"s);

    CWorker_clearTopicsNames(w);

    BOOST_TEST(CWorker_topicsAll(w) == false);
    BOOST_TEST(CWorker_topicsNames(w) == nullptr);

    CWorker_delete(w);
}

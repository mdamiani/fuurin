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
#include <boost/test/data/test_case.hpp>

#include "fuurin/c/cuuid.h"
#include "fuurin/c/ctopic.h"
#include "fuurin/c/cevent.h"
#include "fuurin/c/cworker.h"
#include "fuurin/uuid.h"
#include "fuurin/topic.h"
#include "fuurin/event.h"
#include "c/ceventd.h"

#include <string_view>
#include <algorithm>
#include <future>
#include <tuple>


using namespace std::literals::string_view_literals;
using namespace std::literals::string_literals;

namespace utf = boost::unit_test;
namespace bdata = utf::data;


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

CEvent* eventConvert(CEventD* ev)
{
    return reinterpret_cast<CEvent*>(ev);
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
 * CEvent
 */

BOOST_DATA_TEST_CASE(testCEvent_type,
    bdata::make({
        std::make_tuple(fuurin::Event::Type::Invalid, EventInvalid),
        std::make_tuple(fuurin::Event::Type::COUNT, EventInvalid),
        std::make_tuple(fuurin::Event::Type(int(fuurin::Event::Type::COUNT) + 1), EventInvalid),
        std::make_tuple(fuurin::Event::Type::Started, EventStarted),
        std::make_tuple(fuurin::Event::Type::Stopped, EventStopped),
        std::make_tuple(fuurin::Event::Type::Offline, EventOffline),
        std::make_tuple(fuurin::Event::Type::Online, EventOnline),
        std::make_tuple(fuurin::Event::Type::Delivery, EventDelivery),
        std::make_tuple(fuurin::Event::Type::SyncRequest, EventSyncRequest),
        std::make_tuple(fuurin::Event::Type::SyncBegin, EventSyncBegin),
        std::make_tuple(fuurin::Event::Type::SyncElement, EventSyncElement),
        std::make_tuple(fuurin::Event::Type::SyncSuccess, EventSyncSuccess),
        std::make_tuple(fuurin::Event::Type::SyncError, EventSyncError),
        std::make_tuple(fuurin::Event::Type::SyncDownloadOn, EventSyncDownloadOn),
        std::make_tuple(fuurin::Event::Type::SyncDownloadOff, EventSyncDownloadOff),
    }),
    cppType, wantCType)
{
    CEventD evd{
        .ev = fuurin::Event{cppType, {}},
    };

    auto* ev = eventConvert(&evd);

    BOOST_TEST(wantCType == CEvent_type(ev));
}


BOOST_DATA_TEST_CASE(testCEvent_notif,
    bdata::make({
        std::make_tuple(fuurin::Event::Notification::Discard, EventDiscard),
        std::make_tuple(fuurin::Event::Notification::COUNT, EventDiscard),
        std::make_tuple(fuurin::Event::Notification(int(fuurin::Event::Notification::COUNT) + 1), EventDiscard),
        std::make_tuple(fuurin::Event::Notification::Timeout, EventTimeout),
        std::make_tuple(fuurin::Event::Notification::Success, EventSuccess),
    }),
    cppNotif, wantCNotif)
{
    CEventD evd{
        .ev = fuurin::Event{{}, cppNotif},
    };

    auto* ev = eventConvert(&evd);

    BOOST_TEST(wantCNotif == CEvent_notif(ev));
}


BOOST_AUTO_TEST_CASE(testCEvent_topic_ok)
{
    auto bid = fuurin::Uuid::createRandomUuid();
    auto wid = fuurin::Uuid::createRandomUuid();

    CEventD evd{
        .ev = fuurin::Event{{}, {},
            fuurin::Topic{bid, wid, 100, "topic"sv,
                fuurin::zmq::Part{"payload"sv},
                fuurin::Topic::Event}
                .toPart()},
    };

    auto* ev = eventConvert(&evd);
    auto* tp = CEvent_topic(ev);

    BOOST_REQUIRE(tp != nullptr);

    BOOST_TEST(uuidEqual(bid, CTopic_brokerUuid(tp)));
    BOOST_TEST(uuidEqual(wid, CTopic_workerUuid(tp)));
    BOOST_TEST(100ull == CTopic_seqNum(tp));
    BOOST_TEST(TopicEvent == CTopic_type(tp));
    BOOST_TEST("topic"s == std::string(CTopic_name(tp)));
    BOOST_TEST("payload"s == std::string(CTopic_data(tp)));
    BOOST_TEST(7u == CTopic_size(tp));
}


BOOST_AUTO_TEST_CASE(testCEvent_topic_err)
{
    CEventD evd;

    auto* ev = eventConvert(&evd);
    auto* tp = CEvent_topic(ev);

    BOOST_REQUIRE(tp == nullptr);
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


/**
 * Worker
 */

BOOST_AUTO_TEST_CASE(testCWorker_seqnum)
{
    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 150, "test");
    BOOST_REQUIRE(w != nullptr);

    BOOST_TEST(150ull == CWorker_seqNum(w));

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

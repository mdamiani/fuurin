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
#include "fuurin/c/cbroker.h"
#include "fuurin/c/cworker.h"
#include "fuurin/uuid.h"
#include "fuurin/topic.h"
#include "fuurin/event.h"
#include "fuurin/broker.h"
#include "fuurin/worker.h"
#include "c/ctopicd.h"
#include "c/ceventd.h"
#include "c/cutils.h"
#include "c/cworkerd.h"

#include <string_view>
#include <algorithm>
#include <future>
#include <tuple>
#include <chrono>


using namespace std::literals::string_view_literals;
using namespace std::literals::string_literals;
using namespace std::literals::chrono_literals;

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

CBroker* CBroker_new(CUuid id, const char* name)
{
    return CBroker_new(&id, name);
}

CWorker* CWorker_new(CUuid id, unsigned long long seqn, const char* name)
{
    return CWorker_new(&id, seqn, name);
}
} // namespace


/**
 * CUtils
 */

BOOST_DATA_TEST_CASE(testWithCatch,
    bdata::make({
        std::make_tuple("no exception", 1, 2, 1, false, false, false),
        std::make_tuple("fuurin error", 1, 2, 2, true, false, false),
        std::make_tuple("standard error", 1, 2, 2, false, true, false),
        std::make_tuple("unknown error", 1, 2, 2, false, false, true),
    }),
    name, fVal, cVal, retVal, exFuu, exStd, exErr)
{
    auto e = [exFuu, exStd, exErr]() {
        if (exFuu)
            throw fuurin::err::Error({__FILE__, __LINE__}, "test/err");
        if (exStd)
            throw std::exception{};
        if (exErr)
            throw 3;
    };

    auto f = [fVal, e]() { e(); return fVal; };
    auto c = [cVal]() { return cVal; };

    BOOST_TEST_MESSAGE(name);
    BOOST_TEST(retVal == fuurin::c::withCatch(f, c));
}


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
    CTopic* ct = fuurin::c::getOpaque(&t);

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
    CTopic* ct = fuurin::c::getOpaque(&t);

    t.withSeqNum(1000);
    BOOST_TEST(CTopic_seqNum(ct) == 1000u);
}


BOOST_AUTO_TEST_CASE(testCTopic_type)
{
    fuurin::Topic t;
    CTopic* ct = fuurin::c::getOpaque(&t);

    t.withType(fuurin::Topic::State);
    BOOST_TEST(CTopic_type(ct) == TopicState);

    t.withType(fuurin::Topic::Event);
    BOOST_TEST(CTopic_type(ct) == TopicEvent);
}


BOOST_AUTO_TEST_CASE(testCTopic_name)
{
    fuurin::Topic t;
    CTopic* ct = fuurin::c::getOpaque(&t);

    t.withName("topic1"sv);
    BOOST_TEST(std::string(CTopic_name(ct)) == "topic1"s);
}


BOOST_AUTO_TEST_CASE(testCTopic_data)
{
    fuurin::Topic t;
    CTopic* ct = fuurin::c::getOpaque(&t);

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
    fuurin::c::CEventD evd{
        .ev = fuurin::Event{cppType, {}},
        .tp = fuurin::Topic{},
    };

    auto* ev = fuurin::c::getOpaque(&evd);

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
    fuurin::c::CEventD evd{
        .ev = fuurin::Event{{}, cppNotif},
        .tp = fuurin::Topic{},
    };

    auto* ev = fuurin::c::getOpaque(&evd);

    BOOST_TEST(wantCNotif == CEvent_notif(ev));
}


BOOST_AUTO_TEST_CASE(testCEvent_topic_ok)
{
    auto bid = fuurin::Uuid::createRandomUuid();
    auto wid = fuurin::Uuid::createRandomUuid();

    fuurin::c::CEventD evd{
        .ev = fuurin::Event{{}, {},
            fuurin::Topic{bid, wid, 100, "topic"sv,
                fuurin::zmq::Part{"payload"sv},
                fuurin::Topic::Event}
                .toPart()},
        .tp = fuurin::Topic{},
    };

    auto* ev = fuurin::c::getOpaque(&evd);
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
    fuurin::c::CEventD evd;

    auto* ev = fuurin::c::getOpaque(&evd);
    auto* tp = CEvent_topic(ev);

    BOOST_REQUIRE(tp == nullptr);
}


/**
 * Runner: Broker
 */

BOOST_AUTO_TEST_CASE(testCBroker_create)
{
    CBroker* b = CBroker_new(CUuid_createRandomUuid(), "test");
    BOOST_REQUIRE(b != nullptr);

    CBroker_delete(b);
}


BOOST_AUTO_TEST_CASE(testCBroker_name)
{
    CBroker* b = CBroker_new(CUuid_createRandomUuid(), "test");
    BOOST_REQUIRE(b != nullptr);

    BOOST_TEST(std::string(CBroker_name(b)) == "test"s);

    CBroker_delete(b);
}


BOOST_AUTO_TEST_CASE(testCBroker_uuid)
{
    CUuid wantId = CUuid_createRandomUuid();
    CBroker* b = CBroker_new(wantId, "test");
    BOOST_REQUIRE(b != nullptr);

    auto gotId = CBroker_uuid(b);

    BOOST_TEST(uuidEqual(wantId, gotId));

    CBroker_delete(b);
}


BOOST_AUTO_TEST_CASE(testCBroker_endpoints)
{
    CBroker* b = CBroker_new(CUuid_createRandomUuid(), "test");
    BOOST_REQUIRE(b != nullptr);

    BOOST_TEST(std::string(CBroker_endpointDelivery(b)) == "ipc:///tmp/worker_delivery"s);
    BOOST_TEST(std::string(CBroker_endpointDispatch(b)) == "ipc:///tmp/worker_dispatch"s);
    BOOST_TEST(std::string(CBroker_endpointSnapshot(b)) == "ipc:///tmp/broker_snapshot"s);

    CBroker_clearEndpoints(b);

    CBroker_addEndpoints(b, "endp1", "endp2", "endp3");
    CBroker_addEndpoints(b, "endp4", "endp5", "endp6");

    BOOST_TEST(std::string(CBroker_endpointDelivery(b)) == "endp1"s);
    BOOST_TEST(std::string(CBroker_endpointDispatch(b)) == "endp2"s);
    BOOST_TEST(std::string(CBroker_endpointSnapshot(b)) == "endp3"s);

    CBroker_clearEndpoints(b);

    BOOST_TEST(CBroker_endpointDelivery(b) == nullptr);
    BOOST_TEST(CBroker_endpointDispatch(b) == nullptr);
    BOOST_TEST(CBroker_endpointSnapshot(b) == nullptr);

    CBroker_delete(b);
}


BOOST_AUTO_TEST_CASE(testCBroker_run, *utf::timeout(10))
{
    CBroker* b = CBroker_new(CUuid_createRandomUuid(), "test");
    BOOST_REQUIRE(b != nullptr);

    for (int i = 1; i < 2; ++i) {
        CBroker_start(b);
        BOOST_REQUIRE(CBroker_isRunning(b));
    }

    auto wait = std::async(std::launch::async, [b]() {
        for (int i = 0; i < 2; ++i) {
            CBroker_stop(b);
        }
    });

    CBroker_wait(b);

    BOOST_REQUIRE(!CBroker_isRunning(b));

    CBroker_wait(b);

    wait.get();

    CBroker_delete(b);
}


/**
 * Broker
 */

BOOST_AUTO_TEST_CASE(testCBroker_dispatch, *utf::timeout(10))
{
    CBroker* b = CBroker_new(CUuid_createRandomUuid(), "test");
    BOOST_REQUIRE(b != nullptr);

    CBroker_start(b);

    fuurin::Worker w;
    auto wf = w.start();

    BOOST_REQUIRE(w.waitForOnline(5s));

    w.dispatch("my/topic"sv, fuurin::zmq::Part{"hello1"s});

    auto t = w.waitForTopic();

    BOOST_REQUIRE(t.has_value());
    BOOST_TEST(uuidEqual(t->broker(), CBroker_uuid(b)));
    BOOST_TEST(t->worker() == w.uuid());
    BOOST_TEST(t->seqNum() == 1ull);
    BOOST_TEST(t->type() == fuurin::Topic::State);
    BOOST_TEST(t->name() == "my/topic"s);
    BOOST_TEST(t->data().size() == 6u);
    BOOST_TEST(t->data().toString() == "hello1"s);

    w.stop();
    wf.get();

    CBroker_stop(b);
    CBroker_wait(b);
    CBroker_delete(b);
}


/**
 * Runner: Worker
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


BOOST_AUTO_TEST_CASE(testCWorker_waitForStarted, *utf::timeout(10))
{
    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w != nullptr);

    CWorker_start(w);
    BOOST_TEST(CWorker_waitForStarted(w, 5000));
    BOOST_TEST(!CWorker_waitForStarted(w, 500));

    CWorker_stop(w);
    CWorker_wait(w);
    CWorker_delete(w);
}


BOOST_AUTO_TEST_CASE(testCWorker_waitForStopped, *utf::timeout(10))
{
    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w != nullptr);

    CWorker_start(w);

    CWorker_stop(w);
    BOOST_TEST(CWorker_waitForStopped(w, 5000));
    BOOST_TEST(!CWorker_waitForStopped(w, 500));

    CWorker_wait(w);
    CWorker_delete(w);
}


BOOST_AUTO_TEST_CASE(testCWorker_waitForOnline, *utf::timeout(10))
{
    fuurin::Broker b;
    auto bf = b.start();

    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w != nullptr);

    CWorker_start(w);
    BOOST_TEST(CWorker_waitForOnline(w, 5000));
    BOOST_TEST(!CWorker_waitForOnline(w, 500));

    CWorker_stop(w);
    CWorker_wait(w);
    CWorker_delete(w);

    b.stop();
    bf.get();
}


BOOST_AUTO_TEST_CASE(testCWorker_waitForOffline, *utf::timeout(10))
{
    fuurin::Broker b;
    auto bf = b.start();

    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w != nullptr);

    CWorker_start(w);
    CWorker_waitForOnline(w, 5000);

    CWorker_stop(w);
    BOOST_TEST(CWorker_waitForOffline(w, 5000));
    BOOST_TEST(!CWorker_waitForOffline(w, 500));

    CWorker_wait(w);
    CWorker_delete(w);

    b.stop();
    bf.get();
}


BOOST_AUTO_TEST_CASE(testCWorker_waitForTopic, *utf::timeout(10))
{
    fuurin::Broker b;
    fuurin::Worker w2;

    auto bf = b.start();
    auto w2f = w2.start();

    BOOST_REQUIRE(w2.waitForOnline(5s));

    CWorker* w1 = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w1 != nullptr);

    CWorker_start(w1);
    CWorker_waitForOnline(w1, 5000);

    // send multiple topic to check CTopic poiter is valid after
    // multiple function calls.
    for (uint8_t n = 0; n < 3; ++n) {
        w2.dispatch("my/topic"sv, fuurin::zmq::Part{uint8_t(n + 1)});

        CTopic* t = CWorker_waitForTopic(w1, 5000);
        BOOST_REQUIRE(t != nullptr);

        BOOST_TEST(uuidEqual(b.uuid(), CTopic_brokerUuid(t)));
        BOOST_TEST(uuidEqual(w2.uuid(), CTopic_workerUuid(t)));
        BOOST_TEST(CTopic_seqNum(t) == unsigned(n + 1));
        BOOST_TEST(CTopic_type(t) == TopicState);
        BOOST_TEST(std::string(CTopic_name(t)) == "my/topic"s);
        BOOST_TEST(CTopic_size(t) == 1u);
        BOOST_TEST(uint8_t(*CTopic_data(t)) == n + 1);
    }

    // verify timeout
    BOOST_TEST(CWorker_waitForTopic(w1, 1000) == nullptr);

    CWorker_stop(w1);
    CWorker_wait(w1);
    CWorker_delete(w1);

    b.stop();
    w2.stop();

    bf.get();
    w2f.get();
}


BOOST_AUTO_TEST_CASE(testCWorker_waitForEvent, *utf::timeout(10))
{
    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w != nullptr);
    CEvent* ev;

    // receive first event
    CWorker_start(w);
    ev = CWorker_waitForEvent(w, 5000);
    BOOST_REQUIRE(ev != nullptr);
    BOOST_TEST(CEvent_type(ev) == EventStarted);

    // receive another event
    CWorker_stop(w);
    ev = CWorker_waitForEvent(w, 5000);
    BOOST_REQUIRE(ev != nullptr);
    BOOST_TEST(CEvent_type(ev) == EventStopped);

    // timeout exceeded
    ev = CWorker_waitForEvent(w, 500);
    BOOST_REQUIRE(ev != nullptr);
    BOOST_TEST(CEvent_type(ev) == EventInvalid);

    CWorker_wait(w);
    CWorker_delete(w);
}


BOOST_AUTO_TEST_CASE(testCWorker_dispatch, *utf::timeout(15))
{
    fuurin::Broker b;
    fuurin::Worker w2;

    auto bf = b.start();
    auto w2f = w2.start();

    BOOST_REQUIRE(w2.waitForOnline(5s));

    auto id1 = CUuid_createRandomUuid();
    CWorker* w1 = CWorker_new(id1, 0, "test");
    BOOST_REQUIRE(w1 != nullptr);
    CWorker_start(w1);
    CWorker_waitForOnline(w1, 5000);
    CWorker_dispatch(w1, "topic1", "hello1", 6, TopicEvent);

    auto t = w2.waitForTopic(5s);
    BOOST_TEST(t.has_value());
    BOOST_TEST(t->broker() == b.uuid());
    BOOST_TEST(uuidEqual(t->worker(), id1));
    BOOST_TEST(t->seqNum() == 1ull);
    BOOST_TEST(t->type() == fuurin::Topic::Event);
    BOOST_TEST(t->name() == "topic1"s);
    BOOST_TEST(t->data().size() == 6u);
    BOOST_TEST(t->data().toString() == "hello1"s);

    CWorker_stop(w1);
    CWorker_wait(w1);
    CWorker_delete(w1);

    b.stop();
    w2.stop();

    bf.get();
    w2f.get();
}


BOOST_AUTO_TEST_CASE(testCWorker_sync, *utf::timeout(15))
{
    fuurin::Broker b;
    auto bf = b.start();

    // store topic
    CWorker* w1 = CWorker_new(CUuid_createRandomUuid(), 0, "test1");
    BOOST_REQUIRE(w1 != nullptr);

    CWorker_start(w1);
    BOOST_REQUIRE(CWorker_waitForOnline(w1, 5000));

    CWorker_dispatch(w1, "topic1", "hello1", 6, TopicState);
    CTopic* t1 = CWorker_waitForTopic(w1, 5000);
    BOOST_REQUIRE(t1 != nullptr);

    // sync topic
    CWorker* w2 = CWorker_new(CUuid_createRandomUuid(), 0, "test2");
    BOOST_REQUIRE(w2 != nullptr);

    CWorker_start(w2);
    BOOST_REQUIRE(CWorker_waitForOnline(w2, 5000));

    CWorker_sync(w2);
    CTopic* t2 = CWorker_waitForTopic(w2, 5000);
    BOOST_REQUIRE(t2 != nullptr);
    BOOST_TEST(std::string(CTopic_name(t2)) == "topic1"s);
    BOOST_TEST(std::string(CTopic_data(t2)) == "hello1"s);
    BOOST_TEST(CTopic_seqNum(t2) == 1ull);

    CWorker_stop(w1);
    CWorker_stop(w2);
    CWorker_wait(w1);
    CWorker_wait(w2);
    CWorker_delete(w1);
    CWorker_delete(w2);

    b.stop();
    bf.get();
}


BOOST_AUTO_TEST_CASE(testCWorker_eventFD)
{
    CWorker* w = CWorker_new(CUuid_createRandomUuid(), 0, "test");
    BOOST_REQUIRE(w != nullptr);

    const auto wd = fuurin::c::getPrivD(w);

    BOOST_TEST(CWorker_eventFD(w) > 0);
    BOOST_TEST(CWorker_eventFD(w) == wd->w->eventFD());

    CWorker_delete(w);
}

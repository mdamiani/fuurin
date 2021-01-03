/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE worker
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/mpl/list.hpp>
#include <benchmark/benchmark.h>

#include "test_utils.hpp"

#include "fuurin/broker.h"
#include "fuurin/worker.h"
#include "fuurin/workerconfig.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/elapser.h"
#include "fuurin/errors.h"

#include <string_view>
#include <chrono>
#include <list>
#include <type_traits>


using namespace fuurin;
namespace utf = boost::unit_test;
namespace bdata = utf::data;

using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;


/**
 * BOOST_TEST print functions.
 */
namespace std {
template<typename T>
inline std::ostream& operator<<(std::ostream& os, const std::list<T>& l)
{
    std::string sep;

    os << "[";
    for (const auto& v : l) {
        os << sep;
        if constexpr (std::is_same_v<T, std::chrono::milliseconds>)
            os << v.count();
        else
            os << int(v);
        sep = ", ";
    }
    os << "]";

    return os;
}
} // namespace std


static void testWaitForStart(Worker& w)
{
    testWaitForStart(w, mkCnf(w));
}


struct WorkerFixture
{
    WorkerFixture()
        : w(wid)
        , b(bid)
        , wf(w.start())
        , bf(b.start())
    {
        testWaitForStart(w);
    }

    ~WorkerFixture()
    {
        w.stop();
        b.stop();

        testWaitForStop(w);

        wf.get();
        bf.get();
    }

    static const Uuid wid;
    static const Uuid bid;

    Worker w;
    Broker b;

    std::future<void> wf;
    std::future<void> bf;
};

const Uuid WorkerFixture::wid = Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker.net"sv);
const Uuid WorkerFixture::bid = Uuid::createNamespaceUuid(Uuid::Ns::Dns, "broker.net"sv);


Topic mkT(const std::string& name, Topic::SeqN seqn, const std::string& pay)
{
    using f = WorkerFixture;
    return Topic{f::bid, f::wid, seqn, name, zmq::Part{pay}, Topic::State};
}


BOOST_AUTO_TEST_CASE(testUuid)
{
    auto id = WorkerFixture::wid;
    Worker w(id);
    BOOST_TEST(w.uuid() == id);
}


BOOST_AUTO_TEST_CASE(testName)
{
    Broker b(WorkerFixture::bid, "my_broker");
    Worker w(WorkerFixture::wid, 0, "my_worker");

    BOOST_TEST(b.name() == "my_broker"sv);
    BOOST_TEST(w.name() == "my_worker"sv);
}


BOOST_AUTO_TEST_CASE(testDeliverUuid)
{
    Worker w(WorkerFixture::wid);
    Broker b(WorkerFixture::bid);

    auto wf = w.start();
    auto bf = b.start();

    testWaitForStart(w);

    w.dispatch("topic"sv, zmq::Part{"hello"sv});

    auto ev = testWaitForTopic(w, mkT("topic", 0, "hello"), 1);
    auto t = Topic::fromPart(ev.payload());

    BOOST_TEST(t.worker() == WorkerFixture::wid);
    BOOST_TEST(t.broker() == WorkerFixture::bid);

    w.stop();
    b.stop();

    bf.get();
    wf.get();
}


BOOST_AUTO_TEST_CASE(testTopicPart)
{
    using f = WorkerFixture;
    const Topic t1{f::bid, f::wid, 256, "topic/test"sv, zmq::Part{"topic/data"sv}, Topic::State};
    const Topic t2{Topic::fromPart(t1.toPart())};

    BOOST_TEST(t1 == t2);
}


BOOST_AUTO_TEST_CASE(testTopicPatchSeqNum)
{
    using f = WorkerFixture;
    const Topic::Name name{"topic/test"sv};
    const Topic::Data data{"topic/data"sv};
    const Topic t1{f::bid, f::wid, 256, name, data, Topic::State};
    zmq::Part p1 = t1.toPart();

    const Topic t2{Topic::fromPart(p1)};
    BOOST_TEST(t2.broker() == f::bid);
    BOOST_TEST(t2.worker() == f::wid);
    BOOST_TEST(t2.seqNum() == 256llu);
    BOOST_TEST(t2.name() == name);
    BOOST_TEST(t2.data() == data);

    Topic::withSeqNum(p1, 9223372036854775808ull);

    const Topic t3{Topic::fromPart(p1)};

    BOOST_TEST(t2.broker() == f::bid);
    BOOST_TEST(t2.worker() == f::wid);
    BOOST_TEST(t3.seqNum() == 9223372036854775808ull);
    BOOST_TEST(t2.name() == name);
    BOOST_TEST(t2.data() == data);

    zmq::Part p4{uint16_t(0)};
    BOOST_REQUIRE_THROW(Topic::withSeqNum(p4, 1), err::ZMQPartAccessFailed);
}


namespace fuurin {
class TestRunner : public zmq::PollerWaiter, public Elapser
{
public:
    virtual ~TestRunner() = default;

    Runner r_;
    std::list<std::chrono::milliseconds> timeout_;
    std::list<std::chrono::milliseconds> elapsed_;
    std::list<bool> empty_;
    std::list<Event::Notification> recv_;

    /**
     * PollerWaiter
     */
    void setTimeout(std::chrono::milliseconds tmeo) noexcept override
    {
        timeout_.push_back(tmeo);
    };

    std::chrono::milliseconds timeout() const noexcept override
    {
        return 0ms;
    };

    zmq::PollerEvents wait() override
    {
        BOOST_TEST(!empty_.empty());
        auto v = empty_.front();
        empty_.pop_front();
        return zmq::PollerEvents(zmq::PollerEvents::Read, nullptr, v ? 0 : 1);
    };

    /**
     * PollerWaiter
     */
    void start() noexcept override
    {
    }

    std::chrono::milliseconds elapsed() const noexcept override
    {
        BOOST_TEST(!elapsed_.empty());
        auto v = elapsed_.front();
        const_cast<TestRunner*>(this)->elapsed_.pop_front();
        return v;
    }

    /**
     * Runner
     */
    Event recvEvent()
    {
        BOOST_TEST(!recv_.empty());
        auto v = recv_.front();
        recv_.pop_front();
        return Event{Event::Type::Invalid, v, zmq::Part{}};
    }

    Event waitForEvent(std::chrono::milliseconds timeout)
    {
        return r_.waitForEvent(static_cast<zmq::PollerWaiter&&>(*this),
            static_cast<Elapser&&>(*this), std::bind(&TestRunner::recvEvent, this),
            {}, timeout);
    }
};
} // namespace fuurin

using EM = std::list<bool>;
using ER = std::list<Event::Notification>;
using EL = std::list<std::chrono::milliseconds>;
using TO = std::list<std::chrono::milliseconds>;
using EV = Event::Notification;

BOOST_DATA_TEST_CASE(testWaitForEventTimeout,
    bdata::make({
        std::make_tuple("read success",
            EM{false},
            ER{EV::Success},
            EL{},
            TO{1000ms},
            EV::Success),
        std::make_tuple("read timeout, simple",
            EM{true},
            ER{},
            EL{},
            TO{1000ms},
            EV::Timeout),
        std::make_tuple("read timeout, sequence to zero",
            EM{false, false, false},
            ER{EV::Timeout, EV::Timeout, EV::Timeout},
            EL{500ms, 300ms, 200ms},
            TO{1000ms, 500ms, 200ms},
            EV::Timeout),
        std::make_tuple("read timeout, sequence to below zero",
            EM{false, false, false},
            ER{EV::Timeout, EV::Timeout, EV::Timeout},
            EL{500ms, 300ms, 300ms},
            TO{1000ms, 500ms, 200ms},
            EV::Timeout),
        std::make_tuple("read timeout, sequence to success",
            EM{false, false, false},
            ER{EV::Timeout, EV::Timeout, EV::Success},
            EL{500ms, 300ms},
            TO{1000ms, 500ms, 200ms},
            EV::Success),
    }),
    name, empty, recv, elapsed, wantTimeout, wantEvent)
{
    BOOST_TEST_MESSAGE(name);

    TestRunner r;
    r.empty_ = empty;
    r.recv_ = recv;
    r.elapsed_ = elapsed;
    auto ev = r.waitForEvent(1000ms);
    BOOST_TEST(r.timeout_ == wantTimeout);
    BOOST_TEST(ev.notification() == wantEvent);
}


typedef boost::mpl::list<Broker, Worker> runnerTypes;
BOOST_AUTO_TEST_CASE_TEMPLATE(workerStart, T, runnerTypes)
{
    Worker w;
    BOOST_TEST(!w.isRunning());

    int cnt = 3;
    while (cnt--) {
        auto fut = w.start();
        BOOST_TEST(w.isRunning());
        BOOST_TEST(!w.start().valid());

        BOOST_TEST(w.stop());
        fut.get();

        BOOST_TEST(!w.isRunning());
        BOOST_TEST(!w.stop());
    }
}


BOOST_AUTO_TEST_CASE(testWaitForEventStarted)
{
    Worker w;

    auto wf = w.start();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Started, mkCnf(w));

    w.stop();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Stopped);
}


BOOST_FIXTURE_TEST_CASE(testWaitForEventOffline, WorkerFixture)
{
    b.stop();
    bf.get();
    testWaitForEvent(w, 6s, Event::Notification::Success, Event::Type::Offline);

    bf = b.start();
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Online);
}


BOOST_AUTO_TEST_CASE(testWaitForStarted)
{
    Worker w;
    auto f = w.start();

    BOOST_TEST(true == w.waitForStarted(5s));

    w.stop();
    f.get();
}


BOOST_AUTO_TEST_CASE(testWaitForStopped)
{
    Worker w;
    auto f = w.start();

    w.stop();

    BOOST_TEST(true == w.waitForStopped(5s));

    f.get();
}


BOOST_AUTO_TEST_CASE(testWaitForOnline)
{
    Broker b;
    Worker w;
    auto bf = b.start();
    auto wf = w.start();

    BOOST_TEST(true == w.waitForOnline(5s));

    b.stop();
    w.stop();

    bf.get();
    wf.get();
}


BOOST_AUTO_TEST_CASE(testWaitForOffline)
{
    Broker b;
    Worker w;
    auto bf = b.start();
    auto wf = w.start();

    BOOST_TEST(true == w.waitForOnline(5s));

    b.stop();
    w.stop();

    BOOST_TEST(true == w.waitForOffline(5s));

    bf.get();
    wf.get();
}


BOOST_AUTO_TEST_CASE(testWaitForTopicDelivery)
{
    Broker b;
    Worker w;
    auto bf = b.start();
    auto wf = w.start();

    BOOST_TEST(true == w.waitForOnline(5s));

    const auto t = Topic{b.uuid(), w.uuid(), 1, "topic"sv, zmq::Part{"hello"sv}, Topic::State};

    w.dispatch(t.name(), t.data(), t.type());

    const auto opt = w.waitForTopic(5s);

    BOOST_TEST(opt.has_value());
    BOOST_TEST(opt.value() == t);

    b.stop();
    w.stop();

    BOOST_TEST(true == w.waitForStopped());

    bf.get();
    wf.get();
}


BOOST_AUTO_TEST_CASE(testWaitForTopicSync)
{
    Broker b;
    Worker w;
    auto bf = b.start();
    auto wf = w.start();

    BOOST_TEST(true == w.waitForOnline(5s));

    const auto t = Topic{b.uuid(), w.uuid(), 1, "topic"sv, zmq::Part{"hello"sv}, Topic::State};
    w.dispatch(t.name(), t.data(), t.type());

    const auto opt = w.waitForTopic(5s);
    BOOST_TEST(opt.has_value());
    BOOST_TEST(opt.value() == t);

    // sync
    w.sync();
    const auto ops = w.waitForTopic(5s);
    BOOST_TEST(ops.has_value());
    BOOST_TEST(ops.value() == t);

    b.stop();
    w.stop();

    BOOST_TEST(true == w.waitForStopped());

    bf.get();
    wf.get();
}


BOOST_AUTO_TEST_CASE(testWaitForTopicTimeout)
{
    Worker w;
    auto wf = w.start();

    BOOST_TEST(true == w.waitForStarted(5s));

    w.stop();

    const auto opt = w.waitForTopic(2s);

    BOOST_TEST(!opt.has_value());

    wf.get();
}


BOOST_FIXTURE_TEST_CASE(testSimpleDispatch, WorkerFixture)
{
    w.dispatch("topic"sv, zmq::Part{"hello"sv});

    testWaitForTopic(w, mkT("topic", 0, "hello"), 1);
}


BOOST_FIXTURE_TEST_CASE(testWaitForEventThreadSafe, WorkerFixture)
{
    const auto recvEvent = [this]() {
        int cnt = 0;
        for (;;) {
            const auto& ev = w.waitForEvent(1500ms);
            if (ev.payload().empty() || ev.notification() != Event::Notification::Success)
                break;
            ++cnt;
        }
        return cnt;
    };

    const int iterations = 100;

    for (int i = 0; i < iterations; ++i)
        w.dispatch("topic"sv, zmq::Part{"hello"sv});

    auto f1 = std::async(std::launch::async, recvEvent);
    auto f2 = std::async(std::launch::async, recvEvent);

    const int cnt1 = f1.get();
    const int cnt2 = f2.get();

    BOOST_TEST(cnt1 != 0);
    BOOST_TEST(cnt2 != 0);
    BOOST_TEST(cnt1 + cnt2 == iterations);
}


BOOST_FIXTURE_TEST_CASE(testWaitForEventDiscard, WorkerFixture)
{
    // produce some events
    const int iterations = 3;
    for (int i = 0; i < iterations; ++i)
        w.dispatch("t1"sv, zmq::Part{"hello1"sv});

    // receive just one event
    testWaitForTopic(w, mkT("t1", 0, "hello1"), 1);

    // wait for other events to be received
    std::this_thread::sleep_for(1s);

    // start a new session without reading events
    w.stop();
    wf.get();
    wf = w.start();

    // discard old events
    for (int i = 0; i < iterations - 1; ++i) {
        testWaitForEvent(w, 1500ms, Event::Notification::Discard,
            Event::Type::Delivery, mkT("t1", i + 2, "hello1"));
    }

    // discard old stop event
    testWaitForEvent(w, 1500ms, Event::Notification::Discard, Event::Type::Offline);
    testWaitForEvent(w, 1500ms, Event::Notification::Discard, Event::Type::Stopped);

    // receive new start event
    testWaitForEvent(w, 1500ms, Event::Notification::Success, Event::Type::Started, mkCnf(w, 3, true, {}));
    testWaitForEvent(w, 1500ms, Event::Notification::Success, Event::Type::Online);

    // produce a new event
    w.dispatch("t2"sv, zmq::Part{"hello2"sv});

    // receive new events
    testWaitForTopic(w, mkT("t2", 0, "hello2"), 4);
}


BOOST_FIXTURE_TEST_CASE(testSyncEmpty, WorkerFixture)
{
    w.sync();

    testWaitForSyncStart(w, b, mkCnf(w));
    testWaitForSyncStop(w, b);
}


BOOST_FIXTURE_TEST_CASE(testSyncElementAll, WorkerFixture)
{
    w.dispatch("t1"sv, zmq::Part{"hello1"sv});
    w.dispatch("t2"sv, zmq::Part{"hello2"sv});
    testWaitForTopic(w, mkT("t1", 1, "hello1"), 1);
    testWaitForTopic(w, mkT("t2", 2, "hello2"), 2);

    w.sync();

    testWaitForSyncStart(w, b, mkCnf(w, 2));
    testWaitForSyncTopic(w, mkT("t1", 1, "hello1"), 1);
    testWaitForSyncTopic(w, mkT("t2", 2, "hello2"), 2);
    testWaitForSyncStop(w, b);
}


BOOST_AUTO_TEST_CASE(testSyncElementFilter)
{
    // setup two identical workers
    Broker b{WorkerFixture::bid};
    Worker w1(Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker1.net"sv));
    Worker w2(Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker2.net"sv));

    w1.setTopicsAll();
    w2.setTopicsNames({"topic1"sv, "topic3"sv});

    auto bf = b.start();
    auto wf1 = w1.start();
    auto wf2 = w2.start();

    for (auto w : {&w1, &w2}) {
        testWaitForStart(*w, mkCnf(*w, 0,
                                 std::get<0>(w->topicsNames()),   //
                                 std::get<1>(w->topicsNames()))); //
    }

    auto t1 = mkT("topic1", 1, "hello").withWorker(w2.uuid());
    auto t2 = mkT("topic2", 2, "hello").withWorker(w2.uuid());
    auto t3 = mkT("topic3", 3, "hello").withWorker(w2.uuid());
    auto t4 = mkT("topic4", 4, "hello").withWorker(w2.uuid());

    // w2 can send every topic and w1 receives every topic
    for (auto t : {t1, t2, t3, t4}) {
        w2.dispatch(t.name(), t.data());
        testWaitForTopic(w1, t, t.seqNum());
    }

    // w2 receives subscribed topics only
    for (auto t : {t1, t3})
        testWaitForTopic(w2, t, t.seqNum());

    // w2 syncs with subscribed topics only
    w2.sync();

    testWaitForSyncStart(w2, b, mkCnf(w2, 4,
                                    std::get<0>(w2.topicsNames()),   //
                                    std::get<1>(w2.topicsNames()))); //
    testWaitForSyncTopic(w2, t1, 1);
    testWaitForSyncTopic(w2, t3, 3);
    testWaitForSyncStop(w2, b);

    b.stop();
    w1.stop();
    w2.stop();

    for (auto w : {&w1, &w2})
        testWaitForStop(*w);

    bf.get();
    wf1.get();
    wf2.get();
}


BOOST_AUTO_TEST_CASE(testSyncError_Halt)
{
    Worker w;

    auto wf = w.start();
    auto cnf = mkCnf(w);

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Started, cnf);

    w.sync();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOn);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncRequest, cnf);

    w.stop();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncError, Uuid{});
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOff);

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Stopped);
}


BOOST_AUTO_TEST_CASE(testSyncError_Timeout)
{
    Worker w;

    auto wf = w.start();
    auto cnf = mkCnf(w);

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Started, mkCnf(w));

    w.sync();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOn);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncRequest, cnf);
    testWaitForEvent(w, 5s, Event::Notification::Success, Event::Type::SyncRequest, cnf);
    testWaitForEvent(w, 5s, Event::Notification::Success, Event::Type::SyncError, Uuid{});
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOff);

    w.stop();
}


BOOST_FIXTURE_TEST_CASE(testSyncTopicRecentSameWorker, WorkerFixture)
{
    w.dispatch("topic"sv, zmq::Part{"hello1"sv});
    w.dispatch("topic"sv, zmq::Part{"hello2"sv});

    testWaitForTopic(w, mkT("topic", 0, "hello1"), 1);
    testWaitForTopic(w, mkT("topic", 0, "hello2"), 2);

    w.sync();

    testWaitForSyncStart(w, b, mkCnf(w, 2));
    testWaitForSyncTopic(w, mkT("topic", 2, "hello2"), 2);
    testWaitForSyncStop(w, b);
}


BOOST_FIXTURE_TEST_CASE(testSyncTopicRecentAnotherWorker, WorkerFixture)
{
    auto wid2 = Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker2.net"sv);
    Worker w2(wid2);
    auto wf2 = w2.start();

    testWaitForStart(w2);

    w.dispatch("topic"sv, zmq::Part{"hello1"sv});
    w.dispatch("topic"sv, zmq::Part{"hello1"sv});

    auto t1 = mkT("topic", 2, "hello1");
    auto t2 = mkT("topic", 1, "hello2").withWorker(wid2);

    testWaitForTopic(w, t1, 1);
    testWaitForTopic(w, t1, 2);
    testWaitForTopic(w2, t1, 1);
    testWaitForTopic(w2, t1, 2);

    w2.dispatch("topic"sv, zmq::Part{"hello2"sv});

    testWaitForTopic(w, t2, 1);
    testWaitForTopic(w2, t2, 1);

    w.sync();

    testWaitForSyncStart(w, b, mkCnf(w, 2));
    testWaitForSyncTopic(w, t2, 1);
    testWaitForSyncStop(w, b);

    w2.stop();
    wf2.get();

    testWaitForStop(w2);
}


BOOST_FIXTURE_TEST_CASE(testSyncSkipEvents, WorkerFixture)
{
    auto t1 = mkT("topic1", 1, "hello1");
    auto t2 = mkT("topic2", 2, "hello2");
    auto t3 = mkT("topic3", 3, "hello3");

    w.dispatch(t1.name(), t1.data(), Topic::State);
    w.dispatch(t2.name(), t2.data(), Topic::Event);
    w.dispatch(t3.name(), t3.data(), Topic::State);

    testWaitForTopic(w, t1, t1.seqNum());
    testWaitForTopic(w, t2, t2.seqNum());
    testWaitForTopic(w, t3, t3.seqNum());

    w.sync();

    testWaitForSyncStart(w, b, mkCnf(w, 3));
    testWaitForSyncTopic(w, t1, t1.seqNum());
    testWaitForSyncTopic(w, t3, t3.seqNum());
    testWaitForSyncStop(w, b);
}


BOOST_FIXTURE_TEST_CASE(testStoreEvents, WorkerFixture)
{
    // events are not synced with, however they are still stored
    // at broker side in order to deliver them only once.

    auto t = mkT("topic", 1, "hello");
    w.dispatch(t.name(), t.data(), Topic::Event);
    testWaitForTopic(w, t, t.seqNum());
    BOOST_TEST(w.seqNumber() == 1u);

    // clone worker (same uuid).
    Worker w2{w.uuid()};
    BOOST_TEST(w2.uuid() == w.uuid());
    BOOST_TEST(w2.seqNumber() == 0u);
    auto w2f = w2.start();
    testWaitForStart(w2);

    // same topic will be discarded.
    w2.dispatch(t.name(), t.data(), Topic::Event);
    w2.stop();
    testWaitForStop(w2);
    BOOST_TEST(w2.seqNumber() == 1u);
    w2f.get();
}


BOOST_AUTO_TEST_CASE(testTopicSubscriptionInvalid)
{
    Worker w(WorkerFixture::wid);
    w.setTopicsNames({"UPDT"sv});
    auto wf = w.start();
    BOOST_TEST(w.isRunning());
    BOOST_REQUIRE_THROW(wf.get(), err::Error);
    BOOST_TEST(!w.isRunning());
}


BOOST_AUTO_TEST_CASE(testTopicSubscriptionSimple)
{
    Broker b(WorkerFixture::bid);
    Worker w(WorkerFixture::wid);

    std::string longTopic;
    for (int i = 0; i < 16; ++i)
        longTopic += "very long topic ";
    longTopic.resize(longTopic.size() - 1);

    BOOST_TEST(longTopic.size() == 255u);

    const std::vector<Topic::Name> names{"short topic"sv, longTopic + "12345"};

    w.setTopicsNames(names);
    BOOST_TEST(std::get<0>(w.topicsNames()) == false);
    BOOST_TEST(std::get<1>(w.topicsNames()) == names);

    auto bf = b.start();
    auto wf = w.start();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Started,
        mkCnf(w, 0, false, {"short topic"sv, longTopic}));

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Online);

    w.dispatch("short topic"sv, zmq::Part{"hello1"sv});
    w.dispatch(longTopic, zmq::Part{"hello2"sv});
    w.dispatch("another topic"sv, zmq::Part{"hello3"sv});

    testWaitForTopic(w, mkT("short topic", 0, "hello1"), 1);
    testWaitForTopic(w, mkT(longTopic, 0, "hello2"), 2);

    testWaitForTimeout(w);

    w.stop();
    b.stop();

    wf.get();
    bf.get();
}


BOOST_AUTO_TEST_CASE(testTopicSubscriptionNone)
{
    Broker b(WorkerFixture::bid);
    Worker w(WorkerFixture::wid);

    w.setTopicsNames({});
    BOOST_TEST(std::get<0>(w.topicsNames()) == false);
    BOOST_TEST(std::get<1>(w.topicsNames()) == std::vector<Topic::Name>{});

    auto bf = b.start();
    auto wf = w.start();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Started, mkCnf(w, 0, false, {}));
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Online);

    w.dispatch("topic1"sv, zmq::Part{"hello1"sv});
    w.dispatch("topic2"sv, zmq::Part{"hello2"sv});
    w.dispatch("topic3"sv, zmq::Part{"hello3"sv});

    testWaitForTimeout(w);

    w.stop();
    b.stop();

    wf.get();
    bf.get();
}


BOOST_AUTO_TEST_CASE(testTopicSubscriptionDuplicate)
{
    Broker b{WorkerFixture::bid};
    Worker w{WorkerFixture::wid};

    const std::vector<Topic::Name> names{"topic1"sv, "topic2"sv, "topic1"sv};

    w.setTopicsNames(names);
    BOOST_TEST(std::get<0>(w.topicsNames()) == false);
    BOOST_TEST(std::get<1>(w.topicsNames()) == names);

    auto bf = b.start();
    auto wf = w.start();

    testWaitForStart(w, mkCnf(w, 0, false, {"topic1"sv, "topic2"sv, "topic1"sv}));

    w.dispatch("topic1"sv, zmq::Part{"hello1"sv});
    w.dispatch("topic2"sv, zmq::Part{"hello2"sv});

    testWaitForTopic(w, mkT("topic1", 0, "hello1"), 1);
    testWaitForTopic(w, mkT("topic2", 0, "hello2"), 2);

    w.stop();
    b.stop();

    testWaitForStop(w);

    wf.get();
    bf.get();
}


BOOST_AUTO_TEST_CASE(testTopicDeliveryGroup)
{
    Broker b{WorkerFixture::bid};
    Worker w1{Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker1.net"sv)};
    Worker w2{Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker2.net"sv)};

    w1.setTopicsAll();
    w2.setTopicsNames({"topic1"sv});

    BOOST_TEST(std::get<0>(w1.topicsNames()) == true);
    BOOST_TEST(std::get<1>(w1.topicsNames()) == std::vector<Topic::Name>{});
    BOOST_TEST(std::get<0>(w2.topicsNames()) == false);
    BOOST_TEST(std::get<1>(w2.topicsNames()) == std::vector<Topic::Name>{"topic1"sv});

    auto bf = b.start();
    auto w1f = w1.start();
    auto w2f = w2.start();

    testWaitForEvent(w1, 2s, Event::Notification::Success, Event::Type::Started,
        mkCnf(w1, 0, true, {}));
    testWaitForEvent(w1, 2s, Event::Notification::Success, Event::Type::Online);

    testWaitForEvent(w2, 2s, Event::Notification::Success, Event::Type::Started,
        mkCnf(w2, 0, false, {"topic1"sv}));
    testWaitForEvent(w2, 2s, Event::Notification::Success, Event::Type::Online);

    auto t1 = mkT("topic1", 0, "hello1").withWorker(w1.uuid());
    auto t2 = mkT("topic2", 0, "hello2").withWorker(w2.uuid());
    auto t3 = mkT("topic1", 0, "hello3").withWorker(w1.uuid());

    w1.dispatch(t1.name(), t1.data());
    w2.dispatch(t2.name(), t2.data()); // w2 dispatches topic2 which was not subscribed.
    w1.dispatch(t3.name(), t3.data());

    testWaitForTopic(w1, t1, 1);
    testWaitForTopic(w1, t2, 1);
    testWaitForTopic(w1, t3, 2);

    testWaitForTopic(w2, t1, 1);
    testWaitForTopic(w2, t3, 2);

    w1.stop();
    w2.stop();
    b.stop();

    testWaitForStop(w1);
    testWaitForStop(w2);

    w1f.get();
    w2f.get();
    bf.get();
}


BOOST_FIXTURE_TEST_CASE(testSeqnNotify, WorkerFixture)
{
    // test initial value
    BOOST_TEST(w.seqNumber() == 0ull);

    auto t1 = mkT("topic", 0, "hello");

    // test final value
    w.dispatch("topic"sv, zmq::Part{"hello"sv});
    w.dispatch("topic"sv, zmq::Part{"hello"sv});
    w.dispatch("topic"sv, zmq::Part{"hello"sv});

    testWaitForTopic(w, t1, 1);
    testWaitForTopic(w, t1, 2);
    testWaitForTopic(w, t1, 3);

    BOOST_TEST(w.seqNumber() == 3ull);
    BOOST_TEST(w.seqNumber() == 3ull);
    BOOST_TEST(w.seqNumber() == 3ull);

    w.dispatch("topic"sv, zmq::Part{"hello"sv});

    testWaitForTopic(w, t1, 4);

    BOOST_TEST(w.seqNumber() == 4ull);

    // stop
    w.stop();
    wf.get();

    testWaitForStop(w);

    // start again
    wf = w.start();

    testWaitForEvent(w, 1500ms, Event::Notification::Success, Event::Type::Started, mkCnf(w, 4, true, {}));
    testWaitForEvent(w, 1500ms, Event::Notification::Success, Event::Type::Online);

    w.dispatch("topic"sv, zmq::Part{"hello"sv});

    testWaitForTopic(w, t1, 5);

    BOOST_TEST(w.seqNumber() == 5ull);
}


BOOST_AUTO_TEST_CASE(testSeqnMaxDelivery)
{
    // setup two identical workers
    Broker b(Uuid::createRandomUuid());
    Worker w1(Uuid::createRandomUuid());
    Worker w2(w1.uuid());
    Worker w3(Uuid::createRandomUuid());

    auto bf = b.start();
    auto wf1 = w1.start();
    auto wf2 = w2.start();
    auto wf3 = w3.start();

    for (auto w : {&w1, &w2, &w3})
        testWaitForStart(*w);

    const auto t1 = Topic{b.uuid(), w1.uuid(), 0, "topic"sv, zmq::Part{"hello"sv}, Topic::State};

    // primary worker increments sequence number.
    for (auto i = 1; i <= 3; ++i) {
        w1.dispatch(t1.name(), t1.data());

        for (auto w : {&w1, &w2, &w3})
            testWaitForTopic(*w, t1, i);
    }

    b.stop();
    w1.stop();
    w2.stop();
    w3.stop();

    for (auto w : {&w1, &w2, &w3})
        testWaitForStop(*w);

    // clone worker keeps the maximum delivered sequence number.
    BOOST_TEST(w1.seqNumber() == 3u);
    BOOST_TEST(w2.seqNumber() == 3u);
    BOOST_TEST(w3.seqNumber() == 0u);
}


BOOST_AUTO_TEST_CASE(testBrokerDiscardDelivery)
{
    // setup two identical workers
    Broker b(Uuid::createRandomUuid());
    Worker w1(Uuid::createRandomUuid());
    Worker w2(w1.uuid());

    const auto t1 = Topic{b.uuid(), w1.uuid(), 0, "topic"sv, zmq::Part{"hello"sv}, Topic::State};
    const auto t2 = Topic{b.uuid(), w2.uuid(), 0, "topic"sv, zmq::Part{"hello"sv}, Topic::State};

    auto bf = b.start();
    auto wf1 = w1.start();

    testWaitForStart(w1);

    // primary worker increments sequence number.
    for (auto i = 1; i <= 3; ++i) {
        w1.dispatch(t1.name(), t1.data());
        testWaitForTopic(w1, t1, i);
    }

    // clone will dispatch topics, broker shall discard them.
    auto wf2 = w2.start();

    testWaitForStart(w2);

    for (auto i = 1; i <= 2; ++i)
        w2.dispatch(t2.name(), t2.data());

    testWaitForTimeout(w1);
    testWaitForTimeout(w2);

    b.stop();
    w1.stop();
    w2.stop();

    testWaitForStop(w1);
    testWaitForStop(w2);

    // primary worker keeps previous sequence number.
    BOOST_TEST(w1.seqNumber() == 3u);
    BOOST_TEST(w2.seqNumber() == 2u);
}


BOOST_AUTO_TEST_CASE(testWorkerDiscardDelivery)
{
    // setup two identical workers
    Broker b(Uuid::createRandomUuid());
    Worker w1(Uuid::createRandomUuid());
    Worker w2(w1.uuid());

    const auto t1 = Topic{b.uuid(), w1.uuid(), 0, "topic"sv, zmq::Part{"hello"sv}, Topic::State};
    const auto t2 = Topic{b.uuid(), w2.uuid(), 0, "topic"sv, zmq::Part{"hello"sv}, Topic::State};

    auto bf = b.start();
    auto wf1 = w1.start();

    testWaitForStart(w1);

    // primary worker increments sequence number.
    for (auto i = 1; i <= 3; ++i) {
        w1.dispatch(t1.name(), t1.data());
        testWaitForTopic(w1, t1, i);
    }

    // clone will dispatch topics, worker shall discard them, restart broker.
    b.stop();
    bf.get();
    bf = b.start();
    auto wf2 = w2.start();

    testWaitForStart(w2);

    for (auto i = 1; i <= 2; ++i) {
        w2.dispatch(t2.name(), t2.data());
        testWaitForTopic(w2, t2, i);
    }

    testWaitForTimeout(w1);

    // primary worker keeps previous sequence number.
    BOOST_TEST(w1.seqNumber() == 3u);
    BOOST_TEST(w2.seqNumber() == 2u);

    // primary gets new updates.
    for (auto i = 3; i <= 4; ++i) {
        w2.dispatch(t2.name(), t2.data());
        testWaitForTopic(w2, t2, i);
    }

    testWaitForTopic(w1, t2, 4);

    b.stop();
    w1.stop();
    w2.stop();

    testWaitForStop(w1);
    testWaitForStop(w2);

    BOOST_TEST(w1.seqNumber() == 4u);
    BOOST_TEST(w2.seqNumber() == 4u);
}


BOOST_AUTO_TEST_CASE(testWorkerSeqSync)
{
    // setup two identical workers
    Broker b(Uuid::createRandomUuid());
    Worker w1(Uuid::createRandomUuid());
    Worker w2(w1.uuid());

    const std::vector<Topic> t{
        {b.uuid(), w1.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}, Topic::State},
        {b.uuid(), w1.uuid(), 0, "topic2"sv, zmq::Part{"hello2"sv}, Topic::State},
        {b.uuid(), w1.uuid(), 0, "topic3"sv, zmq::Part{"hello3"sv}, Topic::State},
        {b.uuid(), w1.uuid(), 0, "topic4"sv, zmq::Part{"hello4"sv}, Topic::State},
    };

    auto bf = b.start();
    auto wf1 = w1.start();

    testWaitForStart(w1);

    // primary worker increments sequence number.
    for (auto i = 0; i < 4; ++i) {
        w1.dispatch(t[i].name(), t[i].data());
        testWaitForTopic(w1, t[i], i + 1);
    }

    BOOST_TEST(w1.seqNumber() == 4u);

    // clone will sync with latest sequence number.
    auto wf2 = w2.start();

    testWaitForStart(w2);

    BOOST_TEST(w2.seqNumber() == 0u);

    // clone dispatches twice, broker will filter them out.
    for (auto i = 0; i < 2; ++i)
        w2.dispatch(t[i].name(), t[i].data());

    testWaitForTimeout(w2);

    BOOST_TEST(w2.seqNumber() == 2u);

    // clone will get every topic, without sequence number filtering
    w2.sync();

    testWaitForSyncStart(w2, b, mkCnf(w2, 2));

    for (auto i = 0; i < 4; ++i)
        testWaitForSyncTopic(w2, t[i], i + 1);

    testWaitForSyncStop(w2, b);

    BOOST_TEST(w2.seqNumber() == 4u);

    b.stop();
    w1.stop();
    w2.stop();

    testWaitForStop(w1);
    testWaitForStop(w2);
}


static void BM_workerStart(benchmark::State& state)
{
    for (auto _ : state) {
        Worker w;
        auto fut = w.start();
        w.stop();
        fut.get();
    }
}
BENCHMARK(BM_workerStart);


BOOST_AUTO_TEST_CASE(bench, *utf::disabled())
{
    ::benchmark::RunSpecifiedBenchmarks();
}

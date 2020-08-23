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

#include "fuurin/broker.h"
#include "fuurin/worker.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/elapser.h"

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


Event testWaitForEvent(Worker& w, std::chrono::milliseconds timeout, Event::Notification evRet,
    Event::Type evType, const zmq::Part& evPay = zmq::Part{})
{
    const auto& ev = w.waitForEvent(timeout);

    BOOST_TEST(ev.notification() == evRet);
    BOOST_TEST(ev.type() == evType);

    if (!evPay.empty()) {
        BOOST_TEST(!ev.payload().empty());
        BOOST_TEST(ev.payload() == evPay);
    } else {
        BOOST_TEST(ev.payload().empty());
    }

    return ev;
}


struct WorkerFixture
{
    WorkerFixture()
        : w(wid)
        , b(bid)
        , wf(w.start())
        , bf(b.start())
    {
        testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Started);
        testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Online);
    }

    ~WorkerFixture()
    {
        w.stop();
        b.stop();

        testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Offline);
        testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Stopped);
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


zmq::Part mkT(const std::string& name, const std::string& pay)
{
    using f = WorkerFixture;
    return Topic{f::bid, f::wid, Topic::SeqN{}, name, zmq::Part{pay}}.toPart();
}


BOOST_AUTO_TEST_CASE(testUuid)
{
    auto id = WorkerFixture::wid;
    Worker w(id);
    BOOST_TEST(w.uuid() == id);
}


BOOST_AUTO_TEST_CASE(testDeliverUuid)
{
    Worker w(WorkerFixture::wid);
    Broker b(WorkerFixture::bid);

    auto wf = w.start();
    auto bf = b.start();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Started);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Online);

    w.dispatch("topic"sv, zmq::Part{"hello"sv});

    auto ev = testWaitForEvent(w, 5s, Event::Notification::Success,
        Event::Type::Delivery, mkT("topic", "hello"));

    auto t = Topic::fromPart(ev.payload());

    BOOST_TEST(t.worker() == WorkerFixture::wid);
    BOOST_TEST(t.broker() == WorkerFixture::bid);

    w.stop();
    b.stop();
}


BOOST_AUTO_TEST_CASE(testTopicPart)
{
    using f = WorkerFixture;
    const Topic t1{f::bid, f::wid, 256, "topic/test"sv, zmq::Part{"topic/data"sv}};
    const Topic t2{Topic::fromPart(t1.toPart())};

    BOOST_TEST(t1 == t2);
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
            static_cast<Elapser&&>(*this), std::bind(&TestRunner::recvEvent, this), timeout);
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


BOOST_AUTO_TEST_CASE(testWaitForStart)
{
    Worker w;

    auto wf = w.start();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Started);

    w.stop();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Stopped);
}


BOOST_FIXTURE_TEST_CASE(testWaitForOnline, WorkerFixture)
{
}


BOOST_FIXTURE_TEST_CASE(testWaitForOffline, WorkerFixture)
{
    b.stop();
    bf.get();
    testWaitForEvent(w, 6s, Event::Notification::Success, Event::Type::Offline);

    bf = b.start();
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Online);
}


BOOST_FIXTURE_TEST_CASE(testSimpleDispatch, WorkerFixture)
{
    w.dispatch("topic"sv, zmq::Part{"hello"sv});

    testWaitForEvent(w, 5s, Event::Notification::Success, Event::Type::Delivery, mkT("topic", "hello"));
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
    testWaitForEvent(w, 1500ms, Event::Notification::Success,
        Event::Type::Delivery, mkT("t1", "hello1"));

    // wait for other events to be received
    std::this_thread::sleep_for(1s);

    // start a new session without reading events
    w.stop();
    wf.get();
    wf = w.start();

    // discard old events
    for (int i = 0; i < iterations - 1; ++i) {
        testWaitForEvent(w, 1500ms, Event::Notification::Discard,
            Event::Type::Delivery, mkT("t1", "hello1"));
    }

    // discard old stop event
    testWaitForEvent(w, 1500ms, Event::Notification::Discard, Event::Type::Offline);
    testWaitForEvent(w, 1500ms, Event::Notification::Discard, Event::Type::Stopped);

    // receive new start event
    testWaitForEvent(w, 1500ms, Event::Notification::Success, Event::Type::Started);
    testWaitForEvent(w, 1500ms, Event::Notification::Success, Event::Type::Online);

    // produce a new event
    w.dispatch("t2"sv, zmq::Part{"hello2"sv});

    // receive new events
    testWaitForEvent(w, 1500ms, Event::Notification::Success,
        Event::Type::Delivery, mkT("t2", "hello2"));
}


BOOST_FIXTURE_TEST_CASE(testSyncEmpty, WorkerFixture)
{
    w.sync();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOn);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncBegin);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncSuccess);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOff);
}


BOOST_FIXTURE_TEST_CASE(testSyncElement, WorkerFixture)
{
    w.dispatch("t1"sv, zmq::Part{"hello1"sv});
    w.dispatch("t2"sv, zmq::Part{"hello2"sv});
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Delivery, mkT("t1", "hello1"));
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Delivery, mkT("t2", "hello2"));

    w.sync();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOn);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncBegin);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncElement, mkT("t1", "hello1"));
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncElement, mkT("t2", "hello2"));
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncSuccess);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOff);
}


BOOST_AUTO_TEST_CASE(testSyncError_Halt)
{
    Worker w;

    auto wf = w.start();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Started);

    w.sync();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOn);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncBegin);

    w.stop();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncError);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOff);

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Stopped);
}


BOOST_AUTO_TEST_CASE(testSyncError_Timeout)
{
    Worker w;

    auto wf = w.start();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Started);

    w.sync();

    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOn);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncBegin);
    testWaitForEvent(w, 5s, Event::Notification::Success, Event::Type::SyncBegin);
    testWaitForEvent(w, 5s, Event::Notification::Success, Event::Type::SyncError);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOff);

    w.stop();
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

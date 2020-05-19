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
#include "types.h"

#include <string_view>
#include <chrono>


using namespace fuurin;
namespace utf = boost::unit_test;

using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;


/**
 * BOOST_TEST print functions.
 */
namespace fuurin {
inline std::ostream& operator<<(std::ostream& os, const Runner::EventRead& rd)
{
    switch (rd) {
    case Runner::EventRead::Timeout:
        os << "timeout";
        break;
    case Runner::EventRead::Success:
        os << "success";
        break;
    case Runner::EventRead::Discard:
        os << "discard";
        break;
    }
    return os;
}
} // namespace fuurin


void testWaitForEvent(Worker& w, std::chrono::milliseconds timeout, Runner::EventRead evRet, Runner::event_type_t evType, const std::string& evPay = std::string())
{
    const auto [type, pay, ret] = w.waitForEvent(timeout);

    BOOST_TEST(ret == evRet);
    BOOST_TEST(Worker::eventToString(type) == Worker::eventToString(evType));

    if (!evPay.empty()) {
        BOOST_TEST(!pay.empty());
        BOOST_TEST(pay.toString() == std::string_view(evPay));
    } else {
        BOOST_TEST(pay.empty());
    }
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

    testWaitForEvent(w, 2s, Runner::EventRead::Success,
        toIntegral(Runner::EventType::Started));

    w.stop();

    testWaitForEvent(w, 2s, Runner::EventRead::Success,
        toIntegral(Runner::EventType::Stopped));
}


BOOST_AUTO_TEST_CASE(testWaitForOnline)
{
    Worker w;
    Broker b;

    auto wf = w.start();
    auto bf = b.start();

    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Runner::EventType::Started));
    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Worker::EventType::Online));

    w.stop();
    b.stop();

    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Worker::EventType::Offline));
    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Runner::EventType::Stopped));
}


BOOST_AUTO_TEST_CASE(testWaitForOffline)
{
    Worker w;
    Broker b;

    auto wf = w.start();
    auto bf = b.start();

    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Runner::EventType::Started));
    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Worker::EventType::Online));

    b.stop();
    bf.get();
    testWaitForEvent(w, 6s, Runner::EventRead::Success, toIntegral(Worker::EventType::Offline));

    bf = b.start();
    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Worker::EventType::Online));

    w.stop();
    b.stop();

    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Worker::EventType::Offline));
    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Runner::EventType::Stopped));
}


BOOST_AUTO_TEST_CASE(testSimpleDispatch)
{
    Worker w;
    Broker b;

    auto wf = w.start();
    auto bf = b.start();

    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Runner::EventType::Started));
    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Worker::EventType::Online));

    w.dispatch(zmq::Part{"hello"sv});

    testWaitForEvent(w, 5s, Runner::EventRead::Success,
        toIntegral(Worker::EventType::Delivery), "hello");

    w.stop();
    b.stop();

    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Worker::EventType::Offline));
    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Runner::EventType::Stopped));
}


BOOST_AUTO_TEST_CASE(testWaitForEventThreadSafe)
{
    Worker w;
    Broker b;

    auto wf = w.start();
    auto bf = b.start();

    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Runner::EventType::Started));
    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Worker::EventType::Online));

    const auto recvEvent = [&w]() {
        int cnt = 0;
        for (;;) {
            const auto [ev, pay, ret] = w.waitForEvent(1500ms);
            UNUSED(ev);
            if (pay.empty() || ret != Runner::EventRead::Success)
                break;
            ++cnt;
        }
        return cnt;
    };

    const int iterations = 100;

    for (int i = 0; i < iterations; ++i)
        w.dispatch(zmq::Part{"hello"sv});

    auto f1 = std::async(std::launch::async, recvEvent);
    auto f2 = std::async(std::launch::async, recvEvent);

    const int cnt1 = f1.get();
    const int cnt2 = f2.get();

    BOOST_TEST(cnt1 != 0);
    BOOST_TEST(cnt2 != 0);
    BOOST_TEST(cnt1 + cnt2 == iterations);

    w.stop();
    b.stop();

    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Worker::EventType::Offline));
    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Runner::EventType::Stopped));
}


BOOST_AUTO_TEST_CASE(testWaitForEventDiscard)
{
    Worker w;
    Broker b;

    auto wf = w.start();
    auto bf = b.start();

    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Runner::EventType::Started));
    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Worker::EventType::Online));

    // produce some events
    const int iterations = 3;
    for (int i = 0; i < iterations; ++i)
        w.dispatch(zmq::Part{"hello1"sv});

    // receive just one event
    testWaitForEvent(w, 1500ms, Runner::EventRead::Success,
        toIntegral(Worker::EventType::Delivery), "hello1");

    // wait for other events to be received
    std::this_thread::sleep_for(1s);

    // start a new session without reading events
    w.stop();
    wf.get();
    wf = w.start();

    // discard old events
    for (int i = 0; i < iterations - 1; ++i) {
        testWaitForEvent(w, 1500ms, Runner::EventRead::Discard,
            toIntegral(Worker::EventType::Delivery), "hello1");
    }

    // discard old stop event
    testWaitForEvent(w, 1500ms, Runner::EventRead::Discard, toIntegral(Worker::EventType::Offline));
    testWaitForEvent(w, 1500ms, Runner::EventRead::Discard, toIntegral(Runner::EventType::Stopped));

    // receive new start event
    testWaitForEvent(w, 1500ms, Runner::EventRead::Success, toIntegral(Runner::EventType::Started));
    testWaitForEvent(w, 1500ms, Runner::EventRead::Success, toIntegral(Worker::EventType::Online));

    // produce a new event
    w.dispatch(zmq::Part{"hello2"sv});

    // receive new events
    testWaitForEvent(w, 1500ms, Runner::EventRead::Success,
        toIntegral(Worker::EventType::Delivery), "hello2");

    w.stop();
    b.stop();

    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Worker::EventType::Offline));
    testWaitForEvent(w, 2s, Runner::EventRead::Success, toIntegral(Runner::EventType::Stopped));
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

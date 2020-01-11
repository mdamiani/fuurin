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

#include "fuurin/broker.h"
#include "fuurin/worker.h"
#include "fuurin/zmqpart.h"

#include <string_view>
#include <chrono>


using namespace fuurin;
namespace utf = boost::unit_test;

using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;


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


BOOST_AUTO_TEST_CASE(simpleStore)
{
    Worker w;
    Broker b;

    auto wf = w.start();
    auto bf = b.start();

    w.store(zmq::Part{"hello"sv});

    const auto [ev, ret] = w.waitForEvent(5s);

    BOOST_TEST(ret == Runner::EventRead::Success);
    BOOST_TEST(!ev.empty());
    BOOST_TEST(ev.toString() == "hello"sv);

    w.stop();
    b.stop();
}


BOOST_AUTO_TEST_CASE(waitForEventThreadSafe)
{
    Worker w;
    Broker b;

    auto wf = w.start();
    auto bf = b.start();

    const auto recvEvent = [&w]() {
        int cnt = 0;
        for (;;) {
            const auto [ev, ret] = w.waitForEvent(1500ms);
            if (ev.empty() || ret != Runner::EventRead::Success)
                break;
            ++cnt;
        }
        return cnt;
    };

    const int iterations = 100;

    for (int i = 0; i < iterations; ++i)
        w.store(zmq::Part{"hello"sv});

    auto f1 = std::async(std::launch::async, recvEvent);
    auto f2 = std::async(std::launch::async, recvEvent);

    const int cnt1 = f1.get();
    const int cnt2 = f2.get();

    BOOST_TEST(cnt1 != 0);
    BOOST_TEST(cnt2 != 0);
    BOOST_TEST(cnt1 + cnt2 == iterations);

    w.stop();
    b.stop();
}


BOOST_AUTO_TEST_CASE(waitForEventDiscard)
{
    Worker w;
    Broker b;

    auto wf = w.start();
    auto bf = b.start();

    // produce some events
    const int iterations = 3;
    for (int i = 0; i < iterations; ++i)
        w.store(zmq::Part{"hello1"sv});

    // receive just one event
    {
        const auto [ev, ret] = w.waitForEvent(1500ms);
        BOOST_TEST(ret == Runner::EventRead::Success);
        BOOST_TEST(ev.toString() == "hello1"sv);
    }

    // wait for other events to be received
    std::this_thread::sleep_for(1s);

    // start a new session without reading events
    w.stop();
    wf.get();
    wf = w.start();

    // produce a new event
    w.store(zmq::Part{"hello2"sv});

    // discard old events
    for (int i = 0; i < iterations - 1; ++i) {
        const auto [ev, ret] = w.waitForEvent(1500ms);
        BOOST_TEST(ret == Runner::EventRead::Discard);
        BOOST_TEST(ev.toString() == "hello1"sv);
    }

    // receive new events
    {
        const auto [ev, ret] = w.waitForEvent(1500ms);
        BOOST_TEST(ret == Runner::EventRead::Success);
        BOOST_TEST(ev.toString() == "hello2"sv);
    }

    w.stop();
    b.stop();
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

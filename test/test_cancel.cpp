/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE cancel
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/mpl/list.hpp>
#include <benchmark/benchmark.h>

#include "fuurin/zmqcontext.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqcancel.h"
#include "fuurin/stopwatch.h"
#include "fuurin/errors.h"

#include <string_view>
#include <chrono>
#include <thread>
#include <future>


using namespace fuurin::zmq;
namespace utf = boost::unit_test;

using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;

namespace std {
namespace chrono {
std::ostream& operator<<(std::ostream& os, const ::std::chrono::milliseconds& ns)
{
    return os << ns.count() << "ms";
}
} // namespace chrono
} // namespace std


BOOST_AUTO_TEST_CASE(cancelableInit)
{
    Context ctx;

    BOOST_REQUIRE_THROW(Cancellation c(&ctx, ""),
        fuurin::err::ZMQSocketBindFailed);

    Cancellation c{&ctx, "canc1"};

    BOOST_TEST(c.zmqPointer() != nullptr);
    BOOST_TEST(c.description() == "canc1");
    BOOST_TEST(c.isOpen());
    BOOST_TEST(c.deadline() == 0ms);
    BOOST_TEST(!c.isCanceled());
}


BOOST_AUTO_TEST_CASE(cancelableCancel)
{
    Context ctx;
    Cancellation c{&ctx, "canc1"};

    Poller poll{PollerEvents::Type::Read, 1s, &c};

    BOOST_TEST(!c.isCanceled());

    c.cancel();

    const auto events = poll.wait();
    BOOST_TEST(!events.empty());
    BOOST_TEST(events[0] == &c);
    BOOST_TEST(c.isCanceled());

    // still canceled
    BOOST_TEST(!poll.wait().empty());
}


BOOST_AUTO_TEST_CASE(cancelableDeadline)
{
    Context ctx;
    Cancellation c{&ctx, "canc1"};

    fuurin::StopWatch t;
    t.start();

    Poller poll{PollerEvents::Type::Read, 5s, &c};

    BOOST_TEST(!c.isCanceled());

    // negative deadline is discarded
    c.withDeadline(-1ms);
    BOOST_TEST(!c.isCanceled());
    BOOST_TEST(c.deadline() == 0ms);

    // apply a valid deadline
    c.withDeadline(1s);

    BOOST_TEST(c.deadline() == 1s);

    const auto events = poll.wait();
    BOOST_TEST(!events.empty());
    BOOST_TEST(events[0] == &c);
    BOOST_TEST(c.isCanceled());

    BOOST_TEST(t.elapsed() >= 1s);
    BOOST_TEST(t.elapsed() <= 5s);

    // subsequent deadlines won't restart
    t.start();
    c.withDeadline(5s);
    BOOST_TEST(!poll.wait().empty());
    BOOST_TEST(t.elapsed() <= 1s);
    BOOST_TEST(c.deadline() == 1s);
}


BOOST_AUTO_TEST_CASE(cancelableThreads)
{
    Context ctx;
    Cancellation c{&ctx, "canc1"};

    const auto pollFn = [&c] {
        Poller poll{PollerEvents::Type::Read, 5s, &c};
        poll.wait();
    };

    fuurin::StopWatch t;
    t.start();

    auto f1 = std::async(std::launch::async, pollFn);
    auto f2 = std::async(std::launch::async, pollFn);
    auto f3 = std::async(std::launch::async, pollFn);

    c.setDeadline(1s);

    f1.get();
    f2.get();
    f3.get();

    BOOST_TEST(t.elapsed() >= 1s);
    BOOST_TEST(t.elapsed() <= 5s);
}

/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE timers
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/mpl/list.hpp>
#include <benchmark/benchmark.h>

#include "fuurin/zmqcontext.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqtimer.h"
#include "fuurin/errors.h"

#include <string_view>
#include <chrono>
#include <thread>


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


BOOST_AUTO_TEST_CASE(timerCreate)
{
    Context ctx;

    BOOST_REQUIRE_THROW(Timer t(&ctx, ""),
        fuurin::err::ZMQSocketBindFailed);

    Timer t{&ctx, "timer1"};

    BOOST_TEST(t.description() == "timer1");
    BOOST_TEST(t.isOpen());

    t.setInterval(-1ms);
    BOOST_TEST(t.interval() == 0ms);

    t.setInterval(100ms);
    BOOST_TEST(t.interval() == 100ms);

    BOOST_TEST(!t.isSingleShot());
    t.setSingleShot(true);
    BOOST_TEST(t.isSingleShot());
}


BOOST_AUTO_TEST_CASE(timerSingleShot)
{
    Context ctx;
    Timer t{&ctx, "timer1"};
    t.setInterval(100ms);
    t.setSingleShot(true);

    Poller poll{PollerEvents::Type::Read, &t};
    poll.setTimeout(1s);

    BOOST_TEST(!t.isActive());

    t.start();

    BOOST_TEST(t.isActive());

    const auto events = poll.wait();
    BOOST_TEST(!events.empty());
    BOOST_TEST(events[0] == &t);
    BOOST_TEST(t.isExpired());
    BOOST_TEST(!t.isActive());

    t.consume();
    BOOST_TEST(!t.isExpired());

    // no more ticks
    BOOST_TEST(poll.wait().empty());

    t.stop();
    BOOST_TEST(!t.isActive());
}


BOOST_AUTO_TEST_CASE(timerStop)
{
    Context ctx;
    Timer t{&ctx, "timer1"};
    t.setInterval(5s);

    BOOST_TEST(!t.isActive());
    BOOST_TEST(!t.isExpired());

    Poller poll{PollerEvents::Type::Read, &t};
    poll.setTimeout(1s);

    t.start();

    BOOST_TEST(t.isActive());
    BOOST_TEST(!t.isExpired());

    std::this_thread::sleep_for(500ms);
    t.stop();

    BOOST_TEST(poll.wait().empty());

    BOOST_TEST(!t.isActive());
    BOOST_TEST(!t.isExpired());
}


BOOST_AUTO_TEST_CASE(timerInstant)
{
    Context ctx;
    Timer t{&ctx, "timer1"};
    t.setInterval(-1s); // force 0ms
    t.setSingleShot(true);
    t.start();
    t.consume();
    t.stop();
    BOOST_TEST(!t.isExpired());
    BOOST_TEST(!t.isActive());
}


BOOST_AUTO_TEST_CASE(timerRestart)
{
    Context ctx;
    Timer t{&ctx, "timer1"};
    t.setInterval(500ms);
    t.start();

    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(100ms);
        t.start();
    }

    Poller poll{PollerEvents::Type::Read, &t};
    poll.setTimeout(0ms);

    BOOST_TEST(poll.wait().empty());
}

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


using namespace fuurin::zmq;
namespace utf = boost::unit_test;

using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;


BOOST_AUTO_TEST_CASE(socketTimer)
{
    Context ctx;

    BOOST_REQUIRE_THROW(Timer t(&ctx, ""),
        fuurin::err::ZMQSocketBindFailed);

    Timer t{&ctx, "timer1"};

    BOOST_TEST(t.description() == "timer1");
    BOOST_TEST(t.isOpen());

    Poller poll{PollerEvents::Type::Read, &t};
}

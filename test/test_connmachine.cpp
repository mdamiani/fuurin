/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE workerconn
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include "connmachine.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqtimer.h"
#include "fuurin/errors.h"
#include "types.h"

#include <string_view>
#include <chrono>
#include <tuple>
#include <optional>
#include <memory>
#include <thread>


using namespace fuurin;
namespace utf = boost::unit_test;

using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;


/**
 * BOOST_TEST print functions.
 */
namespace std {
namespace chrono {
inline std::ostream& operator<<(std::ostream& os, const std::chrono::milliseconds& millis)
{
    os << millis.count();
    return os;
}
} // namespace chrono
} // namespace std

namespace fuurin {
inline std::ostream& operator<<(std::ostream& os, const ConnMachine::State& st)
{
    os << toIntegral(st);
    return os;
}
} // namespace fuurin


std::tuple<ConnMachine&, std::function<void(ConnMachine::State, bool, int, int, bool, bool)>>
setupConn()
{
    struct T
    {
        int resetCnt = 0;
        int pongCnt = 0;
        std::optional<ConnMachine::State> stateCurr;

        zmq::Context ctx;
        ConnMachine conn = ConnMachine{
            &ctx, 500ms, 2s,
            [this]() { ++resetCnt; },
            [this]() { ++pongCnt; },
            [this](ConnMachine::State s) { stateCurr = s; }};
    };

    const auto t = std::make_shared<T>();

    const auto testConn = [t](ConnMachine::State state, bool updated, int reset, int pong,
                              bool isRetryActive, bool isTimeoutActive) {
        BOOST_TEST(t->conn.timerRetry() != nullptr);
        BOOST_TEST(t->conn.timerTimeout() != nullptr);
        BOOST_TEST(t->conn.timerRetry()->isSingleShot() == false);
        BOOST_TEST(t->conn.timerTimeout()->isSingleShot() == true);
        BOOST_TEST(t->conn.timerRetry()->interval() == 500ms);
        BOOST_TEST(t->conn.timerTimeout()->interval() == 2s);
        BOOST_TEST(t->conn.timerRetry()->isActive() == isRetryActive);
        BOOST_TEST(t->conn.timerTimeout()->isActive() == isTimeoutActive);
        BOOST_TEST(t->conn.state() == state);
        BOOST_TEST(t->resetCnt == reset);
        BOOST_TEST(t->pongCnt == pong);
        if (updated) {
            BOOST_TEST(t->stateCurr.has_value());
            BOOST_TEST(t->stateCurr.value() == state);
        } else {
            BOOST_TEST(!t->stateCurr.has_value());
        }
    };

    return std::make_tuple(std::ref(t->conn), testConn);
}


BOOST_AUTO_TEST_CASE(testInitConn)
{
    auto [conn, testConn] = setupConn();
    UNUSED(conn);
    testConn(ConnMachine::State::Trying, false, 1, 1, true, true);
}


BOOST_AUTO_TEST_CASE(testOnPingTrying)
{
    auto [conn, testConn] = setupConn();
    testConn(ConnMachine::State::Trying, false, 1, 1, true, true);

    conn.onPing();
    testConn(ConnMachine::State::Stable, true, 1, 2, false, true);
}


BOOST_AUTO_TEST_CASE(testOnPingStable)
{
    auto [conn, testConn] = setupConn();
    testConn(ConnMachine::State::Trying, false, 1, 1, true, true);

    for (int i = 0; i < 2; ++i) {
        conn.onPing();
        testConn(ConnMachine::State::Stable, true, 1, i + 2, false, true);
    }
}


BOOST_AUTO_TEST_CASE(testOnTimerRetryTrying)
{
    auto [conn, testConn] = setupConn();
    testConn(ConnMachine::State::Trying, false, 1, 1, true, true);

    conn.onTimerRetryFired();
    testConn(ConnMachine::State::Trying, false, 1, 2, true, true);
}


BOOST_AUTO_TEST_CASE(testOnTimerRetryStable)
{
    auto [conn, testConn] = setupConn();
    testConn(ConnMachine::State::Trying, false, 1, 1, true, true);

    conn.onPing();
    testConn(ConnMachine::State::Stable, true, 1, 2, false, true);

    BOOST_REQUIRE_THROW(conn.onTimerRetryFired(), fuurin::err::Error);
}


BOOST_AUTO_TEST_CASE(testOnTimerTimeoutTrying)
{
    auto [conn, testConn] = setupConn();
    testConn(ConnMachine::State::Trying, false, 1, 1, true, true);

    conn.onTimerTimeoutFired();
    testConn(ConnMachine::State::Trying, false, 2, 2, true, true);
}


BOOST_AUTO_TEST_CASE(testOnTimerTimeoutStable)
{
    auto [conn, testConn] = setupConn();
    testConn(ConnMachine::State::Trying, false, 1, 1, true, true);

    conn.onPing();
    testConn(ConnMachine::State::Stable, true, 1, 2, false, true);

    conn.onTimerTimeoutFired();
    testConn(ConnMachine::State::Trying, true, 2, 3, true, true);
}


BOOST_AUTO_TEST_CASE(testConsumeTimerTrying)
{
    zmq::Context ctx;
    ConnMachine conn{&ctx, 10s, 10s, []() {}, []() {}, [](ConnMachine::State) {}};
    BOOST_TEST(!conn.timerRetry()->isExpired());

    conn.onTimerRetryFired();
    BOOST_TEST(!conn.timerRetry()->isExpired());

    conn.timerRetry()->stop();
    conn.timerRetry()->setSingleShot(true);
    conn.timerRetry()->setInterval(1ms);
    conn.timerRetry()->start();

    std::this_thread::sleep_for(100ms);
    BOOST_TEST(conn.timerRetry()->isExpired());

    conn.onTimerRetryFired();
    BOOST_TEST(!conn.timerRetry()->isExpired());
}


BOOST_AUTO_TEST_CASE(testConsumeTimerTimeout)
{
    zmq::Context ctx;
    ConnMachine conn{&ctx, 10s, 10s, []() {}, []() {}, [](ConnMachine::State) {}};
    BOOST_TEST(!conn.timerTimeout()->isExpired());

    conn.onTimerTimeoutFired();
    BOOST_TEST(!conn.timerTimeout()->isExpired());

    conn.timerTimeout()->stop();
    conn.timerTimeout()->setSingleShot(true);
    conn.timerTimeout()->setInterval(1ms);
    conn.timerTimeout()->start();

    std::this_thread::sleep_for(100ms);
    BOOST_TEST(conn.timerTimeout()->isExpired());

    conn.onTimerTimeoutFired();
    BOOST_TEST(!conn.timerTimeout()->isExpired());
}

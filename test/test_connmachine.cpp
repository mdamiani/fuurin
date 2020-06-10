/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE connmachine
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

using namespace std::literals::string_literals;
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
    switch (st) {
    case ConnMachine::State::Halted:
        os << "halted";
        break;
    case ConnMachine::State::Trying:
        os << "trying";
        break;
    case ConnMachine::State::Stable:
        os << "stable";
        break;
    }
    return os;
}
} // namespace fuurin


std::tuple<ConnMachine&, std::function<void(ConnMachine::State, bool, int, int, int, bool, bool)>>
setupConn()
{
    struct T
    {
        int closeCnt = 0;
        int openCnt = 0;
        int pongCnt = 0;
        std::optional<ConnMachine::State> stateCurr;

        zmq::Context ctx;
        ConnMachine conn = ConnMachine{
            "conn"sv, &ctx, 500ms, 2s,
            [this]() { ++closeCnt; },
            [this]() { ++openCnt; },
            [this]() { ++pongCnt; },
            [this](ConnMachine::State s) { stateCurr = s; }};
    };

    const auto t = std::make_shared<T>();

    const auto testConn = [t](ConnMachine::State state, bool updated, int close, int open, int pong,
                              bool isRetryActive, bool isTimeoutActive) {
        BOOST_TEST(t->conn.name() == "conn"sv);
        BOOST_TEST(t->conn.timerRetry() != nullptr);
        BOOST_TEST(t->conn.timerTimeout() != nullptr);
        BOOST_TEST(t->conn.timerRetry()->isSingleShot() == false);
        BOOST_TEST(t->conn.timerTimeout()->isSingleShot() == true);
        BOOST_TEST(t->conn.timerRetry()->interval() == 500ms);
        BOOST_TEST(t->conn.timerTimeout()->interval() == 2s);
        BOOST_TEST(t->conn.timerRetry()->isActive() == isRetryActive);
        BOOST_TEST(t->conn.timerTimeout()->isActive() == isTimeoutActive);
        BOOST_TEST(t->conn.timerRetry()->description() == "conn_tmr_retry"s);
        BOOST_TEST(t->conn.timerTimeout()->description() == "conn_tmr_timeout"s);
        BOOST_TEST(t->conn.state() == state);
        BOOST_TEST(t->closeCnt == close);
        BOOST_TEST(t->openCnt == open);
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
    testConn(ConnMachine::State::Halted, false, 1, 0, 0, false, false);
}


/**
 * START
 */
BOOST_AUTO_TEST_CASE(testOnStartInHalted)
{
    auto [conn, testConn] = setupConn();
    conn.onStart();
    testConn(ConnMachine::State::Trying, true, 2, 1, 1, true, true);
}


BOOST_AUTO_TEST_CASE(testOnStartInTrying)
{
    auto [conn, testConn] = setupConn();
    conn.onStart();
    conn.onStart();
    testConn(ConnMachine::State::Trying, true, 2, 1, 1, true, true);
}


BOOST_AUTO_TEST_CASE(testOnStartInStable)
{
    auto [conn, testConn] = setupConn();
    conn.onStart();
    conn.onPing();
    conn.onStart();
    testConn(ConnMachine::State::Stable, true, 2, 1, 2, false, true);
}


/**
 * STOP
 */
BOOST_AUTO_TEST_CASE(testOnStopInHalted)
{
    auto [conn, testConn] = setupConn();
    conn.onStop();
    testConn(ConnMachine::State::Halted, false, 1, 0, 0, false, false);
}


BOOST_AUTO_TEST_CASE(testOnStopInTrying)
{
    auto [conn, testConn] = setupConn();
    conn.onStart();
    conn.onStop();
    testConn(ConnMachine::State::Halted, true, 3, 1, 1, false, false);
}


BOOST_AUTO_TEST_CASE(testOnStopInStable)
{
    auto [conn, testConn] = setupConn();
    conn.onStart();
    conn.onPing();
    conn.onStop();
    testConn(ConnMachine::State::Halted, true, 3, 1, 2, false, false);
}


/**
 * PING
 */
BOOST_AUTO_TEST_CASE(testOnPingInHalted)
{
    auto [conn, testConn] = setupConn();
    conn.onPing();
    testConn(ConnMachine::State::Halted, false, 1, 0, 0, false, false);
}


BOOST_AUTO_TEST_CASE(testOnPingInTrying)
{
    auto [conn, testConn] = setupConn();
    conn.onStart();
    conn.onPing();
    testConn(ConnMachine::State::Stable, true, 2, 1, 2, false, true);
}


BOOST_AUTO_TEST_CASE(testOnPingInStable)
{
    auto [conn, testConn] = setupConn();
    conn.onStart();
    conn.onPing();
    conn.onPing();
    testConn(ConnMachine::State::Stable, true, 2, 1, 3, false, true);
}


/**
 * RETRY
 */
BOOST_AUTO_TEST_CASE(testOnTimerRetryInHalted)
{
    auto [conn, testConn] = setupConn();
    conn.onTimerRetryFired();
    testConn(ConnMachine::State::Halted, false, 1, 0, 0, false, false);
}


BOOST_AUTO_TEST_CASE(testOnTimerRetryInTrying)
{
    auto [conn, testConn] = setupConn();
    conn.onStart();
    conn.onTimerRetryFired();
    testConn(ConnMachine::State::Trying, true, 2, 1, 2, true, true);
}


BOOST_AUTO_TEST_CASE(testOnTimerRetryInStable)
{
    auto [conn, testConn] = setupConn();
    conn.onStart();
    conn.onPing();
    conn.onTimerRetryFired();
    testConn(ConnMachine::State::Stable, true, 2, 1, 2, false, true);
}


/**
 * RETRY
 */
BOOST_AUTO_TEST_CASE(testOnTimerTimeoutInHalted)
{
    auto [conn, testConn] = setupConn();
    conn.onTimerTimeoutFired();
    testConn(ConnMachine::State::Halted, false, 1, 0, 0, false, false);
}


BOOST_AUTO_TEST_CASE(testOnTimerTimeoutInTrying)
{
    auto [conn, testConn] = setupConn();
    conn.onStart();
    conn.onTimerTimeoutFired();
    testConn(ConnMachine::State::Trying, true, 3, 2, 2, true, true);
}


BOOST_AUTO_TEST_CASE(testOnTimerTimeoutInStable)
{
    auto [conn, testConn] = setupConn();
    conn.onStart();
    conn.onPing();
    conn.onTimerTimeoutFired();
    testConn(ConnMachine::State::Trying, true, 3, 2, 3, true, true);
}


/**
 * CONSUME TIMERS
 */
BOOST_AUTO_TEST_CASE(testConsumeTimerTrying)
{
    zmq::Context ctx;
    ConnMachine conn{"conn"sv, &ctx, 10s, 10s, []() {}, []() {}, []() {}, [](ConnMachine::State) {}};
    BOOST_TEST(!conn.timerRetry()->isExpired());

    conn.onTimerRetryFired();
    BOOST_TEST(!conn.timerRetry()->isExpired());

    conn.timerRetry()->stop();
    conn.timerRetry()->setSingleShot(true);
    conn.timerRetry()->setInterval(1ms);
    conn.timerRetry()->start();

    std::this_thread::sleep_for(500ms);
    BOOST_TEST(conn.timerRetry()->isExpired());

    conn.onTimerRetryFired();
    BOOST_TEST(!conn.timerRetry()->isExpired());
}


BOOST_AUTO_TEST_CASE(testConsumeTimerTimeout)
{
    zmq::Context ctx;
    ConnMachine conn{"conn"sv, &ctx, 10s, 10s, []() {}, []() {}, []() {}, [](ConnMachine::State) {}};
    BOOST_TEST(!conn.timerTimeout()->isExpired());

    conn.onTimerTimeoutFired();
    BOOST_TEST(!conn.timerTimeout()->isExpired());

    conn.timerTimeout()->stop();
    conn.timerTimeout()->setSingleShot(true);
    conn.timerTimeout()->setInterval(1ms);
    conn.timerTimeout()->start();

    std::this_thread::sleep_for(500ms);
    BOOST_TEST(conn.timerTimeout()->isExpired());

    conn.onTimerTimeoutFired();
    BOOST_TEST(!conn.timerTimeout()->isExpired());
}

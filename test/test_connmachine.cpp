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
#include <vector>
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

inline std::ostream& operator<<(std::ostream& os, const std::vector<ConnMachine::State>& v)
{
    std::string sep;

    os << "[";
    for (const auto& en : v) {
        os << sep;
        os << en;
        sep = ", ";
    }
    os << "]";

    return os;
}
} // namespace std


auto setupConn()
{
    struct T
    {
        int closeCnt = 0;
        int openCnt = 0;
        int pongCnt = 0;
        std::vector<ConnMachine::State> stateCurr;

        const Uuid id = Uuid::createNamespaceUuid(Uuid::Ns::Dns, "conn.fsm"sv);
        zmq::Context ctx;
        ConnMachine conn = ConnMachine{
            "conn"sv, id, &ctx, 500ms, 2s,
            [this]() { ++closeCnt; },
            [this]() { ++openCnt; },
            [this]() { ++pongCnt; },
            [this](ConnMachine::State s) { stateCurr.push_back(s); }};
    };

    const auto t = std::make_shared<T>();

    const auto testFunc = [t]( //
                              std::vector<ConnMachine::State> state,
                              int close,
                              int open,
                              int pong,
                              bool isRetryActive,
                              bool isTimeoutActive) //
    {
        BOOST_TEST(t->conn.name() == "conn"sv);
        BOOST_TEST(t->conn.uuid() == t->id);
        BOOST_TEST(t->conn.timerRetry() != nullptr);
        BOOST_TEST(t->conn.timerTimeout() != nullptr);
        BOOST_TEST(t->conn.timerRetry()->isSingleShot() == false);
        BOOST_TEST(t->conn.timerTimeout()->isSingleShot() == true);
        BOOST_TEST(t->conn.timerRetry()->interval() == 500ms);
        BOOST_TEST(t->conn.timerTimeout()->interval() == 2s);
        BOOST_TEST(t->conn.timerRetry()->isActive() == isRetryActive);
        BOOST_TEST(t->conn.timerTimeout()->isActive() == isTimeoutActive);
        BOOST_TEST(t->conn.timerRetry()->description() == "conn_conn_tmr_retry"s);
        BOOST_TEST(t->conn.timerTimeout()->description() == "conn_conn_tmr_timeout"s);
        BOOST_TEST(t->closeCnt == close);
        BOOST_TEST(t->openCnt == open);
        BOOST_TEST(t->pongCnt == pong);
        BOOST_TEST(t->stateCurr == state);
        if (!state.empty())
            BOOST_TEST(t->conn.state() == state.back());
        else
            BOOST_TEST(t->conn.state() == ConnMachine::State::Halted);
    };

    return std::make_tuple(std::ref(t->conn), testFunc);
}


/**
 * INIT
 */
BOOST_AUTO_TEST_CASE(testInitConn)
{
    auto [conn, test] = setupConn();
    BOOST_TEST(conn.state() == ConnMachine::State::Halted);
    test({}, 1, 0, 0, false, false);
}


/**
 * START
 */
BOOST_AUTO_TEST_CASE(testOnStartInHalted)
{
    auto [conn, test] = setupConn();
    BOOST_TEST(conn.state() == ConnMachine::State::Halted);

    conn.onStart();
    test({ConnMachine::State::Trying}, 2, 1, 1, true, true);
}


BOOST_AUTO_TEST_CASE(testOnStartInTrying)
{
    auto [conn, test] = setupConn();
    conn.onStart();
    BOOST_TEST(conn.state() == ConnMachine::State::Trying);

    conn.onStart();
    test({ConnMachine::State::Trying}, 2, 1, 1, true, true);
}


BOOST_AUTO_TEST_CASE(testOnStartInStable)
{
    auto [conn, test] = setupConn();
    conn.onStart();
    conn.onPing();
    BOOST_TEST(conn.state() == ConnMachine::State::Stable);

    conn.onStart();
    test({ConnMachine::State::Trying, ConnMachine::State::Stable},
        2, 1, 2, false, true);
}


/**
 * STOP
 */
BOOST_AUTO_TEST_CASE(testOnStopInHalted)
{
    auto [conn, test] = setupConn();
    BOOST_TEST(conn.state() == ConnMachine::State::Halted);

    conn.onStop();
    test({}, 1, 0, 0, false, false);
}


BOOST_AUTO_TEST_CASE(testOnStopInTrying)
{
    auto [conn, test] = setupConn();
    conn.onStart();
    BOOST_TEST(conn.state() == ConnMachine::State::Trying);

    conn.onStop();
    test({ConnMachine::State::Trying, ConnMachine::State::Halted},
        3, 1, 1, false, false);
}


BOOST_AUTO_TEST_CASE(testOnStopInStable)
{
    auto [conn, test] = setupConn();
    conn.onStart();
    conn.onPing();
    BOOST_TEST(conn.state() == ConnMachine::State::Stable);

    conn.onStop();
    test({ConnMachine::State::Trying, ConnMachine::State::Stable, ConnMachine::State::Halted},
        3, 1, 2, false, false);
}


/**
 * PING
 */
BOOST_AUTO_TEST_CASE(testOnPingInHalted)
{
    auto [conn, test] = setupConn();
    BOOST_TEST(conn.state() == ConnMachine::State::Halted);

    conn.onPing();
    test({}, 1, 0, 0, false, false);
}


BOOST_AUTO_TEST_CASE(testOnPingInTrying)
{
    auto [conn, test] = setupConn();
    conn.onStart();
    BOOST_TEST(conn.state() == ConnMachine::State::Trying);

    conn.onPing();
    test({ConnMachine::State::Trying, ConnMachine::State::Stable},
        2, 1, 2, false, true);
}


BOOST_AUTO_TEST_CASE(testOnPingInStable)
{
    auto [conn, test] = setupConn();
    conn.onStart();
    conn.onPing();
    BOOST_TEST(conn.state() == ConnMachine::State::Stable);

    conn.onPing();
    test({ConnMachine::State::Trying, ConnMachine::State::Stable},
        2, 1, 3, false, true);
}


/**
 * RETRY
 */
BOOST_AUTO_TEST_CASE(testOnTimerRetryInHalted)
{
    auto [conn, test] = setupConn();
    BOOST_TEST(conn.state() == ConnMachine::State::Halted);

    conn.onTimerRetryFired();
    test({}, 1, 0, 0, false, false);
}


BOOST_AUTO_TEST_CASE(testOnTimerRetryInTrying)
{
    auto [conn, test] = setupConn();
    conn.onStart();
    BOOST_TEST(conn.state() == ConnMachine::State::Trying);

    conn.onTimerRetryFired();
    test({ConnMachine::State::Trying}, 2, 1, 2, true, true);
}


BOOST_AUTO_TEST_CASE(testOnTimerRetryInStable)
{
    auto [conn, test] = setupConn();
    conn.onStart();
    conn.onPing();
    BOOST_TEST(conn.state() == ConnMachine::State::Stable);

    conn.onTimerRetryFired();
    test({ConnMachine::State::Trying, ConnMachine::State::Stable},
        2, 1, 2, false, true);
}


/**
 * TIMEOUT
 */
BOOST_AUTO_TEST_CASE(testOnTimerTimeoutInHalted)
{
    auto [conn, test] = setupConn();
    BOOST_TEST(conn.state() == ConnMachine::State::Halted);

    conn.onTimerTimeoutFired();
    test({}, 1, 0, 0, false, false);
}


BOOST_AUTO_TEST_CASE(testOnTimerTimeoutInTrying)
{
    auto [conn, test] = setupConn();
    conn.onStart();
    BOOST_TEST(conn.state() == ConnMachine::State::Trying);

    conn.onTimerTimeoutFired();
    test({ConnMachine::State::Trying}, 3, 2, 2, true, true);
}


BOOST_AUTO_TEST_CASE(testOnTimerTimeoutInStable)
{
    auto [conn, test] = setupConn();
    conn.onStart();
    conn.onPing();
    BOOST_TEST(conn.state() == ConnMachine::State::Stable);

    conn.onTimerTimeoutFired();
    test({ConnMachine::State::Trying, ConnMachine::State::Stable, ConnMachine::State::Trying},
        3, 2, 3, true, true);
}


/**
 * CONSUME TIMERS
 */
BOOST_AUTO_TEST_CASE(testConsumeTimerTrying)
{
    zmq::Context ctx;
    ConnMachine conn{"conn"sv, Uuid{}, &ctx, 10s, 10s, []() {}, []() {}, []() {}, [](ConnMachine::State) {}};
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
    ConnMachine conn{"conn"sv, Uuid{}, &ctx, 10s, 10s, []() {}, []() {}, []() {}, [](ConnMachine::State) {}};
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

/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE syncmachine
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include "syncmachine.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqtimer.h"
#include "fuurin/errors.h"

#include <string_view>
#include <chrono>
#include <tuple>
#include <vector>
#include <memory>
#include <thread>
#include <unordered_map>


using namespace fuurin;
namespace utf = boost::unit_test;
namespace bdata = utf::data;

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

template<typename T>
inline std::ostream& operator<<(std::ostream& os, const std::unordered_map<int, T>& m)
{
    std::string sep;

    os << "[";
    for (const auto [key, value] : m) {
        os << sep;
        os << key << ": " << int(value);
        sep = ", ";
    }
    os << "]";

    return os;
}

inline std::ostream& operator<<(std::ostream& os, const std::vector<SyncMachine::State>& v)
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


auto setupMach(int maxIndex, int maxRetry)
{
    struct T
    {
        std::unordered_map<int, int> close;
        std::unordered_map<int, int> open;
        std::unordered_map<int, SyncMachine::seqn_t> sync;
        std::vector<SyncMachine::State> state;

        const Uuid id = Uuid::createNamespaceUuid(Uuid::Ns::Dns, "mach.fsm"sv);
        zmq::Context ctx;
        SyncMachine mach;

        T(int maxIndex, int maxRetry)
            : mach{SyncMachine{
                  "mach"sv,
                  id,
                  &ctx,
                  maxIndex,
                  maxRetry,
                  2s,
                  [this](int idx) { close[idx]++; },
                  [this](int idx) { open[idx]++; },
                  [this](int idx, SyncMachine::seqn_t seqn) { sync[idx] = seqn; },
                  [this](SyncMachine::State s) { state.push_back(s); },
              }}
        {
        }
    };

    const auto t = std::make_shared<T>(maxIndex, maxRetry);

    const auto testFunc = [t, maxRetry, maxIndex]( //
                              std::vector<SyncMachine::State> state,
                              std::unordered_map<int, int> close,
                              std::unordered_map<int, int> open,
                              std::unordered_map<int, SyncMachine::seqn_t> sync,
                              bool isTimeoutActive,
                              int currIndex,
                              int nextIndex,
                              int retryCount,
                              SyncMachine::seqn_t seqNum) //
    {
        BOOST_TEST(t->mach.name() == "mach"sv);
        BOOST_TEST(t->mach.uuid() == t->id);
        BOOST_TEST(t->mach.maxRetry() == maxRetry);
        BOOST_TEST(t->mach.maxIndex() == maxIndex);
        BOOST_TEST(t->mach.timerTimeout() != nullptr);
        BOOST_TEST(t->mach.timerTimeout()->isSingleShot() == true);
        BOOST_TEST(t->mach.timerTimeout()->interval() == 2s);
        BOOST_TEST(t->mach.timerTimeout()->isActive() == isTimeoutActive);
        BOOST_TEST(t->mach.timerTimeout()->description() == "mach_sync_tmr_timeout"s);
        BOOST_TEST(t->mach.currentIndex() == currIndex);
        BOOST_TEST(t->mach.nextIndex() == nextIndex);
        BOOST_TEST(t->mach.retryCount() == retryCount);
        BOOST_TEST(t->mach.sequenceNumber() == seqNum);
        BOOST_TEST(t->close == close);
        BOOST_TEST(t->open == open);
        BOOST_TEST(t->sync == sync);
        BOOST_TEST(t->state == state);
        if (!state.empty())
            BOOST_TEST(t->mach.state() == state.back());
        else
            BOOST_TEST(t->mach.state() == SyncMachine::State::Halted);
    };

    return std::make_tuple(std::ref(t->mach), testFunc);
}


/**
 * INIT
 */
BOOST_AUTO_TEST_CASE(testInitMach)
{
    auto [mach, test] = setupMach(2, 3);
    BOOST_TEST(mach.state() == SyncMachine::State::Halted);
    test({}, {{0, 1}, {1, 1}, {2, 1}}, {}, {}, false, 0, 1, 0, 0);
}


/**
 * SYNC
 */
BOOST_AUTO_TEST_CASE(testOnSyncInHalted)
{
    auto [mach, test] = setupMach(1, 3);
    BOOST_TEST(mach.state() == SyncMachine::State::Halted);

    mach.onSync();
    test({SyncMachine::State::Download}, {{0, 1}, {1, 1}}, {{0, 1}}, {{0, 1}}, true, 0, 1, 0, 1);
}


BOOST_AUTO_TEST_CASE(testOnSyncInHaltedOneIndex)
{
    auto [mach, test] = setupMach(0, 3);
    BOOST_TEST(mach.state() == SyncMachine::State::Halted);

    mach.onSync();
    test({SyncMachine::State::Download}, {{0, 1}}, {{0, 1}}, {{0, 1}}, true, 0, 0, 0, 1);
}


BOOST_AUTO_TEST_CASE(testOnSyncInDownload)
{
    auto [mach, test] = setupMach(1, 3);
    mach.onSync();
    BOOST_TEST(mach.state() == SyncMachine::State::Download);

    mach.onSync();
    test({SyncMachine::State::Download}, {{0, 1}, {1, 1}}, {{0, 1}}, {{0, 1}}, true, 0, 1, 0, 1);
}


BOOST_AUTO_TEST_CASE(testOnSyncInSynced)
{
    auto [mach, test] = setupMach(1, 3);
    mach.onSync();
    mach.onReply(0, 1, SyncMachine::ReplyType::Complete);
    BOOST_TEST(mach.state() == SyncMachine::State::Synced);

    mach.onSync();
    test({SyncMachine::State::Download, SyncMachine::State::Synced, SyncMachine::State::Download},
        {{0, 1}, {1, 1}}, {{0, 1}}, {{0, 2}}, true, 0, 1, 0, 2);
}


BOOST_AUTO_TEST_CASE(testOnSyncInFailed)
{
    auto [mach, test] = setupMach(1, 0);
    mach.onSync();
    mach.onTimerTimeoutFired();
    BOOST_TEST(mach.state() == SyncMachine::State::Failed);

    mach.onSync();
    test({SyncMachine::State::Download, SyncMachine::State::Failed, SyncMachine::State::Download},
        {{0, 2}, {1, 1}}, {{0, 1}, {1, 1}}, {{0, 1}, {1, 2}}, true, 1, 0, 0, 2);
}


/**
 * HALT
 */
BOOST_AUTO_TEST_CASE(testOnHaltInHalted)
{
    auto [mach, test] = setupMach(1, 3);
    BOOST_TEST(mach.state() == SyncMachine::State::Halted);

    mach.onHalt();
    BOOST_TEST(mach.state() == SyncMachine::State::Halted);
    test({}, {{0, 1}, {1, 1}}, {}, {}, false, 0, 1, 0, 0);
}


BOOST_AUTO_TEST_CASE(testOnHaltInHaltedOneIndex)
{
    auto [mach, test] = setupMach(0, 3);
    BOOST_TEST(mach.state() == SyncMachine::State::Halted);

    mach.onHalt();
    BOOST_TEST(mach.state() == SyncMachine::State::Halted);
    test({}, {{0, 1}}, {}, {}, false, 0, 0, 0, 0);
}


BOOST_AUTO_TEST_CASE(testOnHaltInDownload)
{
    auto [mach, test] = setupMach(1, 3);
    mach.onSync();
    BOOST_TEST(mach.state() == SyncMachine::State::Download);

    mach.onHalt();
    test({SyncMachine::State::Download, SyncMachine::State::Halted},
        {{0, 2}, {1, 1}}, {{0, 1}}, {{0, 1}}, false, 0, 1, 0, 0);
}


BOOST_AUTO_TEST_CASE(testOnHaltInSynced)
{
    auto [mach, test] = setupMach(1, 3);
    mach.onSync();
    mach.onReply(0, 1, SyncMachine::ReplyType::Complete);
    BOOST_TEST(mach.state() == SyncMachine::State::Synced);

    mach.onHalt();
    test({SyncMachine::State::Download, SyncMachine::State::Synced, SyncMachine::State::Halted},
        {{0, 2}, {1, 1}}, {{0, 1}}, {{0, 1}}, false, 0, 1, 0, 0);
}


BOOST_AUTO_TEST_CASE(testOnHaltInFailed)
{
    auto [mach, test] = setupMach(1, 0);
    mach.onSync();
    mach.onTimerTimeoutFired();
    BOOST_TEST(mach.state() == SyncMachine::State::Failed);

    mach.onHalt();
    test({SyncMachine::State::Download, SyncMachine::State::Failed, SyncMachine::State::Halted},
        {{0, 2}, {1, 1}}, {{0, 1}}, {{0, 1}}, false, 0, 1, 0, 0);
}


/**
 * REPLY
 */
BOOST_DATA_TEST_CASE(testOnReplyInHalted,
    bdata::make({
        std::make_tuple(0, 0, SyncMachine::ReplyType::Complete),
        std::make_tuple(1, 0, SyncMachine::ReplyType::Complete),
        std::make_tuple(0, 1, SyncMachine::ReplyType::Complete),
        std::make_tuple(1, 1, SyncMachine::ReplyType::Complete),
        std::make_tuple(0, 0, SyncMachine::ReplyType::Snapshot),
        std::make_tuple(1, 0, SyncMachine::ReplyType::Snapshot),
        std::make_tuple(0, 1, SyncMachine::ReplyType::Snapshot),
        std::make_tuple(1, 1, SyncMachine::ReplyType::Snapshot),
    }),
    index, seqn, reply)
{
    auto [mach, test] = setupMach(1, 3);
    BOOST_TEST(mach.state() == SyncMachine::State::Halted);

    BOOST_TEST(mach.onReply(index, seqn, reply) == SyncMachine::ReplyResult::Unexpected);
    BOOST_TEST(mach.state() == SyncMachine::State::Halted);
    test({}, {{0, 1}, {1, 1}}, {}, {}, false, 0, 1, 0, 0);
}


using SS = SyncMachine::State;
using RT = SyncMachine::ReplyType;
using RR = SyncMachine::ReplyResult;

BOOST_DATA_TEST_CASE(testOnReplyInDownload,
    bdata::make({
        std::make_tuple(0, 0, RT::Complete, RR::Discarded, SS::Download, true),
        std::make_tuple(1, 0, RT::Complete, RR::Discarded, SS::Download, true),
        std::make_tuple(0, 1, RT::Complete, RR::Accepted, SS::Synced, false),
        std::make_tuple(1, 1, RT::Complete, RR::Discarded, SS::Download, true),
        std::make_tuple(0, 0, RT::Snapshot, RR::Discarded, SS::Download, true),
        std::make_tuple(1, 0, RT::Snapshot, RR::Discarded, SS::Download, true),
        std::make_tuple(0, 1, RT::Snapshot, RR::Accepted, SS::Download, true),
        std::make_tuple(1, 1, RT::Snapshot, RR::Discarded, SS::Download, true),
    }),
    index, seqn, reply, wantResult, wantState, wantTimer)
{
    auto [mach, test] = setupMach(1, 3);
    mach.onSync();
    BOOST_TEST(mach.state() == SyncMachine::State::Download);

    BOOST_TEST(mach.onReply(index, seqn, reply) == wantResult);
    BOOST_TEST(mach.state() == wantState);

    std::vector<SyncMachine::State> change{SyncMachine::State::Download};
    if (wantState != SyncMachine::State::Download)
        change.push_back(wantState);

    test(change, {{0, 1}, {1, 1}}, {{0, 1}}, {{0, 1}}, wantTimer, 0, 1, 0, 1);
}


BOOST_DATA_TEST_CASE(testOnReplyInSynced,
    bdata::make({
        std::make_tuple(0, 0, SyncMachine::ReplyType::Complete),
        std::make_tuple(1, 0, SyncMachine::ReplyType::Complete),
        std::make_tuple(0, 1, SyncMachine::ReplyType::Complete),
        std::make_tuple(1, 1, SyncMachine::ReplyType::Complete),
        std::make_tuple(0, 0, SyncMachine::ReplyType::Snapshot),
        std::make_tuple(1, 0, SyncMachine::ReplyType::Snapshot),
        std::make_tuple(0, 1, SyncMachine::ReplyType::Snapshot),
        std::make_tuple(1, 1, SyncMachine::ReplyType::Snapshot),
    }),
    index, seqn, reply)
{
    auto [mach, test] = setupMach(1, 3);
    mach.onSync();
    mach.onReply(0, 1, SyncMachine::ReplyType::Complete);
    BOOST_TEST(mach.state() == SyncMachine::State::Synced);

    BOOST_TEST(mach.onReply(index, seqn, reply) == SyncMachine::ReplyResult::Unexpected);
    test({SyncMachine::State::Download, SyncMachine::State::Synced},
        {{0, 1}, {1, 1}}, {{0, 1}}, {{0, 1}}, false, 0, 1, 0, 1);
}


BOOST_DATA_TEST_CASE(testOnReplyInFailed,
    bdata::make({
        std::make_tuple(0, 0, SyncMachine::ReplyType::Complete),
        std::make_tuple(1, 0, SyncMachine::ReplyType::Complete),
        std::make_tuple(0, 1, SyncMachine::ReplyType::Complete),
        std::make_tuple(1, 1, SyncMachine::ReplyType::Complete),
        std::make_tuple(0, 0, SyncMachine::ReplyType::Snapshot),
        std::make_tuple(1, 0, SyncMachine::ReplyType::Snapshot),
        std::make_tuple(0, 1, SyncMachine::ReplyType::Snapshot),
        std::make_tuple(1, 1, SyncMachine::ReplyType::Snapshot),
    }),
    index, seqn, reply)
{
    auto [mach, test] = setupMach(1, 0);
    mach.onSync();
    mach.onTimerTimeoutFired();
    BOOST_TEST(mach.state() == SyncMachine::State::Failed);

    BOOST_TEST(mach.onReply(index, seqn, reply) == SyncMachine::ReplyResult::Unexpected);
    test({SyncMachine::State::Download, SyncMachine::State::Failed},
        {{0, 2}, {1, 1}}, {{0, 1}}, {{0, 1}}, false, 0, 1, 0, 1);
}


/**
 * TIMEOUT
 */
BOOST_AUTO_TEST_CASE(testOnTimeoutInHalted)
{
    auto [mach, test] = setupMach(1, 3);
    BOOST_TEST(mach.state() == SyncMachine::State::Halted);

    mach.onTimerTimeoutFired();
    BOOST_TEST(mach.state() == SyncMachine::State::Halted);
    test({}, {{0, 1}, {1, 1}}, {}, {}, false, 0, 1, 0, 0);
}


using SS = SyncMachine::State;
using RT = SyncMachine::ReplyType;
using RR = SyncMachine::ReplyResult;
using VT = std::vector<SyncMachine::State>;
using MC = std::unordered_map<int, int>;
using MO = std::unordered_map<int, int>;
using MS = std::unordered_map<int, SyncMachine::seqn_t>;

BOOST_DATA_TEST_CASE(testOnTimeoutInDownload,
    bdata::make({
        std::make_tuple("timeout, no retry",
            1, 0, 1, -1, SS::Failed, VT{SS::Download, SS::Failed},
            MC{{0, 2}, {1, 1}}, MO{{0, 1}}, MS{{0, 1}},
            false, 0, 1, 0, 1),
        std::make_tuple("timeout, retry",
            1, 1, 1, -1, SS::Download, VT{SS::Download},
            MC{{0, 2}, {1, 1}}, MO{{0, 1}, {1, 1}}, MS{{0, 1}, {1, 2}},
            true, 1, 0, 1, 2),
        std::make_tuple("timeout, retry more",
            2, 3, 3, -1, SS::Download, VT{SS::Download},
            MC{{0, 2}, {1, 2}, {2, 2}}, MO{{0, 2}, {1, 1}, {2, 1}}, MS{{0, 4}, {1, 2}, {2, 3}},
            true, 0, 1, 3, 4),
        std::make_tuple("timeout, fail",
            1, 1, 2, -1, SS::Failed, VT{SS::Download, SS::Failed},
            MC{{0, 2}, {1, 2}}, MO{{0, 1}, {1, 1}}, MS{{0, 1}, {1, 2}},
            false, 1, 0, 1, 2),
        std::make_tuple("timeout, fail more",
            2, 2, 4, -1, SS::Failed, VT{SS::Download, SS::Failed},
            MC{{0, 2}, {1, 2}, {2, 2}}, MO{{0, 1}, {1, 1}, {2, 1}}, MS{{0, 1}, {1, 2}, {2, 3}},
            false, 2, 0, 2, 3),
        std::make_tuple("timeout, change next",
            2, 3, 1, 2, SS::Download, VT{SS::Download},
            MC{{0, 2}, {1, 1}, {2, 1}}, MO{{0, 1}, {2, 1}}, MS{{0, 1}, {2, 2}},
            true, 2, 0, 1, 2),
        std::make_tuple("timeout, change next, negative",
            2, 3, 1, -10, SS::Download, VT{SS::Download},
            MC{{0, 2}, {1, 1}, {2, 1}}, MO{{0, 2}}, MS{{0, 2}},
            true, 0, 1, 1, 2),
    }),
    name, maxIndex, maxRetry, numTimeout, nextIndex,
    wantState, wantTrans, wantClose, wantOpen, wantSync,
    wantTimer, wantCurr, wantNext, wantRetry, wantSeqn)
{
    BOOST_TEST_MESSAGE(name);

    auto [mach, test] = setupMach(maxIndex, maxRetry);
    mach.onSync();
    BOOST_TEST(mach.state() == SyncMachine::State::Download);

    if (nextIndex != -1)
        mach.setNextIndex(nextIndex);

    for (int i = 0; i < numTimeout; ++i)
        mach.onTimerTimeoutFired();

    BOOST_TEST(mach.state() == wantState);
    test(wantTrans, wantClose, wantOpen, wantSync, wantTimer, wantCurr, wantNext, wantRetry, wantSeqn);
}


BOOST_AUTO_TEST_CASE(testOnTimeoutInSynced)
{
    auto [mach, test] = setupMach(1, 3);
    mach.onSync();
    mach.onReply(0, 1, SyncMachine::ReplyType::Complete);
    BOOST_TEST(mach.state() == SyncMachine::State::Synced);

    mach.onTimerTimeoutFired();
    test({SyncMachine::State::Download, SyncMachine::State::Synced},
        {{0, 1}, {1, 1}}, {{0, 1}}, {{0, 1}}, false, 0, 1, 0, 1);
}


BOOST_AUTO_TEST_CASE(testOnTimeoutInFailed)
{
    auto [mach, test] = setupMach(1, 0);
    mach.onSync();
    mach.onTimerTimeoutFired();
    BOOST_TEST(mach.state() == SyncMachine::State::Failed);

    mach.onTimerTimeoutFired();
    test({SyncMachine::State::Download, SyncMachine::State::Failed},
        {{0, 2}, {1, 1}}, {{0, 1}}, {{0, 1}}, false, 0, 1, 0, 1);
}


/**
 * CONSUME TIMERS
 */
BOOST_AUTO_TEST_CASE(testConsumeTimerTimeout)
{
    zmq::Context ctx;
    SyncMachine mach{
        "mach"sv,
        Uuid{},
        &ctx,
        0,
        0,
        2s,
        [](int) {},
        [](int) {},
        [](int, SyncMachine::seqn_t) {},
        [](SyncMachine::State) {},
    };

    BOOST_TEST(!mach.timerTimeout()->isExpired());

    mach.onTimerTimeoutFired();
    BOOST_TEST(!mach.timerTimeout()->isExpired());

    mach.timerTimeout()->stop();
    mach.timerTimeout()->setSingleShot(true);
    mach.timerTimeout()->setInterval(1ms);
    mach.timerTimeout()->start();

    std::this_thread::sleep_for(500ms);
    BOOST_TEST(mach.timerTimeout()->isExpired());

    mach.onTimerTimeoutFired();
    BOOST_TEST(!mach.timerTimeout()->isExpired());
}

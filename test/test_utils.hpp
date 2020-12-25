/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef FUURIN_TEST_UTILS_H
#define FUURIN_TEST_UTILS_H

#include <boost/test/unit_test.hpp>

#include "fuurin/broker.h"
#include "fuurin/worker.h"
#include "fuurin/workerconfig.h"
#include "fuurin/zmqpart.h"
#include "fuurin/event.h"

#include <chrono>


using namespace std::literals::chrono_literals;

using namespace fuurin;


Event testWaitForEvent(Worker& w, std::chrono::milliseconds timeout, Event::Notification evRet,
    Event::Type evType)
{
    const auto& ev = w.waitForEvent(timeout);

    BOOST_TEST(ev.notification() == evRet);
    BOOST_TEST(ev.type() == evType);
    BOOST_TEST(ev.payload().empty());

    return ev;
}


template<typename T>
Event testWaitForEvent(Worker& w, std::chrono::milliseconds timeout, Event::Notification evRet,
    Event::Type evType, const T& evT)
{
    const auto& ev = w.waitForEvent(timeout);

    BOOST_TEST(ev.notification() == evRet);
    BOOST_TEST(ev.type() == evType);

    BOOST_TEST(!ev.payload().empty());
    BOOST_TEST(T::fromPart(ev.payload()) == evT);

    return ev;
}


void testWaitForStart(Worker& w, const WorkerConfig& cnf)
{
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Started, cnf);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Online);
}


void testWaitForStop(Worker& w)
{
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Offline);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Stopped);
}


Event testWaitForTopic(Worker& w, Topic t, int seqn)
{
    return testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::Delivery, t.withSeqNum(seqn));
}


void testWaitForSyncStart(Worker& w, Broker& b, const WorkerConfig& cnf)
{
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOn);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncRequest, cnf);
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncBegin, b.uuid());
}


void testWaitForSyncStop(Worker& w, Broker& b)
{
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncSuccess, b.uuid());
    testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncDownloadOff);
}


Event testWaitForSyncTopic(Worker& w, Topic t, int seqn)
{
    return testWaitForEvent(w, 2s, Event::Notification::Success, Event::Type::SyncElement, t.withSeqNum(seqn));
}


Event testWaitForTimeout(Worker& w)
{
    return testWaitForEvent(w, 3s, Event::Notification::Timeout, Event::Type::Invalid);
}

#endif // FUURIN_TEST_UTILS_H

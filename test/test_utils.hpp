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
#include "fuurin/topic.h"
#include "types.h"

#include <zmq.h>

#include <chrono>
#include <vector>
#include <optional>


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


WorkerConfig mkCnf(const Worker& w, Topic::SeqN seqn = 0, bool wildc = true,
    const std::vector<Topic::Name>& names = {},
    const std::vector<std::string>& endp1 = {"ipc:///tmp/worker_delivery"},
    const std::vector<std::string>& endp2 = {"ipc:///tmp/worker_dispatch"},
    const std::vector<std::string>& endp3 = {"ipc:///tmp/broker_snapshot"})
{
    return WorkerConfig{w.uuid(), seqn, wildc, names, endp1, endp2, endp3};
}


void testReadEventAsync(
    Worker& w,
    void* zpoller,
    bool wait,
    std::chrono::milliseconds timeout,
    std::optional<Event::Type> evType = {},
    std::optional<WorkerConfig> cfg = {}) //
{
    if (wait) {
        zmq_poller_event_t zev;
        const int rc = zmq_poller_wait(zpoller, &zev, zmq::getMillis<long>(timeout));

        if (evType) {
            BOOST_TEST(rc == 0);
            BOOST_TEST(zev.fd == w.eventFD());
        } else {
            BOOST_TEST(rc == -1);
        }
    }

    if (evType) {
        if (cfg) {
            testWaitForEvent(w, 0s, Event::Notification::Success, evType.value(), cfg.value());
        } else {
            testWaitForEvent(w, 0s, Event::Notification::Success, evType.value());
        }
    } else {
        testWaitForTimeout(w);
    }
};


#endif // FUURIN_TEST_UTILS_H

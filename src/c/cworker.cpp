/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/c/cworker.h"
#include "fuurin/topic.h"
#include "fuurin/worker.h"
#include "fuurin/logger.h"
#include "ceventd.h"
#include "cutils.h"

#include <memory>
#include <chrono>
#include <string_view>


using namespace fuurin;


namespace {
struct CWorkerD
{
    std::unique_ptr<Worker> w;
    std::future<void> f;
    CEventD evd;
};
} // namespace


CWorker* CWorker_new(CUuid id, unsigned long long seqn, const char* name)
{
    static_assert(sizeof(seqn) == sizeof(Topic::SeqN));

    auto ret = new CWorkerD;

    return c::withCatch(
        [ret, id, seqn, name]() {
            ret->w = std::make_unique<Worker>(c::uuidConvert(id), seqn, name);
            return reinterpret_cast<CWorker*>(ret);
        },
        [ret]() {
            delete ret;
            return static_cast<CWorker*>(nullptr);
        });
}


void CWorker_delete(CWorker* w)
{
    delete reinterpret_cast<CWorkerD*>(w);
}


const char* CWorker_name(CWorker* w)
{
    return reinterpret_cast<CWorkerD*>(w)->w->name().data();
}


CUuid CWorker_uuid(CWorker* w)
{
    return c::uuidConvert(reinterpret_cast<CWorkerD*>(w)->w->uuid());
}


unsigned long long CWorker_seqNum(CWorker* w)
{
    static_assert(sizeof(decltype(CWorker_seqNum(nullptr))) == sizeof(Topic::SeqN));

    return reinterpret_cast<CWorkerD*>(w)->w->seqNumber();
}


void CWorker_addEndpoints(CWorker* w, const char* delivery, const char* dispatch, const char* snapshot)
{
    Worker* ww = reinterpret_cast<CWorkerD*>(w)->w.get();

    auto endp1 = ww->endpointDelivery();
    auto endp2 = ww->endpointDispatch();
    auto endp3 = ww->endpointSnapshot();

    endp1.push_back(delivery);
    endp2.push_back(dispatch);
    endp3.push_back(snapshot);

    ww->setEndpoints(endp1, endp2, endp3);
}


void CWorker_clearEndpoints(CWorker* w)
{
    reinterpret_cast<CWorkerD*>(w)->w->setEndpoints({}, {}, {});
}


const char* CWorker_endpointDelivery(CWorker* w)
{
    const auto& endp = reinterpret_cast<CWorkerD*>(w)->w->endpointDelivery();
    return endp.empty() ? nullptr : endp.front().data();
}


const char* CWorker_endpointDispatch(CWorker* w)
{
    const auto& endp = reinterpret_cast<CWorkerD*>(w)->w->endpointDispatch();
    return endp.empty() ? nullptr : endp.front().data();
}


const char* CWorker_endpointSnapshot(CWorker* w)
{
    const auto& endp = reinterpret_cast<CWorkerD*>(w)->w->endpointSnapshot();
    return endp.empty() ? nullptr : endp.front().data();
}


void CWorker_start(CWorker* w)
{
    CWorkerD* wd = reinterpret_cast<CWorkerD*>(w);
    auto f = wd->w->start();
    if (!f.valid())
        return;

    CWorker_wait(w);
    wd->f = std::move(f);
}


void CWorker_stop(CWorker* w)
{
    reinterpret_cast<CWorkerD*>(w)->w->stop();
}


void CWorker_wait(CWorker* w)
{
    CWorkerD* wwd = reinterpret_cast<CWorkerD*>(w);
    if (!wwd->f.valid())
        return;

    return c::withCatch(
        [wwd]() {
            wwd->f.get();
        },
        []() {});
}


bool CWorker_isRunning(CWorker* w)
{
    return reinterpret_cast<CWorkerD*>(w)->w->isRunning();
}


void CWorker_setTopicsAll(CWorker* w)
{
    return reinterpret_cast<CWorkerD*>(w)->w->setTopicsAll();
}


void CWorker_addTopicsNames(CWorker* w, const char* name)
{
    Worker* ww = reinterpret_cast<CWorkerD*>(w)->w.get();

    bool all;
    std::vector<Topic::Name> names;
    std::tie(all, names) = ww->topicsNames();

    names.push_back(std::string{name});

    ww->setTopicsNames(names);
}


void CWorker_clearTopicsNames(CWorker* w)
{
    return reinterpret_cast<CWorkerD*>(w)->w->setTopicsNames({});
}


bool CWorker_topicsAll(CWorker* w)
{
    auto [all, names] = reinterpret_cast<CWorkerD*>(w)->w->topicsNames();
    return all;
}


const char* CWorker_topicsNames(CWorker* w)
{
    auto [all, names] = reinterpret_cast<CWorkerD*>(w)->w->topicsNames();
    return names.empty() ? nullptr : std::string_view(names.front()).data();
}


void CWorker_dispatch(CWorker* w, const char* name, const char* data, size_t size, TopicType_t type)
{
    Topic::Type ftype;
    switch (type) {
    case TopicState:
        ftype = Topic::Type::State;
        break;

    case TopicEvent:
        ftype = Topic::Type::Event;
        break;
    }

    return c::withCatch(
        [wwd = reinterpret_cast<CWorkerD*>(w), name, data, size, ftype]() {
            wwd->w->dispatch(std::string_view(name), zmq::Part(data, size), ftype);
        },
        []() {});
}


void CWorker_sync(CWorker* w)
{
    reinterpret_cast<CWorkerD*>(w)->w->sync();
}


CEvent* CWorker_waitForEvent(CWorker* w, unsigned long timeout_ms)
{
    return c::withCatch(
        [wwd = reinterpret_cast<CWorkerD*>(w), timeout_ms]() {
            wwd->evd.ev = wwd->w->waitForEvent(std::chrono::milliseconds(timeout_ms));
            return reinterpret_cast<CEvent*>(&wwd->evd);
        },
        [wwd = reinterpret_cast<CWorkerD*>(w)]() {
            wwd->evd.ev = {};
            return reinterpret_cast<CEvent*>(&wwd->evd);
        });
}


bool CWorker_waitForStarted(CWorker* w, unsigned long timeout_ms)
{
    return c::withCatch(
        [wwd = reinterpret_cast<CWorkerD*>(w), timeout_ms]() {
            return wwd->w->waitForStarted(std::chrono::milliseconds(timeout_ms));
        },
        []() {
            return false;
        });
}


bool CWorker_waitForStopped(CWorker* w, unsigned long timeout_ms)
{
    return c::withCatch(
        [wwd = reinterpret_cast<CWorkerD*>(w), timeout_ms]() {
            return wwd->w->waitForStopped(std::chrono::milliseconds(timeout_ms));
        },
        []() {
            return false;
        });
}


bool CWorker_waitForOnline(CWorker* w, unsigned long timeout_ms)
{
    return c::withCatch(
        [wwd = reinterpret_cast<CWorkerD*>(w), timeout_ms]() {
            return wwd->w->waitForOnline(std::chrono::milliseconds(timeout_ms));
        },
        []() {
            return false;
        });
}


bool CWorker_waitForOffline(CWorker* w, unsigned long timeout_ms)
{
    return c::withCatch(
        [wwd = reinterpret_cast<CWorkerD*>(w), timeout_ms]() {
            return wwd->w->waitForOffline(std::chrono::milliseconds(timeout_ms));
        },
        []() {
            return false;
        });
}


CTopic* CWorker_waitForTopic(CWorker* w, unsigned long timeout_ms)
{
    return c::withCatch(
        [wwd = reinterpret_cast<CWorkerD*>(w), timeout_ms]() {
            if (auto tp = wwd->w->waitForTopic(std::chrono::milliseconds(timeout_ms)); tp) {
                wwd->evd.tp = *tp;
                return reinterpret_cast<CTopic*>(&wwd->evd.tp);
            } else {
                return static_cast<CTopic*>(nullptr);
            }
        },
        []() {
            return static_cast<CTopic*>(nullptr);
        });
}

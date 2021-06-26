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
#include "fuurin/errors.h"
#include "fuurin/logger.h"

#include <memory>
#include <string_view>


using namespace std::literals::string_view_literals;

using namespace fuurin;


namespace {
struct CWorkerD
{
    std::unique_ptr<Worker> w;
    std::future<void> f;
};
} // namespace


CWorker* CWorker_new(CUuid id, unsigned long long seqn, const char* name)
{
    try {
        Uuid::Bytes bytes;
        static_assert(sizeof(seqn) == sizeof(Topic::SeqN));
        static_assert(bytes.size() == sizeof(id.bytes));

        auto ret = new CWorkerD;

        std::copy_n(id.bytes, sizeof(id.bytes), bytes.begin());
        ret->w = std::make_unique<Worker>(Uuid::fromBytes(bytes), seqn, name);

        return reinterpret_cast<CWorker*>(ret);

    } catch (const std::exception&) {
        return nullptr;
    }
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
    CUuid ret;
    const auto id = reinterpret_cast<CWorkerD*>(w)->w->uuid();
    static_assert(sizeof(ret.bytes) == id.size());
    std::copy_n(id.bytes().begin(), id.size(), ret.bytes);
    return ret;
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
    CWorkerD* wd = reinterpret_cast<CWorkerD*>(w);
    if (!wd->f.valid())
        return;

    try {
        wd->f.get();
    } catch (const err::Error& e) {
        log::Arg args[] = {log::Arg{"error"sv, std::string_view(e.what())}, e.arg()};
        log::Logger::error(e.loc(), args, 2);
    } catch (const std::exception&) {
    }
}


bool CWorker_isRunning(CWorker* w)
{
    return reinterpret_cast<CWorkerD*>(w)->w->isRunning();
}

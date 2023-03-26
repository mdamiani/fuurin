/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/c/cbroker.h"
#include "fuurin/logger.h"
#include "cbrokerd.h"
#include "cutils.h"

#include <string_view>


using namespace fuurin;


CBroker* CBroker_new(CUuid* id, const char* name)
{
    auto ret = new c::CBrokerD;

    return c::withCatch(
        [ret, id, name]() {
            ret->b = std::make_unique<Broker>(c::uuidConvert(*id), name);
            return c::getOpaque(ret);
        },
        [ret]() {
            delete ret;
            return static_cast<CBroker*>(nullptr);
        });
}


void CBroker_delete(CBroker* b)
{
    delete c::getPrivD(b);
}


const char* CBroker_name(CBroker* b)
{
    return c::getPrivD(b)->b->name().data();
}


CUuid CBroker_uuid(CBroker* b)
{
    return c::uuidConvert(c::getPrivD(b)->b->uuid());
}


void CBroker_addEndpoints(CBroker* b, const char* delivery, const char* dispatch, const char* snapshot)
{
    Broker* bb = c::getPrivD(b)->b.get();

    auto endp1 = bb->endpointDelivery();
    auto endp2 = bb->endpointDispatch();
    auto endp3 = bb->endpointSnapshot();

    endp1.push_back(delivery);
    endp2.push_back(dispatch);
    endp3.push_back(snapshot);

    bb->setEndpoints(endp1, endp2, endp3);
}


void CBroker_clearEndpoints(CBroker* b)
{
    c::getPrivD(b)->b->setEndpoints({}, {}, {});
}


const char* CBroker_endpointDelivery(CBroker* b)
{
    const auto& endp = c::getPrivD(b)->b->endpointDelivery();
    return endp.empty() ? nullptr : endp.front().data();
}


const char* CBroker_endpointDispatch(CBroker* b)
{
    const auto& endp = c::getPrivD(b)->b->endpointDispatch();
    return endp.empty() ? nullptr : endp.front().data();
}


const char* CBroker_endpointSnapshot(CBroker* b)
{
    const auto& endp = c::getPrivD(b)->b->endpointSnapshot();
    return endp.empty() ? nullptr : endp.front().data();
}


void CBroker_start(CBroker* b)
{
    auto bd = c::getPrivD(b);
    auto f = bd->b->start();
    if (!f.valid())
        return;

    CBroker_wait(b);
    bd->f = std::move(f);
}


void CBroker_stop(CBroker* b)
{
    c::getPrivD(b)->b->stop();
}


void CBroker_wait(CBroker* b)
{
    auto bd = c::getPrivD(b);
    if (!bd->f.valid())
        return;

    return c::withCatch(
        [bd]() {
            bd->f.get();
        },
        []() {});
}


bool CBroker_isRunning(CBroker* b)
{
    return c::getPrivD(b)->b->isRunning();
}

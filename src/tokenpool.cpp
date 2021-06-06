/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/tokenpool.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpart.h"
#include "failure.h"


namespace fuurin {


TokenPool::TokenPool()
    : zmqCtx_{std::make_unique<zmq::Context>()}
    , zmqPut_{std::make_unique<zmq::Socket>(zmqCtx_.get(), zmq::Socket::CLIENT)}
    , zmqGet_{std::make_unique<zmq::Socket>(zmqCtx_.get(), zmq::Socket::SERVER)}
{
    zmqGet_->setEndpoints({"inproc://token_pool_channel_putget"});
    zmqPut_->setEndpoints({"inproc://token_pool_channel_putget"});

    zmqGet_->bind();
    zmqPut_->connect();
}


TokenPool::TokenPool(id_t idMin, id_t idMax)
    : TokenPool{}
{
    for (id_t id = idMin; id <= idMax; ++id)
        put(id);
}


TokenPool::~TokenPool() noexcept
{
}


auto TokenPool::put(id_t id) -> void
{
    zmqPut_->send(zmq::Part{id});
}


auto TokenPool::get() -> id_t
{
    zmq::Part id;
    const int r = zmqGet_->recv(&id);

    static_assert(sizeof(id_t) == sizeof(uint32_t),
        "TokenPool::get(), id_t and uint32_t sizes don't match");

    ASSERT(r == sizeof(uint32_t),
        "TokenPool::get(), bad received bytes count");

    return id.toUint32();
}


auto TokenPool::tryGet() -> std::optional<id_t>
{
    zmq::Part id;
    const int r = zmqGet_->tryRecv(&id);

    if (r == -1)
        return {};

    ASSERT(r == sizeof(uint32_t),
        "TokenPool::tryGet(), bad received bytes count");

    return {id.toUint32()};
}

} // namespace fuurin

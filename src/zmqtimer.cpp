/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/zmqtimer.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"


using namespace std::literals::string_literals;


namespace fuurin {
namespace zmq {


Timer::Timer(Context* ctx, const std::string& name)
    : ctx_{ctx}
    , name_{name}
    , trigger_{std::make_unique<zmq::Socket>(ctx, zmq::Socket::PAIR)}
    , receiver_{std::make_unique<zmq::Socket>(ctx, zmq::Socket::PAIR)}
{
    const std::string endpoint = "inproc://"s + name_;

    trigger_->setEndpoints({endpoint});
    receiver_->setEndpoints({endpoint});

    receiver_->bind();
    trigger_->connect();
}


Timer::~Timer() noexcept
{
}


void* Timer::zmqPointer() const noexcept
{
    return receiver_->zmqPointer();
}


bool Timer::isOpen() const noexcept
{
    return true;
}


std::string Timer::description() const
{
    return name_;
}

} // namespace zmq
} // namespace fuurin

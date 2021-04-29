/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/zmqcancelable.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "zmqiotimer.h"


using namespace std::literals::string_literals;
using namespace std::literals::chrono_literals;

namespace fuurin {
namespace zmq {


Cancelable::Cancelable(Context* ctx, const std::string& name)
    : ctx_{ctx}
    , name_{name}
    , trigger_{std::make_unique<zmq::Socket>(ctx, zmq::Socket::RADIO)}
    , receiver_{std::make_unique<zmq::Socket>(ctx, zmq::Socket::DISH)}
    , deadline_{0ms}
{
    const std::string endpoint = "inproc://"s + name_;

    trigger_->setEndpoints({endpoint});
    receiver_->setEndpoints({endpoint});

    receiver_->setGroups({"deadln"});

    receiver_->bind();
    trigger_->connect();
}


Cancelable::~Cancelable() noexcept
{
    stop();
}


void* Cancelable::zmqPointer() const noexcept
{
    return receiver_->zmqPointer();
}


bool Cancelable::isOpen() const noexcept
{
    return true;
}


std::string Cancelable::description() const
{
    return name_;
}


Cancelable& Cancelable::withDeadline(std::chrono::milliseconds timeout)
{
    setDeadline(timeout);
    return *this;
}


void Cancelable::setDeadline(std::chrono::milliseconds timeout)
{
    if (timeout < 0ms)
        return;

    start(timeout);
}


std::chrono::milliseconds Cancelable::deadline() const noexcept
{
    return deadline_;
}


void Cancelable::cancel()
{
    start(0ms);
}


bool Cancelable::isCanceled() const
{
    Poller poll{PollerEvents::Type::Read, 0ms, const_cast<Cancelable*>(this)};
    return !poll.wait().empty();
}


void Cancelable::start(std::chrono::milliseconds timeout)
{
    if (isCanceled())
        return;

    deadline_ = timeout;

    auto tup = IOSteadyTimer::makeIOSteadyTimer(ctx_, timeout, true,
        Part{uint8_t(1)}.withGroup("deadln"), trigger_.get());
    cancelFuture_ = std::move(std::get<0>(tup));
    timer_.reset(std::get<1>(tup));

    IOSteadyTimer::postTimerStart(ctx_, timer_.get());
}


void Cancelable::stop()
{
    if (!timer_)
        return;

    IOSteadyTimer::postTimerCancel(ctx_, timer_.get());

    // wait for timer completion.
    cancelFuture_.get();

    timer_.reset();
}

} // namespace zmq
} // namespace fuurin

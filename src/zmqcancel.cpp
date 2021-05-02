/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/zmqcancel.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "zmqiotimer.h"


using namespace std::literals::string_literals;
using namespace std::literals::chrono_literals;

namespace fuurin {
namespace zmq {


Cancellation::Cancellation(Context* ctx, const std::string& name)
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


Cancellation::~Cancellation() noexcept
{
    stop();
}


void* Cancellation::zmqPointer() const noexcept
{
    return receiver_->zmqPointer();
}


bool Cancellation::isOpen() const noexcept
{
    return true;
}


std::string Cancellation::description() const
{
    return name_;
}


Cancellation& Cancellation::withDeadline(std::chrono::milliseconds timeout)
{
    setDeadline(timeout);
    return *this;
}


void Cancellation::setDeadline(std::chrono::milliseconds timeout)
{
    if (timeout < 0ms)
        return;

    start(timeout);
}


std::chrono::milliseconds Cancellation::deadline() const noexcept
{
    return deadline_;
}


void Cancellation::cancel()
{
    start(0ms);
}


bool Cancellation::isCanceled() const
{
    Poller poll{PollerEvents::Type::Read, 0ms, const_cast<Cancellation*>(this)};
    return !poll.wait().empty();
}


void Cancellation::start(std::chrono::milliseconds timeout)
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


void Cancellation::stop()
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

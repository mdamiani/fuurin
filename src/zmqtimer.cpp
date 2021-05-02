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
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqpart.h"
#include "zmqiotimer.h"


using namespace std::literals::string_literals;
using namespace std::literals::chrono_literals;


namespace fuurin {
namespace zmq {


Timer::Timer(Context* ctx, const std::string& name)
    : ctx_{ctx}
    , name_{name}
    , trigger_{std::make_unique<zmq::Socket>(ctx, zmq::Socket::PUSH)}
    , receiver_{std::make_unique<zmq::Socket>(ctx, zmq::Socket::PULL)}
    , interval_{0ms}
    , singleshot_{false}
{
    const std::string endpoint = "inproc://"s + name_;

    trigger_->setEndpoints({endpoint});
    receiver_->setEndpoints({endpoint});

    trigger_->setConflate(true);
    receiver_->setConflate(true);

    receiver_->bind();
    trigger_->connect();
}


Timer::~Timer() noexcept
{
    stop();
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


void Timer::setInterval(std::chrono::milliseconds value) noexcept
{
    if (value < 0ms)
        value = 0ms;

    interval_ = value;
}


std::chrono::milliseconds Timer::interval() const noexcept
{
    return interval_;
}


void Timer::setSingleShot(bool value) noexcept
{
    singleshot_ = value;
}


bool Timer::isSingleShot() const noexcept
{
    return singleshot_;
}


void Timer::start()
{
    stop();

    auto tup = IOSteadyTimer::makeIOSteadyTimer(ctx_, interval_, singleshot_,
        Part{uint8_t(1)}, trigger_.get());
    cancelFuture_ = std::move(std::get<0>(tup));
    timer_.reset(std::get<1>(tup));

    IOSteadyTimer::postTimerStart(ctx_, timer_.get());
}


void Timer::stop()
{
    if (!timer_)
        return;

    IOSteadyTimer::postTimerCancel(ctx_, timer_.get());

    // wait for timer completion.
    cancelFuture_.get();

    timer_.reset();
}


void Timer::consume()
{
    Part p;
    receiver_->recv(&p);
}


bool Timer::isExpired() const
{
    Poller poll{PollerEvents::Type::Read, 0ms, const_cast<Timer*>(this)};
    return !poll.wait().empty();
}


bool Timer::isActive()
{
    return cancelFuture_.valid() &&
        cancelFuture_.wait_for(0ms) != std::future_status::ready;
}

} // namespace zmq
} // namespace fuurin
